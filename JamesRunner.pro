QT += widgets

CONFIG += c++17
TEMPLATE = app
TARGET = JamesRunner

SOURCES += \
    src/main.cpp \
    src/GameWidget.cpp

HEADERS += \
    src/GameWidget.h

msvc:QMAKE_CXXFLAGS += /utf-8
