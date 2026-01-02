#ifndef GUESTSERVERCLIENT_H
#define GUESTSERVERCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

struct GuestServerMetrics {
    struct {
        double usage;      // CPU usage percentage
        quint64 frequency; // CPU frequency in MHz
    } cpu;
    
    struct {
        quint64 used;      // Used RAM in MB
        quint64 total;     // Total RAM in MB
        double percentage; // RAM usage percentage
    } ram;
    
    struct {
        quint64 used;      // Used disk space in MB
        quint64 total;     // Total disk space in MB
        double percentage; // Disk usage percentage
    } disk;
    
    QDateTime lastUpdated; // When the metrics were last updated
};

class GuestServerClient : public QObject
{
    Q_OBJECT
    
public:
    explicit GuestServerClient(const QString &host, quint16 port, const QString &authKey, QObject *parent = nullptr);
    ~GuestServerClient();
    
    void startMonitoring(int intervalMs = 5000);
    void stopMonitoring();
    void setServerEndpoint(const QString &host, quint16 port, const QString &authKey = QString());
    bool isMonitoring() const { return m_isMonitoring; }
    int intervalMs() const { return m_intervalMs; }
    
    GuestServerMetrics currentMetrics() const;
    
signals:
    void metricsUpdated(const GuestServerMetrics &metrics);
    void connectionError(const QString &error);
    
private slots:
    void fetchMetrics();
    void onMetricsReply(QNetworkReply *reply);
    
private:
    QNetworkAccessManager *m_networkManager;
    QTimer *m_timer;
    QString m_baseUrl;
    QString m_authKey;
    GuestServerMetrics m_currentMetrics;
    bool m_isMonitoring;
    int m_intervalMs;
};

#endif // GUESTSERVERCLIENT_H
