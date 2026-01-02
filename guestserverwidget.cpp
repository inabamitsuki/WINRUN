#include "guestserverwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDateTime>
#include <QStyle>

GuestServerWidget::GuestServerWidget(const QString &host, quint16 port, const QString &authKey, QWidget *parent)
    : QWidget(parent)
    , m_client(new GuestServerClient(host, port, authKey, this))
    , m_endpointConfigured(!host.isEmpty() && port != 0)
    , m_shouldAutoStart(false)
    , m_monitorIntervalMs(5000)
{
    setupUI();
    
    connect(m_client, &GuestServerClient::metricsUpdated,
            this, &GuestServerWidget::updateMetrics);
    connect(m_client, &GuestServerClient::connectionError,
            this, &GuestServerWidget::onConnectionError);
    
    // Initialize with default values
    m_currentMetrics = GuestServerMetrics{};
    m_currentMetrics.lastUpdated = QDateTime::currentDateTime();
    updateMetricsDisplay();
}

GuestServerWidget::~GuestServerWidget()
{
    stopMonitoring();
}

void GuestServerWidget::startMonitoring(int intervalMs)
{
    if (intervalMs > 0) {
        m_monitorIntervalMs = intervalMs;
    }

    m_shouldAutoStart = true;

    if (!m_endpointConfigured) {
        m_statusLabel->setText(tr("Status: Waiting for VM IP"));
        m_statusLabel->setStyleSheet("color: #f39c12;");
        return;
    }

    if (!m_client->isMonitoring()) {
        m_client->startMonitoring(m_monitorIntervalMs);
    }
    m_statusLabel->setText(tr("Status: Monitoring..."));
    m_statusLabel->setStyleSheet("color: #27ae60;"); // Green
}

void GuestServerWidget::stopMonitoring()
{
    m_client->stopMonitoring();
    m_shouldAutoStart = false;
    m_statusLabel->setText(tr("Status: Stopped"));
    m_statusLabel->setStyleSheet("color: #7f8c8d;"); // Gray
}

void GuestServerWidget::configureServer(const QString &host, quint16 port, const QString &authKey)
{
    const bool hasEndpoint = !host.isEmpty() && port != 0;
    m_endpointConfigured = hasEndpoint;
    m_client->setServerEndpoint(host, port, authKey);

    if (!hasEndpoint) {
        m_client->stopMonitoring();
        m_statusLabel->setText(tr("Status: Waiting for VM IP"));
        m_statusLabel->setStyleSheet("color: #f39c12;");
    } else {
        if (m_shouldAutoStart) {
            m_client->startMonitoring(m_monitorIntervalMs);
            m_statusLabel->setText(tr("Status: Monitoring..."));
            m_statusLabel->setStyleSheet("color: #27ae60;");
        } else {
            m_statusLabel->setText(tr("Status: Ready"));
            m_statusLabel->setStyleSheet("color: #2980b9;");
        }
    }
}

bool GuestServerWidget::isMonitoring() const
{
    return m_client->isMonitoring();
}

void GuestServerWidget::updateMetrics(const GuestServerMetrics &metrics)
{
    m_currentMetrics = metrics;
    updateMetricsDisplay();
}

void GuestServerWidget::onConnectionError(const QString &error)
{
    m_statusLabel->setText(tr("Error: %1").arg(error));
    m_statusLabel->setStyleSheet("color: #e74c3c;"); // Red
}

void GuestServerWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(15);
    
    // Status
    m_statusLabel = new QLabel(tr("Status: Not connected"));
    m_lastUpdatedLabel = new QLabel();
    
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_lastUpdatedLabel);
    
    // CPU
    m_cpuLabel = new QLabel(tr("CPU"));
    m_cpuUsage = new QProgressBar();
    m_cpuUsage->setRange(0, 100);
    m_cpuUsage->setTextVisible(true);
    m_cpuUsage->setFormat("%p%");
    m_cpuFreqLabel = new QLabel();
    
    // RAM
    m_ramLabel = new QLabel(tr("RAM"));
    m_ramUsage = new QProgressBar();
    m_ramUsage->setRange(0, 100);
    m_ramUsage->setTextVisible(true);
    m_ramUsage->setFormat("%p%");
    m_ramUsageLabel = new QLabel();
    
    // Disk
    m_diskLabel = new QLabel(tr("Disk"));
    m_diskUsage = new QProgressBar();
    m_diskUsage->setRange(0, 100);
    m_diskUsage->setTextVisible(true);
    m_diskUsage->setFormat("%p%");
    m_diskUsageLabel = new QLabel();
    
    // Add to layout
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(5);
    
    formLayout->addRow(m_cpuLabel, m_cpuUsage);
    formLayout->addRow("", m_cpuFreqLabel);
    
    formLayout->addItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));
    
    formLayout->addRow(m_ramLabel, m_ramUsage);
    formLayout->addRow("", m_ramUsageLabel);
    
    formLayout->addItem(new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));
    
    formLayout->addRow(m_diskLabel, m_diskUsage);
    formLayout->addRow("", m_diskUsageLabel);
    
    mainLayout->addLayout(statusLayout);
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    
    // Style
    setStyleSheet(
        "QLabel { color: #2c3e50; }"
        "QProgressBar { height: 20px; text-align: center; }"
        "QProgressBar::chunk { background-color: #3498db; }"
    );
}

void GuestServerWidget::updateMetricsDisplay()
{
    // Update CPU
    m_cpuUsage->setValue(static_cast<int>(m_currentMetrics.cpu.usage));
    m_cpuFreqLabel->setText(tr("Frequency: %1 MHz").arg(m_currentMetrics.cpu.frequency));
    
    // Update RAM
    m_ramUsage->setValue(static_cast<int>(m_currentMetrics.ram.percentage));
    m_ramUsageLabel->setText(
        tr("Used: %1 MB / %2 MB").arg(m_currentMetrics.ram.used).arg(m_currentMetrics.ram.total)
    );
    
    // Update Disk
    m_diskUsage->setValue(static_cast<int>(m_currentMetrics.disk.percentage));
    m_diskUsageLabel->setText(
        tr("Used: %1 MB / %2 MB").arg(m_currentMetrics.disk.used).arg(m_currentMetrics.disk.total)
    );
    
    // Update last updated time
    m_lastUpdatedLabel->setText(
        tr("Last updated: %1").arg(m_currentMetrics.lastUpdated.toString("hh:mm:ss"))
    );
}
