cmake_minimum_required(VERSION 3.16)
project(WmvInspectorPrototype)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation")
set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
find_package(Qt5 5.13 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(inspector WIN32 main.cpp)
target_link_libraries(inspector Qt5::Core Qt5::Gui Qt5::Widgets)

add_executable(shell WIN32 shell.cpp)
target_link_libraries(shell Qt5::Core Qt5::Gui Qt5::Widgets)

# Offscreen render mode is a console-ish path; keep the console subsystem off but
# allow --shot to work by attaching to the parent console when present.
set_target_properties(inspector PROPERTIES WIN32_EXECUTABLE TRUE)
