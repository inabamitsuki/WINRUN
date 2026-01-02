#include "guestserverdialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>

GuestServerDialog::GuestServerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Connect to Guest Server");
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

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    QFormLayout *form = new QFormLayout();
    
    hostnameEdit = new QLineEdit(this);
    hostnameEdit->setPlaceholderText("server.example.com or IP address");
    
    portSpin = new QSpinBox(this);
    portSpin->setRange(1, 65535);
    portSpin->setValue(3389);
    
    usernameEdit = new QLineEdit(this);
    usernameEdit->setPlaceholderText("Leave empty for default");
    
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("Leave empty for none");
    
    form->addRow("Hostname:", hostnameEdit);
    form->addRow("Port:", portSpin);
    form->addRow("Username (optional):", usernameEdit);
    form->addRow("Password (optional):", passwordEdit);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *connectButton = new QPushButton("Connect", this);
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(connectButton);
    
    mainLayout->addLayout(form);
    mainLayout->addLayout(buttonLayout);
    
    connect(connectButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString GuestServerDialog::hostname() const {
    return hostnameEdit->text().trimmed();
}

int GuestServerDialog::port() const {
    return portSpin->value();
}

QString GuestServerDialog::username() const {
    return usernameEdit->text().trimmed();
}

QString GuestServerDialog::password() const {
    return passwordEdit->text();
}
