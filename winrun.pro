QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    addprogramdialog.cpp \
    connectdialog.cpp

HEADERS += \
    mainwindow.h \
    addprogramdialog.h \
    connectdialog.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

# Copy icon folder to build directory
win32: {
    ICON_SOURCES = $$files(icon/*.png)
    for(ICON, ICON_SOURCES) {
        ICON_DEST = $$OUT_PWD/$${ICON}
        !exists($$ICON_DEST) {
            QMAKE_PRE_LINK += $$QMAKE_COPY $$shell_quote($$PWD/$${ICON}) $$shell_quote($$ICON_DEST) $$escape_expand(\\n\t)
        }
    }
}

unix:!macx: {
    target.path = /usr/local/bin
    INSTALLS += target
}
