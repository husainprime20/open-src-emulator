#pragma once

#include <QString>
#include <QStringList>

struct AppSettings {
    QString runtimeEngine = "android-sdk";
    QString androidEmulatorExecutable;
    QString avdName;
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
    QStringList missingBootFiles() const;
};
