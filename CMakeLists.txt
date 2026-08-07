cmake_minimum_required(VERSION 3.16)
project(WoWModelViewerQt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Reuse the upstream tree: we link its core/wow libraries unchanged.
#
# The upstream checkout lives inside this repository (submodule "upstream"), so the
# default is relative and the build works from a fresh clone. Point WMV_SRC / WMV_BUILD
# elsewhere if you keep the upstream tree outside the repo.
set(WMV_SRC   "${CMAKE_CURRENT_SOURCE_DIR}/upstream" CACHE PATH "WMV source tree")
set(WMV_BUILD "${WMV_SRC}/build-x64"                 CACHE PATH "WMV build tree")
set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation")
set(VCPKG_STATIC "C:/Users/braun/vcpkg/installed/x64-windows-static-md" CACHE PATH "vcpkg static triplet")

# Fail early and legibly. Without this, a missing upstream tree surfaces as a wall of
# "cannot open include file" or an unresolved-symbol dump much later in the build.
if(NOT EXISTS "${WMV_SRC}/Source/core")
  message(FATAL_ERROR
    "WMV_SRC does not look like a WMV source tree: ${WMV_SRC}\n"
    "Expected ${WMV_SRC}/Source/core to exist.\n"
    "If you just cloned this repository, fetch the submodule:\n"
    "    git submodule update --init --recursive")
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
  ItemBrowser.h
  LightPanel.cpp
  LightPanel.h
  ExportController.cpp
  ExportController.h
  TimelinePanel.cpp
  TimelinePanel.h
  FileTreeModel.cpp
  FileTreeModel.h
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
target_compile_definitions(WoWModelViewer-Qt PRIVATE GLEW_STATIC)

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
