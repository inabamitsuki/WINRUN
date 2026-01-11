#ifndef APPSCACHE_H
#define APPSCACHE_H

#include <QString>
#include <QList>
#include <QObject>

// Forward declaration - we'll include the full header in cpp
struct InstalledApp;

class AppsCache : public QObject
{
    Q_OBJECT

public:
    explicit AppsCache(QObject *parent = nullptr);
    
    // Save apps to cache file
    bool saveApps(const QList<InstalledApp> &apps);
    
    // Load apps from cache file
    bool loadApps(QList<InstalledApp> &apps);
    
    // Clear cache
    bool clearCache();
    
    // Get cache file path
    static QString getCacheFilePath();
    
    // Check if cache exists
    bool cacheExists() const;

private:
    QString m_cacheFilePath;
};

#endif // APPSCACHE_H
