#include "mainwindow.h"
#include "addprogramdialog.h"
#include "connectdialog.h"
#include "guestserverdialog.h"
#include <QApplication>
#include <QStyleFactory>
#include <QDebug>
#include <QCheckBox>
#include <QMouseEvent>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QMessageBox>
#include <QStandardPaths>

namespace {
constexpr quint16 kGuestServerPort = 7148;

QString runProcess(const QString &program, const QStringList &args, bool *ok)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(5000)) {
        if (ok) *ok = false;
        p.kill();
        return {};
    }
    if (ok) {
        *ok = (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
    }
    return QString::fromLocal8Bit(p.readAllStandardOutput());
}

QString extractIpAddress(const QString &text)
{
    static const QRegularExpression ipRegex(QStringLiteral("(\\d{1,3}(?:\\.\\d{1,3}){3})(?:/\\d{1,2})?"));
    auto match = ipRegex.match(text);
    if (match.hasMatch()) {
        QString value = match.captured(1);
        return value;
    }
    return {};
}

QStringList extractNetworksFromXml(const QString &xml)
{
    QStringList networks;
    QRegularExpression networkRegex(QStringLiteral("<interface[^>]*>.*?<source[^>]*network=\"([^\"']+)\""), QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = networkRegex.globalMatch(xml);
    while (it.hasNext()) {
        auto match = it.next();
        QString network = match.captured(1);
        if (!network.isEmpty() && !networks.contains(network)) {
            networks.append(network);
        }
    }
    return networks;
}

QString getMacFromXml(const QString &xml)
{
    QRegularExpression re(QStringLiteral("<mac\\s+address=\"?([^\"'/>]+)\"?"));
    auto match = re.match(xml);
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return {};
}

QString getIpFromDomIfAddr(const QString &vmName)
{
    const QList<QStringList> commands = {
        {QStringLiteral("domifaddr"), vmName, QStringLiteral("--source"), QStringLiteral("agent")},
        {QStringLiteral("domifaddr"), vmName}
    };

    for (const QStringList &args : commands) {
        bool ok = false;
        QString output = runProcess(QStringLiteral("virsh"), args, &ok);
        if (!ok || output.isEmpty()) {
            continue;
        }

        QString ip = extractIpAddress(output);
        if (!ip.isEmpty()) {
            return ip;
        }
    }

    return {};
}

QString getIpForMac(const QString &mac, const QStringList &preferredNetworks)
{
    QStringList networks = preferredNetworks;
    if (networks.isEmpty()) {
        networks = {QStringLiteral("default"), QStringLiteral("virbr0"), QStringLiteral("bridge")};
    }

    for (const QString &network : networks) {
        bool ok = false;
        QString output = runProcess(QStringLiteral("virsh"), {QStringLiteral("net-dhcp-leases"), network, QStringLiteral("--mac"), mac}, &ok);
        if (!ok || output.isEmpty()) {
            continue;
        }
        QString ip = extractIpAddress(output);
        if (!ip.isEmpty()) {
            return ip;
        }
    }
    return {};
}

QString resolveGuestIp(const QString &vmName)
{
    QString ip = getIpFromDomIfAddr(vmName);
    if (!ip.isEmpty()) {
        return ip;
    }

    bool ok = false;
    QString xml = runProcess(QStringLiteral("virsh"), {QStringLiteral("dumpxml"), vmName}, &ok);
    if (!ok || xml.isEmpty()) {
        return {};
    }

    QString mac = getMacFromXml(xml);
    if (mac.isEmpty()) {
        return {};
    }

    QStringList networks = extractNetworksFromXml(xml);
    return getIpForMac(mac, networks);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      addProgramDialog(nullptr),
      m_guestServerWidget(new GuestServerWidget("", 0, "", this)),
      m_guestServerAppsClient(new GuestServerAppsClient("", 0, this)),
      m_appsListWidget(new AppsListWidget(this)),
      rdpProcess(new QProcess(this)),
      m_guestServerRefreshTimer(new QTimer(this)),
      m_vmListRefreshTimer(new QTimer(this))
{
    // Set window properties
    setWindowTitle("WinRun");
    setMinimumSize(800, 600);
    
    // Setup styles
    sidebarStyle = "QFrame { background-color: #1a535c; border: none; } ";
    
    headerStyle = "QFrame { background-color: #4ecdc4; border: none; padding: 15px; } "
                 "QLabel { color: white; font-size: 18px; font-weight: bold; } ";
    
    navButtonStyle = "QPushButton { "
                    "    text-align: left; "
                    "    padding: 18px 24px; "
                    "    border: none; "
                    "    color: #b2ebf2; "
                    "    background: transparent; "
                    "    font-size: 20px; "
                    "} "
                    "QPushButton:hover { "
                    "    background-color: #2a7a83; "
                    "    color: white; "
                    "} "
                    "QPushButton:checked { "
                    "    background-color: #4ecdc4; "
                    "    color: white; "
                    "    border-left: 4px solid white; "
                    "} ";
    
    addButtonStyle = "QPushButton { "
                    "    background-color: #1a535c; "
                    "    color: white; "
                    "    border: none; "
                    "    border-radius: 15px; "
                    "    padding: 15px 30px; "
                    "    font-size: 16px; "
                    "    font-weight: bold; "
                    "} "
                    "QPushButton:hover { "
                    "    background-color: #2a7a83; "
                    "} ";
    
    // Setup guest server refresh timer (retry every 5 seconds if IP not found)
    m_guestServerRefreshTimer->setInterval(5000);
    m_guestServerRefreshTimer->setSingleShot(false);
    connect(m_guestServerRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshGuestServerEndpoint);
    
    // Setup VM list refresh timer (refresh every 3 seconds to catch state changes)
    m_vmListRefreshTimer->setInterval(3000);
    m_vmListRefreshTimer->setSingleShot(false);
    connect(m_vmListRefreshTimer, &QTimer::timeout, this, [this]() {
        refreshVMList();
        updateVmControls();
        // Refresh guest server endpoint when VM list is refreshed (if on Desktop page)
        if (stackedWidget && stackedWidget->currentWidget() == desktopPage) {
            refreshGuestServerEndpoint();
        }
    });
    m_vmListRefreshTimer->start();
    
    setupUI();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Create central widget and main layout
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Main horizontal layout (sidebar + content)
    mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Setup sidebar and content
    setupSidebar();
    setupMainContent();
    
    // Add sidebar and content to main layout
    mainLayout->addWidget(sidebar, 1);
    mainLayout->addLayout(contentLayout, 5);
}

void MainWindow::setupSidebar()
{
    // Create sidebar frame
    sidebar = new QFrame();
    sidebar->setStyleSheet(sidebarStyle);
    sidebar->setFixedWidth(240);
    
    // Create sidebar layout
    sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setSpacing(0);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    
    // Add spacer to push buttons to top
    QSpacerItem *verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    
    // Create navigation buttons
    allAppsBtn = new QPushButton("  All App");
    allAppsBtn->setIcon(QIcon(":/icons/icon/main.png"));
    allAppsBtn->setIconSize(QSize(28, 28));
    allAppsBtn->setCheckable(true);
    allAppsBtn->setChecked(true);
    
    desktopBtn = new QPushButton("  Desktop");
    desktopBtn->setIcon(QIcon(":/icons/icon/desktop.png"));
    desktopBtn->setIconSize(QSize(28, 28));
    desktopBtn->setCheckable(true);
    
    fileBtn = new QPushButton("  File");
    fileBtn->setIcon(QIcon(":/icons/icon/folder.png"));
    fileBtn->setIconSize(QSize(28, 28));
    fileBtn->setCheckable(true);
    
    settingsBtn = new QPushButton("  Setting");
    settingsBtn->setIcon(QIcon(":/icons/icon/settings.png"));
    settingsBtn->setIconSize(QSize(28, 28));
    settingsBtn->setCheckable(true);
    
    aboutBtn = new QPushButton("  About");
    aboutBtn->setIcon(QIcon(":/icons/icon/info.png"));
    aboutBtn->setIconSize(QSize(28, 28));
    aboutBtn->setCheckable(true);
    
    // Set button styles
    QList<QPushButton*> buttons = {allAppsBtn, desktopBtn, fileBtn, settingsBtn, aboutBtn};
    for (QPushButton* btn : buttons) {
        btn->setStyleSheet(navButtonStyle);
        btn->setCursor(Qt::PointingHandCursor);
    }
    
    // Add widgets to sidebar layout
    sidebarLayout->addSpacing(20);
    sidebarLayout->addWidget(allAppsBtn);
    sidebarLayout->addWidget(desktopBtn);
    sidebarLayout->addWidget(fileBtn);
    sidebarLayout->addWidget(settingsBtn);
    sidebarLayout->addWidget(aboutBtn);
    sidebarLayout->addItem(verticalSpacer);
    
    // Connect signals
    connect(allAppsBtn, &QPushButton::clicked, this, &MainWindow::onAllAppsClicked);
    connect(desktopBtn, &QPushButton::clicked, this, &MainWindow::onDesktopClicked);
    connect(fileBtn, &QPushButton::clicked, this, &MainWindow::onFileClicked);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(aboutBtn, &QPushButton::clicked, this, &MainWindow::onAboutClicked);
}

void MainWindow::setupMainContent()
{
    // Create content layout
    contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(0);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create header
    header = new QFrame();
    header->setStyleSheet("QFrame { background-color: #1a535c; border: none; padding: 15px; }");
    header->setFixedHeight(60);
    
    headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);
    
    titleLabel = new QLabel("All Programs");
    
    // Create collapse button
    collapseBtn = new QPushButton();
    collapseBtn->setIcon(QIcon(":/icons/icon/menu.png"));
    collapseBtn->setIconSize(QSize(24, 24));
    collapseBtn->setStyleSheet(
        "QPushButton { "
        "    background: transparent; "
        "    border: none; "
        "    padding: 5px; "
        "    border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "    background-color: rgba(255, 255, 255, 0.2); "
        "}"
    );
    collapseBtn->setCursor(Qt::PointingHandCursor);
    collapseBtn->setCheckable(true);
    connect(collapseBtn, &QPushButton::toggled, this, &MainWindow::onCollapseClicked);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(collapseBtn);
    
    // Create scroll area for content
    contentScrollArea = new QScrollArea();
    contentScrollArea->setWidgetResizable(true);
    contentScrollArea->setFrameShape(QFrame::NoFrame);
    contentScrollArea->setStyleSheet(
        "QScrollArea { "
        "    border: none; "
        "    background-color: white; "
        "}"
        "QScrollBar:vertical { "
        "    background: #f0f0f0; "
        "    width: 12px; "
        "    border-radius: 6px; "
        "    margin: 0px; "
        "}"
        "QScrollBar::handle:vertical { "
        "    background: #c0c0c0; "
        "    min-height: 30px; "
        "    border-radius: 6px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "    background: #a0a0a0; "
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "    height: 0px; "
        "}"
        "QScrollBar:horizontal { "
        "    background: #f0f0f0; "
        "    height: 12px; "
        "    border-radius: 6px; "
        "    margin: 0px; "
        "}"
        "QScrollBar::handle:horizontal { "
        "    background: #c0c0c0; "
        "    min-width: 30px; "
        "    border-radius: 6px; "
        "}"
        "QScrollBar::handle:horizontal:hover { "
        "    background: #a0a0a0; "
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { "
        "    width: 0px; "
        "}"
    );
    
    // Create stacked widget for pages
    stackedWidget = new QStackedWidget();
    
    // Create All Programs page
    allProgramsPage = new QWidget();
    allProgramsLayout = new QVBoxLayout(allProgramsPage);
    allProgramsLayout->setContentsMargins(40, 20, 40, 40);
    allProgramsLayout->setSpacing(20);
    allProgramsLayout->setAlignment(Qt::AlignTop);

    // Logo and Title
    QHBoxLayout *logoLayout = new QHBoxLayout();
    logoLabel = new QLabel();
    logoLabel->setPixmap(QPixmap(":/icons/icon/logo.png").scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    winrunLabel = new QLabel("WinRun");
    winrunLabel->setStyleSheet("font-size: 32px; font-weight: bold; color: #1a535c; margin-left: 15px;");
    logoLayout->addWidget(logoLabel);
    logoLayout->addWidget(winrunLabel);
    logoLayout->addStretch();
    allProgramsLayout->addLayout(logoLayout);
    
    // Installed Programs Section
    QLabel *appsLabel = new QLabel("Installed Programs");
    appsLabel->setStyleSheet(
        "font-size: 18px; font-weight: bold; color: #1a535c; "
        "margin-top: 20px; margin-bottom: 10px; padding-bottom: 5px; "
        "border-bottom: 1px solid #e0e0e0;"
    );
    allProgramsLayout->addWidget(appsLabel);
    
    // Add apps list widget
    allProgramsLayout->addWidget(m_appsListWidget, 1);
    
    // Add Programs button
    addProgramsBtn = new QPushButton("Add new programs");
    addProgramsBtn->setStyleSheet(addButtonStyle);
    addProgramsBtn->setCursor(Qt::PointingHandCursor);
    addProgramsBtn->setMinimumSize(200, 50);
    
    // Add hint text
    addProgramsHint = new QLabel("Add new programs");
    addProgramsHint->setStyleSheet("color: #ff6b6b; font-size: 14px; margin-top: 5px;");
    addProgramsHint->setAlignment(Qt::AlignCenter);
    addProgramsHint->hide();
    
    // Add widgets to layout (logo and title at top, button at bottom)
    // Logo and title are already added above
    allProgramsLayout->addStretch();
    allProgramsLayout->addWidget(addProgramsBtn, 0, Qt::AlignCenter);
    allProgramsLayout->addWidget(addProgramsHint, 0, Qt::AlignCenter);
    
    // Setup other pages
    setupDesktopPage();
    setupFilePage();
    setupSettingsPage();
    setupAboutPage();
    
    // Add pages to stacked widget
    stackedWidget->addWidget(allProgramsPage);
    stackedWidget->addWidget(desktopPage);
    stackedWidget->addWidget(filePage);
    stackedWidget->addWidget(settingsPage);
    stackedWidget->addWidget(aboutPage);
    
    // Set stacked widget as the scroll area's widget
    contentScrollArea->setWidget(stackedWidget);
    
    // Add header and scroll area to content layout
    contentLayout->addWidget(header);
    contentLayout->addWidget(contentScrollArea);
    
    // Connect signals
    connect(addProgramsBtn, &QPushButton::clicked, this, &MainWindow::onAddProgramsClicked);
    
    // Connect apps client signals
    connect(m_guestServerAppsClient, &GuestServerAppsClient::appsReceived,
            m_appsListWidget, &AppsListWidget::setApps);
    connect(m_guestServerAppsClient, &GuestServerAppsClient::appsReceived,
            this, &MainWindow::onAppsReceived);
    connect(m_guestServerAppsClient, &GuestServerAppsClient::iconReceived,
            m_appsListWidget, &AppsListWidget::setIcon);
    connect(m_guestServerAppsClient, &GuestServerAppsClient::error,
            this, [this](const QString &error) {
                qWarning() << "Apps client error:" << error;
            });
}

// Slots implementation
void MainWindow::onAllAppsClicked()
{
    // Update button states
    allAppsBtn->setChecked(true);
    desktopBtn->setChecked(false);
    fileBtn->setChecked(false);
    settingsBtn->setChecked(false);
    aboutBtn->setChecked(false);
    
    // Show the all programs page
    stackedWidget->setCurrentWidget(allProgramsPage);
    
    // Stop guest server monitoring when not on Desktop page
    if (m_guestServerWidget) {
        m_guestServerWidget->stopMonitoring();
    }
    if (m_guestServerRefreshTimer && m_guestServerRefreshTimer->isActive()) {
        m_guestServerRefreshTimer->stop();
    }
    
    // Refresh apps list
    refreshAppsList();
}

void MainWindow::onDesktopClicked()
{
    // Update button states
    allAppsBtn->setChecked(false);
    desktopBtn->setChecked(true);
    fileBtn->setChecked(false);
    settingsBtn->setChecked(false);
    aboutBtn->setChecked(false);
    
    // Show the desktop page
    stackedWidget->setCurrentWidget(desktopPage);
    titleLabel->setText("Desktop");
    
    // Start guest server monitoring on Desktop page
    if (m_guestServerWidget) {
        m_guestServerWidget->setVisible(true);
        refreshGuestServerEndpoint(); // This will start the timer if IP not found
        m_guestServerWidget->startMonitoring(5000); // Update every 5 seconds
    }
    
    // Refresh apps list when switching to Desktop (in case endpoint was configured)
    refreshAppsList();
    
    refreshVMList();
    updateVmControls();
}

void MainWindow::onFileClicked()
{
    stackedWidget->setCurrentWidget(filePage);
    titleLabel->setText("File Manager");
    
    // Update button states
    allAppsBtn->setChecked(false);
    desktopBtn->setChecked(false);
    fileBtn->setChecked(true);
    settingsBtn->setChecked(false);
    aboutBtn->setChecked(false);
    
    // Stop guest server monitoring and refresh timer when not on Desktop/All Apps page
    if (m_guestServerWidget) {
        m_guestServerWidget->stopMonitoring();
    }
    if (m_guestServerRefreshTimer && m_guestServerRefreshTimer->isActive()) {
        m_guestServerRefreshTimer->stop();
    }
}

void MainWindow::onSettingsClicked()
{
    stackedWidget->setCurrentWidget(settingsPage);
    titleLabel->setText("Settings");
    
    // Update button states
    allAppsBtn->setChecked(false);
    desktopBtn->setChecked(false);
    fileBtn->setChecked(false);
    settingsBtn->setChecked(true);
    aboutBtn->setChecked(false);
    
    // Stop guest server monitoring and refresh timer when not on Desktop/All Apps page
    if (m_guestServerWidget) {
        m_guestServerWidget->stopMonitoring();
    }
    if (m_guestServerRefreshTimer && m_guestServerRefreshTimer->isActive()) {
        m_guestServerRefreshTimer->stop();
    }
}

void MainWindow::onAboutClicked()
{
    stackedWidget->setCurrentWidget(aboutPage);
    titleLabel->setText("About");
    
    // Update button states
    allAppsBtn->setChecked(false);
    desktopBtn->setChecked(false);
    fileBtn->setChecked(false);
    settingsBtn->setChecked(false);
    aboutBtn->setChecked(true);
    
    // Stop guest server monitoring and refresh timer when not on Desktop/All Apps page
    if (m_guestServerWidget) {
        m_guestServerWidget->stopMonitoring();
    }
    if (m_guestServerRefreshTimer && m_guestServerRefreshTimer->isActive()) {
        m_guestServerRefreshTimer->stop();
    }
}

void MainWindow::onAddProgramsClicked()
{
    if (!addProgramDialog) {
        addProgramDialog = new AddProgramDialog(this);
        
        // Position the dialog near the button
        QPoint buttonPos = addProgramsBtn->mapToGlobal(QPoint(0, 0));
        QSize buttonSize = addProgramsBtn->size();
        
        // Calculate position to center the dialog below the button
        int x = buttonPos.x() + buttonSize.width()/2 - 200; // 200 is half of dialog width
        int y = buttonPos.y() + buttonSize.height() + 5; // 5px below the button
        
        addProgramDialog->move(x, y);
    }
    
    // Show the dialog
    addProgramDialog->exec();
    
    // Clean up the dialog when done
    if (addProgramDialog) {
        addProgramDialog->deleteLater();
        addProgramDialog = nullptr;
    }
}

void MainWindow::onCollapseClicked(bool checked)
{
    // Toggle sidebar visibility
    sidebar->setVisible(!checked);
    
    // Toggle between menu icons
    if (checked) {
        collapseBtn->setIcon(QIcon(":/icons/icon/menur.png"));
        sidebar->setFixedWidth(0);
    } else {
        collapseBtn->setIcon(QIcon(":/icons/icon/menu.png"));
        sidebar->setFixedWidth(240);
    }
}

void MainWindow::onConnectToGuestServer()
{
    GuestServerDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString hostname = dialog.hostname();
        int port = dialog.port();
        QString username = dialog.username();
        QString password = dialog.password();
        
        QStringList args;
        args << QString("/v:%1:%2").arg(hostname).arg(port);
        
        if (!username.isEmpty()) {
            args << "/u:" + username;
        }
        
        if (!password.isEmpty()) {
            args << "/p:" + password;
        }
        
        QString program;
#ifdef Q_OS_WINDOWS
        program = "mstsc.exe";
#else
        program = "xfreerdp";
        // Add some common RDP options for FreeRDP
        args << "/f" << "/multimon" << "/w:1920" << "/h:1080";
#endif
        
        // Kill any existing RDP connection
        if (rdpProcess->state() != QProcess::NotRunning) {
            rdpProcess->kill();
            rdpProcess->waitForFinished(1000);
        }
        
        // Start the RDP client
        rdpProcess->start(program, args);
        
        if (!rdpProcess->waitForStarted(3000)) {
            QMessageBox::warning(this, "Connection Failed", 
                "Failed to start RDP client. Make sure 'xfreerdp' is installed on Linux or 'mstsc' is available on Windows.");
        }
    }
}

void MainWindow::setupDesktopPage()
{
    desktopPage = new QWidget();
    QHBoxLayout *rootLayout = new QHBoxLayout(desktopPage);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Right side: VM controls and actions
    QWidget *controlsContainer = new QWidget();
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsContainer);
    controlsLayout->setContentsMargins(30, 30, 30, 30);
    controlsLayout->setSpacing(18);

    QHBoxLayout *vmSelectLayout = new QHBoxLayout();
    QLabel *vmText = new QLabel("VM :");
    vmText->setStyleSheet("color: #2a7a83; font-size: 20px; font-weight: 600;");
    vmCombo = new QComboBox();
    vmCombo->setMinimumWidth(240);
    vmCombo->setStyleSheet("QComboBox { font-size: 16px; padding: 6px; }");
    connect(vmCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onVmSelectionChanged);
    vmSelectLayout->addWidget(vmText);
    vmSelectLayout->addWidget(vmCombo, 1);
    controlsLayout->addLayout(vmSelectLayout);

    vmStatusLabel = new QLabel("Status: Unknown");
    vmStatusLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #607d8b;");
    controlsLayout->addWidget(vmStatusLabel);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(12);
    QString ctrlBtnStyle =
        "QPushButton { background-color: #1a535c; color: white; border: none; padding: 12px; border-radius: 6px; } "
        "QPushButton:hover { background-color: #2a7a83; }";

    vmStartBtn = new QPushButton();
    vmStartBtn->setFixedSize(96, 72);
    vmStartBtn->setStyleSheet(ctrlBtnStyle);
    vmStartBtn->setIcon(QIcon(":/icons/icon/start.png"));
    vmStartBtn->setIconSize(QSize(36, 36));
    vmStartBtn->setToolTip("Start VM");

    vmStopBtn = new QPushButton();
    vmStopBtn->setFixedSize(96, 72);
    vmStopBtn->setStyleSheet(ctrlBtnStyle);
    vmStopBtn->setIcon(QIcon(":/icons/icon/stop.png"));
    vmStopBtn->setIconSize(QSize(36, 36));
    vmStopBtn->setToolTip("Stop VM");

    vmRestartBtn = new QPushButton();
    vmRestartBtn->setFixedSize(96, 72);
    vmRestartBtn->setStyleSheet(ctrlBtnStyle);
    vmRestartBtn->setIcon(QIcon(":/icons/icon/reset.png"));
    vmRestartBtn->setIconSize(QSize(36, 36));
    vmRestartBtn->setToolTip("Restart VM");

    buttonRow->addWidget(vmStartBtn);
    buttonRow->addWidget(vmStopBtn);
    buttonRow->addWidget(vmRestartBtn);
    controlsLayout->addLayout(buttonRow);

    vmConnectBtn = new QPushButton(QString::fromUtf8("🔗 Connect to Desktop"));
    vmConnectBtn->setStyleSheet(
        "QPushButton { background-color: #1a535c; color: white; border: none; padding: 14px 16px; font-size: 18px; font-weight: 600; border-radius: 6px; } "
        "QPushButton:hover { background-color: #2a7a83; }"
    );
    controlsLayout->addWidget(vmConnectBtn);

    guestServerBtn = new QPushButton("Connect to Guest Server");
    guestServerBtn->setStyleSheet(
        "QPushButton { background-color: #ff9f1c; color: white; border: none; padding: 12px 16px; border-radius: 6px; font-weight: 600; } "
        "QPushButton:hover { background-color: #ffbf69; }"
    );
    connect(guestServerBtn, &QPushButton::clicked, this, &MainWindow::onConnectToGuestServer);
    controlsLayout->addWidget(guestServerBtn);

    // Guest Server Monitoring Section
    QLabel *monitorLabel = new QLabel("Guest Server Monitoring");
    monitorLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #2a7a83; "
        "margin-top: 20px; margin-bottom: 10px; padding-bottom: 5px; "
        "border-bottom: 1px solid #e0e0e0;"
    );
    controlsLayout->addWidget(monitorLabel);
    
    // Add guest server widget with a frame for better visual separation
    QFrame *monitorFrame = new QFrame();
    monitorFrame->setFrameShape(QFrame::StyledPanel);
    monitorFrame->setStyleSheet(
        "QFrame { "
        "  background-color: #f8f9fa; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 6px; "
        "  padding: 15px; "
        "  margin-bottom: 10px;"
        "}"
    );
    
    QVBoxLayout *monitorLayout = new QVBoxLayout(monitorFrame);
    monitorLayout->setContentsMargins(5, 5, 5, 5);
    monitorLayout->addWidget(m_guestServerWidget);
    controlsLayout->addWidget(monitorFrame);

    controlsLayout->addStretch();

    rootLayout->addWidget(controlsContainer, 2);

    connect(vmStartBtn, &QPushButton::clicked, this, &MainWindow::onVmStart);
    connect(vmStopBtn, &QPushButton::clicked, this, &MainWindow::onVmStop);
    connect(vmRestartBtn, &QPushButton::clicked, this, &MainWindow::onVmRestart);
    connect(vmConnectBtn, &QPushButton::clicked, this, &MainWindow::onVmConnect);

    refreshVMList();
    updateVmControls();
}

void MainWindow::setupFilePage()
{
    filePage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(filePage);
    layout->setAlignment(Qt::AlignCenter);
    
    QLabel *label = new QLabel("File Manager");
    label->setStyleSheet("font-size: 24px; color: #1a535c;");
    label->setAlignment(Qt::AlignCenter);
    
    // Add a sample file list or other file manager components here
    
    layout->addWidget(label);
}

void MainWindow::setupSettingsPage()
{
    settingsPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(settingsPage);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->setContentsMargins(30, 20, 30, 20);
    
    QLabel *titleLabel = new QLabel("Settings");
    titleLabel->setStyleSheet("font-size: 24px; color: #1a535c; font-weight: bold; margin-bottom: 20px;");
    
    // Add some sample settings
    QCheckBox *darkMode = new QCheckBox("Dark Mode");
    QCheckBox *notifications = new QCheckBox("Enable Notifications");
    QPushButton *saveBtn = new QPushButton("Save Settings");
    
    // Style the components
    QString checkBoxStyle = "QCheckBox { font-size: 14px; color: #1a535c; margin: 10px 0; } ";
    darkMode->setStyleSheet(checkBoxStyle);
    notifications->setStyleSheet(checkBoxStyle);
    
    saveBtn->setStyleSheet("QPushButton { "
        "background-color: #1a535c; "
        "color: white; "
        "border: none; "
        "padding: 8px 20px; "
        "border-radius: 4px; "
        "margin-top: 20px; }");
    
    layout->addWidget(titleLabel);
    layout->addWidget(darkMode);
    layout->addWidget(notifications);
    layout->addWidget(saveBtn);
    layout->addStretch();
}

void MainWindow::refreshVMList()
{
    if (!vmCombo) return;
    vmCombo->clear();
    vmStateByName.clear();

    QString out, err;
    if (!runLibvirtCommand({"list"}, &out, &err)) {
        qWarning() << "libvirt manager List failed:" << err;
        vmCombo->addItem("---------");
        return;
    }

    QRegularExpression ansi("\\x1B\\[[0-9;]*m");
    out.replace(ansi, "");

    QStringList lines = out.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (!line.startsWith('|')) continue;
        QStringList cols = line.split('|');
        if (cols.size() < 3) continue;
        QString name = cols.value(1).trimmed();
        QString state = cols.value(2).trimmed();
        if (name.compare("Name", Qt::CaseInsensitive) == 0) continue;
        if (name.isEmpty()) continue;
        vmStateByName.insert(name, state);
        vmCombo->addItem(name);
    }

    if (vmCombo->count() == 0) {
        vmCombo->addItem("---------");
    }
}

void MainWindow::updateVmControls()
{
    bool hasVm = vmCombo && vmCombo->currentIndex() >= 0 && vmCombo->currentText() != "---------";
    QString state = vmStateByName.value(vmCombo ? vmCombo->currentText() : QString());
    QString st = state.toLower();
    bool running = st.contains("run");
    vmStartBtn->setEnabled(hasVm && !running);
    vmStopBtn->setEnabled(hasVm && running);
    vmRestartBtn->setEnabled(hasVm && running);
    vmConnectBtn->setEnabled(hasVm);

    // Update status label
    if (vmStatusLabel) {
        QString label = "Unknown";
        QString color = "#888";
        if (hasVm) {
            if (st.contains("run")) { label = "Running"; color = "#2ecc71"; }
            else if (st.contains("stop")) { label = "Stopped"; color = "#e74c3c"; }
            else if (st.contains("pause")) { label = "Paused"; color = "#f1c40f"; }
        }
        vmStatusLabel->setText(label);
        vmStatusLabel->setStyleSheet(QString("font-size: 18px; font-weight: 700; color: %1;").arg(color));
    }
}

void MainWindow::refreshGuestServerEndpoint()
{
    if (!m_guestServerWidget) {
        return;
    }

    QString ip;
    QString vmName = vmCombo ? vmCombo->currentText() : QString();
    bool hasVm = !vmName.isEmpty() && vmName != "---------";

    if (hasVm) {
        // Check if VM is running first
        QString state = vmStateByName.value(vmName).toLower();
        bool isRunning = state.contains("run");
        
        if (isRunning) {
            // Use the robust IP resolution with multiple fallback methods
            // First try QEMU agent (getIpFromDomIfAddr)
            ip = getIpFromDomIfAddr(vmName);
            
            // If that fails, try MAC-based lookup
            if (ip.isEmpty()) {
                bool ok = false;
                QString xml = runProcess(QStringLiteral("virsh"), {QStringLiteral("dumpxml"), vmName}, &ok);
                if (ok && !xml.isEmpty()) {
                    QStringList networks = extractNetworksFromXml(xml);
                    QString mac = getMacFromXml(xml);
                    if (!mac.isEmpty()) {
                        ip = getIpForMac(mac, networks);
                    }
                }
            }
        }
    }

    if (!ip.isEmpty()) {
        // IP found - stop the refresh timer and configure the server
        if (m_guestServerRefreshTimer->isActive()) {
            m_guestServerRefreshTimer->stop();
        }
        m_currentGuestServerIp = ip;
        m_guestServerWidget->configureServer(ip, kGuestServerPort);
        m_guestServerAppsClient->setServerEndpoint(ip, kGuestServerPort);
        // Refresh apps list when endpoint is configured
        refreshAppsList();
        qDebug() << "Guest server endpoint configured:" << ip << ":" << kGuestServerPort;
    } else {
        // IP not found - clear endpoint and start/continue timer if on All Apps page
        m_currentGuestServerIp.clear();
        m_guestServerWidget->configureServer(QString(), 0);
        m_guestServerAppsClient->setServerEndpoint(QString(), 0);
        
        // Start timer if we're on Desktop page and have a VM selected
        if (hasVm && stackedWidget && stackedWidget->currentWidget() == desktopPage) {
            if (!m_guestServerRefreshTimer->isActive()) {
                m_guestServerRefreshTimer->start();
            }
            // Only log error if we have a VM selected and it's running but IP couldn't be resolved
            QString state = vmStateByName.value(vmName).toLower();
            bool isRunning = state.contains("run");
            if (isRunning) {
                qDebug() << "Could not resolve VM IP for guest server. VM:" << vmName << "Will retry...";
            } else {
                qDebug() << "VM not running. VM:" << vmName << "Waiting for VM to start...";
            }
        } else {
            // Stop timer if not on Desktop page or no VM selected
            if (m_guestServerRefreshTimer->isActive()) {
                m_guestServerRefreshTimer->stop();
            }
            // Only log if VM is selected but we're not on the right page
            if (hasVm) {
                qDebug() << "VM selected but not on Desktop page. VM:" << vmName;
            }
        }
    }
}

void MainWindow::refreshAppsList()
{
    if (!m_guestServerAppsClient || !m_appsListWidget) {
        return;
    }
    
    // Only fetch if we have a valid endpoint and we're on All Apps page
    if (!m_currentGuestServerIp.isEmpty() && 
        stackedWidget && 
        stackedWidget->currentWidget() == allProgramsPage) {
        m_guestServerAppsClient->fetchApps();
    } else {
        // Clear apps list if no endpoint or not on All Apps page
        if (stackedWidget && stackedWidget->currentWidget() == allProgramsPage) {
            m_appsListWidget->clear();
        }
    }
}

QString MainWindow::findLibvirtManager() const
{
    QByteArray envPath = qgetenv("WINRUN_LIBVIRT_MGR");
    if (!envPath.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(envPath))) {
        return QString::fromLocal8Bit(envPath);
    }

    QString appDir = QCoreApplication::applicationDirPath();
    QDir dir(appDir);
    QStringList rels = {
        QStringLiteral("libvirt_rdp_manager"),
        QStringLiteral("syscore/libvirt_rdp_manager/target/release/libvirt_rdp_manager"),
        QStringLiteral("syscore/libvirt_rdp_manager/target/debug/libvirt_rdp_manager")
    };

    for (int up = 0; up < 5; ++up) {
        for (const QString &rel : rels) {
            QString cand = dir.filePath(rel);
            if (QFileInfo::exists(cand)) return cand;
        }
        dir.cdUp();
    }
    return QString();
}

bool MainWindow::runLibvirtCommand(const QStringList &args, QString *out, QString *err, int timeoutMs)
{
    QString prog = findLibvirtManager();
    if (prog.isEmpty()) {
        if (err) *err = "libvirt_rdp_manager not found";
        return false;
    }

    QProcess p;
    p.setProgram(prog);
    p.setArguments(args);
    p.start();
    if (!p.waitForFinished(timeoutMs)) {
        if (err) *err = "timeout";
        p.kill();
        return false;
    }
    if (out) *out = QString::fromLocal8Bit(p.readAllStandardOutput());
    if (err) *err = QString::fromLocal8Bit(p.readAllStandardError());
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void MainWindow::onVmStart()
{
    QString vm = vmCombo ? vmCombo->currentText() : QString();
    if (vm.isEmpty() || vm == "---------") return;
    QString out, err;
    if (!runLibvirtCommand({"start", vm}, &out, &err)) {
        qWarning() << "Start failed:" << err;
    }
    refreshVMList();
    updateVmControls();
    // Refresh guest server endpoint after VM starts (with a small delay for network to initialize)
    QTimer::singleShot(3000, this, &MainWindow::refreshGuestServerEndpoint);
}

void MainWindow::onVmStop()
{
    QString vm = vmCombo ? vmCombo->currentText() : QString();
    if (vm.isEmpty() || vm == "---------") return;
    QString out, err;
    if (!runLibvirtCommand({"stop", vm}, &out, &err)) {
        qWarning() << "Stop failed:" << err;
    }
    refreshVMList();
    updateVmControls();
    // Clear guest server endpoint when VM stops
    refreshGuestServerEndpoint();
}

void MainWindow::onVmRestart()
{
    QString vm = vmCombo ? vmCombo->currentText() : QString();
    if (vm.isEmpty() || vm == "---------") return;
    QString out, err;
    if (!runLibvirtCommand({"restart", vm}, &out, &err)) {
        qWarning() << "Restart failed:" << err;
    }
    refreshVMList();
    updateVmControls();
    // Refresh guest server endpoint after VM restarts (with a delay for network to initialize)
    QTimer::singleShot(5000, this, &MainWindow::refreshGuestServerEndpoint);
}

void MainWindow::onVmConnect()
{
    QString vm = vmCombo ? vmCombo->currentText() : QString();
    if (vm.isEmpty() || vm == "---------") return;
    // Prompt for credentials and port, show detected IP
    ConnectDialog dlg(vm, this);
    // Position near the button
    QPoint btnPos = vmConnectBtn->mapToGlobal(QPoint(0, vmConnectBtn->height()+8));
    dlg.move(btnPos);
    if (dlg.exec() != QDialog::Accepted) return;

    QString prog = findLibvirtManager();
    if (prog.isEmpty()) {
        qWarning() << "libvirt manager not found";
        return;
    }
    QStringList args = {"connect", vm};
    if (!dlg.username().isEmpty()) { args << "--username" << dlg.username(); }
    if (!dlg.password().isEmpty()) { args << "--password" << dlg.password(); }
    args << "--port" << QString::number(dlg.port());
    QProcess::startDetached(prog, args);
}

void MainWindow::onVmSelectionChanged(int)
{
    updateVmControls();
    refreshGuestServerEndpoint();
}

void MainWindow::setupAboutPage()
{
    aboutPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(aboutPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 40, 0, 0); // Add 40px top margin
    
    // Add logo
    QLabel *logoLabel = new QLabel();
    QPixmap logoPixmap(":/icons/icon/logo.png");
    logoLabel->setPixmap(logoPixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    
    // Add app name and version
    QLabel *appName = new QLabel("WINRUN");
    appName->setAlignment(Qt::AlignCenter);
    appName->setStyleSheet("font-size: 32px; font-weight: bold; color: #1a535c; margin: 10px 0;");
    
    QLabel *version = new QLabel("Version 0.7.28");
    version->setAlignment(Qt::AlignCenter);
    version->setStyleSheet("font-size: 14px; color: #666; margin: 0 auto 20px;");
    
    // Add description
    QLabel *description = new QLabel("The best solution to run Windows software on Linux-based operating systems.\n\n"
                                    "© 2024 - 2025 WINRUN / ShiroNEX. All rights reserved.");
    description->setAlignment(Qt::AlignCenter);
    description->setStyleSheet("color: #555;");
    
    layout->addWidget(logoLabel);
    layout->addWidget(appName);
    layout->addWidget(version);
    layout->addWidget(description);
    layout->addStretch();
}

void MainWindow::onAppsReceived(const QList<InstalledApp> &apps)
{
    // When apps are received on All Programs page, stop scanning
    // but keep the system monitor running on Desktop page
    if (stackedWidget && stackedWidget->currentWidget() == allProgramsPage) {
        // Stop the guest server refresh timer when apps are successfully received
        if (m_guestServerRefreshTimer && m_guestServerRefreshTimer->isActive()) {
            m_guestServerRefreshTimer->stop();
            qDebug() << "Apps received on All Programs page. Stopped scanning. Total apps:" << apps.size();
        }
    }
}
