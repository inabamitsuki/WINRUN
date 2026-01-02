#ifndef CONNECTDIALOG_H
#define CONNECTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectDialog(const QString &vmName, QWidget *parent = nullptr);
    QString username() const;
    QString password() const;
    int port() const;
    QString ipAddress() const;

private:
    void initUI();
    void resolveIp();
    static QString getMacFromXml(const QString &xml);
    static QString runProcess(const QString &program, const QStringList &args, bool *ok);
    static QString getIpForMac(const QString &mac);

    QString vm;
    QLabel *ipLabel;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QSpinBox *portSpin;
    QPushButton *connectBtn;
    QPushButton *cancelBtn;
    QString ip;
};

#endif // CONNECTDIALOG_H
