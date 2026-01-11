#ifndef APPSLISTWIDGET_H
#define APPSLISTWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include "guestserverappsclient.h"

class AppsListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppsListWidget(QWidget *parent = nullptr);
    ~AppsListWidget();
    
    void setApps(const QList<InstalledApp> &apps);
    void setIcon(const QString &iconPath, const QByteArray &iconData);
    void clear();

private slots:
    void onAppClicked();

private:
    void setupUI();
    void updateAppsDisplay();
    void launchAppWithXfreerdp(const QString &appName, const QString &appPath);
    
    QScrollArea *m_scrollArea;
    QWidget *m_scrollContent;
    QGridLayout *m_gridLayout;
    QList<InstalledApp> m_apps;
    QMap<QString, QByteArray> m_iconCache;
    QMap<QPushButton*, InstalledApp> m_buttonToApp;  // Store full app info for launching
    QMap<QString, QPushButton*> m_iconPathToButton;
    QMap<QPushButton*, QLabel*> m_buttonToIconLabel;
};

#endif // APPSLISTWIDGET_H

