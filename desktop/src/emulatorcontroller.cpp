#include "emulatorcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace {
QString diskFormat(const QString &path)
{
    return path.endsWith(".qcow2", Qt::CaseInsensitive) || path.endsWith(".qcow", Qt::CaseInsensitive)
        ? "qcow2" : "raw";
}

void appendDisk(QStringList &args, const QString &path, const QString &id, bool readOnly)
{
    if (path.isEmpty()) return;
    const QString drive = QString("file=%1,if=none,id=%2,format=%3%4")
        .arg(QFileInfo(path).absoluteFilePath()).arg(id).arg(diskFormat(path)).arg(readOnly ? ",readonly=on" : "");
    args << "-drive" << drive;
    args << "-device" << QString("virtio-blk-pci,drive=%1").arg(id);
}
}

EmulatorController::EmulatorController(QObject *parent) : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this] {
        emit outputReceived(QString::fromLocal8Bit(m_process.readAll()));
    });
    connect(&m_process, &QProcess::started, this, [this] { emit stateChanged(true); });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) { emit stateChanged(false); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            const QString message = m_process.errorString();
            emit launchFailed(message);
            emit stateChanged(false);
        }
    });
}

bool EmulatorController::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QStringList EmulatorController::buildArguments(const AppSettings &settings, QString *error) const
{
    const QStringList missing = settings.missingBootFiles();
    if (!missing.isEmpty()) {
        if (error) *error = "Set up these items first: " + missing.join(", ");
        return {};
    }
    if (settings.runtimeEngine == "android-sdk") {
        // Let the official Android Emulator load the AVD's matching kernel, ramdisk,
        // system/vendor images, device profile, and boot parameters as a set.
        if (settings.memoryMiB < 1536 || settings.memoryMiB > 8192) {
            if (error) *error = "Android Emulator RAM must be between 1536 and 8192 MiB.";
            return {};
        }
        return {"-avd", settings.avdName.trimmed(), "-memory", QString::number(settings.memoryMiB)};
    }
    if (settings.cpuCores < 1 || settings.cpuCores > 64) {
        if (error) *error = "CPU cores must be between 1 and 64.";
        return {};
    }
    if (settings.memoryMiB < 1024 || settings.memoryMiB > 262144) {
        if (error) *error = "Memory must be between 1024 MiB and 262144 MiB.";
        return {};
    }
    if (settings.displayWidth < 320 || settings.displayHeight < 320 ||
        settings.displayWidth > 7680 || settings.displayHeight > 7680) {
        if (error) *error = "Display dimensions must be between 320 and 7680 pixels.";
        return {};
    }

    QStringList args{"-name", "OpenSrc Android 11", "-machine", "q35"};
    args << "-accel" << (settings.useWHPX ? "whpx" : "tcg,thread=multi");
    args << "-cpu" << "max" << "-smp" << QString::number(settings.cpuCores)
         << "-m" << QString::number(settings.memoryMiB);
    args << "-kernel" << QFileInfo(settings.kernelImage).absoluteFilePath();
    if (!settings.initrdImage.isEmpty())
        args << "-initrd" << QFileInfo(settings.initrdImage).absoluteFilePath();

    const QString append = "console=ttyS0 androidboot.hardware=ranchu androidboot.selinux=enforcing";
    args << "-append" << append;
    appendDisk(args, settings.systemImage, "android_system", true);
    appendDisk(args, settings.vendorImage, "android_vendor", true);
    appendDisk(args, settings.userdataImage, "android_userdata", false);

    const QString gpu = settings.gpuDevice == "virtio-gpu-pci" ? "virtio-gpu-pci" : "virtio-vga";
    args << "-device" << QString("%1,xres=%2,yres=%3").arg(gpu).arg(settings.displayWidth).arg(settings.displayHeight);
    args << "-device" << "qemu-xhci" << "-device" << "usb-kbd" << "-device" << "usb-tablet";
    args << "-display" << "sdl";
    if (settings.enableAudio)
        args << "-audiodev" << "dsound,id=opensrc-audio" << "-device" << "ich9-intel-hda"
             << "-device" << "hda-output,audiodev=opensrc-audio";
    if (settings.enableNetwork)
        args << "-netdev" << "user,id=opensrc-net" << "-device" << "virtio-net-pci,netdev=opensrc-net";
    args << "-serial" << "mon:stdio";
    args << "-no-reboot";
    return args;
}

bool EmulatorController::launch(const AppSettings &settings, QString *error)
{
    if (isRunning()) {
        if (error) *error = "The Android instance is already running.";
        return false;
    }
    const QStringList args = buildArguments(settings, error);
    if (args.isEmpty()) return false;

    const QString executable = settings.runtimeEngine == "android-sdk"
        ? settings.androidEmulatorExecutable : settings.qemuExecutable;
    m_runningEngine = settings.runtimeEngine;
    m_process.setProgram(QFileInfo(executable).absoluteFilePath());
    m_process.setArguments(args);
    m_process.setWorkingDirectory(QFileInfo(executable).absolutePath());
    if (settings.runtimeEngine == "android-sdk") {
        // Point the emulator at the exact SDK/AVD tree OpenSrc provisioned. Without
        // this the emulator falls back to %USERPROFILE%\.android\avd and would not
        // find the managed device.
        const QString runtimeRoot = settings.effectiveRuntimeRoot();
        const QString sdkRoot = androidRuntimeSdkRoot(runtimeRoot);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert("ANDROID_SDK_ROOT", sdkRoot);
        environment.insert("ANDROID_HOME", sdkRoot);
        environment.insert("ANDROID_USER_HOME", QDir(runtimeRoot).filePath("android-user"));
        environment.insert("ANDROID_PREFS_ROOT", QDir(runtimeRoot).filePath("android-user"));
        environment.insert("ANDROID_AVD_HOME", androidRuntimeAvdHome(runtimeRoot));
        m_process.setProcessEnvironment(environment);
    }
    m_process.start();
    if (!m_process.waitForStarted(2500)) {
        if (error) *error = m_process.errorString();
        return false;
    }
    return true;
}

void EmulatorController::stop()
{
    if (!isRunning()) return;
    if (m_runningEngine == "raw-qemu") {
        // Raw QEMU exposes the monitor on stdin through -serial mon:stdio.
        m_process.write("quit\n");
        if (m_process.waitForFinished(2500)) return;
    }
    m_process.terminate();
    if (!m_process.waitForFinished(2000)) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

qint64 EmulatorController::processId() const
{
    return m_process.processId();
}
