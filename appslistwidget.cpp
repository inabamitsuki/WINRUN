#include "appslistwidget.h"
#include <QPixmap>
#include <QIcon>
#include <QSize>
#include <QFrame>
#include <QDebug>
#include <QProcess>

AppsListWidget::AppsListWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

AppsListWidget::~AppsListWidget()
{
}

void AppsListWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(
        "QScrollArea { "
        "    border: none; "
        "    background-color: transparent; "
        "}"
        "QScrollBar:vertical { "
        "    background: #f0f0f0; "
        "    width: 12px; "
        "    border-radius: 6px; "
        "}"
        "QScrollBar::handle:vertical { "
        "    background: #c0c0c0; "
        "    min-height: 30px; "
        "    border-radius: 6px; "
        "}"
        "QScrollBar::handle:vertical:hover { "
        "    background: #a0a0a0; "
        "}"
    );
    
    m_scrollContent = new QWidget();
    m_gridLayout = new QGridLayout(m_scrollContent);
    m_gridLayout->setSpacing(15);
    m_gridLayout->setContentsMargins(10, 10, 10, 10);
    
    m_scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(m_scrollArea);
}

void AppsListWidget::setApps(const QList<InstalledApp> &apps)
{
    m_apps = apps;
    updateAppsDisplay();
}

void AppsListWidget::setIcon(const QString &iconPath, const QByteArray &iconData)
{
    // Update the specific icon if we have a button for it
    if (m_iconPathToButton.contains(iconPath)) {
        QPushButton *button = m_iconPathToButton[iconPath];
        if (m_buttonToIconLabel.contains(button)) {
            QLabel *iconLabel = m_buttonToIconLabel[button];
            
            // If icon data is empty, it means icon fetch failed - keep showing first letter
            if (iconData.isEmpty()) {
                qDebug() << "Icon fetch failed for:" << iconPath << "- keeping first letter display";
                return;
            }
            
            // Cache the icon data
            m_iconCache[iconPath] = iconData;
            
            // Load and display the icon
            QPixmap pixmap;
            if (pixmap.loadFromData(iconData)) {
                iconLabel->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                iconLabel->setStyleSheet("background-color: transparent;");
                qDebug() << "Icon loaded successfully for:" << iconPath;
            } else {
                // Failed to load pixmap from data - keep showing first letter
                qDebug() << "Failed to load pixmap from icon data for:" << iconPath;
            }
        }
    }
}

void AppsListWidget::clear()
{
    m_apps.clear();
    m_iconCache.clear();
    m_buttonToApp.clear();
    m_iconPathToButton.clear();
    m_buttonToIconLabel.clear();
    
    // Clear all widgets from grid
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void AppsListWidget::updateAppsDisplay()
{
    // Clear existing widgets
    QLayoutItem *item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_buttonToApp.clear();
    m_iconPathToButton.clear();
    m_buttonToIconLabel.clear();
    
    const int columns = 4;
    int row = 0;
    int col = 0;
    
    for (const InstalledApp &app : m_apps) {
        // Create app button
        QPushButton *appButton = new QPushButton(m_scrollContent);
        appButton->setFixedSize(120, 140);
        appButton->setStyleSheet(
            "QPushButton { "
            "    background-color: white; "
            "    border: 1px solid #e0e0e0; "
            "    border-radius: 8px; "
            "    padding: 10px; "
            "    text-align: center; "
            "}"
            "QPushButton:hover { "
            "    background-color: #f5f5f5; "
            "    border: 1px solid #1a535c; "
            "}"
        );
        
        // Create vertical layout for button content
        QVBoxLayout *buttonLayout = new QVBoxLayout(appButton);
        buttonLayout->setContentsMargins(5, 5, 5, 5);
        buttonLayout->setSpacing(8);
        
        // Icon
        QLabel *iconLabel = new QLabel(appButton);
        iconLabel->setFixedSize(64, 64);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("background-color: transparent;");
        
        // Store mapping for icon updates
        m_buttonToIconLabel[appButton] = iconLabel;
        if (!app.iconPath.isEmpty()) {
            m_iconPathToButton[app.iconPath] = appButton;
        }
        
        // Try to load icon from cache
        if (!app.iconPath.isEmpty() && m_iconCache.contains(app.iconPath)) {
            QPixmap pixmap;
            if (pixmap.loadFromData(m_iconCache[app.iconPath])) {
                iconLabel->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                // Fallback: show first letter
                iconLabel->setText(app.name.isEmpty() ? QString("?") : QString(app.name.at(0).toUpper()));
                iconLabel->setStyleSheet(
                    "background-color: #1a535c; "
                    "color: white; "
                    "border-radius: 8px; "
                    "font-size: 24px; "
                    "font-weight: bold;"
                );
            }
        } else {
            // Fallback: show first letter (icon will be updated when received)
            iconLabel->setText(app.name.isEmpty() ? QString("?") : QString(app.name.at(0).toUpper()));
            iconLabel->setStyleSheet(
                "background-color: #1a535c; "
                "color: white; "
                "border-radius: 8px; "
                "font-size: 24px; "
                "font-weight: bold;"
            );
        }
        
        // App name
        QLabel *nameLabel = new QLabel(app.name, appButton);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setWordWrap(true);
        nameLabel->setStyleSheet(
            "color: #1a535c; "
            "font-size: 12px; "
            "font-weight: 500; "
            "background-color: transparent;"
        );
        nameLabel->setMaximumHeight(40);
        
        buttonLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
        buttonLayout->addWidget(nameLabel);
        buttonLayout->addStretch();
        
        // Store mapping for click handler - store full app info
        m_buttonToApp[appButton] = app;
        
        connect(appButton, &QPushButton::clicked, this, &AppsListWidget::onAppClicked);
        
        // Add to grid
        m_gridLayout->addWidget(appButton, row, col);
        
        col++;
        if (col >= columns) {
            col = 0;
            row++;
        }
    }
    
    // Add stretch at the end
    m_gridLayout->setRowStretch(row + 1, 1);
}

void AppsListWidget::onAppClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button || !m_buttonToApp.contains(button)) {
        return;
    }
    
    const InstalledApp &app = m_buttonToApp[button];
    qDebug() << "App clicked:" << app.name;
    
    QString executablePath;
    bool isUWP = false;
    
    // Check if this is a UWP app (launch args start with "shell:AppsFolder")
    if (app.installLocation.startsWith("shell:AppsFolder")) {
        isUWP = true;
        executablePath = app.installLocation;
    } else {
        // For regular apps, use icon_path if it's an .exe, otherwise try installLocation
        if (!app.iconPath.isEmpty() && app.iconPath.endsWith(".exe", Qt::CaseInsensitive)) {
            executablePath = app.iconPath;
        } else if (!app.installLocation.isEmpty()) {
            // Try to construct path from install location
            // If installLocation is a directory, we need to find the .exe
            // For now, if iconPath exists and is valid, use it
            executablePath = app.iconPath.isEmpty() ? app.installLocation : app.iconPath;
        } else {
            qWarning() << "Could not determine executable path for:" << app.name;
            return;
        }
    }
    
    // Launch the application via xfreerdp3
    // Get server info from parent widget (MainWindow)
    QWidget *parent = this;
    while (parent && parent->parent()) {
        parent = parent->parentWidget();
    }
    
    // Try to get the server IP from MainWindow
    // For now, we'll emit a signal that MainWindow can handle
    // Or we can launch xfreerdp3 directly if we have the server info
    
    launchAppWithXfreerdp(app.name, executablePath);
}

void AppsListWidget::launchAppWithXfreerdp(const QString &appName, const QString &appPath)
{
    // Get server endpoint from environment or use defaults
    QString serverIp = qgetenv("WINRUN_SERVER_IP");
    QString serverPort = qgetenv("WINRUN_SERVER_PORT");
    QString username = qgetenv("WINRUN_USERNAME");
    QString password = qgetenv("WINRUN_PASSWORD");
    
    // Set default values
    if (serverIp.isEmpty()) {
        serverIp = "192.168.122.201";
    }
    
    if (serverPort.isEmpty()) {
        serverPort = "3389";
    }
    
    if (username.isEmpty()) {
        username = "mitsuki";
    }
    
    if (password.isEmpty()) {
        password = "3314";
    }
    
    // Build xfreerdp3 command
    // Format: xfreerdp3 /v:192.168.122.201 /u:mitsuki /p:3314 /cert:ignore /auth-pkg-list:!kerberos /app:program:"C:\\Users\\mitsuki\\AppData\\Local\\LINE\\bin\\LineLauncher.exe"
    
    QStringList args;
    args << QString("/v:%1:%2").arg(serverIp, serverPort);
    args << QString("/u:%1").arg(username);
    args << QString("/p:%1").arg(password);
    args << "/cert:ignore";
    args << "/auth-pkg-list:!kerberos";
    args << QString("/app:program:\"%1\"").arg(appPath);
    
    qDebug() << "Launching app with xfreerdp3:" << appName;
    qDebug() << "Command: xfreerdp3" << args.join(" ");
    
    bool launched = QProcess::startDetached("xfreerdp3", args);
    
    if (!launched) {
        qWarning() << "Failed to launch app with xfreerdp3:" << appName;
        qWarning() << "Make sure xfreerdp3 is installed and in PATH";
    } else {
        qDebug() << "Successfully launched:" << appName << "via xfreerdp3";
    }
}

