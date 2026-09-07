# Standalone-configurable on purpose: the main project refuses to configure without
# the upstream build tree (its import libraries are checked with FATAL_ERROR), but the
# decoder test needs none of that -- just Qt5Core, the decoder source and the
# header-only wow_enums.h. So CI or a fresh checkout can run
#
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
#
# without game data, without the FBX SDK, without a compiled upstream.
cmake_minimum_required(VERSION 3.16)
project(WMVTests)

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED QT_LOCATION)
  set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation")
endif()
set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
find_package(Qt5 5.13 COMPONENTS Core Gui Widgets Svg REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(whtest
  whtest/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WowheadDressingRoom.cpp)

target_include_directories(whtest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../upstream/Source/games/wow)

target_link_libraries(whtest Qt5::Core)

enable_testing()
add_test(NAME wowhead_corpus
         COMMAND whtest ${CMAKE_CURRENT_SOURCE_DIR}/corpus/corpus1.json)

# The STL exporter's geometry half, compiled straight from the upstream plugin source: pure
# C++ and glm, no Qt, no game data. Proves the height in the file, the plate placement, the
# winding under a mirror and the binary format before any real export is run.
set(STL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../upstream/Source/plugins/exporters/stl)
add_executable(stltest
  stltest/main.cpp
  ${STL_DIR}/PrintMesh.cpp)
target_include_directories(stltest PRIVATE
  ${STL_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../upstream/ThirdParty)
add_test(NAME stl_geometry COMMAND stltest)

# The interface's building blocks. Compiled from the front-end's own sources, so a change to
# Theme.h or UiKit.cpp is checked here rather than only being looked at in a screenshot.
# Runs offscreen: no display, no game data, no GL.
add_executable(uitest
  uitest/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../Theme.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../UiKit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../resources/resources.qrc)
target_include_directories(uitest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(uitest Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Svg)
add_test(NAME ui_blocks COMMAND uitest)
set_tests_properties(ui_blocks PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
