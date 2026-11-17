#pragma once

#include <QObject>
#include <QTimer>

class PerformanceMonitor final : public QObject {
    Q_OBJECT
public:
    explicit PerformanceMonitor(QObject *parent = nullptr);
    void setProcessId(qint64 pid);
    void start();
    void stop();

signals:
    void sampleReady(double hostCpuPercent, quint64 processMemoryMiB);

private slots:
    void sample();

private:
    QTimer m_timer;
    qint64 m_processId = 0;
    quint64 m_previousTotal = 0;
    quint64 m_previousIdle = 0;
    bool m_hasCpuBaseline = false;
};
