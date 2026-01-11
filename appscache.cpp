#include "appscache.h"
#include "guestserverappsclient.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

AppsCache::AppsCache(QObject *parent)
    : QObject(parent)
{
    m_cacheFilePath = getCacheFilePath();
}

QString AppsCache::getCacheFilePath()
{
    // Use application cache directory
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    
    // Create directory if it doesn't exist
    QDir dir(cacheDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    return cacheDir + "/apps_cache.json";
}

bool AppsCache::saveApps(const QList<InstalledApp> &apps)
{
    QJsonArray appsArray;
    
    for (const InstalledApp &app : apps) {
        QJsonObject appObj;
        appObj["name"] = app.name;
        appObj["publisher"] = app.publisher;
        appObj["install_location"] = app.installLocation;
        appObj["display_version"] = app.displayVersion;
        appObj["icon_path"] = app.iconPath;
        appObj["uninstall_string"] = app.uninstallString;
        
        // Store icon data as base64 if available
        if (!app.iconData.isEmpty()) {
            appObj["icon_data"] = QString::fromLatin1(app.iconData.toBase64());
        }
        
        appsArray.append(appObj);
    }
    
    QJsonObject rootObj;
    rootObj["apps"] = appsArray;
    
    QJsonDocument jsonDoc(rootObj);
    
    QFile file(m_cacheFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open cache file for writing:" << m_cacheFilePath;
        return false;
    }
    
    file.write(jsonDoc.toJson());
    file.close();
    
    qDebug() << "Apps cache saved to:" << m_cacheFilePath << "(" << apps.size() << "apps)";
    return true;
}

bool AppsCache::loadApps(QList<InstalledApp> &apps)
{
    QFile file(m_cacheFilePath);
    if (!file.exists()) {
        qDebug() << "Cache file does not exist:" << m_cacheFilePath;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open cache file for reading:" << m_cacheFilePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);
    if (!jsonDoc.isObject()) {
        qWarning() << "Invalid cache file format";
        return false;
    }
    
    QJsonObject rootObj = jsonDoc.object();
    QJsonArray appsArray = rootObj["apps"].toArray();
    
    apps.clear();
    
    for (const QJsonValue &value : appsArray) {
        QJsonObject appObj = value.toObject();
        
        InstalledApp app;
        app.name = appObj["name"].toString();
        app.publisher = appObj["publisher"].toString();
        app.installLocation = appObj["install_location"].toString();
        app.displayVersion = appObj["display_version"].toString();
        app.iconPath = appObj["icon_path"].toString();
        app.uninstallString = appObj["uninstall_string"].toString();
        
        // Load icon data if available
        if (appObj.contains("icon_data")) {
            QString base64Data = appObj["icon_data"].toString();
            app.iconData = QByteArray::fromBase64(base64Data.toLatin1());
        }
        
        if (!app.name.isEmpty()) {
            apps.append(app);
        }
    }
    
    qDebug() << "Apps cache loaded from:" << m_cacheFilePath << "(" << apps.size() << "apps)";
    return true;
}

bool AppsCache::clearCache()
{
    QFile file(m_cacheFilePath);
    if (file.exists()) {
        if (!file.remove()) {
            qWarning() << "Failed to delete cache file:" << m_cacheFilePath;
            return false;
        }
    }
    
    qDebug() << "Cache cleared";
    return true;
}

bool AppsCache::cacheExists() const
{
    return QFile::exists(m_cacheFilePath);
}
