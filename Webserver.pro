#-------------------------------------------------
#
# Project created by QtCreator 2025-12-21T20:43:24
#
#-------------------------------------------------

QT += core gui multimedia network websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Webserver
TEMPLATE = app

# Qt 5.6.3需要启用C++11支持
CONFIG += c++11

# 设置应用程序图标（Windows）
win32 {
    RC_FILE = appicon.rc
    LIBS += -luser32
}

# Mac平台图标设置
macx {
    ICON = logo.icns  # 如果需要Mac图标，可以创建icns文件
}

# Linux/Unix平台图标设置
unix:!macx {
    desktop.files = webserver.desktop
    desktop.path = $$PREFIX/share/applications
    INSTALLS += desktop

    icons.files = logo.png  # Linux通常使用PNG格式
    icons.path = $$PREFIX/share/icons
    INSTALLS += icons
}

SOURCES += main.cpp\
        mainwindow.cpp \
    websocket.cpp

HEADERS  += mainwindow.h \
    websocket.h
RESOURCES += resources.qrc
# 添加系统托盘支持
QT += widgets

FORMS    +=
