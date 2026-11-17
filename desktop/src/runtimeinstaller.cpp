#include "runtimeinstaller.h"

#include "settings.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QXmlStreamReader>

namespace {
constexpr auto kRepositoryUrl = "https://dl.google.com/android/repository/repository2-1.xml";
constexpr auto kJavaReleasesUrl = "https://api.github.com/repos/adoptium/temurin17-binaries/releases/latest";
constexpr auto kAvdName = "OpenSrc_Default_API_35";
constexpr auto kSystemImage = "system-images;android-35;google_apis;x86_64";

// Each phase of setup owns a slice of the progress bar so that the bar moves
// monotonically even though individual downloads and sdkmanager runs report
// their own percentages.
constexpr int kToolsSliceBegin = 2;
constexpr int kToolsSliceEnd = 16;
constexpr int kJavaSliceBegin = 16;
constexpr int kJavaSliceEnd = 46;
constexpr int kPackagesSliceBegin = 48;
constexpr int kPackagesSliceEnd = 96;

bool copyDirectory(const QString &source, const QString &destination)
{
    QDir sourceDir(source);
    if (!sourceDir.exists() || !QDir().mkpath(destination)) return false;
    const QFileInfoList entries =
        sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    for (const QFileInfo &entry : entries) {
        const QString target = QDir(destination).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), target)) return false;
        } else {
            if (QFile::exists(target) && !QFile::remove(target)) return false;
            if (!QFile::copy(entry.absoluteFilePath(), target)) return false;
        }
    }
    return true;
}

// Returns the JRE/JDK root (the directory containing bin\java.exe), which is
// what JAVA_HOME must point at.
QString findJavaHome(const QString &root)
{
    if (root.isEmpty()) return {};
    QDirIterator it(root, {"java.exe"}, QDir::Files, QDirIterator::Subdirectories);
    if (!it.hasNext()) return {};
    const QString binDir = QFileInfo(it.next()).absolutePath();
    return QDir::cleanPath(QFileInfo(binDir).absolutePath());
}

#ifdef Q_OS_WIN
// cmd.exe requires every argument to be quoted when the batch path contains
// spaces, and sdkmanager/avdmanager arguments never contain double quotes.
QString quoteForCmd(const QString &value)
{
    QString cleaned = value;
    cleaned.remove('"');
    return '"' + cleaned + '"';
}
#endif

bool isInstalledJava(const QString &root)
{
    return !findJavaHome(root).isEmpty();
}
}  // namespace

RuntimeInstaller::RuntimeInstaller(QObject *parent) : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        handleOutput(m_process.readAllStandardOutput());
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        handleOutput(m_process.readAllStandardError());
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &RuntimeInstaller::processFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && m_stage != Stage::Idle)
            fail("Could not start the Android runtime setup tool: " + m_process.errorString());
    });
}

QString RuntimeInstaller::defaultRuntimeRoot()
{
    return defaultAndroidRuntimeRoot();
}

QString RuntimeInstaller::defaultAvdName()
{
    return QString::fromLatin1(kAvdName);
}

QString RuntimeInstaller::systemImageId()
{
    return QString::fromLatin1(kSystemImage);
}

QString RuntimeInstaller::sdkRootFor(const QString &runtimeRoot)
{
    return androidRuntimeSdkRoot(runtimeRoot);
}

QString RuntimeInstaller::avdHome(const QString &runtimeRoot)
{
    return androidRuntimeAvdHome(runtimeRoot);
}

QString RuntimeInstaller::avdIniPath(const QString &runtimeRoot, const QString &avdName)
{
    return QDir(androidRuntimeAvdHome(runtimeRoot)).filePath(avdName.trimmed() + ".ini");
}

QString RuntimeInstaller::emulatorExecutable(const QString &runtimeRoot)
{
    return androidRuntimeEmulator(runtimeRoot);
}

bool RuntimeInstaller::isBusy() const
{
    return m_stage != Stage::Idle;
}

QString RuntimeInstaller::sdkManagerPath() const
{
#ifdef Q_OS_WIN
    return QDir(m_sdkRoot).filePath("cmdline-tools/latest/bin/sdkmanager.bat");
#else
    return QDir(m_sdkRoot).filePath("cmdline-tools/latest/bin/sdkmanager");
#endif
}

QString RuntimeInstaller::avdManagerPath() const
{
#ifdef Q_OS_WIN
    return QDir(m_sdkRoot).filePath("cmdline-tools/latest/bin/avdmanager.bat");
#else
    return QDir(m_sdkRoot).filePath("cmdline-tools/latest/bin/avdmanager");
#endif
}

QString RuntimeInstaller::emulatorPath() const
{
    return androidRuntimeEmulator(m_runtimeRoot);
}

QString RuntimeInstaller::javaHome() const
{
    return findJavaHome(QDir(m_sdkRoot).filePath("openjdk"));
}

QProcessEnvironment RuntimeInstaller::toolEnvironment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString java = javaHome();
    env.insert("ANDROID_SDK_ROOT", m_sdkRoot);
    env.insert("ANDROID_HOME", m_sdkRoot);
    env.insert("ANDROID_USER_HOME", m_userHome);
    env.insert("ANDROID_PREFS_ROOT", m_userHome);
    env.insert("ANDROID_AVD_HOME", m_avdHome);
    if (!java.isEmpty()) {
        env.insert("JAVA_HOME", java);
        env.insert("PATH", QDir(java).filePath("bin") + QDir::listSeparator() + env.value("PATH"));
    }
    return env;
}

void RuntimeInstaller::start(const QString &runtimeRoot)
{
    if (isBusy()) return;
#ifndef Q_OS_WIN
    Q_UNUSED(runtimeRoot)
    emit finished(false,
                  "The self-managed Android runtime installer currently supports Windows x64 only.");
#else
    m_runtimeRoot = runtimeRoot.trimmed().isEmpty() ? defaultRuntimeRoot() : runtimeRoot.trimmed();
    m_sdkRoot = sdkRootFor(m_runtimeRoot);
    m_avdHome = avdHome(m_runtimeRoot);
    m_userHome = QDir(m_runtimeRoot).filePath("android-user");
    const QStringList required{m_runtimeRoot, m_sdkRoot, m_avdHome, m_userHome,
                               QDir(m_sdkRoot).filePath("licenses")};
    for (const QString &folder : required) {
        if (!QDir().mkpath(folder)) {
            emit finished(false, "OpenSrc could not create its private runtime folder: " + folder);
            return;
        }
    }
    if (!m_tempDir.isValid()) {
        emit finished(false,
                      "Could not create a temporary folder for runtime setup. Check free space on "
                      "your system drive.");
        return;
    }
    setProgress(0);
    m_stage = Stage::Idle;
    m_lastToolPercent = 0;
    if (!QFile::exists(sdkManagerPath())) {
        emit statusChanged("Fetching Android command-line tools…");
        requestRepositoryMetadata();
        return;
    }
    // Resume: tooling exists, so only re-run the steps that are still missing.
    if (!isInstalledJava(QDir(m_sdkRoot).filePath("openjdk"))) {
        emit statusChanged("Fetching the private Java runtime needed by Android tools…");
        requestJavaMetadata();
        return;
    }
    // Licenses and packages are both handled by sdkmanager, which is idempotent:
    // already-installed packages are reported and skipped.
    setProgress(kJavaSliceEnd);
    beginLicenses();
#endif
}

void RuntimeInstaller::cancel()
{
    if (m_stage == Stage::Idle && !m_reply) return;
    // Leave the active stage before tearing anything down: killing the child
    // process makes it emit finished(), and processFinished() must not advance
    // the setup chain with a half-run tool.
    m_stage = Stage::Idle;
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_downloadFile.isOpen()) m_downloadFile.close();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
    emit statusChanged("Runtime setup cancelled");
    emit finished(false, "Runtime setup was cancelled.");
}

void RuntimeInstaller::setProgress(int percent)
{
    const int clamped = qBound(0, percent, 100);
    emit progressChanged(clamped);
}

void RuntimeInstaller::requestRepositoryMetadata()
{
    m_stage = Stage::DownloadMetadata;
    setProgress(kToolsSliceBegin);
    QNetworkRequest request{QUrl(QString::fromLatin1(kRepositoryUrl))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply) return;
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            fail("Could not fetch Android SDK package metadata: " + error);
            return;
        }
        const QByteArray xmlData = reply->readAll();
        reply->deleteLater();
        QXmlStreamReader xml(xmlData);
        bool inToolsPackage = false;
        bool inArchive = false;
        bool archiveIsWindows = false;
        bool archiveIsStable = false;
        bool lastWasStable = false;
        QString archiveUrl;
        QString archiveHash;
        QString selectedUrl;
        QString selectedHash;
        bool selectedStable = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == QLatin1String("remotePackage")) {
                    inToolsPackage =
                        xml.attributes().value("path") == QLatin1String("cmdline-tools;latest");
                    lastWasStable = false;
                } else if (inToolsPackage && xml.name() == QLatin1String("channelRef")) {
                    // channel-0 is the stable channel; anything else is a preview.
                    lastWasStable = xml.attributes().value("ref") == QLatin1String("channel-0");
                } else if (inToolsPackage && xml.name() == QLatin1String("archive")) {
                    inArchive = true;
                    archiveIsWindows = false;
                    archiveIsStable = lastWasStable;
                    archiveUrl.clear();
                    archiveHash.clear();
                } else if (inArchive && xml.name() == QLatin1String("host-os")) {
                    archiveIsWindows = xml.readElementText() == QLatin1String("windows");
                } else if (inArchive && xml.name() == QLatin1String("url")) {
                    archiveUrl = xml.readElementText();
                } else if (inArchive && xml.name() == QLatin1String("checksum")) {
                    // repository2-1.xml omits the type attribute for SHA-1, so the
                    // algorithm is inferred from the digest length instead.
                    const QString value = xml.readElementText().trimmed();
                    const QString type = xml.attributes().value("type").toString().toLower();
                    if (value.size() == 40 || value.size() == 64) archiveHash = value;
                    else if (type == QLatin1String("sha1") || type == QLatin1String("sha256"))
                        archiveHash = value;
                }
            } else if (xml.isEndElement()) {
                if (xml.name() == QLatin1String("archive") && inArchive) {
                    // Prefer the first stable-channel Windows archive; fall back to
                    // any Windows archive if the package only ships previews.
                    if (archiveIsWindows && !archiveUrl.isEmpty() && !selectedStable) {
                        selectedUrl = archiveUrl;
                        selectedHash = archiveHash;
                        if (archiveIsStable) selectedStable = true;
                    }
                    inArchive = false;
                } else if (xml.name() == QLatin1String("remotePackage") && inToolsPackage) {
                    inToolsPackage = false;
                }
            }
        }
        if (xml.hasError() || selectedUrl.isEmpty()) {
            fail("Android's package catalog did not contain the Windows command-line tools archive.");
            return;
        }
        // Archive URLs in the catalog are relative to Google's repository root.
        QUrl url(QStringLiteral("https://dl.google.com/android/repository/"));
        url = url.resolved(QUrl(selectedUrl));
        m_toolsArchive = QDir(m_tempDir.path()).filePath("commandlinetools.zip");
        downloadFile(url, m_toolsArchive, Stage::DownloadTools, kToolsSliceBegin, kToolsSliceEnd,
                     selectedHash);
    });
}

void RuntimeInstaller::requestJavaMetadata()
{
    m_stage = Stage::DownloadJavaMetadata;
    setProgress(kJavaSliceBegin);
    QNetworkRequest request{QUrl(QString::fromLatin1(kJavaReleasesUrl))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "OpenSrc-Android-Runtime-Setup");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply) return;
        if (reply->error() != QNetworkReply::NoError) {
            const QString error = reply->errorString();
            reply->deleteLater();
            fail("Could not fetch the private Java runtime package metadata: " + error);
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        if (!doc.isObject()) {
            fail("The private Java runtime package metadata was not valid JSON.");
            return;
        }
        const QJsonArray assets = doc.object().value("assets").toArray();
        QUrl downloadUrl;
        QString expectedHash;
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value("name").toString();
            if (name.contains("jre_x64_windows_hotspot") && name.endsWith(".zip")) {
                downloadUrl = QUrl(asset.value("browser_download_url").toString());
                expectedHash = asset.value("digest").toString();
                break;
            }
        }
        if (downloadUrl.isEmpty()) {
            fail("The Java runtime release catalog did not contain a Windows x64 JRE package.");
            return;
        }
        m_javaArchive = QDir(m_tempDir.path()).filePath("openjdk17.zip");
        downloadFile(downloadUrl, m_javaArchive, Stage::DownloadJava, kJavaSliceBegin, kJavaSliceEnd,
                     expectedHash);
    });
}

void RuntimeInstaller::downloadFile(const QUrl &url, const QString &path, Stage stage,
                                    int progressOffset, int progressSpan, const QString &expectedHash)
{
    if (m_downloadFile.isOpen()) m_downloadFile.close();
    m_stage = stage;
    m_progressOffset = progressOffset;
    m_progressSpan = progressSpan;
    m_hashAlgorithm = expectedHash.startsWith("sha256:", Qt::CaseInsensitive) ? "sha256" : "sha1";
    QString expected = expectedHash;
    if (expected.startsWith("sha256:", Qt::CaseInsensitive) ||
        expected.startsWith("sha1:", Qt::CaseInsensitive))
        expected = expected.section(':', 1);
    m_expectedHash = expected;
    m_downloadFile.setFileName(path);
    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Could not create the download file: " + path);
        return;
    }
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    if (url.host().contains("github.com") || url.host().contains("githubusercontent"))
        request.setRawHeader("User-Agent", "OpenSrc-Android-Runtime-Setup");
    m_reply = m_network.get(request);
    QNetworkReply *const reply = m_reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (m_reply != reply || !m_downloadFile.isOpen()) return;
        const QByteArray bytes = reply->readAll();
        if (m_downloadFile.write(bytes) != bytes.size())
            fail("Disk write failed while downloading runtime components.");
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this, reply](qint64 received, qint64 total) {
                if (m_reply != reply || total <= 0) return;
                const int fraction = static_cast<int>((received * 100) / total);
                setProgress(m_progressOffset + (fraction * m_progressSpan) / 100);
            });
    connect(m_reply, &QNetworkReply::finished, this, [this, reply, path, stage] {
        // A cancelled or superseded reply must not advance the pipeline.
        if (m_reply != reply) return;
        m_reply = nullptr;
        const QByteArray tail = reply->readAll();
        if (!tail.isEmpty() && m_downloadFile.isOpen()) m_downloadFile.write(tail);
        const auto networkError = reply->error();
        const QString networkMessage = reply->errorString();
        reply->deleteLater();
        if (m_downloadFile.isOpen()) m_downloadFile.close();
        if (networkError != QNetworkReply::NoError) {
            fail("Runtime download failed: " + networkMessage);
            return;
        }
        if (!m_expectedHash.isEmpty()) {
            emit statusChanged("Verifying the downloaded package…");
            const QCryptographicHash::Algorithm algorithm =
                m_hashAlgorithm == "sha256" ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1;
            QCryptographicHash hash(algorithm);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                fail("Could not verify the downloaded runtime archive.");
                return;
            }
            while (!file.atEnd()) hash.addData(file.read(1024 * 1024));
            if (QString::fromLatin1(hash.result().toHex()) != m_expectedHash.toLower()) {
                fail("Runtime archive checksum verification failed; the damaged download was rejected.");
                return;
            }
        }
        if (stage == Stage::DownloadTools) extractTools();
        else if (stage == Stage::DownloadJava)
            extractArchive(m_javaArchive, "Unpacking the private Java runtime…", Stage::ExtractJava);
    });
}

void RuntimeInstaller::extractArchive(const QString &archive, const QString &description,
                                      Stage nextStage)
{
    if (!QFile::exists(archive)) {
        fail("The downloaded runtime archive is missing.");
        return;
    }
    const QString tempOutput = QDir(m_tempDir.path()).filePath(
        nextStage == Stage::ExtractTools ? "tools" : "java");
    QDir().mkpath(tempOutput);
    emit statusChanged(description);
    // Never start a new child while the previous one is still alive: the finished
    // signal identifies the stage by the current member state.
    if (m_process.state() != QProcess::NotRunning && !m_process.waitForFinished(1000)) {
        fail("A previous setup step did not finish before unpacking runtime files.");
        return;
    }
    m_stage = nextStage;
    m_process.setProgram("tar.exe");
    m_process.setArguments({"-xf", archive, "-C", tempOutput});
    m_process.setWorkingDirectory(m_tempDir.path());
    m_process.start();
    if (!m_process.waitForStarted(5000))
        fail("Windows tar.exe could not be started to unpack runtime files: " +
             m_process.errorString());
}

void RuntimeInstaller::extractTools()
{
    extractArchive(m_toolsArchive, "Unpacking Android command-line tools…", Stage::ExtractTools);
}

void RuntimeInstaller::installExtractedTools()
{
    emit statusChanged("Installing Android command-line tools into OpenSrc…");
    const QString output = QDir(m_tempDir.path()).filePath("tools");
    // Archives from Google contain a top-level "cmdline-tools" folder. Some builds
    // wrap it one level deeper, so accept either layout.
    QString source = QDir(output).filePath("cmdline-tools");
    if (!QFileInfo::exists(QDir(source).filePath("bin"))) {
        // next() mutates the iterator, so it cannot be const.
        QDirIterator it(output, {"cmdline-tools"}, QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
        if (it.hasNext()) source = it.next();
    }
    const QString destination = QDir(m_sdkRoot).filePath("cmdline-tools/latest");
    if (!copyDirectory(source, destination)) {
        fail("Could not install the downloaded Android command-line tools into OpenSrc's runtime folder.");
        return;
    }
    if (!QFile::exists(sdkManagerPath())) {
        fail("The Android command-line tools archive was not in the expected format.");
        return;
    }
    if (!isInstalledJava(QDir(m_sdkRoot).filePath("openjdk"))) requestJavaMetadata();
    else beginLicenses();
}

void RuntimeInstaller::beginLicenses()
{
    emit statusChanged("Accepting the Android SDK licenses you approved…");
    setProgress(kPackagesSliceBegin);
    runToolBatch(sdkManagerPath(), {"--sdk_root=" + m_sdkRoot, "--licenses"},
                 Stage::AcceptLicenses, QByteArray("y\n").repeated(64),
                 "Android SDK license acceptance failed");
}

void RuntimeInstaller::installPackages()
{
    emit statusChanged("Downloading Android Emulator, platform tools, and the Android 15 system image…");
    setProgress(kPackagesSliceBegin);
    runToolBatch(sdkManagerPath(),
                 {"--sdk_root=" + m_sdkRoot, "--install", "emulator", "platform-tools",
                  QString::fromLatin1(kSystemImage)},
                 Stage::InstallPackages, QByteArray("y\n").repeated(64),
                 "Android SDK package installation failed");
}

void RuntimeInstaller::createAvd()
{
    const QString avdIni = QDir(m_avdHome).filePath(QString::fromLatin1(kAvdName) + ".ini");
    if (QFile::exists(avdIni) && QFile::exists(QDir(m_avdHome).filePath(
                                    QString::fromLatin1(kAvdName) + ".avd/config.ini"))) {
        m_stage = Stage::Idle;
        setProgress(100);
        emit statusChanged("OpenSrc Android runtime is ready");
        emit finished(true, {});
        return;
    }
    if (!QFile::exists(emulatorPath()) || !QFileInfo::exists(QDir(m_sdkRoot).filePath("system-images/android-35/google_apis/x86_64"))) {
        fail("The Android Emulator or system image is still missing after package installation.");
        return;
    }
    emit statusChanged("Creating the default OpenSrc Android device…");
    setProgress(kPackagesSliceEnd);
    runToolBatch(avdManagerPath(),
                 {"--sdk_root=" + m_sdkRoot, "create", "avd", "--force", "--name",
                  QString::fromLatin1(kAvdName), "--package", QString::fromLatin1(kSystemImage),
                  "--device", "pixel_2"},
                 Stage::CreateAvd, "no\n", "Creating the OpenSrc Android device failed");
}

void RuntimeInstaller::runToolBatch(const QString &batch, const QStringList &arguments, Stage stage,
                                    const QByteArray &input, const QString &description)
{
    m_stage = stage;
    m_failureDescription = description;
    // sdkmanager.bat / avdmanager.bat are batch scripts, and CreateProcess (used
    // by QProcess) cannot execute them directly. Route them through cmd.exe.
    // The /s /c form with a wrapping pair of quotes makes cmd strip exactly the
    // outer pair and use the rest verbatim, so quoted paths survive.
#ifdef Q_OS_WIN
    QStringList parts{quoteForCmd(batch)};
    for (const QString &argument : arguments) parts << quoteForCmd(argument);
    const QString command = '"' + parts.join(' ') + '"';
    m_process.setProgram("cmd.exe");
    m_process.setArguments({});
    m_process.setNativeArguments("/d /s /c " + command);
#else
    m_process.setProgram(batch);
    m_process.setArguments(arguments);
#endif
    m_process.setWorkingDirectory(m_sdkRoot);
    m_process.setProcessEnvironment(toolEnvironment());
    m_process.start();
    if (!m_process.waitForStarted(10000)) {
        fail(description + ": " + m_process.errorString());
        return;
    }
    if (!input.isEmpty()) m_process.write(input);
    // Every tool call either needs no further input or has already been given the
    // license answers the user accepted, so stdin is closed to prevent hangs.
    m_process.closeWriteChannel();
}

void RuntimeInstaller::handleOutput(const QByteArray &bytes)
{
    const QString text = QString::fromLocal8Bit(bytes);
    for (const QString &line : text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        emit logReceived(trimmed);
        // sdkmanager redraws progress with carriage returns, so keep a bounded
        // tail of real output to quote when a stage fails.
        if (!trimmed.startsWith("[") && !trimmed.startsWith("  progress")) {
            m_outputTail.append(trimmed);
            while (m_outputTail.size() > 12) m_outputTail.removeFirst();
        }
    }
    if (m_stage != Stage::InstallPackages) return;
    // sdkmanager draws "[====>   ] 42% Unpacking ..." progress and uses carriage
    // returns, so the whole chunk is scanned and the highest value kept.
    static const QRegularExpression progressPattern(R"((\d{1,3})\s*%)");
    auto it = progressPattern.globalMatch(text);
    int best = -1;
    while (it.hasNext()) {
        const int value = it.next().captured(1).toInt();
        if (value <= 100) best = qMax(best, value);
    }
    if (best < 0) return;
    m_lastToolPercent = qMax(m_lastToolPercent, best);
    const int span = kPackagesSliceEnd - kPackagesSliceBegin;
    setProgress(kPackagesSliceBegin + (m_lastToolPercent * span) / 100);
}

void RuntimeInstaller::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_stage == Stage::Idle) return;
    if (m_finishing) return;
    const Stage finishedStage = m_stage;
    const QString description = m_failureDescription;
    m_failureDescription.clear();
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        fail(description.isEmpty()
                 ? QString("Android runtime setup stage failed (exit code %1).").arg(exitCode)
                 : QString("%1 (exit code %2).").arg(description).arg(exitCode));
        return;
    }
    if (finishedStage == Stage::ExtractTools) {
        installExtractedTools();
    } else if (finishedStage == Stage::ExtractJava) {
        const QString java = findJavaHome(QDir(m_tempDir.path()).filePath("java"));
        if (java.isEmpty()) {
            fail("The downloaded Java runtime did not contain java.exe.");
            return;
        }
        const QString destination = QDir(m_sdkRoot).filePath("openjdk");
        if (!copyDirectory(java, destination)) {
            fail("Could not install the private Java runtime into OpenSrc's runtime folder.");
            return;
        }
        if (javaHome().isEmpty()) {
            fail("The private Java runtime could not be found after installation.");
            return;
        }
        beginLicenses();
    } else if (finishedStage == Stage::AcceptLicenses) {
        installPackages();
    } else if (finishedStage == Stage::InstallPackages) {
        createAvd();
    } else if (finishedStage == Stage::CreateAvd) {
        m_stage = Stage::Idle;
        setProgress(100);
        emit statusChanged("OpenSrc Android runtime is ready");
        emit finished(true, {});
    }
}

void RuntimeInstaller::fail(const QString &reason)
{
    // fail() can be reached from a process-signal handler or a network callback,
    // and tearing down the process re-enters processFinished(). Guard so the
    // failure is reported exactly once.
    if (m_finishing) return;
    m_finishing = true;
    const auto finishGuard = qScopeGuard([this] { m_finishing = false; });
    if (m_stage == Stage::Idle && !m_reply) return;
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (m_downloadFile.isOpen()) m_downloadFile.close();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(3000);
    }
    m_stage = Stage::Idle;
    emit statusChanged("Android runtime setup failed");
    emit logReceived(reason);
    // The last lines of tool output are what actually explain a failure (for
    // example sdkmanager's own message), so surface them with the failure.
    for (const QString &line : m_outputTail) emit logReceived("  last output: " + line);
    QString detail = reason;
    if (!m_outputTail.isEmpty()) detail += "\n\nLast tool output:\n" + m_outputTail.join('\n');
    m_outputTail.clear();
    emit finished(false, detail);
}
