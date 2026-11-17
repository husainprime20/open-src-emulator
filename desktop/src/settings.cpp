#include "settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString configPath()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(root);
    return root + "/settings.json";
}
}

AppSettings AppSettings::load()
{
    AppSettings settings;
    QFile file(configPath());
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return settings;

    const QJsonObject json = document.object();
    settings.qemuExecutable = json.value("qemuExecutable").toString();
    settings.kernelImage = json.value("kernelImage").toString();
    settings.initrdImage = json.value("initrdImage").toString();
    settings.systemImage = json.value("systemImage").toString();
    settings.vendorImage = json.value("vendorImage").toString();
    settings.userdataImage = json.value("userdataImage").toString();
    settings.theme = json.value("theme").toString(settings.theme);
    settings.accent = json.value("accent").toString(settings.accent);
    settings.gpuDevice = json.value("gpuDevice").toString(settings.gpuDevice);
    settings.cpuCores = json.value("cpuCores").toInt(settings.cpuCores);
    settings.memoryMiB = json.value("memoryMiB").toInt(settings.memoryMiB);
    settings.displayWidth = json.value("displayWidth").toInt(settings.displayWidth);
    settings.displayHeight = json.value("displayHeight").toInt(settings.displayHeight);
    settings.enableAudio = json.value("enableAudio").toBool(settings.enableAudio);
    settings.enableNetwork = json.value("enableNetwork").toBool(settings.enableNetwork);
    settings.useWHPX = json.value("useWHPX").toBool(settings.useWHPX);
    return settings;
}

bool AppSettings::save(QString *error) const
{
    QJsonObject json;
    json.insert("schemaVersion", 1);
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

QStringList AppSettings::missingBootFiles() const
{
    QStringList missing;
    if (qemuExecutable.isEmpty() || !QFile::exists(qemuExecutable))
        missing << "QEMU executable";
    if (kernelImage.isEmpty() || !QFile::exists(kernelImage))
        missing << "AOSP x86_64 kernel";
    if (!initrdImage.isEmpty() && !QFile::exists(initrdImage))
        missing << "Selected initrd image";
    if (systemImage.isEmpty() || !QFile::exists(systemImage))
        missing << "Android system image";
    if (!vendorImage.isEmpty() && !QFile::exists(vendorImage))
        missing << "Selected vendor image";
    if (!userdataImage.isEmpty() && !QFile::exists(userdataImage))
        missing << "Selected userdata image";
    return missing;
}
