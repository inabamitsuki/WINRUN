#include "guestserverclient.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>

GuestServerClient::GuestServerClient(const QString &host, quint16 port, const QString &authKey, QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
    , m_baseUrl(host.isEmpty() || port == 0 ? QString() : QString("http://%1:%2").arg(host).arg(port))
    , m_authKey(authKey)
    , m_isMonitoring(false)
    , m_intervalMs(5000)
{
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &GuestServerClient::onMetricsReply);
    
    connect(m_timer, &QTimer::timeout, this, &GuestServerClient::fetchMetrics);
    
    // Initialize with default values
    m_currentMetrics = GuestServerMetrics{};
    m_currentMetrics.lastUpdated = QDateTime::currentDateTime();
}

GuestServerClient::~GuestServerClient()
{
    stopMonitoring();
}

void GuestServerClient::startMonitoring(int intervalMs)
{
    if (intervalMs > 0) {
        m_intervalMs = intervalMs;
    }

    m_isMonitoring = true;
    m_timer->start(m_intervalMs);
    fetchMetrics(); // Initial fetch or refresh
}

void GuestServerClient::stopMonitoring()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    m_isMonitoring = false;
}

void GuestServerClient::setServerEndpoint(const QString &host, quint16 port, const QString &authKey)
{
    if (host.isEmpty() || port == 0) {
        m_baseUrl.clear();
    } else {
        m_baseUrl = QString("http://%1:%2").arg(host).arg(port);
    }
    m_authKey = authKey;
    if (m_isMonitoring) {
        fetchMetrics();
    }
}

GuestServerMetrics GuestServerClient::currentMetrics() const
{
    return m_currentMetrics;
}

void GuestServerClient::fetchMetrics()
{
    if (m_baseUrl.isEmpty()) {
        return;
    }
    
    QUrl url(m_baseUrl + "/metrics");
    QNetworkRequest request(url);
    
    // Add authentication header if key is provided
    if (!m_authKey.isEmpty()) {
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_authKey).toUtf8());
    }
    
    // Set headers for JSON response
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // Make the request
    m_networkManager->get(request);
}

void GuestServerClient::onMetricsReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit connectionError(reply->errorString());
        return;
    }
    
    // Read the response
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        emit connectionError("Invalid JSON response from server");
        return;
    }
    
    QJsonObject json = jsonDoc.object();
    
    // Parse CPU metrics
    QJsonObject cpuObj = json["cpu"].toObject();
    m_currentMetrics.cpu.usage = cpuObj["usage"].toDouble();
    m_currentMetrics.cpu.frequency = static_cast<quint64>(cpuObj["frequency"].toDouble());
    
    // Parse RAM metrics
    QJsonObject ramObj = json["ram"].toObject();
    m_currentMetrics.ram.used = static_cast<quint64>(ramObj["used"].toDouble());
    m_currentMetrics.ram.total = static_cast<quint64>(ramObj["total"].toDouble());
    m_currentMetrics.ram.percentage = ramObj["percentage"].toDouble();
    
    // Parse Disk metrics
    QJsonObject diskObj = json["disk"].toObject();
    m_currentMetrics.disk.used = static_cast<quint64>(diskObj["used"].toDouble());
    m_currentMetrics.disk.total = static_cast<quint64>(diskObj["total"].toDouble());
    m_currentMetrics.disk.percentage = diskObj["percentage"].toDouble();
    
    m_currentMetrics.lastUpdated = QDateTime::currentDateTime();
    
    emit metricsUpdated(m_currentMetrics);
}
