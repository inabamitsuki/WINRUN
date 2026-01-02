#include "addprogramdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

AddProgramDialog::AddProgramDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add New Program");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setStyleSheet(
        "QDialog { "
        "    background-color: #ffffff; "
        "    border: 1px solid #1a535c; "
        "    border-radius: 8px; "
        "    padding: 15px; "
        "}"
        "QLabel { "
        "    color: #1a535c; "
        "    font-size: 14px; "
        "}"
        "QLineEdit { "
        "    padding: 8px; "
        "    border: 1px solid #ddd; "
        "    border-radius: 4px; "
        "    min-width: 250px; "
        "}"
    );

    // Create form layout
    QFormLayout *formLayout = new QFormLayout();
    
    // Program name
    nameEdit = new QLineEdit(this);
    formLayout->addRow("Program Name:", nameEdit);
    
    // Program path with browse button
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathEdit = new QLineEdit(this);
    browseButton = new QPushButton("Browse...", this);
    browseButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #f8f9fa; "
        "    border: 1px solid #ddd; "
        "    border-radius: 4px; "
        "    padding: 5px 10px; "
        "}"
        "QPushButton:hover { "
        "    background-color: #e9ecef; "
        "}"
    );
    
    pathLayout->addWidget(pathEdit);
    pathLayout->addWidget(browseButton);
    formLayout->addRow("Program Path:", pathLayout);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    addButton = new QPushButton("Add", this);
    addButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #1a535c; "
        "    color: white; "
        "    border: none; "
        "    padding: 8px 20px; "
        "    border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "    background-color: #2a7a83; "
        "}"
    );
    
    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #f8f9fa; "
        "    border: 1px solid #ddd; "
        "    padding: 8px 20px; "
        "    border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "    background-color: #e9ecef; "
        "}"
    );
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(addButton);
    
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(browseButton, &QPushButton::clicked, this, &AddProgramDialog::onBrowseClicked);
    connect(addButton, &QPushButton::clicked, this, &AddProgramDialog::onAddClicked);
    connect(cancelButton, &QPushButton::clicked, this, &AddProgramDialog::reject);
}

void AddProgramDialog::onBrowseClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select Program", 
                                                  QDir::homePath(), 
                                                  "Executable Files (*.exe);;All Files (*.*)");
    if (!filePath.isEmpty()) {
        pathEdit->setText(filePath);
        
        // Suggest a name based on the file name if name is empty
        if (nameEdit->text().isEmpty()) {
            QFileInfo fileInfo(filePath);
            nameEdit->setText(fileInfo.baseName());
        }
    }
}

void AddProgramDialog::onAddClicked()
{
    if (nameEdit->text().isEmpty() || pathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please fill in all fields");
        return;
    }
    
    // Here you would typically save the program details
    // For now, we'll just accept the dialog
    accept();
}
