#include "appslistwidget.h"
#include <QPixmap>
#include <QIcon>
#include <QSize>
#include <QFrame>
#include <QDebug>

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
    if (iconData.isEmpty()) {
        return;
    }
    
    m_iconCache[iconPath] = iconData;
    
    // Update the specific icon if we have a button for it
    if (m_iconPathToButton.contains(iconPath)) {
        QPushButton *button = m_iconPathToButton[iconPath];
        if (m_buttonToIconLabel.contains(button)) {
            QLabel *iconLabel = m_buttonToIconLabel[button];
            
            // Load and display the icon
            QPixmap pixmap;
            if (pixmap.loadFromData(iconData)) {
                iconLabel->setPixmap(pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                iconLabel->setStyleSheet("background-color: transparent;");
            }
        }
    } else {
        // If button not found, refresh entire display
        updateAppsDisplay();
    }
}

void AppsListWidget::clear()
{
    m_apps.clear();
    m_iconCache.clear();
    m_buttonToAppPath.clear();
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
    m_buttonToAppPath.clear();
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
        
        // Store mapping for click handler
        m_buttonToAppPath[appButton] = app.installLocation;
        
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
    if (button && m_buttonToAppPath.contains(button)) {
        QString appPath = m_buttonToAppPath[button];
        qDebug() << "App clicked:" << appPath;
        // TODO: Launch the application or show details
    }
}

