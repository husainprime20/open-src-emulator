cmake_minimum_required(VERSION 3.24)
project(OpenSrc VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(OpenSrc WIN32
    desktop/src/main.cpp
    desktop/src/mainwindow.h
    desktop/src/mainwindow.cpp
    desktop/src/settings.h
    desktop/src/settings.cpp
    desktop/src/emulatorcontroller.h
    desktop/src/emulatorcontroller.cpp
    desktop/src/performancemonitor.h
    desktop/src/performancemonitor.cpp
    desktop/src/runtimeinstaller.h
    desktop/src/runtimeinstaller.cpp
)

target_link_libraries(OpenSrc PRIVATE Qt6::Widgets Qt6::Network)
target_include_directories(OpenSrc PRIVATE desktop/src)
if(WIN32)
    target_link_libraries(OpenSrc PRIVATE psapi)
endif()

if(MSVC)
    target_compile_options(OpenSrc PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
else()
    target_compile_options(OpenSrc PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS OpenSrc RUNTIME DESTINATION .)
