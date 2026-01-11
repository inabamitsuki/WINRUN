#include "guestserverappsclient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDebug>

GuestServerAppsClient::GuestServerAppsClient(const QString &host, quint16 port, QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_baseUrl(host.isEmpty() || port == 0 ? QString() : QString("http://%1:%2").arg(host).arg(port))
    , m_cache(new AppsCache(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, [this](QNetworkReply *reply) {
                if (reply->request().url().path().contains("/apps")) {
                    onAppsReply(reply);
                } else if (reply->request().url().path().contains("/get-icon")) {
                    onIconReply(reply);
                }
            });
}

GuestServerAppsClient::~GuestServerAppsClient()
{
    // Save apps to cache when client is destroyed
    saveAppsToCache();
}

void GuestServerAppsClient::setServerEndpoint(const QString &host, quint16 port)
{
    if (host.isEmpty() || port == 0) {
        m_baseUrl.clear();
    } else {
        m_baseUrl = QString("http://%1:%2").arg(host).arg(port);
    }
}

void GuestServerAppsClient::fetchApps()
{
    if (m_baseUrl.isEmpty()) {
        emit error("Server endpoint not configured");
        return;
    }
    
    QUrl url(m_baseUrl + "/apps");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // The reply will be handled by the QNetworkAccessManager::finished signal
    // connected in the constructor, so we don't need to connect here
    m_networkManager->get(request);
}

void GuestServerAppsClient::onAppsReply(QNetworkReply *reply)
{
    // Check if reply is still valid (might have been deleted by duplicate call)
    if (!reply) {
        return;
    }
    
    // Mark for deletion first to prevent duplicate handling
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        return;
    }
    
    QByteArray responseData = reply->readAll();
    
    // Skip empty responses (might be from duplicate handling)
    if (responseData.isEmpty()) {
        qDebug() << "Skipping empty response (likely duplicate)";
        return;
    }
    
    // Debug: log the raw response
    qDebug() << "Apps response:" << responseData.left(500);
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "Invalid JSON response. Raw data:" << responseData;
        emit error(QString("Invalid JSON response from server: %1").arg(QString::fromUtf8(responseData.left(200))));
        return;
    }
    
    QJsonObject json = jsonDoc.object();
    
    // Check if response contains an error
    if (json.contains("error")) {
        QString errorMsg = json["error"].toString();
        qWarning() << "Server returned error:" << errorMsg;
        emit error(QString("Server error: %1").arg(errorMsg));
        return;
    }
    
    QJsonArray appsArray = json["apps"].toArray();
    
    m_apps.clear();
    
    for (const QJsonValue &value : appsArray) {
        QJsonObject appObj = value.toObject();
        
        InstalledApp app;
        app.name = appObj["name"].toString();
        app.publisher = appObj["publisher"].toString();
        app.installLocation = appObj["install_location"].toString();
        app.displayVersion = appObj["display_version"].toString();
        // Handle null icon_path - toString() returns empty string for null, which is fine
        app.iconPath = appObj["icon_path"].isNull() ? QString() : appObj["icon_path"].toString();
        app.uninstallString = appObj["uninstall_string"].isNull() ? QString() : appObj["uninstall_string"].toString();
        
        if (!app.name.isEmpty()) {
            m_apps.append(app);
            
            // Fetch icon if path is available and not an uninstaller
            if (!app.iconPath.isEmpty()) {
                QString iconPathLower = app.iconPath.toLower();
                // Skip uninstallers and maintenance tools
                if (!iconPathLower.contains("uninstall") && 
                    !iconPathLower.contains("unins000") &&
                    !iconPathLower.contains("maintenance service")) {
                    fetchIcon(app.iconPath);
                }
            }
        }
    }
    
    emit appsReceived(m_apps);
}

void GuestServerAppsClient::fetchIcon(const QString &iconPath)
{
    if (m_baseUrl.isEmpty() || iconPath.isEmpty()) {
        return;
    }
    
    // Skip if already fetching this icon
    if (m_pendingIconRequests.contains(iconPath)) {
        return;
    }
    
    QUrl url(m_baseUrl + "/get-icon");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    // Format as form data: path=<iconPath>
    QString encodedPath = QUrl::toPercentEncoding(iconPath);
    QByteArray postData = ("path=" + encodedPath).toUtf8();
    
    QNetworkReply *reply = m_networkManager->post(request, postData);
    m_pendingIconRequests[iconPath] = reply;
}

void GuestServerAppsClient::onIconReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    // Find which icon path this reply corresponds to
    QString iconPath;
    for (auto it = m_pendingIconRequests.begin(); it != m_pendingIconRequests.end(); ++it) {
        if (it.value() == reply) {
            iconPath = it.key();
            m_pendingIconRequests.erase(it);
            break;
        }
    }
    
    if (iconPath.isEmpty()) {
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Failed to fetch icon:" << iconPath << reply->errorString();
        // Emit empty icon data to signal failure - UI will show first letter
        emit iconReceived(iconPath, QByteArray());
        return;
    }
    
    QByteArray base64Data = reply->readAll().trimmed();
    if (!base64Data.isEmpty()) {
        QByteArray iconData = QByteArray::fromBase64(base64Data);
        emit iconReceived(iconPath, iconData);
        
        // Update the app in our list
        for (auto &app : m_apps) {
            if (app.iconPath == iconPath) {
                app.iconData = iconData;
                break;
            }
        }
    } else {
        // Empty response - emit empty icon data
        emit iconReceived(iconPath, QByteArray());
    }
}

void GuestServerAppsClient::saveAppsToCache()
{
    if (!m_apps.isEmpty() && m_cache) {
        m_cache->saveApps(m_apps);
    }
}

void GuestServerAppsClient::loadAppsFromCache()
{
    if (m_cache && m_cache->cacheExists()) {
        QList<InstalledApp> cachedApps;
        if (m_cache->loadApps(cachedApps)) {
            m_apps = cachedApps;
            emit appsReceived(m_apps);
        }
    }
}

