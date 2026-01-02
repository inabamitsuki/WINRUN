#ifndef GUESTSERVERDIALOG_H
#define GUESTSERVERDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>

class GuestServerDialog : public QDialog {
    Q_OBJECT
public:
    explicit GuestServerDialog(QWidget *parent = nullptr);
    
    QString hostname() const;
    int port() const;
    QString username() const;
    QString password() const;

private:
    QLineEdit *hostnameEdit;
    QSpinBox *portSpin;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
};

#endif // GUESTSERVERDIALOG_H
