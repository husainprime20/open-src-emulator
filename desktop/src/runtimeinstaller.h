#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QUrl>
#include <QStringList>

class QNetworkReply;

// Provisions a private Android runtime (Java, SDK tools, emulator, system image,
// and a ready-to-boot AVD) inside a folder owned by OpenSrc, without requiring
// Android Studio, a system Java, or a system-wide SDK.
class RuntimeInstaller final : public QObject {
    Q_OBJECT
public:
    explicit RuntimeInstaller(QObject *parent = nullptr);

    bool isBusy() const;

    static QString defaultRuntimeRoot();
    static QString defaultAvdName();
    static QString systemImageId();
    static QString emulatorExecutable(const QString &runtimeRoot);
    static QString sdkRootFor(const QString &runtimeRoot);
    static QString avdHome(const QString &runtimeRoot);
    static QString avdIniPath(const QString &runtimeRoot, const QString &avdName);

public slots:
    void start(const QString &runtimeRoot);
    void cancel();

signals:
    void statusChanged(const QString &status);
    void progressChanged(int percent);
    void logReceived(const QString &line);
    void finished(bool success, const QString &error);

private:
    enum class Stage {
        Idle, DownloadMetadata, DownloadTools, ExtractTools, DownloadJavaMetadata,
        DownloadJava, ExtractJava, AcceptLicenses, InstallPackages, CreateAvd
    };

    void requestRepositoryMetadata();
    void requestJavaMetadata();
    void downloadFile(const QUrl &url, const QString &path, Stage stage,
                      int progressOffset, int progressSpan, const QString &expectedHash = {});
    void extractArchive(const QString &archive, const QString &description, Stage nextStage);
    void extractTools();
    void installExtractedTools();
    void beginLicenses();
    void installPackages();
    void createAvd();
    void runToolBatch(const QString &batch, const QStringList &arguments, Stage stage,
                      const QByteArray &input, const QString &description);
    void handleOutput(const QByteArray &bytes);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void fail(const QString &reason);
    void setProgress(int percent);

    QString sdkManagerPath() const;
    QString avdManagerPath() const;
    QString javaHome() const;
    QString emulatorPath() const;
    QProcessEnvironment toolEnvironment() const;

    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr;
    QFile m_downloadFile;
    QProcess m_process;
    QTemporaryDir m_tempDir;
    Stage m_stage = Stage::Idle;
    QString m_runtimeRoot;
    QString m_sdkRoot;
    QString m_avdHome;
    QString m_userHome;
    QString m_toolsArchive;
    QString m_javaArchive;
    QString m_expectedHash;
    QString m_hashAlgorithm;
    QString m_failureDescription;
    int m_progressOffset = 0;
    int m_progressSpan = 100;
    int m_lastToolPercent = 0;
    bool m_finishing = false;
    bool m_usedDeviceDefinition = false;
    QStringList m_outputTail;
};
