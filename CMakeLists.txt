cmake_minimum_required(VERSION 3.16)

project(rigol_gui_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_add_executable(rigol_gui_cpp
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_link_libraries(rigol_gui_cpp
    PRIVATE Qt6::Widgets
)