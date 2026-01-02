#ifndef GUESTSERVERWIDGET_H
#define GUESTSERVERWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include "guestserverclient.h"

class GuestServerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit GuestServerWidget(const QString &host, quint16 port, const QString &authKey, QWidget *parent = nullptr);
    ~GuestServerWidget();
    
    void startMonitoring(int intervalMs = 5000);
    void stopMonitoring();
    void configureServer(const QString &host, quint16 port, const QString &authKey = QString());
    bool isMonitoring() const;
    bool isEndpointConfigured() const { return m_endpointConfigured; }
    
private slots:
    void updateMetrics(const GuestServerMetrics &metrics);
    void onConnectionError(const QString &error);
    
private:
    void setupUI();
    void updateMetricsDisplay();
    
    GuestServerClient *m_client;
    
    // UI Elements
    QLabel *m_statusLabel;
    QLabel *m_lastUpdatedLabel;
    
    // CPU
    QLabel *m_cpuLabel;
    QProgressBar *m_cpuUsage;
    QLabel *m_cpuFreqLabel;
    
    // RAM
    QLabel *m_ramLabel;
    QProgressBar *m_ramUsage;
    QLabel *m_ramUsageLabel;
    
    // Disk
    QLabel *m_diskLabel;
    QProgressBar *m_diskUsage;
    QLabel *m_diskUsageLabel;
    
    // Current metrics
    GuestServerMetrics m_currentMetrics;
    bool m_endpointConfigured;
    bool m_shouldAutoStart;
    int m_monitorIntervalMs;
};

#endif // GUESTSERVERWIDGET_H
