#pragma once

#include "settings.h"

#include <QObject>
#include <QProcess>
#include <QStringList>

class EmulatorController final : public QObject {
    Q_OBJECT
public:
    explicit EmulatorController(QObject *parent = nullptr);
    bool isRunning() const;
    QStringList buildArguments(const AppSettings &settings, QString *error = nullptr) const;
    bool launch(const AppSettings &settings, QString *error = nullptr);
    void stop();
    qint64 processId() const;

signals:
    void stateChanged(bool running);
    void outputReceived(const QString &text);
    void launchFailed(const QString &message);

private:
    QProcess m_process;
    QString m_runningEngine;
};
