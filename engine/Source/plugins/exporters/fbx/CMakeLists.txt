# fbx exporter lib
include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)

message(STATUS "Building FBX exporter")

add_definitions(-DQT_PLUGIN)

# The FBX SDK 2020.3 (x64), staged under ThirdParty/fbx_x64 -- it is not in the repository; the
# build workflow and README say where it comes from.
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/ThirdParty/fbx_x64/include)

set(src FBXExporter.cpp
  FBXAnimExporter.cpp
  FBXHeaders.cpp)

set(headers FBXExporter.h
  FBXAnimExporter.h
  FBXHeaders.h)

source_group("Header Files" FILES ${headers})

set(useful_files fbxexporter.json)
set_source_files_properties(${useful_files} PROPERTIES HEADER_FILE_ONLY TRUE)

use_glew()
use_core()
use_wow()

set(NAME fbxexporter)
add_library(${NAME} SHARED ${src} ${headers} ${useful_files})
set_property(TARGET ${NAME} PROPERTY FOLDER "plugins")

set(FBX_SDK_LIB ${CMAKE_SOURCE_DIR}/ThirdParty/fbx_x64/lib/libfbxsdk.lib)
set(FBX_SDK_DLL ${CMAKE_SOURCE_DIR}/ThirdParty/fbx_x64/lib/libfbxsdk.dll)

target_link_libraries(${NAME} Qt5::Core ${extralibs} ${FBX_SDK_LIB})

set(BIN_DIR "${CMAKE_SOURCE_DIR}/bin/plugins/")

if(WIN32)
  install(TARGETS ${NAME} RUNTIME DESTINATION ${BIN_DIR})
  install(FILES "${FBX_SDK_DLL}" DESTINATION "${CMAKE_SOURCE_DIR}/bin")
endif(WIN32)
