#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QTime>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
QLabel *label(const QString &text, const QString &className = {})
{
    auto *result = new QLabel(text);
    if (!className.isEmpty()) result->setProperty("class", className);
    result->setWordWrap(true);
    return result;
}
QFrame *divider()
{
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setObjectName("divider");
    return line;
}
QString qemuFilter()
{
#ifdef Q_OS_WIN
    return "QEMU system executable (qemu-system-x86_64.exe);;All files (*.*)";
#else
    return "QEMU system executable (qemu-system-x86_64);;All files (*)";
#endif
}

QString findAndroidEmulator()
{
    QString sdkRoot = qEnvironmentVariable("ANDROID_SDK_ROOT");
    if (sdkRoot.isEmpty()) sdkRoot = qEnvironmentVariable("ANDROID_HOME");
#ifdef Q_OS_WIN
    if (sdkRoot.isEmpty()) sdkRoot = qEnvironmentVariable("LOCALAPPDATA") + "/Android/Sdk";
#endif
    if (!sdkRoot.isEmpty()) {
#ifdef Q_OS_WIN
        const QString candidate = QDir(sdkRoot).filePath("emulator/emulator.exe");
#else
        const QString candidate = QDir(sdkRoot).filePath("emulator/emulator");
#endif
        if (QFile::exists(candidate)) return QDir::toNativeSeparators(candidate);
    }
#ifdef Q_OS_WIN
    return QStandardPaths::findExecutable("emulator.exe");
#else
    return QStandardPaths::findExecutable("emulator");
#endif
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_settings(AppSettings::load()), m_emulator(), m_monitor()
{
    setWindowTitle("OpenSrc · Android, in your control");
    setMinimumSize(1080, 700);
    resize(1380, 860);

    auto *shell = new QWidget;
    shell->setObjectName("appShell");
    auto *root = new QHBoxLayout(shell);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(224);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 24, 14, 16);
    sideLayout->setSpacing(7);

    auto *brandRow = new QHBoxLayout;
    auto *mark = label("▥", "brandMark");
    mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(34, 34);
    auto *brand = new QVBoxLayout;
    brand->addWidget(label("opensrc", "brandName"));
    brand->addWidget(label("ANDROID, UNLOCKED", "brandSub"));
    brandRow->addWidget(mark);
    brandRow->addLayout(brand);
    brandRow->addStretch();
    sideLayout->addLayout(brandRow);
    sideLayout->addSpacing(31);
    sideLayout->addWidget(label("WORKSPACE", "navCaption"));
    sideLayout->addWidget(makeNavButton("Overview", "◫", "overview"));
    sideLayout->addWidget(makeNavButton("Android instance", "▣", "hardware"));
    sideLayout->addWidget(makeNavButton("Keymapping", "⌘", "controls"));
    sideLayout->addWidget(makeNavButton("Performance", "◴", "performance"));
    sideLayout->addWidget(divider());
    sideLayout->addWidget(label("PERSONALIZE", "navCaption"));
    sideLayout->addWidget(makeNavButton("Appearance", "◐", "appearance"));
    sideLayout->addWidget(makeNavButton("Graphics path", "⬡", "architecture"));
    sideLayout->addWidget(makeNavButton("Logs & diagnostics", "≋", "logs"));
    sideLayout->addStretch();

    auto *localCard = makePanel("localCard");
    auto *localLayout = new QVBoxLayout(localCard);
    localLayout->setContentsMargins(12, 12, 12, 12);
    localLayout->setSpacing(6);
    localLayout->addWidget(label("⌑  YOUR DEVICE, YOUR RULES", "navCaption"));
    localLayout->addWidget(label("Local-first by default", "cardTitleSmall"));
    localLayout->addWidget(label("Profiles stay in your Windows user folder. No account or cloud sync.", "mutedText"));
    sideLayout->addWidget(localCard);
    auto *profileRow = new QHBoxLayout;
    auto *avatar = label("O", "avatar");
    avatar->setFixedSize(30, 30);
    avatar->setAlignment(Qt::AlignCenter);
    auto *profileCopy = new QVBoxLayout;
    profileCopy->addWidget(label("Local profile", "profileName"));
    profileCopy->addWidget(label("Private · on this PC", "profileHint"));
    profileRow->addWidget(avatar);
    profileRow->addLayout(profileCopy);
    profileRow->addStretch();
    sideLayout->addSpacing(9);
    sideLayout->addLayout(profileRow);
    root->addWidget(sidebar);

    auto *mainArea = new QWidget;
    mainArea->setObjectName("mainArea");
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto *topBar = new QWidget;
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(57);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(28, 0, 28, 0);
    m_breadcrumb = label("Workspace   /   <b>Overview</b>", "breadcrumb");
    m_breadcrumb->setObjectName("breadcrumbTitle");
    topLayout->addWidget(m_breadcrumb);
    topLayout->addStretch();
    topLayout->addWidget(label("●  LOCAL · OFFLINE", "connectionBadge"));
    topLayout->addSpacing(16);
    topLayout->addWidget(label("◉", "topAvatar"));
    mainLayout->addWidget(topBar);

    m_pages = new QStackedWidget;
    m_pages->setObjectName("pages");
    m_pages->addWidget(makeOverviewPage());
    m_pages->addWidget(makeHardwarePage());
    m_pages->addWidget(makeControlsPage());
    m_pages->addWidget(makeAppearancePage());
    m_pages->addWidget(makeArchitecturePage());
    m_pages->addWidget(makeLogsPage());
    mainLayout->addWidget(m_pages, 1);
    root->addWidget(mainArea, 1);
    setCentralWidget(shell);

    loadSettingsIntoUi();
    if (m_mappingTable) connect(m_mappingTable, &QTableWidget::itemChanged, this, [this] { saveKeymap(); });
    applyTheme();
    navigate("overview");
    refreshReadiness();
    updateRuntimeUi(false);

    connect(&m_emulator, &EmulatorController::stateChanged, this, [this](bool running) {
        updateRuntimeUi(running);
        if (running) {
            m_monitor.setProcessId(m_emulator.processId());
            m_monitor.start();
            appendLog("Android runtime process started; startup status is in the emulator output.");
        } else {
            m_monitor.stop();
            appendLog("Android runtime process stopped.");
        }
    });
    connect(&m_emulator, &EmulatorController::outputReceived, this, &MainWindow::appendLog);
    connect(&m_monitor, &PerformanceMonitor::sampleReady, this, [this](double cpu, quint64 memory) {
        if (m_cpuValue) m_cpuValue->setText(QString::number(cpu, 'f', 0) + "%");
        if (m_memoryValue) m_memoryValue->setText(QString::number(memory) + " MiB");
    });
}

MainWindow::~MainWindow()
{
    m_emulator.stop();
}

QFrame *MainWindow::makePanel(const QString &objectName) const
{
    auto *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    frame->setAttribute(Qt::WA_StyledBackground, true);
    auto *shadow = new QGraphicsDropShadowEffect(frame);
    shadow->setBlurRadius(27);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 30));
    frame->setGraphicsEffect(shadow);
    return frame;
}

QPushButton *MainWindow::makeNavButton(const QString &text, const QString &glyph, const QString &pageKey)
{
    auto *button = new QPushButton(glyph + "    " + text);
    button->setObjectName("navButton");
    button->setCheckable(true);
    connect(button, &QPushButton::clicked, this, [this, pageKey] { navigate(pageKey); });
    m_navButtons.insert(text, button);
    return button;
}

QWidget *MainWindow::makeOverviewPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 26, 30, 25);
    layout->setSpacing(14);
    auto *heading = new QHBoxLayout;
    auto *headingText = new QVBoxLayout;
    headingText->addWidget(label("●  YOUR ANDROID GAMING SPACE", "eyebrow"));
    headingText->addWidget(label("Good to have you back.", "pageTitle"));
    headingText->addWidget(label("A lighter way to play. Everything you need, nothing you don’t.", "subtitle"));
    heading->addLayout(headingText);
    heading->addStretch();
    auto *buildTag = label("ANDROID SDK  ·  AVD", "buildTag");
    buildTag->setAlignment(Qt::AlignCenter);
    heading->addWidget(buildTag, 0, Qt::AlignVCenter);
    layout->addLayout(heading);

    auto *body = new QHBoxLayout;
    body->setSpacing(14);
    auto *hero = makePanel("heroPanel");
    auto *heroLayout = new QVBoxLayout(hero);
    heroLayout->setContentsMargins(20, 17, 20, 17);
    heroLayout->setSpacing(12);
    auto *heroTop = new QHBoxLayout;
    heroTop->addWidget(label("▣   OPEN SRC ONE", "eyebrow"));
    heroTop->addStretch();
    heroTop->addWidget(label("●  SYSTEM IMAGE NEEDED", "warningBadge"));
    heroLayout->addLayout(heroTop);
    auto *center = new QHBoxLayout;
    auto *phoneFrame = new QFrame;
    phoneFrame->setObjectName("phoneMockup");
    phoneFrame->setFixedSize(156, 201);
    auto *phoneLayout = new QVBoxLayout(phoneFrame);
    phoneLayout->setContentsMargins(12, 10, 12, 10);
    phoneLayout->addWidget(label("9:41", "phoneStatus"), 0, Qt::AlignHCenter);
    phoneLayout->addStretch();
    phoneLayout->addWidget(label("11", "androidMark"), 0, Qt::AlignHCenter);
    phoneLayout->addWidget(label("A clean Android canvas", "phoneCaption"), 0, Qt::AlignHCenter);
    phoneLayout->addStretch();
    phoneLayout->addWidget(label("◉       ▦       ◈", "phoneDock"), 0, Qt::AlignHCenter);
    center->addStretch(); center->addWidget(phoneFrame); center->addStretch();
    heroLayout->addLayout(center, 1);
    auto *heroBottom = new QHBoxLayout;
    auto *statusStack = new QVBoxLayout;
    m_runtimeStatus = label("Runtime not configured", "cardTitleSmall");
    statusStack->addWidget(m_runtimeStatus);
    statusStack->addWidget(label("Install Android Emulator and create an AVD to get started.", "mutedText"));
    heroBottom->addLayout(statusStack, 1);
    auto *setup = new QPushButton("Configure Android  →");
    setup->setObjectName("secondaryButton");
    connect(setup, &QPushButton::clicked, this, [this] { navigate("hardware"); });
    heroBottom->addWidget(setup);
    heroLayout->addLayout(heroBottom);
    body->addWidget(hero, 1);

    auto *right = new QVBoxLayout;
    right->setSpacing(14);
    auto *perf = makePanel();
    auto *perfLayout = new QVBoxLayout(perf);
    perfLayout->setContentsMargins(16, 15, 16, 14);
    perfLayout->setSpacing(8);
    auto *perfTop = new QHBoxLayout;
    perfTop->addWidget(label("◴", "metricIcon"));
    perfTop->addWidget(label("LIVE PERFORMANCE", "eyebrow"));
    perfTop->addStretch();
    m_perfStatus = label("IDLE", "idleBadge");
    perfTop->addWidget(m_perfStatus);
    perfLayout->addLayout(perfTop);
    perfLayout->addWidget(label("Low clutter. Full visibility.", "cardTitle"));
    perfLayout->addWidget(label("Host CPU and Android Emulator process memory. No telemetry is sent.", "mutedText"));
    auto *metrics = new QHBoxLayout;
    auto *cpuCol = new QVBoxLayout;
    cpuCol->addWidget(label("HOST CPU", "metricLabel"));
    m_cpuValue = label("—", "metricValue"); cpuCol->addWidget(m_cpuValue);
    auto *memCol = new QVBoxLayout;
    memCol->addWidget(label("PROCESS RAM", "metricLabel"));
    m_memoryValue = label("—", "metricValue"); memCol->addWidget(m_memoryValue);
    metrics->addLayout(cpuCol); metrics->addStretch(); metrics->addLayout(memCol);
    perfLayout->addLayout(metrics);
    right->addWidget(perf, 1);

    auto *inputPanel = makePanel();
    auto *inputLayout = new QVBoxLayout(inputPanel);
    inputLayout->setContentsMargins(16, 14, 16, 14);
    inputLayout->setSpacing(7);
    inputLayout->addWidget(label("INPUT STUDIO", "eyebrow"));
    inputLayout->addWidget(label("Your controls, your muscle memory.", "cardTitle"));
    inputLayout->addWidget(label("Import a normalized mapping profile, then tune it here.", "mutedText"));
    auto *keyPreview = new QHBoxLayout;
    for (const QString &key : {"W", "A", "S", "D"}) {
        auto *keycap = label(key, "keycap");
        keycap->setAlignment(Qt::AlignCenter);
        keycap->setFixedSize(27, 25);
        keyPreview->addWidget(keycap);
    }
    keyPreview->addSpacing(5);
    keyPreview->addWidget(label("Move", "mutedText"));
    keyPreview->addStretch();
    auto *editControls = new QPushButton("Edit keymapping   ↗");
    editControls->setObjectName("textButton");
    connect(editControls, &QPushButton::clicked, this, [this] { navigate("controls"); });
    keyPreview->addWidget(editControls);
    inputLayout->addLayout(keyPreview);
    right->addWidget(inputPanel, 1);
    body->addLayout(right, 1);
    layout->addLayout(body, 1);

    auto *quickHeading = new QHBoxLayout;
    quickHeading->addWidget(label("GET STARTED", "eyebrow"));
    quickHeading->addStretch();
    quickHeading->addWidget(label("Setup stays on this PC", "mutedText"));
    layout->addLayout(quickHeading);
    auto *quick = new QHBoxLayout;
    quick->setSpacing(10);
    const QList<QPair<QString, QString>> entries = {{"01", "Virtual hardware"}, {"02", "Keymapping"}, {"03", "Graphics path"}};
    for (const auto &entry : entries) {
        auto *tile = makePanel("quickPanel");
        auto *tileLayout = new QHBoxLayout(tile);
        tileLayout->setContentsMargins(12, 10, 12, 10);
        tileLayout->addWidget(label(entry.first, "stepNumber"));
        tileLayout->addWidget(label(entry.second, "quickTitle"));
        tileLayout->addStretch();
        tileLayout->addWidget(label("›", "mutedText"));
        quick->addWidget(tile, 1);
    }
    layout->addLayout(quick);
    auto *launchRow = new QHBoxLayout;
    launchRow->addWidget(label("ⓘ  OpenSrc is an independent project. Android system images are not bundled.", "footnote"));
    launchRow->addStretch();
    m_stopButton = new QPushButton("Stop VM");
    m_stopButton->setObjectName("stopButton");
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, &m_emulator, &EmulatorController::stop);
    m_launchButton = new QPushButton("⏻   Launch Android");
    m_launchButton->setObjectName("launchButton");
    connect(m_launchButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        QString error;
        if (!m_emulator.launch(m_settings, &error)) {
            if (!error.isEmpty()) QMessageBox::warning(this, "Can’t launch yet", error);
            return;
        }
        const QStringList args = m_emulator.buildArguments(m_settings);
        const QString executable = m_settings.runtimeEngine == "android-sdk"
            ? m_settings.androidEmulatorExecutable : m_settings.qemuExecutable;
        appendLog("Launching: " + QDir::toNativeSeparators(executable) + " " + args.join(' '));
    });
    launchRow->addWidget(m_stopButton);
    launchRow->addWidget(m_launchButton);
    layout->addLayout(launchRow);
    return page;
}

QWidget *MainWindow::makePathField(const QString &key, const QString &fieldLabel, const QString &filter)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *edit = new QLineEdit;
    edit->setObjectName("pathEdit");
    edit->setPlaceholderText("Select " + fieldLabel.toLower());
    edit->setClearButtonEnabled(true);
    auto *browse = new QPushButton("Browse…");
    browse->setObjectName("browseButton");
    layout->addWidget(edit, 1); layout->addWidget(browse);
    m_pathEdits.insert(key, edit);
    connect(browse, &QPushButton::clicked, this, [this, key, filter] { selectPath(key, filter); });
    connect(edit, &QLineEdit::editingFinished, this, [this, key] {
        saveSettings();
        if (key == "androidEmulatorExecutable") refreshAvds();
        refreshReadiness();
    });
    return row;
}

QWidget *MainWindow::makeHardwarePage()
{
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(30, 26, 30, 25);
    outer->setSpacing(13);
    outer->addWidget(label("INSTANCE CONFIGURATION", "eyebrow"));
    outer->addWidget(label("Virtual hardware", "pageTitle"));
    outer->addWidget(label("Use Android Studio’s supported AVD runtime, or experiment with raw QEMU and custom images.", "subtitle"));
    auto *engineRow = new QHBoxLayout;
    engineRow->addWidget(label("RUNTIME ENGINE", "fieldLabel"));
    m_engineCombo = new QComboBox;
    m_engineCombo->addItem("Android Emulator SDK · recommended", "android-sdk");
    m_engineCombo->addItem("Raw QEMU · experimental", "raw-qemu");
    engineRow->addWidget(m_engineCombo);
    engineRow->addStretch();
    engineRow->addWidget(label("The SDK engine loads the AVD’s kernel, ramdisk, and disks together.", "mutedText"));
    outer->addLayout(engineRow);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *contents = new QWidget;
    auto *columns = new QHBoxLayout(contents);
    columns->setContentsMargins(0, 2, 6, 8);
    columns->setSpacing(14);

    m_sdkPanel = makePanel();
    auto *sdkLayout = new QVBoxLayout(m_sdkPanel);
    sdkLayout->setContentsMargins(17, 16, 17, 16);
    sdkLayout->setSpacing(11);
    sdkLayout->addWidget(label("SUPPORTED ANDROID RUNTIME", "eyebrow"));
    sdkLayout->addWidget(label("Launch an Android Virtual Device", "cardTitle"));
    sdkLayout->addWidget(label("Install Android Emulator and an x86_64 system image with Android Studio’s SDK Manager. Create a device in Device Manager. No kernel or image extraction is needed in this mode.", "mutedText"));
    sdkLayout->addWidget(label("Android Emulator executable (emulator.exe)", "fieldLabel"));
    sdkLayout->addWidget(makePathField("androidEmulatorExecutable", "Android Emulator executable", "Android Emulator (emulator.exe);;Executable (*.exe);;All files (*)"));
    sdkLayout->addWidget(label("Available virtual devices (AVDs)", "fieldLabel"));
    auto *avdRow = new QHBoxLayout;
    m_avdCombo = new QComboBox;
    m_avdCombo->setEditable(true);
    m_avdCombo->setInsertPolicy(QComboBox::NoInsert);
    m_avdCombo->setPlaceholderText("Create an AVD in Android Studio, then refresh");
    auto *refreshAvdButton = new QPushButton("Refresh AVDs");
    refreshAvdButton->setObjectName("browseButton");
    avdRow->addWidget(m_avdCombo, 1);
    avdRow->addWidget(refreshAvdButton);
    sdkLayout->addLayout(avdRow);
    sdkLayout->addWidget(label("In Android Studio: Tools → Device Manager → Create device. Install an API 30 x86_64 image, or choose another installed image.", "calloutText"));
    connect(refreshAvdButton, &QPushButton::clicked, this, &MainWindow::refreshAvds);
    connect(m_avdCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        saveSettings();
        refreshReadiness();
    });
    connect(m_avdCombo->lineEdit(), &QLineEdit::editingFinished, this, [this] {
        saveSettings();
        refreshReadiness();
    });
    columns->addWidget(m_sdkPanel, 3);

    auto *images = makePanel();
    m_rawImagePanel = images;
    auto *imageLayout = new QVBoxLayout(images);
    imageLayout->setContentsMargins(17, 16, 17, 16);
    imageLayout->setSpacing(9);
    imageLayout->addWidget(label("ADVANCED / EXPERIMENTAL", "eyebrow"));
    imageLayout->addWidget(label("Raw QEMU boot files", "cardTitle"));
    imageLayout->addWidget(label("Use only a matching QEMU-built Android image set. Google SDK AVD files are not reliably bootable through this generic raw-QEMU path.", "mutedText"));
    imageLayout->addWidget(label("QEMU system executable · not the installer", "fieldLabel"));
    imageLayout->addWidget(makePathField("qemuExecutable", "QEMU executable", qemuFilter()));
    imageLayout->addWidget(label("Kernel image · required", "fieldLabel"));
    imageLayout->addWidget(makePathField("kernelImage", "kernel image", "Kernel / boot files (*.bin *.img *.elf);;All files (*)"));
    imageLayout->addWidget(label("Initrd / ramdisk · optional", "fieldLabel"));
    imageLayout->addWidget(makePathField("initrdImage", "initrd image", "Ramdisk images (*.img *.gz *.cpio);;All files (*)"));
    imageLayout->addWidget(label("System image · required", "fieldLabel"));
    imageLayout->addWidget(makePathField("systemImage", "system image", "Android images (*.img *.qcow2 *.qcow);;All files (*)"));
    imageLayout->addWidget(label("Vendor image · optional", "fieldLabel"));
    imageLayout->addWidget(makePathField("vendorImage", "vendor image", "Android images (*.img *.qcow2 *.qcow);;All files (*)"));
    imageLayout->addWidget(label("Writable userdata · optional", "fieldLabel"));
    imageLayout->addWidget(makePathField("userdataImage", "userdata image", "Android images (*.img *.qcow2 *.qcow);;All files (*)"));
    columns->addWidget(images, 3);

    auto *hardware = makePanel();
    auto *hwLayout = new QVBoxLayout(hardware);
    hwLayout->setContentsMargins(17, 16, 17, 16);
    hwLayout->setSpacing(10);
    hwLayout->addWidget(label("02  /  VIRTUAL DEVICE", "eyebrow"));
    hwLayout->addWidget(label("Shape your instance", "cardTitle"));
    hwLayout->addWidget(label("Runtime settings are checked before launch.", "mutedText"));
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setVerticalSpacing(9);
    m_cpuSpin = new QSpinBox; m_cpuSpin->setRange(1, 64); m_cpuSpin->setSuffix(" cores");
    m_memorySpin = new QSpinBox; m_memorySpin->setRange(1536, 8192); m_memorySpin->setSingleStep(512); m_memorySpin->setSuffix(" MiB");
    m_widthSpin = new QSpinBox; m_widthSpin->setRange(320, 7680); m_widthSpin->setSingleStep(80); m_widthSpin->setSuffix(" px");
    m_heightSpin = new QSpinBox; m_heightSpin->setRange(320, 7680); m_heightSpin->setSingleStep(80); m_heightSpin->setSuffix(" px");
    m_gpuCombo = new QComboBox;
    m_gpuCombo->addItem("VirtIO VGA · compatible", "virtio-vga");
    m_gpuCombo->addItem("VirtIO GPU PCI · experimental", "virtio-gpu-pci");
    m_whpxCheck = new QCheckBox("Use Windows Hypervisor Platform (WHPX)");
    m_audioCheck = new QCheckBox("Enable QEMU audio device");
    m_networkCheck = new QCheckBox("Enable guest networking (user-mode NAT)");
    form->addRow("CPU allocation", m_cpuSpin);
    form->addRow("Memory cap", m_memorySpin);
    form->addRow("Display width", m_widthSpin);
    form->addRow("Display height", m_heightSpin);
    form->addRow("GPU device", m_gpuCombo);
    hwLayout->addLayout(form);
    hwLayout->addWidget(m_whpxCheck); hwLayout->addWidget(m_audioCheck); hwLayout->addWidget(m_networkCheck);
    hwLayout->addWidget(divider());
    hwLayout->addWidget(label("Android Emulator mode uses the AVD’s CPU, GPU, display, audio, and network profile; the memory field overrides its RAM. Raw QEMU controls are experimental.", "calloutText"));
    hwLayout->addStretch();
    columns->addWidget(hardware, 2);
    scroll->setWidget(contents);
    outer->addWidget(scroll, 1);
    auto *footer = new QHBoxLayout;
    footer->addWidget(label("Android Emulator and raw QEMU receive direct argument lists, not shell commands.", "footnote"));
    footer->addStretch();
    auto *save = new QPushButton("Save instance settings");
    save->setObjectName("primaryButton");
    connect(save, &QPushButton::clicked, this, [this] { saveSettings(); refreshReadiness(); });
    footer->addWidget(save);
    outer->addLayout(footer);
    connect(m_engineCombo, &QComboBox::currentTextChanged, this, [this] {
        applyEngineUi();
        saveSettings();
        refreshReadiness();
    });
    return page;
}

QWidget *MainWindow::makeControlsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 26, 30, 25);
    layout->setSpacing(13);
    auto *heading = new QHBoxLayout;
    auto *headingStack = new QVBoxLayout;
    headingStack->addWidget(label("INPUT STUDIO", "eyebrow"));
    headingStack->addWidget(label("Controls, your way.", "pageTitle"));
    headingStack->addWidget(label("Edit keyboard bindings and keep profiles local to this PC.", "subtitle"));
    heading->addLayout(headingStack); heading->addStretch();
    auto *importButton = new QPushButton("⇧  Import profile"); importButton->setObjectName("secondaryButton");
    auto *exportButton = new QPushButton("↓  Export JSON"); exportButton->setObjectName("primaryButton");
    auto *addButton = new QPushButton("＋  Add control"); addButton->setObjectName("browseButton");
    heading->addWidget(importButton); heading->addWidget(exportButton); heading->addWidget(addButton);
    layout->addLayout(heading);
    auto *notice = makePanel("noticePanel");
    auto *noticeRow = new QHBoxLayout(notice);
    noticeRow->setContentsMargins(13, 10, 13, 10);
    noticeRow->addWidget(label("⇄", "metricIcon"));
    noticeRow->addWidget(label("Vendor compatibility", "cardTitleSmall"));
    noticeRow->addWidget(label("Profiles edit and export locally; guest input injection is not connected yet. Normalized JSON / simple action=key is supported. Direct MSI / BlueStacks conversion needs fixtures.", "mutedText"), 1);
    layout->addWidget(notice);
    auto *editor = makePanel();
    auto *editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(16, 14, 16, 12);
    auto *profileHeader = new QHBoxLayout;
    profileHeader->addWidget(label("BATTLE ROYALE   ·   DEFAULT PROFILE", "eyebrow"));
    profileHeader->addStretch(); profileHeader->addWidget(label("LOCAL ONLY", "localBadge"));
    editorLayout->addLayout(profileHeader);
    m_mappingTable = new QTableWidget(0, 3);
    m_mappingTable->setHorizontalHeaderLabels({"ACTION", "KEY BINDING", ""});
    m_mappingTable->horizontalHeader()->setStretchLastSection(false);
    m_mappingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mappingTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_mappingTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mappingTable->verticalHeader()->hide();
    m_mappingTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mappingTable->setShowGrid(false);
    m_mappingTable->setMinimumHeight(330);
    m_mappingTable->setObjectName("mappingTable");
    editorLayout->addWidget(m_mappingTable, 1);
    layout->addWidget(editor, 1);
    for (const auto &entry : QList<QPair<QString, QString>>{{"Move", "W A S D"}, {"Aim & pan", "Mouse"}, {"Fire", "Left click"}, {"Jump", "Space"}, {"Crouch", "C"}, {"Inventory", "Tab"}})
        addMapping(entry.first, entry.second);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importKeymap);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportKeymap);
    connect(addButton, &QPushButton::clicked, this, [this] { addMapping("New action", "Unassigned"); saveKeymap(); });
    return page;
}

void MainWindow::navigate(const QString &pageKey)
{
    const QMap<QString, int> pageIndex{{"overview", 0}, {"performance", 0}, {"hardware", 1}, {"controls", 2},
                                       {"appearance", 3}, {"architecture", 4}, {"logs", 5}};
    const QMap<QString, QString> pageTitle{{"overview", "Overview"}, {"performance", "Performance"},
                                           {"hardware", "Android instance"}, {"controls", "Keymapping"},
                                           {"appearance", "Appearance"}, {"architecture", "Graphics path"},
                                           {"logs", "Logs & diagnostics"}};
    m_pages->setCurrentIndex(pageIndex.value(pageKey, 0));
    if (m_breadcrumb) m_breadcrumb->setText("Workspace   /   <b>" + pageTitle.value(pageKey, "Overview") + "</b>");
    const QString activeNav = pageTitle.value(pageKey, "Overview");
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it)
        it.value()->setChecked(it.key() == activeNav);
}

void MainWindow::loadSettingsIntoUi()
{
    const auto getEdit = [this](const QString &key) -> QLineEdit * {
        return qobject_cast<QLineEdit *>(m_pathEdits.value(key));
    };
    if (m_settings.androidEmulatorExecutable.isEmpty())
        m_settings.androidEmulatorExecutable = findAndroidEmulator();
    if (m_engineCombo) {
        const QSignalBlocker blocker(m_engineCombo);
        const int index = m_engineCombo->findData(m_settings.runtimeEngine);
        if (index >= 0) m_engineCombo->setCurrentIndex(index);
    }
    if (getEdit("androidEmulatorExecutable"))
        getEdit("androidEmulatorExecutable")->setText(m_settings.androidEmulatorExecutable);
    if (m_avdCombo) {
        const QSignalBlocker blocker(m_avdCombo);
        m_avdCombo->setEditText(m_settings.avdName);
    }
    if (getEdit("qemuExecutable")) getEdit("qemuExecutable")->setText(m_settings.qemuExecutable);
    if (getEdit("kernelImage")) getEdit("kernelImage")->setText(m_settings.kernelImage);
    if (getEdit("initrdImage")) getEdit("initrdImage")->setText(m_settings.initrdImage);
    if (getEdit("systemImage")) getEdit("systemImage")->setText(m_settings.systemImage);
    if (getEdit("vendorImage")) getEdit("vendorImage")->setText(m_settings.vendorImage);
    if (getEdit("userdataImage")) getEdit("userdataImage")->setText(m_settings.userdataImage);
    if (m_cpuSpin) m_cpuSpin->setValue(m_settings.cpuCores);
    if (m_memorySpin) m_memorySpin->setValue(m_settings.memoryMiB);
    if (m_widthSpin) m_widthSpin->setValue(m_settings.displayWidth);
    if (m_heightSpin) m_heightSpin->setValue(m_settings.displayHeight);
    if (m_gpuCombo) {
        const int index = m_gpuCombo->findData(m_settings.gpuDevice);
        if (index >= 0) m_gpuCombo->setCurrentIndex(index);
    }
    if (m_whpxCheck) m_whpxCheck->setChecked(m_settings.useWHPX);
    if (m_audioCheck) m_audioCheck->setChecked(m_settings.enableAudio);
    if (m_networkCheck) m_networkCheck->setChecked(m_settings.enableNetwork);
    if (m_themeCombo) {
        const QSignalBlocker blocker(m_themeCombo);
        const int index = m_themeCombo->findText(m_settings.theme);
        if (index >= 0) m_themeCombo->setCurrentIndex(index);
    }
    if (m_configPathLabel) m_configPathLabel->setText(m_settings.configFilePath());
    applyEngineUi();
    refreshAvds();

    if (m_mappingTable) {
        QFile file(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/keymap.json");
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
            QJsonArray rows;
            if (document.isArray()) rows = document.array();
            else if (document.isObject()) rows = document.object().value("mappings").toArray();
            if (!rows.isEmpty()) {
                m_mappingTable->setRowCount(0);
                for (const QJsonValue &row : rows) {
                    const QJsonObject item = row.toObject();
                    addMapping(item.value("action").toString(), item.value("key").toString());
                }
            }
        }
    }
}

void MainWindow::collectSettingsFromUi()
{
    const auto text = [this](const QString &key) {
        const auto *edit = qobject_cast<QLineEdit *>(m_pathEdits.value(key));
        return edit ? edit->text().trimmed() : QString();
    };
    if (m_engineCombo) m_settings.runtimeEngine = m_engineCombo->currentData().toString();
    m_settings.androidEmulatorExecutable = text("androidEmulatorExecutable");
    if (m_avdCombo) m_settings.avdName = m_avdCombo->currentText().trimmed();
    m_settings.qemuExecutable = text("qemuExecutable");
    m_settings.kernelImage = text("kernelImage");
    m_settings.initrdImage = text("initrdImage");
    m_settings.systemImage = text("systemImage");
    m_settings.vendorImage = text("vendorImage");
    m_settings.userdataImage = text("userdataImage");
    if (m_cpuSpin) m_settings.cpuCores = m_cpuSpin->value();
    if (m_memorySpin) m_settings.memoryMiB = m_memorySpin->value();
    if (m_widthSpin) m_settings.displayWidth = m_widthSpin->value();
    if (m_heightSpin) m_settings.displayHeight = m_heightSpin->value();
    if (m_gpuCombo) m_settings.gpuDevice = m_gpuCombo->currentData().toString();
    if (m_whpxCheck) m_settings.useWHPX = m_whpxCheck->isChecked();
    if (m_audioCheck) m_settings.enableAudio = m_audioCheck->isChecked();
    if (m_networkCheck) m_settings.enableNetwork = m_networkCheck->isChecked();
    if (m_themeCombo) m_settings.theme = m_themeCombo->currentText();
}

void MainWindow::saveSettings()
{
    collectSettingsFromUi();
    QString error;
    if (!m_settings.save(&error)) {
        appendLog("Could not save settings: " + error);
        return;
    }
    if (m_configPathLabel) m_configPathLabel->setText(m_settings.configFilePath());
}

void MainWindow::applyTheme()
{
    const QString theme = m_themeCombo ? m_themeCombo->currentText() : m_settings.theme;
    const bool light = theme == "Paper";
    const QString bg = light ? "#eef0f3" : (theme == "Aurora" ? "#0c1116" : "#0b0c10");
    const QString panel = light ? "#ffffff" : (theme == "Aurora" ? "#151d22" : "#14161d");
    const QString raised = light ? "#f4f5f7" : (theme == "Aurora" ? "#1a2429" : "#191b23");
    const QString text = light ? "#202127" : "#f0f1f4";
    const QString muted = light ? "#747985" : "#9297a4";
    const QString line = light ? "#dfe1e6" : "#292c35";
    QColor accent(m_settings.accent);
    if (!accent.isValid()) accent = QColor("#b9f37a");
    const QString accentHex = accent.name();
    const QString accentText = light ? "#18300d" : "#10150b";

    const QString css = QString(R"QSS(
        QWidget#appShell { background:%1; color:%2; font-family:"Segoe UI"; font-size:10pt; }
        QWidget#sidebar { background:%3; border-right:1px solid %4; }
        QWidget#mainArea { background:%1; }
        QWidget#topBar { background:%3; border-bottom:1px solid %4; }
        QStackedWidget#pages { background:transparent; }
        QFrame#glassPanel, QFrame#heroPanel, QFrame#localCard, QFrame#quickPanel,
        QFrame#noticePanel, QFrame#previewPanel, QFrame#guestLane, QFrame#hostLane,
        QFrame#warningPanel { background:%5; border:1px solid %4; border-radius:14px; }
        QFrame#heroPanel { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %6,stop:0.55 %5,stop:1 %3); border:1px solid %7; }
        QFrame#quickPanel { border-radius:10px; }
        QFrame#phoneMockup { background:qlineargradient(x1:0,y1:0,x2:0.8,y2:1,stop:0 #34413a,stop:0.55 #1e2925,stop:1 #17201b); border:4px solid #30353b; border-radius:22px; }
        QFrame#pipelineNode { background:%8; border:1px solid %4; border-radius:9px; }
        QFrame#hostNode { background:%9; border:1px solid %10; border-radius:9px; }
        QFrame#divider { max-height:1px; background:%4; border:none; margin:8px 2px; }
        QLabel { color:%2; background:transparent; }
        QLabel[class="brandMark"] { color:%11; background:%12; border:1px solid %7; border-radius:10px; font-size:20pt; font-weight:700; }
        QLabel[class="brandName"] { font-size:15pt; font-weight:800; letter-spacing:-1px; }
        QLabel[class="brandSub"], QLabel[class="navCaption"], QLabel[class="eyebrow"], QLabel[class="metricLabel"] { color:%13; font-size:8pt; font-weight:700; letter-spacing:1.2px; }
        QLabel[class="navCaption"] { font-size:7pt; }
        QLabel[class="pageTitle"] { color:%2; font-size:24pt; font-weight:650; letter-spacing:-1px; }
        QLabel[class="subtitle"], QLabel[class="mutedText"], QLabel[class="footnote"], QLabel[class="profileHint"] { color:%14; font-size:9pt; }
        QLabel[class="cardTitle"] { font-size:12pt; font-weight:650; }
        QLabel[class="cardTitleSmall"], QLabel[class="quickTitle"], QLabel[class="profileName"] { font-size:9pt; font-weight:650; }
        QLabel[class="metricValue"] { font-size:19pt; font-weight:700; color:%11; }
        QLabel[class="metricIcon"], QLabel[class="androidMark"], QLabel[class="warningMark"] { color:%11; font-size:21pt; font-weight:700; }
        QLabel[class="androidMark"] { font-size:34pt; }
        QLabel[class="warningMark"] { color:#e9ad59; }
        QLabel[class="phoneStatus"], QLabel[class="phoneDock"] { color:#e8eee8; font-size:9pt; }
        QLabel[class="phoneCaption"] { color:#d4dfd8; font-size:8pt; }
        QLabel[class="buildTag"], QLabel[class="localBadge"], QLabel[class="connectionBadge"] { color:%11; background:%12; border:1px solid %7; border-radius:9px; padding:7px 9px; font-size:8pt; font-weight:700; }
        QLabel[class="warningBadge"] { color:#e5b874; background:rgba(226,171,86,0.12); border:1px solid rgba(226,171,86,0.22); border-radius:8px; padding:6px 8px; font-size:7pt; font-weight:700; }
        QLabel[class="idleBadge"] { color:%14; background:%8; border-radius:6px; padding:4px 6px; font-size:7pt; font-weight:700; }
        QLabel[class="stepNumber"] { color:%11; font-family:Consolas; font-size:9pt; font-weight:700; }
        QLabel[class="flowArrow"] { color:%11; font-size:14pt; }
        QLabel[class="pipelineText"] { font-size:9pt; font-weight:600; }
        QLabel[class="fieldLabel"] { color:%14; font-size:8pt; font-weight:600; }
        QLabel[class="filePathText"] { color:%14; font-family:Consolas; font-size:8pt; }
        QLabel[class="keycap"] { color:%2; background:%8; border:1px solid %4; border-radius:5px; font-family:Consolas; font-weight:700; }
        QLabel[class="avatar"], QLabel[class="topAvatar"] { color:%15; background:%11; border-radius:15px; font-weight:800; }
        QLabel[class="topAvatar"] { padding:6px; }
        QLabel[class="breadcrumb"] { color:%14; font-size:9pt; }
        QPushButton { color:%2; background:%8; border:1px solid %4; border-radius:8px; padding:8px 11px; font-size:9pt; font-weight:600; }
        QPushButton:hover { border-color:%11; background:%12; }
        QPushButton:pressed { padding-top:9px; padding-bottom:7px; }
        QPushButton:disabled { color:%13; background:%3; }
        QPushButton#navButton { text-align:left; color:%14; background:transparent; border:1px solid transparent; padding:9px 10px; }
        QPushButton#navButton:hover { color:%2; background:%8; border-color:%4; }
        QPushButton#navButton:checked { color:%2; background:%12; border-color:%7; }
        QPushButton#primaryButton, QPushButton#launchButton { color:%15; background:%11; border:1px solid %11; }
        QPushButton#launchButton { padding:10px 16px; font-size:10pt; }
        QPushButton#primaryButton:hover, QPushButton#launchButton:hover { background:%16; border-color:%16; }
        QPushButton#secondaryButton { color:%11; background:%12; border:1px solid %7; }
        QPushButton#textButton { color:%11; background:transparent; border:none; padding:4px; }
        QPushButton#stopButton { color:#e99595; background:rgba(210,85,85,0.10); border:1px solid rgba(210,85,85,0.25); }
        QLineEdit, QSpinBox, QComboBox { color:%2; background:%8; border:1px solid %4; border-radius:7px; padding:7px 8px; selection-background-color:%11; }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color:%11; }
        QComboBox::drop-down { border:none; width:20px; }
        QCheckBox { color:%2; spacing:8px; font-size:9pt; }
        QCheckBox::indicator { width:16px; height:16px; border:1px solid %4; border-radius:5px; background:%8; }
        QCheckBox::indicator:checked { background:%11; border-color:%11; }
        QTableWidget#mappingTable { color:%2; background:transparent; border:none; alternate-background-color:%8; selection-background-color:%12; gridline-color:%4; }
        QHeaderView::section { color:%13; background:%8; border:none; border-bottom:1px solid %4; padding:9px; font-size:8pt; font-weight:700; }
        QScrollArea { background:transparent; }
        QScrollBar:vertical { background:transparent; width:9px; margin:2px; }
        QScrollBar::handle:vertical { background:%4; min-height:28px; border-radius:4px; }
        QTextEdit#logView { color:#c5d2c2; background:#101218; border:1px solid %4; border-radius:9px; padding:10px; font-family:Consolas; font-size:9pt; }
        QMessageBox { background:%5; }
    )QSS").arg(bg).arg(text).arg(light ? "#e6e8ed" : "#0e1015").arg(line).arg(panel)
        .arg(light ? "#fbfcfd" : "#191b22").arg(accentHex).arg(raised)
        .arg(light ? "#edf4e9" : "#14231c").arg(light ? "#d8e7d7" : "#274534")
        .arg(accentHex).arg(light ? "#eff4e8" : "#20281d").arg(muted).arg(muted)
        .arg(accentText).arg(light ? "#a3d874" : "#a9df6d");
    qApp->setStyleSheet(css);
}

void MainWindow::updateRuntimeUi(bool running)
{
    if (m_runtimeStatus && running) m_runtimeStatus->setText("Android VM process running");
    else if (!running) refreshReadiness();
    if (m_perfStatus) m_perfStatus->setText(running ? "LIVE" : "IDLE");
    if (m_launchButton) m_launchButton->setEnabled(!running);
    if (m_stopButton) m_stopButton->setEnabled(running);
}

void MainWindow::appendLog(const QString &text)
{
    if (!m_logView || text.trimmed().isEmpty()) return;
    m_logView->append("<span style='color:#8c93a1'>[" + QTime::currentTime().toString("HH:mm:ss") + "]</span> " + text.toHtmlEscaped());
}

void MainWindow::refreshReadiness()
{
    if (!m_runtimeStatus) return;
    const QStringList missing = m_settings.missingBootFiles();
    if (!missing.isEmpty()) {
        m_runtimeStatus->setText("Needs setup · " + QString::number(missing.size()) + " required item(s) missing");
    } else if (m_settings.runtimeEngine == "android-sdk") {
        m_runtimeStatus->setText("Settings complete · selected AVD must exist in Android Studio");
    } else {
        m_runtimeStatus->setText("Files found · raw QEMU Android boot is unverified");
    }
}

void MainWindow::refreshAvds()
{
    if (!m_avdCombo) return;
    const auto *edit = qobject_cast<QLineEdit *>(m_pathEdits.value("androidEmulatorExecutable"));
    const QString executable = edit ? edit->text().trimmed() : m_settings.androidEmulatorExecutable;
    const QString previous = m_avdCombo->currentText().trimmed().isEmpty()
        ? m_settings.avdName : m_avdCombo->currentText().trimmed();
    if (executable.isEmpty() || !QFile::exists(executable)) {
        refreshReadiness();
        return;
    }

    QProcess probe;
    probe.setProgram(QFileInfo(executable).absoluteFilePath());
    probe.setArguments({"-list-avds"});
    probe.start();
    if (!probe.waitForStarted(3000) || !probe.waitForFinished(8000)) {
        probe.kill();
        appendLog("Could not query AVDs from emulator.exe: " + probe.errorString());
        refreshReadiness();
        return;
    }
    const QString output = QString::fromLocal8Bit(probe.readAllStandardOutput());
    const QString errors = QString::fromLocal8Bit(probe.readAllStandardError()).trimmed();
    QStringList names;
    for (const QString &line : output.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts)) {
        const QString name = line.trimmed();
        if (!name.isEmpty() && !names.contains(name)) names << name;
    }
    {
        const QSignalBlocker blocker(m_avdCombo);
        m_avdCombo->clear();
        m_avdCombo->addItems(names);
        if (!previous.isEmpty()) {
            const int index = m_avdCombo->findText(previous);
            if (index >= 0) m_avdCombo->setCurrentIndex(index);
            else m_avdCombo->setEditText(previous);
        }
    }
    if (previous.isEmpty() && !m_avdCombo->currentText().isEmpty()) {
        m_settings.avdName = m_avdCombo->currentText().trimmed();
        saveSettings();
    }
    if (probe.exitCode() != 0 || !errors.isEmpty())
        appendLog("AVD discovery: " + (errors.isEmpty() ? QString("emulator exited with code %1").arg(probe.exitCode()) : errors));
    else
        appendLog(QString("Found %1 Android Virtual Device(s).").arg(names.size()));
    refreshReadiness();
}

void MainWindow::applyEngineUi()
{
    const bool sdkEngine = m_engineCombo && m_engineCombo->currentData().toString() == "android-sdk";
    if (m_sdkPanel) m_sdkPanel->setVisible(sdkEngine);
    if (m_rawImagePanel) m_rawImagePanel->setVisible(!sdkEngine);
    if (m_cpuSpin) m_cpuSpin->setEnabled(!sdkEngine);
    if (m_widthSpin) m_widthSpin->setEnabled(!sdkEngine);
    if (m_heightSpin) m_heightSpin->setEnabled(!sdkEngine);
    if (m_gpuCombo) m_gpuCombo->setEnabled(!sdkEngine);
    if (m_whpxCheck) {
        m_whpxCheck->setEnabled(!sdkEngine);
        m_whpxCheck->setToolTip(sdkEngine ? "The Android Emulator selects its Windows accelerator automatically." : QString());
    }
    if (m_audioCheck) m_audioCheck->setEnabled(!sdkEngine);
    if (m_networkCheck) m_networkCheck->setEnabled(!sdkEngine);
}

void MainWindow::selectPath(const QString &key, const QString &filter)
{
    auto *edit = qobject_cast<QLineEdit *>(m_pathEdits.value(key));
    if (!edit) return;
    const QString start = edit->text().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation) : edit->text();
    const QString path = QFileDialog::getOpenFileName(this, "Select " + key, start, filter);
    if (path.isEmpty()) return;
    edit->setText(QDir::toNativeSeparators(path));
    saveSettings();
    if (key == "androidEmulatorExecutable") refreshAvds();
    refreshReadiness();
}

void MainWindow::addMapping(const QString &action, const QString &binding)
{
    if (!m_mappingTable) return;
    const int row = m_mappingTable->rowCount();
    m_mappingTable->insertRow(row);
    auto *actionItem = new QTableWidgetItem(action);
    auto *bindingItem = new QTableWidgetItem(binding);
    actionItem->setFlags(actionItem->flags() | Qt::ItemIsEditable);
    bindingItem->setFlags(bindingItem->flags() | Qt::ItemIsEditable);
    m_mappingTable->setItem(row, 0, actionItem);
    m_mappingTable->setItem(row, 1, bindingItem);
    auto *remove = new QPushButton("×");
    remove->setObjectName("removeMappingButton");
    remove->setToolTip("Remove this mapping");
    m_mappingTable->setCellWidget(row, 2, remove);
    connect(remove, &QPushButton::clicked, this, [this, remove] {
        for (int current = 0; current < m_mappingTable->rowCount(); ++current) {
            if (m_mappingTable->cellWidget(current, 2) == remove) {
                m_mappingTable->removeRow(current);
                saveKeymap();
                break;
            }
        }
    });
}

void MainWindow::saveKeymap()
{
    if (!m_mappingTable) return;
    QJsonArray rows;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        const auto *action = m_mappingTable->item(row, 0);
        const auto *key = m_mappingTable->item(row, 1);
        if (!action || !key || action->text().trimmed().isEmpty()) continue;
        QJsonObject item;
        item.insert("action", action->text().trimmed());
        item.insert("key", key->text().trimmed());
        rows.append(item);
    }
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/keymap.json";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject root;
        root.insert("format", "opensrc-keymap-v1");
        root.insert("mappings", rows);
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void MainWindow::importKeymap()
{
    const QString path = QFileDialog::getOpenFileName(this, "Import keymap", {},
        "Keymap files (*.json *.cfg *.ini *.txt);;All files (*.*)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Import failed", file.errorString());
        return;
    }
    const QByteArray bytes = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    QList<QPair<QString, QString>> mappings;
    if (parseError.error == QJsonParseError::NoError) {
        QJsonArray rows;
        if (document.isArray()) rows = document.array();
        else if (document.isObject()) {
            const QJsonObject root = document.object();
            for (const QString &field : {"mappings", "controls", "keymap"}) {
                if (root.value(field).isArray()) { rows = root.value(field).toArray(); break; }
            }
        }
        for (const QJsonValue &value : rows) {
            const QJsonObject row = value.toObject();
            QString action = row.value("action").toString();
            if (action.isEmpty()) action = row.value("name").toString();
            if (action.isEmpty()) action = row.value("label").toString();
            QString key = row.value("key").toString();
            if (key.isEmpty()) key = row.value("keys").toVariant().toString();
            if (key.isEmpty()) key = row.value("binding").toString();
            if (key.isEmpty()) key = row.value("keyCode").toVariant().toString();
            if (!action.isEmpty() && !key.isEmpty()) mappings.append({action, key});
        }
    } else {
        const QString text = QString::fromUtf8(bytes);
        for (QString line : text.split('\n')) {
            line = line.trimmed();
            if (line.isEmpty() || line.startsWith('#') || line.startsWith(';') || line.startsWith('[')) continue;
            const qsizetype separator = line.indexOf('=');
            if (separator <= 0) continue;
            const QString action = line.left(separator).trimmed();
            const QString key = line.mid(separator + 1).trimmed();
            if (!action.isEmpty() && !key.isEmpty()) mappings.append({action, key});
        }
    }
    if (mappings.isEmpty()) {
        QMessageBox::warning(this, "No mappings found",
            "OpenSrc imports normalized JSON arrays/objects or simple action=key text. This file may use an unsupported vendor format.");
        return;
    }
    m_mappingTable->setRowCount(0);
    for (const auto &entry : mappings) addMapping(entry.first, entry.second);
    saveKeymap();
    appendLog("Imported " + QString::number(mappings.size()) + " controls from " + QFileInfo(path).fileName());
}

void MainWindow::exportKeymap()
{
    const QString path = QFileDialog::getSaveFileName(this, "Export keymap", "opensrc-keymap.json", "JSON profile (*.json)");
    if (path.isEmpty()) return;
    QJsonArray rows;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        const auto *action = m_mappingTable->item(row, 0);
        const auto *key = m_mappingTable->item(row, 1);
        if (!action || !key) continue;
        QJsonObject item;
        item.insert("action", action->text());
        item.insert("key", key->text());
        rows.append(item);
    }
    QJsonObject root;
    root.insert("format", "opensrc-keymap-v1");
    root.insert("mappings", rows);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Export failed", file.errorString());
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendLog("Exported keymap: " + path);
}

QWidget *MainWindow::makeAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 26, 30, 25);
    layout->setSpacing(14);
    layout->addWidget(label("MAKE IT YOURS", "eyebrow"));
    layout->addWidget(label("Your space. Your style.", "pageTitle"));
    layout->addWidget(label("Tune the desktop shell; the Android guest remains its own separate system.", "subtitle"));

    auto *themePanel = makePanel();
    auto *themeLayout = new QVBoxLayout(themePanel);
    themeLayout->setContentsMargins(18, 16, 18, 16);
    themeLayout->setSpacing(12);
    themeLayout->addWidget(label("COLOR LANGUAGE", "eyebrow"));
    themeLayout->addWidget(label("Glass, soft depth, your accent.", "cardTitle"));
    themeLayout->addWidget(label("Choose a foundation and a highlight color. Changes apply immediately and save locally.", "mutedText"));
    auto *themeRow = new QHBoxLayout;
    m_themeCombo = new QComboBox;
    m_themeCombo->addItems({"Obsidian", "Paper", "Aurora"});
    m_themeCombo->setMinimumWidth(180);
    auto *accentButton = new QPushButton("Choose accent color…");
    accentButton->setObjectName("browseButton");
    themeRow->addWidget(label("Theme", "fieldLabel"));
    themeRow->addWidget(m_themeCombo);
    themeRow->addSpacing(22);
    themeRow->addWidget(label("Accent", "fieldLabel"));
    themeRow->addWidget(accentButton);
    themeRow->addStretch();
    themeLayout->addLayout(themeRow);
    auto *preview = makePanel("previewPanel");
    auto *previewLayout = new QHBoxLayout(preview);
    previewLayout->setContentsMargins(13, 12, 13, 12);
    previewLayout->addWidget(label("✦", "metricIcon"));
    previewLayout->addWidget(label("Frosted glass panels", "cardTitleSmall"));
    previewLayout->addStretch();
    auto *sampleButton = new QPushButton("Soft raised control");
    sampleButton->setObjectName("primaryButton");
    previewLayout->addWidget(sampleButton);
    themeLayout->addWidget(preview);
    layout->addWidget(themePanel);

    auto *privacyPanel = makePanel();
    auto *privacyLayout = new QVBoxLayout(privacyPanel);
    privacyLayout->setContentsMargins(18, 16, 18, 16);
    privacyLayout->addWidget(label("LOCAL BY DESIGN", "eyebrow"));
    privacyLayout->addWidget(label("No account. No silent sync.", "cardTitle"));
    privacyLayout->addWidget(label("Settings and profiles are saved as JSON in the current Windows user’s application-data folder. OpenSrc starts the selected Android Emulator or raw-QEMU process with the configured options. The guest OS is separate and has its own network and privacy settings.", "mutedText"));
    privacyLayout->addWidget(divider());
    m_configPathLabel = label(m_settings.configFilePath(), "filePathText");
    privacyLayout->addWidget(m_configPathLabel);
    layout->addWidget(privacyPanel);
    layout->addStretch();

    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [this] { applyTheme(); saveSettings(); });
    connect(accentButton, &QPushButton::clicked, this, [this] {
        const QColor color = QColorDialog::getColor(QColor(m_settings.accent), this, "Choose OpenSrc accent");
        if (color.isValid()) { m_settings.accent = color.name(); applyTheme(); saveSettings(); }
    });
    return page;
}

QWidget *MainWindow::makeArchitecturePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 26, 30, 25);
    layout->setSpacing(13);
    layout->addWidget(label("RENDERING EXPERIMENTS", "eyebrow"));
    layout->addWidget(label("Inside the graphics path.", "pageTitle"));
    layout->addWidget(label("A Windows-specific view of the pipeline. The provided VirGL diagram describes a Linux host path; WHPX is a CPU accelerator, not a GPU renderer.", "subtitle"));

    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    auto *guest = makePanel("guestLane");
    auto *guestLayout = new QVBoxLayout(guest);
    guestLayout->setContentsMargins(16, 15, 16, 15);
    guestLayout->setSpacing(8);
    guestLayout->addWidget(label("ANDROID GUEST  ·  SELECTED AVD", "eyebrow"));
    const QStringList guestSteps = {"Android framework and apps", "AVD system image", "AVD hardware profile", "Persistent AVD user data"};
    for (int i = 0; i < guestSteps.size(); ++i) {
        auto *step = makePanel("pipelineNode");
        auto *stepLayout = new QHBoxLayout(step);
        stepLayout->setContentsMargins(10, 7, 10, 7);
        stepLayout->addWidget(label(QString::number(i + 1).rightJustified(2, '0'), "stepNumber"));
        stepLayout->addWidget(label(guestSteps.at(i), "pipelineText"));
        stepLayout->addStretch();
        guestLayout->addWidget(step);
        if (i + 1 < guestSteps.size()) guestLayout->addWidget(label("↓", "flowArrow"), 0, Qt::AlignHCenter);
    }
    columns->addWidget(guest, 1);

    auto *host = makePanel("hostLane");
    auto *hostLayout = new QVBoxLayout(host);
    hostLayout->setContentsMargins(16, 15, 16, 15);
    hostLayout->setSpacing(8);
    hostLayout->addWidget(label("WINDOWS HOST  ·  ANDROID EMULATOR", "eyebrow"));
    const QStringList hostSteps = {"OpenSrc starts the selected AVD", "Android Emulator manages virtual devices", "Android Emulator selects a graphics backend", "Device appears in the Emulator window"};
    for (int i = 0; i < hostSteps.size(); ++i) {
        auto *step = makePanel("hostNode");
        auto *stepLayout = new QHBoxLayout(step);
        stepLayout->setContentsMargins(10, 9, 10, 9);
        stepLayout->addWidget(label(QString::number(i + 1).rightJustified(2, '0'), "stepNumber"));
        stepLayout->addWidget(label(hostSteps.at(i), "pipelineText"));
        stepLayout->addStretch();
        hostLayout->addWidget(step);
        if (i + 1 < hostSteps.size()) hostLayout->addWidget(label("↕", "flowArrow"), 0, Qt::AlignHCenter);
    }
    hostLayout->addSpacing(8);
    auto *whpx = makePanel("noticePanel");
    auto *whpxLayout = new QVBoxLayout(whpx);
    whpxLayout->setContentsMargins(11, 10, 11, 10);
    whpxLayout->addWidget(label("CPU VIRTUALIZATION", "eyebrow"));
    whpxLayout->addWidget(label("Android Emulator → Windows hypervisor (when available)", "pipelineText"));
    whpxLayout->addWidget(label("The Android Emulator chooses its supported acceleration and graphics settings. OpenSrc does not configure a separate GPU passthrough path.", "mutedText"));
    hostLayout->addWidget(whpx);
    hostLayout->addStretch();
    columns->addWidget(host, 1);
    layout->addLayout(columns, 1);
    auto *caveat = makePanel("warningPanel");
    auto *caveatLayout = new QHBoxLayout(caveat);
    caveatLayout->setContentsMargins(13, 10, 13, 10);
    caveatLayout->addWidget(label("!", "warningMark"));
    caveatLayout->addWidget(label("OpenSrc delegates rendering to Google's Android Emulator in AVD mode. Raw QEMU does not currently provide an integrated Android display or a validated Windows VirGL path.", "calloutText"));
    layout->addWidget(caveat);
    return page;
}

QWidget *MainWindow::makeLogsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(30, 26, 30, 25);
    layout->setSpacing(13);
    auto *heading = new QHBoxLayout;
    auto *copy = new QVBoxLayout;
    copy->addWidget(label("ANDROID RUNTIME", "eyebrow"));
    copy->addWidget(label("Logs & diagnostics.", "pageTitle"));
    copy->addWidget(label("Inspect Android Emulator or raw QEMU output and launch arguments.", "subtitle"));
    heading->addLayout(copy);
    heading->addStretch();
    auto *clear = new QPushButton("Clear log"); clear->setObjectName("browseButton");
    connect(clear, &QPushButton::clicked, this, [this] { if (m_logView) m_logView->clear(); });
    heading->addWidget(clear);
    layout->addLayout(heading);
    auto *panel = makePanel();
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(14, 13, 14, 13);
    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(label("LIVE OUTPUT", "eyebrow"));
    toolbar->addStretch();
    toolbar->addWidget(label("No remote reporting", "localBadge"));
    panelLayout->addLayout(toolbar);
    m_logView = new QTextEdit;
    m_logView->setObjectName("logView");
    m_logView->setReadOnly(true);
    m_logView->setPlaceholderText("Android Emulator or raw QEMU output appears here after launch.");
    panelLayout->addWidget(m_logView, 1);
    layout->addWidget(panel, 1);
    return page;
}
