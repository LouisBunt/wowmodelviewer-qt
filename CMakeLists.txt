cmake_minimum_required(VERSION 3.16)
project(WoWModelViewerQt VERSION 1.8.0)

# The version the running application reports -- window title, About box, trace and log.
# Without it a bug report cannot say which build it came from. Keep in step with
# MyAppVersion in installer/MVMidnight.iss.
set(WMV_QT_VERSION "1.8.0")

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

# Reuse the upstream tree: we link its core/wow libraries unchanged.
#
# The upstream checkout lives inside this repository (submodule "upstream"), so the
# default is relative and the build works from a fresh clone. Point WMV_SRC / WMV_BUILD
# elsewhere if you keep the upstream tree outside the repo.
set(WMV_SRC   "${CMAKE_CURRENT_SOURCE_DIR}/upstream" CACHE PATH "WMV source tree")
set(WMV_BUILD "${WMV_SRC}/build-x64"                 CACHE PATH "WMV build tree")
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

# Fail early and legibly. Without this, a missing upstream tree surfaces as a wall of
# "cannot open include file" or an unresolved-symbol dump much later in the build.
if(NOT EXISTS "${WMV_SRC}/Source/core")
  message(FATAL_ERROR
    "WMV_SRC does not look like a WMV source tree: ${WMV_SRC}\n"
    "Expected ${WMV_SRC}/Source/core to exist.\n"
    "If you just cloned this repository, fetch the submodule:\n"
    "    git submodule update --init --recursive")
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
      "Missing upstream import library: ${lib}\n"
      "Build the upstream tree first (Configure.bat / Build.bat in ${WMV_SRC}),\n"
      "or set -DWMV_BUILD=<path to an existing upstream build-x64>.")
  endif()
endforeach()

set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
find_package(Qt5 5.13 COMPONENTS Core Gui Widgets Network REQUIRED)

# OrbitCamera still lives under Source/wowmodelviewer, but it is toolkit-free (glm
# only), so we simply compile it in rather than duplicating it.
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
  MVLinkCode.cpp
  WowheadDressingRoom.cpp
  WowheadDressingRoom.h
  BlenderAddonInstaller.cpp
  BlenderAddonInstaller.h
  resources/resources.qrc
  resources/appicon.rc
  ${WMV_SRC}/Source/wowmodelviewer/OrbitCamera.cpp)

set_target_properties(WoWModelViewer-Qt PROPERTIES OUTPUT_NAME "WoWModelViewer-Qt")

target_include_directories(WoWModelViewer-Qt PRIVATE
  ${WMV_SRC}/Source/core
  ${WMV_SRC}/Source/games/wow
  ${WMV_SRC}/Source/wowmodelviewer
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
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network
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
