#pragma once

#include <QString>
#include <QStringList>

// Identifier of the managed Android image and virtual device that OpenSrc provisions.
inline constexpr auto kOpenSrcSystemImage = "system-images;android-35;google_apis;x86_64";
inline constexpr auto kOpenSrcDefaultAvdName = "OpenSrc_Default_API_35";

// The folder OpenSrc owns. The Inno Setup installer creates "<install>\Runtime"
// on the system (C:) drive; the app also works from a portable ZIP.
QString defaultAndroidRuntimeRoot();
QString androidRuntimeSdkRoot(const QString &runtimeRoot);
QString androidRuntimeAvdHome(const QString &runtimeRoot);
QString androidRuntimeEmulator(const QString &runtimeRoot);
QString androidRuntimeAvdConfig(const QString &runtimeRoot, const QString &avdName);

struct AppSettings {
    QString runtimeRoot;
    QString runtimeEngine = "android-sdk";
    QString androidEmulatorExecutable;
    QString avdName = kOpenSrcDefaultAvdName;
    QString qemuExecutable;
    QString kernelImage;
    QString initrdImage;
    QString systemImage;
    QString vendorImage;
    QString userdataImage;
    QString theme = "Obsidian";
    QString accent = "#b9f37a";
    QString gpuDevice = "virtio-vga";
    int cpuCores = 4;
    int memoryMiB = 4096;
    int displayWidth = 1280;
    int displayHeight = 720;
    bool enableAudio = true;
    bool enableNetwork = false;
    bool useWHPX = true;

    static AppSettings load();
    bool save(QString *error = nullptr) const;
    QString configFilePath() const;
    QString effectiveRuntimeRoot() const;
    QStringList missingBootFiles() const;
};
