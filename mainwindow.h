#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPoint>

#include <QMainWindow>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QComboBox>
#include <QMap>
#include <QProcess>
#include <QTabWidget>
#include <QTimer>
#include <QScrollArea>
#include "guestserverwidget.h"
#include "guestserverappsclient.h"
#include "appslistwidget.h"

// Forward declaration
class AddProgramDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUI();
    void setupSidebar();
    void setupMainContent();
    void refreshVMList();
    void updateVmControls();
    QString findLibvirtManager() const;
    bool runLibvirtCommand(const QStringList &args, QString *out = nullptr, QString *err = nullptr, int timeoutMs = 15000);
    void refreshGuestServerEndpoint();
    void refreshAppsList();
    
    // Main widgets
    QWidget *centralWidget;
    QHBoxLayout *mainLayout;
    QVBoxLayout *sidebarLayout;
    QVBoxLayout *contentLayout;
    
    // Sidebar widgets
    QFrame *sidebar;
    QPushButton *allAppsBtn;
    QPushButton *desktopBtn;
    QPushButton *fileBtn;
    QPushButton *settingsBtn;
    QPushButton *aboutBtn;
    
    // Header
    QFrame *header;
    QHBoxLayout *headerLayout;
    QLabel *titleLabel;
    QPushButton *collapseBtn;
    
    // Content
    QScrollArea *contentScrollArea;
    QStackedWidget *stackedWidget;
    QWidget *allProgramsPage;
    QWidget *desktopPage;
    QWidget *filePage;
    QWidget *settingsPage;
    QWidget *aboutPage;
    QFrame *vmPreviewFrame;
    QComboBox *vmCombo;
    QLabel *vmStatusLabel;
    QPushButton *vmStartBtn;
    QPushButton *vmStopBtn;
    QPushButton *vmRestartBtn;
    QPushButton *vmConnectBtn;
    QPushButton *guestServerBtn;
    QMap<QString, QString> vmStateByName;
    QProcess *rdpProcess;
    
    // Guest Server
    GuestServerWidget *m_guestServerWidget;
    GuestServerAppsClient *m_guestServerAppsClient;
    AppsListWidget *m_appsListWidget;
    QTabWidget *m_tabWidget;
    QString m_currentGuestServerIp;
    QTimer *m_guestServerRefreshTimer;
    QTimer *m_vmListRefreshTimer;
    
    // All Programs Page
    QVBoxLayout *allProgramsLayout;
    QLabel *logoLabel;
    QLabel *winrunLabel;
    QPushButton *addProgramsBtn;
    QLabel *addProgramsHint;
    
    // Styles
    QString sidebarStyle;
    QString headerStyle;
    QString navButtonStyle;
    QString addButtonStyle;
    
private:
    void setupDesktopPage();
    void setupFilePage();
    void setupSettingsPage();
    void setupAboutPage();
    
private slots:
    void onAllAppsClicked();
    void onDesktopClicked();
    void onFileClicked();
    void onSettingsClicked();
    void onAboutClicked();
    void onAddProgramsClicked();
    void onVmStart();
    void onVmStop();
    void onVmRestart();
    void onVmConnect();
    void onConnectToGuestServer();
    void onVmSelectionChanged(int index);
    
private:
    AddProgramDialog *addProgramDialog;
    void onCollapseClicked(bool checked = false);
};

#endif // MAINWINDOW_H
