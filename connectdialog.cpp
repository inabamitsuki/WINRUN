#include "connectdialog.h"

#include <QRegularExpression>
#include <QProcess>
#include <QCoreApplication>

ConnectDialog::ConnectDialog(const QString &vmName, QWidget *parent)
    : QDialog(parent), vm(vmName)
{
    setWindowTitle("Connect to Desktop");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);
    setStyleSheet(
        "QDialog { background: #ffffff; border: 1px solid #1a535c; border-radius: 8px; }"
        "QLabel { color: #1a535c; font-size: 14px; }"
        "QLineEdit { padding: 8px; border: 1px solid #ddd; border-radius: 4px; min-width: 250px; }"
        "QSpinBox { padding: 6px; border: 1px solid #ddd; border-radius: 4px; }"
        "QPushButton { background-color: #1a535c; color: white; border: none; padding: 8px 20px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #2a7a83; }"
    );

    initUI();
    resolveIp();
}

void ConnectDialog::initUI()
{
    QVBoxLayout *main = new QVBoxLayout(this);

    QLabel *title = new QLabel(QStringLiteral("Connect to %1").arg(vm));
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #1a535c;");

    ipLabel = new QLabel("IP: resolving...");

    QFormLayout *form = new QFormLayout();
    usernameEdit = new QLineEdit(this);
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    portSpin = new QSpinBox(this);
    portSpin->setRange(1, 65535);
    portSpin->setValue(3389);

    form->addRow("Username:", usernameEdit);
    form->addRow("Password:", passwordEdit);
    form->addRow("Port:", portSpin);

    QHBoxLayout *btns = new QHBoxLayout();
    btns->addStretch();
    cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setStyleSheet("QPushButton { background-color: #f8f9fa; color: #1a535c; border: 1px solid #ddd; }");
    connectBtn = new QPushButton("Connect", this);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(connectBtn, &QPushButton::clicked, this, &QDialog::accept);

    btns->addWidget(cancelBtn);
    btns->addWidget(connectBtn);

    main->addWidget(title);
    main->addSpacing(6);
    main->addWidget(ipLabel);
    main->addSpacing(10);
    main->addLayout(form);
    main->addSpacing(10);
    main->addLayout(btns);
}

QString ConnectDialog::username() const { return usernameEdit->text(); }
QString ConnectDialog::password() const { return passwordEdit->text(); }
int ConnectDialog::port() const { return portSpin->value(); }
QString ConnectDialog::ipAddress() const { return ip; }

void ConnectDialog::resolveIp()
{
    bool ok = false;
    QString xml = runProcess("virsh", {"dumpxml", vm}, &ok);
    if (!ok || xml.isEmpty()) {
        ipLabel->setText("IP: unknown");
        return;
    }
    QString mac = getMacFromXml(xml);
    if (mac.isEmpty()) {
        ipLabel->setText("IP: unknown");
        return;
    }
    ip = getIpForMac(mac);
    if (ip.isEmpty()) ipLabel->setText("IP: unknown");
    else ipLabel->setText(QStringLiteral("IP: %1").arg(ip));
}

QString ConnectDialog::getMacFromXml(const QString &xml)
{
    QRegularExpression re("<mac\\s+address=\"?([^\"'/>]+)\"?");
    auto m = re.match(xml);
    if (m.hasMatch()) return m.captured(1);
    return QString();
}

QString ConnectDialog::runProcess(const QString &program, const QStringList &args, bool *ok)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(5000)) { if (ok) *ok = false; p.kill(); return {}; }
    if (ok) *ok = (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
    return QString::fromLocal8Bit(p.readAllStandardOutput());
}

QString ConnectDialog::getIpForMac(const QString &mac)
{
    const QStringList nets = {"default", "virbr0", "bridge"};
    QRegularExpression ipre("(\\d{1,3}(?:\\.\\d{1,3}){3})(?:/\\d{1,2})?");
    for (const QString &net : nets) {
        bool ok = false;
        QString out = runProcess("virsh", {"net-dhcp-leases", net, "--mac", mac}, &ok);
        if (!ok || out.isEmpty()) continue;
        auto m = ipre.match(out);
        if (m.hasMatch()) return m.captured(1);
    }
    return QString();
}
