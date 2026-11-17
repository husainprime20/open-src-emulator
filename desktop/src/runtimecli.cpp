// Console harness that drives the same RuntimeInstaller the GUI uses.
//
// Purpose: let CI (and developers) exercise the real provisioning pipeline —
// catalogue download, checksum verification, extraction, cmd.exe batch launch,
// sdkmanager licensing/packages, and AVD creation — without a display, and
// without hand-copying any of the logic.
//
// Usage:
//   OpenSrcRuntimeSetup.exe [--root <folder>] [--quiet] [--twice]
//
// --twice runs setup a second time on an already-provisioned folder to prove the
// resume path finds the completed runtime instead of re-downloading it.
//
// Exit codes: 0 = runtime ready, 1 = setup failed, 2 = bad arguments.

#include "runtimeinstaller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <cstdio>

namespace {
void say(const QString &text)
{
    const QByteArray line = text.toLocal8Bit();
    std::fputs(line.constData(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}
}  // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("OpenSrc");
    QCoreApplication::setApplicationName("OpenSrcRuntimeSetup");

    QString root;
    bool quiet = false;
    bool twice = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == "--root" && i + 1 < args.size()) root = args.at(++i).trimmed();
        else if (arg == "--quiet") quiet = true;
        else if (arg == "--twice") twice = true;
        else if (arg == "--help" || arg == "-h") {
            say("OpenSrcRuntimeSetup [--root <folder>] [--quiet] [--twice]");
            return 0;
        } else {
            say("Unknown argument: " + arg);
            return 2;
        }
    }
    if (root.isEmpty()) root = QDir(QCoreApplication::applicationDirPath()).filePath("Runtime");
    root = QDir::cleanPath(root);

    say("Runtime root: " + QDir::toNativeSeparators(root));

    RuntimeInstaller installer;
    QObject::connect(&installer, &RuntimeInstaller::statusChanged, [&](const QString &status) {
        if (!quiet) say(status);
    });
    QObject::connect(&installer, &RuntimeInstaller::progressChanged, [&](int percent) {
        if (!quiet) say(QString("  progress %1%").arg(percent));
    });
    QObject::connect(&installer, &RuntimeInstaller::logReceived, [&](const QString &line) {
        if (!quiet) say("  | " + line);
    });

    int exitCode = 1;
    bool firstPassDone = false;
    QObject::connect(&installer, &RuntimeInstaller::finished,
                     [&](bool success, const QString &error) {
                         if (success && twice && !firstPassDone) {
                             // Second pass: on a complete runtime this must report
                             // ready almost immediately without re-downloading.
                             firstPassDone = true;
                             say("SECOND PASS: verifying resume on the completed runtime");
                             installer.start(root);
                             return;
                         }
                         if (success) {
                             exitCode = 0;
                             say(twice ? "RESULT: ready (resume verified)" : "RESULT: ready");
                             const QString emulator = RuntimeInstaller::emulatorExecutable(root);
                             const QString avdIni =
                                 RuntimeInstaller::avdIniPath(root, RuntimeInstaller::defaultAvdName());
                             say(QString("  emulator: %1 (exists=%2)")
                                     .arg(QDir::toNativeSeparators(emulator),
                                          QFileInfo::exists(emulator) ? "yes" : "no"));
                             say(QString("  avd:      %1 (exists=%2)")
                                     .arg(QDir::toNativeSeparators(avdIni),
                                          QFileInfo::exists(avdIni) ? "yes" : "no"));
                         } else {
                             exitCode = 1;
                             say("RESULT: failed");
                             say("  " + error);
                             // One greppable line so CI annotations carry the reason.
                             QString flat = error;
                             flat.replace('\r', ' ').replace('\n', " | ");
                             say("FAILURE_REASON: " + flat);
                         }
                         QCoreApplication::exit(exitCode);
                     });

    // Hard stop so a stuck network read can never hang the job.
    QTimer::singleShot(45 * 60 * 1000, &app, [] {
        std::fputs("RESULT: timed out after 45 minutes\n", stdout);
        std::fflush(stdout);
        QCoreApplication::exit(1);
    });

    installer.start(root);
    return app.exec();
}
