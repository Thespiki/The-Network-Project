cmake_minimum_required(VERSION 3.21)

project(TheNetworkProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt automatic processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(tnp
    src/main.cpp
    src/MainWindow.cpp
    src/CanvasView.cpp
    include/MainWindow.h
    include/CanvasView.h
    resources/resources.qrc
)

target_include_directories(tnp PRIVATE include)
target_link_libraries(tnp PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(tnp PRIVATE /W4 /permissive-)
else()
    target_compile_options(tnp PRIVATE -Wall -Wextra -Wpedantic)
endif()
