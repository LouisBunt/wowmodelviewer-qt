cmake_minimum_required(VERSION 3.16)
project(WoWModelViewerQt VERSION 1.5.0)

# The version the running application reports -- window title, About box, trace and log.
# Without it a bug report cannot say which build it came from. Five other places carry the
# same number and none of them can read this one, so a release moves all six:
#   installer/MVMidnight.iss          MyAppVersion, MyAppVersionNumeric
#   installer/setupui/CMakeLists.txt  project(... VERSION)
#   installer/setupui/main.cpp        Options::version, the fallback when /VERSION is absent
#   installer/setupui/setupui.rc      VERSIONINFO -- what SmartScreen shows while the user
#                                     decides whether to trust the extracted front-end
#   installer/THIRD-PARTY-NOTICES.txt the "Stand" line
# 1.0.0 is the public numbering: releases before it (1.5.0-1.8.0) were the early-access
# line that only existed on this machine and in the repo history. The public count
# starts where the public does. After 1.1.0 it jumped to 1.5.0 with the online mode;
# that 1.5.0 is a public release, not the early-access build of the same number.
set(WMV_QT_VERSION "1.5.0")

# The product name, in one place. It reached five files last time it changed.
set(WMV_APP_NAME "ModelViewer: Midnight")
set(WMV_PROJECT_URL "https://github.com/LouisBunt/wowmodelviewer-qt")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# The sources are UTF-8 without a BOM. Left to itself MSVC reads them in the system code
# page, which on a German Windows is CP1252: the umlauts throughout the German UI strings
# only survived that by accident, and anything outside CP1252 -- the Japanese artist name
# in the contact dialog, for one -- would not. Say it explicitly for source and execution
# charset both, which is what Qt's tr() and fromUtf8() already assume.
if(MSVC)
  add_compile_options(/utf-8)
endif()

# The engine -- core.dll, wow.dll and the plugins -- lives in engine/ and is built on its
# own, before this (engine/CMakeLists.txt says how); this build links its import libraries.
# The defaults are relative, so a fresh clone builds. Point WMV_SRC / WMV_BUILD elsewhere to
# use an engine tree that was built somewhere else.
set(WMV_SRC   "${CMAKE_CURRENT_SOURCE_DIR}/engine" CACHE PATH "Engine source tree")
set(WMV_BUILD "${WMV_SRC}/build-x64"               CACHE PATH "Engine build tree")
set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation")

# vcpkg (for GLEW). Defaulted from the VCPKG_ROOT environment variable rather than a path
# inside one developer's user profile -- for a GPLv3 release the buildable source IS part of
# what ships, and a hardcoded C:/Users/<name>/... makes it unbuildable everywhere else.
if(DEFINED ENV{VCPKG_ROOT})
  set(VCPKG_STATIC_DEFAULT "$ENV{VCPKG_ROOT}/installed/x64-windows-static-md")
else()
  set(VCPKG_STATIC_DEFAULT "C:/vcpkg/installed/x64-windows-static-md")
endif()
set(VCPKG_STATIC "${VCPKG_STATIC_DEFAULT}" CACHE PATH "vcpkg static triplet")

# Fail early and legibly. Without this, a missing engine tree surfaces as a wall of
# "cannot open include file" or an unresolved-symbol dump much later in the build.
if(NOT EXISTS "${WMV_SRC}/Source/core")
  message(FATAL_ERROR
    "WMV_SRC does not look like the engine source tree: ${WMV_SRC}\n"
    "Expected ${WMV_SRC}/Source/core to exist. The engine is the engine/ folder of this\n"
    "repository; leave WMV_SRC unset to use it.")
endif()

if(NOT EXISTS "${QT_LOCATION}/lib/cmake/Qt5")
  message(FATAL_ERROR
    "Qt5 not found at: ${QT_LOCATION}\n"
    "Install Qt 5.13.2 (msvc2017_64), or point at yours:\n"
    "    cmake -S . -B build -DQT_LOCATION=<path to Qt/5.13.2/msvc2017_64>")
endif()

if(NOT EXISTS "${VCPKG_STATIC}/lib/libglew32.lib")
  message(FATAL_ERROR
    "GLEW not found under: ${VCPKG_STATIC}\n"
    "Install it with vcpkg and set VCPKG_ROOT, or point at it directly:\n"
    "    vcpkg install glew:x64-windows-static-md\n"
    "    cmake -S . -B build -DVCPKG_STATIC=<vcpkg>/installed/x64-windows-static-md")
endif()

foreach(lib
    "${WMV_BUILD}/Source/core/Release/core.lib"
    "${WMV_BUILD}/Source/games/wow/Release/wow.lib")
  if(NOT EXISTS "${lib}")
    message(FATAL_ERROR
      "Missing engine import library: ${lib}\n"
      "Build the engine first:\n"
      "    cmake -S engine -B engine/build-x64 -G \"Visual Studio 17 2022\" -A x64\n"
      "    cmake --build engine/build-x64 --config Release\n"
      "or set -DWMV_BUILD=<path to an existing engine build-x64>.")
  endif()
endforeach()

set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
find_package(Qt5 5.13 COMPONENTS Core Gui Widgets Network Svg REQUIRED)

add_executable(WoWModelViewer-Qt WIN32
  main.cpp
  GLHost.cpp
  GLHost.h
  MainWindow.cpp
  MainWindow.h
  MenuController.cpp
  MenuController.h
  CharacterPanel.cpp
  CharacterPanel.h
  InspectorTabs.cpp
  InspectorTabs.h
  ItemBrowser.cpp
  NpcBrowser.cpp
  ItemBrowser.h
  LightPanel.cpp
  LightPanel.h
  ExportController.cpp
  ExportController.h
  TimelinePanel.cpp
  TimelinePanel.h
  FileTreeModel.cpp
  FileTreeModel.h
  GameSource.cpp
  GameSource.h
  LoadingSplash.cpp
  LoadingSplash.h
  SourceDialog.cpp
  SourceDialog.h
  MVLinkCode.cpp
  WowheadDressingRoom.cpp
  WowheadDressingRoom.h
  BlenderAddonInstaller.cpp
  BlenderAddonInstaller.h
  Theme.cpp
  Theme.h
  MidnightStyle.cpp
  MidnightStyle.h
  UiKit.cpp
  UiKit.h
  # From the wx program's sources, which are otherwise gone; it was toolkit-free (glm only).
  OrbitCamera.cpp
  OrbitCamera.h
  resources/resources.qrc
  resources/appicon.rc)

set_target_properties(WoWModelViewer-Qt PROPERTIES OUTPUT_NAME "WoWModelViewer-Qt")

target_include_directories(WoWModelViewer-Qt PRIVATE
  ${WMV_SRC}/Source/core
  ${WMV_SRC}/Source/games/wow
  ${WMV_SRC}/ThirdParty
  ${VCPKG_STATIC}/include)

# GLEW is initialised inside wow.dll (games/wow/video.cpp); we only need the
# headers plus the same static lib so the symbols resolve consistently.
target_compile_definitions(WoWModelViewer-Qt PRIVATE
  GLEW_STATIC
  WMV_QT_VERSION="${WMV_QT_VERSION}"
  WMV_APP_NAME="${WMV_APP_NAME}"
  WMV_PROJECT_URL="${WMV_PROJECT_URL}")

target_link_libraries(WoWModelViewer-Qt
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg
  opengl32 glu32
  ${VCPKG_STATIC}/lib/libglew32.lib
  ${WMV_BUILD}/Source/core/Release/core.lib
  ${WMV_BUILD}/Source/games/wow/Release/wow.lib)

# The decoder test suite. tests/ is also configurable standalone (see tests/CMakeLists
# for why); from here it simply becomes a ctest target of the main build.
include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
