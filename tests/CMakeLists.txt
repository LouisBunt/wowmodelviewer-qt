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
find_package(Qt5 5.13 COMPONENTS Core REQUIRED)

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
