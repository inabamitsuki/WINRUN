#ifndef ADDPROGRAMDIALOG_H
#define ADDPROGRAMDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QFileDialog>

class AddProgramDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddProgramDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseClicked();
    void onAddClicked();

private:
    QLineEdit *nameEdit;
    QLineEdit *pathEdit;
    QPushButton *browseButton;
    QPushButton *addButton;
    QPushButton *cancelButton;
};

#endif // ADDPROGRAMDIALOG_H
