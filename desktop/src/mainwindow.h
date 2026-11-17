#pragma once

#include "emulatorcontroller.h"
#include "performancemonitor.h"
#include "settings.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMap>

class QFrame;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    AppSettings m_settings;
    EmulatorController m_emulator;
    PerformanceMonitor m_monitor;
    QStackedWidget *m_pages = nullptr;
    QMap<QString, QPushButton *> m_navButtons;
    QMap<QString, QWidget *> m_pathEdits;
    QTableWidget *m_mappingTable = nullptr;
    QTextEdit *m_logView = nullptr;
    QLabel *m_runtimeStatus = nullptr;
    QLabel *m_cpuValue = nullptr;
    QLabel *m_memoryValue = nullptr;
    QLabel *m_perfStatus = nullptr;
    QPushButton *m_launchButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_gpuCombo = nullptr;
    QSpinBox *m_cpuSpin = nullptr;
    QSpinBox *m_memorySpin = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QCheckBox *m_audioCheck = nullptr;
    QCheckBox *m_networkCheck = nullptr;
    QCheckBox *m_whpxCheck = nullptr;
    QLabel *m_configPathLabel = nullptr;
    QLabel *m_breadcrumb = nullptr;

    QWidget *makeOverviewPage();
    QWidget *makeHardwarePage();
    QWidget *makeControlsPage();
    QWidget *makeAppearancePage();
    QWidget *makeArchitecturePage();
    QWidget *makeLogsPage();
    QPushButton *makeNavButton(const QString &label, const QString &glyph, const QString &pageKey);
    QWidget *makePathField(const QString &key, const QString &label, const QString &filter);
    QFrame *makePanel(const QString &objectName = "glassPanel") const;
    void navigate(const QString &pageKey);
    void loadSettingsIntoUi();
    void collectSettingsFromUi();
    void saveSettings();
    void applyTheme();
    void updateRuntimeUi(bool running);
    void appendLog(const QString &text);
    void importKeymap();
    void exportKeymap();
    void saveKeymap();
    void addMapping(const QString &action, const QString &binding);
    void refreshReadiness();
    void selectPath(const QString &key, const QString &filter);
};
