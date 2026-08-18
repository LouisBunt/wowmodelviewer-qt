# The setup front-end: a small Qt program that IS the visible installer.
#
# Standalone on purpose -- it must build without the upstream tree, because packaging
# happens on machines that only need Qt and MSVC. The real install work is done by the
# Inno engine (MVMidnight.iss), which extracts this program, shows it instead of the
# classic wizard, and is then re-run by it in silent mode. See MVMidnight.iss [Code].
#
#   cmake -S installer/setupui -B build-setupui -G "Visual Studio 17 2022" -A x64
#   cmake --build build-setupui --config Release
cmake_minimum_required(VERSION 3.16)
project(MVMidnightSetupUI VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# Same reasoning as the main CMakeLists: the German UI strings are UTF-8 without a BOM,
# and MSVC left to itself reads CP1252 on a German Windows.
if(MSVC)
  add_compile_options(/utf-8)
endif()

# Same default, same cache variable name as the application build, so one -D serves both.
set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation")
if(NOT EXISTS "${QT_LOCATION}/lib/cmake/Qt5")
  message(FATAL_ERROR
    "Qt5 not found at: ${QT_LOCATION}\n"
    "    cmake -S installer/setupui -B build-setupui -DQT_LOCATION=<path>")
endif()
list(APPEND CMAKE_PREFIX_PATH "${QT_LOCATION}")

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(setupui WIN32 main.cpp setupui.rc)

# Theme.h lives at the repository root; the colours here ARE the application's.
target_include_directories(setupui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(setupui PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
