#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString configPath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(root);
    return root + "/settings.json";
}

// True when OpenSrc can actually create files in this folder. Used to decide
// whether the runtime may live next to the executable (the installer creates
// C:\OpenSrc\Runtime with per-user write access) or must fall back to the
// user's local application data.
bool ensureWritableDirectory(const QString &path)
{
    if (path.isEmpty() || !QDir().mkpath(path)) return false;
    const QString probePath = QDir(path).filePath(".opensrc-write-test");
    QFile probe(probePath);
    if (!probe.open(QIODevice::WriteOnly)) return false;
    probe.close();
    QFile::remove(probePath);
    return true;
}
}  // namespace

QString defaultAndroidRuntimeRoot()
{
    static const QString resolved = [] {
        // Preferred: a Runtime folder beside OpenSrc.exe, i.e. on the drive where
        // OpenSrc was installed (C: by default in the Windows installer).
        const QString besideExe = QDir(QCoreApplication::applicationDirPath()).filePath("Runtime");
        if (ensureWritableDirectory(besideExe)) return QDir::cleanPath(besideExe);
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (ensureWritableDirectory(base))
            return QDir::cleanPath(QDir(base).filePath("AndroidRuntime"));
        const QString profile = qEnvironmentVariable("USERPROFILE");
        return QDir::cleanPath(QDir(profile.isEmpty() ? QDir::homePath() : profile)
                                   .filePath("OpenSrc/AndroidRuntime"));
    }();
    return resolved;
}

QString androidRuntimeSdkRoot(const QString &runtimeRoot)
{
    return QDir(runtimeRoot).filePath("sdk");
}

QString androidRuntimeAvdHome(const QString &runtimeRoot)
{
    return QDir(runtimeRoot).filePath("avd");
}

QString androidRuntimeEmulator(const QString &runtimeRoot)
{
#ifdef Q_OS_WIN
    return QDir(androidRuntimeSdkRoot(runtimeRoot)).filePath("emulator/emulator.exe");
#else
    return QDir(androidRuntimeSdkRoot(runtimeRoot)).filePath("emulator/emulator");
#endif
}

QString androidRuntimeAvdConfig(const QString &runtimeRoot, const QString &avdName)
{
    return QDir(androidRuntimeAvdHome(runtimeRoot))
        .filePath(avdName.trimmed() + ".avd/config.ini");
}

AppSettings AppSettings::load()
{
    AppSettings settings;
    settings.runtimeRoot = defaultAndroidRuntimeRoot();
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly)) return settings;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return settings;

    const QJsonObject json = document.object();
    const QString savedRoot = json.value("runtimeRoot").toString().trimmed();
    if (!savedRoot.isEmpty()) settings.runtimeRoot = QDir::cleanPath(savedRoot);
    const QString runtimeEngine = json.value("runtimeEngine").toString();
    if (!runtimeEngine.isEmpty()) settings.runtimeEngine = runtimeEngine;
    const QString avdName = json.value("avdName").toString();
    if (!avdName.trimmed().isEmpty()) settings.avdName = avdName.trimmed();
    settings.qemuExecutable = json.value("qemuExecutable").toString();
    settings.kernelImage = json.value("kernelImage").toString();
    settings.initrdImage = json.value("initrdImage").toString();
    settings.systemImage = json.value("systemImage").toString();
    settings.vendorImage = json.value("vendorImage").toString();
    settings.userdataImage = json.value("userdataImage").toString();
    const QString theme = json.value("theme").toString();
    const QString accent = json.value("accent").toString();
    const QString gpuDevice = json.value("gpuDevice").toString();
    if (!theme.isEmpty()) settings.theme = theme;
    if (!accent.isEmpty()) settings.accent = accent;
    if (!gpuDevice.isEmpty()) settings.gpuDevice = gpuDevice;
    settings.cpuCores = json.value("cpuCores").toInt(settings.cpuCores);
    settings.memoryMiB = json.value("memoryMiB").toInt(settings.memoryMiB);
    settings.displayWidth = json.value("displayWidth").toInt(settings.displayWidth);
    settings.displayHeight = json.value("displayHeight").toInt(settings.displayHeight);
    settings.enableAudio = json.value("enableAudio").toBool(settings.enableAudio);
    settings.enableNetwork = json.value("enableNetwork").toBool(settings.enableNetwork);
    settings.useWHPX = json.value("useWHPX").toBool(settings.useWHPX);
    // Emulator path is always derived from the managed runtime root; never trust
    // a stale path from an older version or another machine.
    settings.androidEmulatorExecutable = androidRuntimeEmulator(settings.runtimeRoot);
    return settings;
}

bool AppSettings::save(QString *error) const
{
    QJsonObject json;
    json.insert("schemaVersion", 3);
    json.insert("runtimeRoot", runtimeRoot);
    json.insert("runtimeEngine", runtimeEngine);
    json.insert("androidEmulatorExecutable", androidEmulatorExecutable);
    json.insert("avdName", avdName);
    json.insert("qemuExecutable", qemuExecutable);
    json.insert("kernelImage", kernelImage);
    json.insert("initrdImage", initrdImage);
    json.insert("systemImage", systemImage);
    json.insert("vendorImage", vendorImage);
    json.insert("userdataImage", userdataImage);
    json.insert("theme", theme);
    json.insert("accent", accent);
    json.insert("gpuDevice", gpuDevice);
    json.insert("cpuCores", cpuCores);
    json.insert("memoryMiB", memoryMiB);
    json.insert("displayWidth", displayWidth);
    json.insert("displayHeight", displayHeight);
    json.insert("enableAudio", enableAudio);
    json.insert("enableNetwork", enableNetwork);
    json.insert("useWHPX", useWHPX);

    QSaveFile file(configPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(QJsonDocument(json).toJson(QJsonDocument::Indented)) < 0) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QString AppSettings::configFilePath() const
{
    return configPath();
}

QString AppSettings::effectiveRuntimeRoot() const
{
    return runtimeRoot.trimmed().isEmpty() ? defaultAndroidRuntimeRoot()
                                           : QDir::cleanPath(runtimeRoot.trimmed());
}

QStringList AppSettings::missingBootFiles() const
{
    QStringList missing;
    if (runtimeEngine == "android-sdk") {
        const QString root = effectiveRuntimeRoot();
        const QString emulator = androidRuntimeEmulator(root);
        if (androidEmulatorExecutable.trimmed().isEmpty() || !QFile::exists(emulator))
            missing << "Android runtime (use “Install Android runtime” in Android instance)";
        if (avdName.trimmed().isEmpty() ||
            !QFile::exists(androidRuntimeAvdConfig(root, avdName)))
            missing << "OpenSrc Android device (created automatically by runtime setup)";
        return missing;
    }
    if (qemuExecutable.isEmpty() || !QFile::exists(qemuExecutable))
        missing << "QEMU system executable (not the installer)";
    if (kernelImage.isEmpty() || !QFile::exists(kernelImage)) missing << "AOSP x86_64 kernel";
    if (!initrdImage.isEmpty() && !QFile::exists(initrdImage)) missing << "Selected initrd image";
    if (systemImage.isEmpty() || !QFile::exists(systemImage)) missing << "Android system image";
    if (!vendorImage.isEmpty() && !QFile::exists(vendorImage)) missing << "Selected vendor image";
    if (!userdataImage.isEmpty() && !QFile::exists(userdataImage)) missing << "Selected userdata image";
    return missing;
}
