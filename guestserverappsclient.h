#ifndef GUESTSERVERAPPSCLIENT_H
#define GUESTSERVERAPPSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>

struct InstalledApp {
    QString name;
    QString publisher;
    QString installLocation;
    QString displayVersion;
    QString iconPath;
    QString uninstallString;
    QByteArray iconData; // Base64 decoded icon
};

class GuestServerAppsClient : public QObject
{
    Q_OBJECT

public:
    explicit GuestServerAppsClient(const QString &host, quint16 port, QObject *parent = nullptr);
    ~GuestServerAppsClient();
    
    void fetchApps();
    void fetchIcon(const QString &iconPath);
    void setServerEndpoint(const QString &host, quint16 port);
    
    QList<InstalledApp> apps() const { return m_apps; }

signals:
    void appsReceived(const QList<InstalledApp> &apps);
    void iconReceived(const QString &iconPath, const QByteArray &iconData);
    void error(const QString &error);

private slots:
    void onAppsReply(QNetworkReply *reply);
    void onIconReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_networkManager;
    QString m_baseUrl;
    QList<InstalledApp> m_apps;
    QMap<QString, QNetworkReply*> m_pendingIconRequests;
};

#endif // GUESTSERVERAPPSCLIENT_H

