cmake_minimum_required(VERSION 3.16)

project(JamesRunner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 QUIET COMPONENTS Widgets)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(QT_PACKAGE Qt5)
endif()

add_executable(JamesRunner
    src/main.cpp
    src/GameWidget.h
    src/GameWidget.cpp
)

target_link_libraries(JamesRunner PRIVATE ${QT_PACKAGE}::Widgets)

if (MSVC)
    target_compile_options(JamesRunner PRIVATE /utf-8)
endif()
