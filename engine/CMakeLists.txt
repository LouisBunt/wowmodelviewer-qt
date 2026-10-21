# The engine: core.dll, wow.dll and the five plugins MViewer links and ships.
#
# It is configured on its own, before the application, because the application links its import
# libraries:
#
#   cmake -S engine -B engine/build-x64 -G "Visual Studio 17 2022" -A x64 ^
#         -DQT_LOCATION=<Qt>/5.13.2/msvc2017_64 -DWMV_VCPKG_ROOT=<vcpkg>
#   cmake --build engine/build-x64 --config Release
#
# The code comes from WoW Model Viewer (GPLv3); engine/README.md says where from and what changed.
# What it no longer builds: the wxWidgets program it used to ship with, and with it the wxWidgets
# dependency, the 32-bit configuration and the NSIS installer. MViewer is the program now.
cmake_minimum_required(VERSION 3.16)
project(MViewerEngine)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The engine builds for x64 only (Visual Studio: configure it with -A x64).")
endif()

# vcpkg: GLEW from the static-md triplet, libpng and libjpeg from x64-windows. Defaulted from the
# VCPKG_ROOT environment variable, like the application, rather than from one machine's path.
if(DEFINED ENV{VCPKG_ROOT})
  set(WMV_VCPKG_ROOT_DEFAULT "$ENV{VCPKG_ROOT}")
else()
  set(WMV_VCPKG_ROOT_DEFAULT "C:/vcpkg")
endif()
set(WMV_VCPKG_ROOT "${WMV_VCPKG_ROOT_DEFAULT}" CACHE PATH "Root of the vcpkg checkout")
set(WMV_VCPKG_X64 "${WMV_VCPKG_ROOT}/installed/x64-windows")
# static GLEW (libglew32.lib, /MD CRT) -- avoids the dynamic import-lib symbol mismatch.
set(WMV_VCPKG_X64_STATIC "${WMV_VCPKG_ROOT}/installed/x64-windows-static-md")

# Qt5, the same installation the application builds against.
set(QT_LOCATION "C:/Qt/Qt5.13.2/5.13.2/msvc2017_64" CACHE PATH "Qt5 installation to build against")
set(CMAKE_PREFIX_PATH ${QT_LOCATION}/lib/cmake)
find_package(Qt5 5.13.2 COMPONENTS Core Network Widgets Xml Gui REQUIRED)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)

# group cmake auto targets in a dedicated folder for Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER QTAutoMoc)

if(DEFINED BUILDNUMBER)
  add_definitions(-D_BUILDNUMBER=${BUILDNUMBER})
endif()

# add external libraries compilation
add_subdirectory(ThirdParty/CxImage)

# CascLib asks for CMake 3.2, and CMake 4 no longer accepts anything below 3.5. Raising the floor
# here leaves the vendored CMakeLists untouched; CMake 3 ignores the variable.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# configure CASC lib for our usage
# NOTE: these must be CACHE ... FORCE sets, not option(VAR ON) — option()'s 2nd arg
# is the help string, so option(CASC_UNICODE ON) silently leaves CASC_UNICODE OFF.
# With CascLib 3.0 that causes a UNICODE-mismatch #error since the engine builds _UNICODE.
set(CASC_UNICODE ON CACHE BOOL "Force CASC Unicode build to match the engine" FORCE)
set(CASC_BUILD_SHARED_LIB OFF CACHE BOOL "Do not build the CASC shared lib" FORCE)
set(CASC_BUILD_TESTS OFF CACHE BOOL "Do not build CASC tests" FORCE)
set(CASC_BUILD_STATIC_LIB ON CACHE BOOL "Force Static library building" FORCE)
set(CASCLIB_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/casclib)
set(CMAKE_INSTALL_PREFIX ${CASCLIB_INSTALL_LOCATION})
add_subdirectory(ThirdParty/casclib)
set_property(TARGET casc_static PROPERTY FOLDER "ThirdParty libraries")

# configure StormLib (legacy MoPaQ archives, for old Vanilla/TBC/WotLK clients) -- mirrors the
# casclib vendoring above. StormLib MUST be built UNICODE to match the engine's _UNICODE build: its
# archive-path API is TCHAR-based, so an ANSI StormLib linked against a _UNICODE consumer would
# mismatch TCHAR (wchar_t vs char) exactly like the CASC_UNICODE case noted above. Static lib,
# no tests, no install.
set(STORM_UNICODE ON CACHE BOOL "Force StormLib Unicode build to match the engine" FORCE)
set(STORM_BUILD_TESTS OFF CACHE BOOL "Do not build StormLib tests" FORCE)
set(STORM_SKIP_INSTALL ON CACHE BOOL "Do not install StormLib" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build StormLib as a static library" FORCE)
add_subdirectory(ThirdParty/stormlib)
set_property(TARGET storm PROPERTY FOLDER "ThirdParty libraries")

# core, wow and the plugins.
add_subdirectory(Source)
