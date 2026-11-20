# Road Traffic Tuner - X-Plane 12 plugin
#
# Expects the X-Plane SDK unzipped so that this folder contains SDK/CHeaders
# and SDK/Libraries (or pass -DXPSDK=/path/to/SDK).

cmake_minimum_required(VERSION 3.16)

# Universal macOS binary (Apple silicon + Intel); must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")  # X-Plane 12 needs macOS 12+

project(RoadTrafficTuner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# X-Plane SDK: use -DXPSDK=/path/to/SDK if given, otherwise look for a folder
# named SDK inside this project or next to it (in its parent folder).
if(NOT XPSDK)
  foreach(candidate "${CMAKE_SOURCE_DIR}/SDK" "${CMAKE_SOURCE_DIR}/../SDK")
    if(EXISTS "${candidate}/CHeaders/XPLM/XPLMPlugin.h")
      get_filename_component(XPSDK "${candidate}" ABSOLUTE)
      break()
    endif()
  endforeach()
endif()
if(NOT XPSDK OR NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMPlugin.h")
  message(FATAL_ERROR
    "X-Plane SDK not found.\n"
    "Download it from github.com/X-Plane/xpsdk (Releases), rename the unzipped "
    "folder to SDK, and put it inside this project folder or next to it. "
    "Or pass its location with -DXPSDK=/path/to/SDK.")
endif()
message(STATUS "Using X-Plane SDK at ${XPSDK}")

add_library(RoadTrafficTuner MODULE src/rtt_plugin.cpp src/rtt_core.cpp)

target_include_directories(RoadTrafficTuner PRIVATE
  "${XPSDK}/CHeaders/XPLM" "${XPSDK}/CHeaders/Widgets")
target_compile_definitions(RoadTrafficTuner PRIVATE
  XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1)

set_target_properties(RoadTrafficTuner PROPERTIES
  PREFIX "" SUFFIX ".xpl" OUTPUT_NAME "RoadTrafficTuner"
  CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)

if(APPLE)
  target_compile_definitions(RoadTrafficTuner PRIVATE APL=1 IBM=0 LIN=0)
  target_link_options(RoadTrafficTuner PRIVATE "-F${XPSDK}/Libraries/Mac")
  target_link_libraries(RoadTrafficTuner PRIVATE "-framework XPLM" "-framework XPWidgets")
  set(PLATFORM_DIR mac_x64)
elseif(WIN32)
  target_compile_definitions(RoadTrafficTuner PRIVATE APL=0 IBM=1 LIN=0
    NOMINMAX _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(RoadTrafficTuner PRIVATE
    "${XPSDK}/Libraries/Win/XPLM_64.lib" "${XPSDK}/Libraries/Win/XPWidgets_64.lib")
  if(MSVC)
    target_compile_options(RoadTrafficTuner PRIVATE /utf-8)
    set_property(TARGET RoadTrafficTuner PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
  set(PLATFORM_DIR win_x64)
else()
  target_compile_definitions(RoadTrafficTuner PRIVATE APL=0 IBM=0 LIN=1)
  set(PLATFORM_DIR lin_x64)
endif()

# Output in X-Plane's plugin layout: RoadTrafficTuner/<platform>/RoadTrafficTuner.xpl
# ($<1:...> stops multi-config generators adding a Release/ subfolder.)
set(OUT_DIR "${CMAKE_BINARY_DIR}/RoadTrafficTuner/${PLATFORM_DIR}")
set_target_properties(RoadTrafficTuner PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "$<1:${OUT_DIR}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${OUT_DIR}>")
