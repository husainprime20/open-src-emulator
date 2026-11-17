#include "performancemonitor.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

PerformanceMonitor::PerformanceMonitor(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PerformanceMonitor::sample);
}

void PerformanceMonitor::setProcessId(qint64 pid)
{
    m_processId = pid;
}

void PerformanceMonitor::start()
{
    m_hasCpuBaseline = false;
    m_timer.start();
    sample();
}

void PerformanceMonitor::stop()
{
    m_timer.stop();
    m_processId = 0;
    m_hasCpuBaseline = false;
    emit sampleReady(0.0, 0);
}

void PerformanceMonitor::sample()
{
#ifdef Q_OS_WIN
    FILETIME idleTime{}, kernelTime{}, userTime{};
    double cpuPercent = 0.0;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER idle{}, kernel{}, user{};
        idle.LowPart = idleTime.dwLowDateTime; idle.HighPart = idleTime.dwHighDateTime;
        kernel.LowPart = kernelTime.dwLowDateTime; kernel.HighPart = kernelTime.dwHighDateTime;
        user.LowPart = userTime.dwLowDateTime; user.HighPart = userTime.dwHighDateTime;
        const quint64 total = kernel.QuadPart + user.QuadPart;
        const quint64 idleUnits = idle.QuadPart;
        if (m_hasCpuBaseline && total > m_previousTotal) {
            const quint64 totalDelta = total - m_previousTotal;
            const quint64 idleDelta = idleUnits >= m_previousIdle ? idleUnits - m_previousIdle : 0;
            cpuPercent = 100.0 * (1.0 - static_cast<double>(idleDelta) / static_cast<double>(totalDelta));
        }
        m_previousTotal = total;
        m_previousIdle = idleUnits;
        m_hasCpuBaseline = true;
    }

    quint64 memoryMiB = 0;
    if (m_processId > 0) {
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
                                     static_cast<DWORD>(m_processId));
        if (process) {
            PROCESS_MEMORY_COUNTERS counters{};
            counters.cb = sizeof(counters);
            if (GetProcessMemoryInfo(process, &counters, sizeof(counters)))
                memoryMiB = static_cast<quint64>(counters.WorkingSetSize / (1024 * 1024));
            CloseHandle(process);
        }
    }
    emit sampleReady(qBound(0.0, cpuPercent, 100.0), memoryMiB);
#else
    emit sampleReady(0.0, 0);
#endif
}
