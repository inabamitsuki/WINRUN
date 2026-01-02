cmake_minimum_required(VERSION 3.5)

project(winrun VERSION 1.0 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt components
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Network REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Network REQUIRED)

# Set up the executable
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    addprogramdialog.cpp
    addprogramdialog.h
    connectdialog.cpp
    connectdialog.h
    guestserverclient.cpp
    guestserverclient.h
    guestserverwidget.cpp
    guestserverwidget.h
    guestserverdialog.cpp
    guestserverdialog.h
    guestserverappsclient.cpp
    guestserverappsclient.h
    appslistwidget.cpp
    appslistwidget.h
    resources.qrc
)

# Add the executable
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)
