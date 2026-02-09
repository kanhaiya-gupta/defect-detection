# Normitri — Defect detection pipeline for shopping items (C++23)
#
# Layout: target-based; PUBLIC/PRIVATE for deps; install export for find_package(Normitri).
# Dependencies: OpenCV (required), GTest (required when NORMITRI_BUILD_TESTS=ON).
# Conan: run 'conan install . --output-folder=build' then configure; CMake auto-detects Conan generators below.

cmake_minimum_required(VERSION 3.21)

# Policies for consistent, modern behavior
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)  # CMAKE_ROLE and Apple platforms
endif()

project(Normitri
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Defect detection pipeline for shopping items (C++23)"
)

# -----------------------------------------------------------------------------
# C++ standard and options
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NORMITRI_BUILD_TESTS "Build unit and integration tests" ON)
option(NORMITRI_BUILD_APP "Build normitri-cli executable" ON)
option(NORMITRI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI)" OFF)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
# Auto-detect Conan generators (cmake_layout: build/Release/generators, or flat: build/)
# so find_package(OpenCV) and find_package(GTest) work without extra -D on the command line.
set(_conan_generators "")
if(EXISTS "${CMAKE_BINARY_DIR}/build/Release/generators/OpenCVConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/build/Release/generators")
  set(_conan_generators "${CMAKE_BINARY_DIR}/build/Release/generators")
elseif(EXISTS "${CMAKE_BINARY_DIR}/OpenCVConfig.cmake")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")
  set(_conan_generators "${CMAKE_BINARY_DIR}")
endif()
if(_conan_generators)
  message(STATUS "Conan generators found: ${_conan_generators}")
endif()
unset(_conan_generators)

find_package(OpenCV REQUIRED)

if(NORMITRI_BUILD_TESTS)
  find_package(GTest REQUIRED)
endif()

# -----------------------------------------------------------------------------
# Project modules (warnings, package config)
# -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(GNUInstallDirs)

# -----------------------------------------------------------------------------
# Core library (Frame, Pipeline, Defect, etc.)
# -----------------------------------------------------------------------------
add_library(normitri_core
  src/core/frame.cpp
  src/core/pipeline.cpp
)
target_include_directories(normitri_core
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)
target_compile_features(normitri_core PUBLIC cxx_std_23)
target_compile_definitions(normitri_core PUBLIC
  NORMITRI_VERSION_STRING="${PROJECT_VERSION}"
)
normitri_enable_warnings(normitri_core)

# -----------------------------------------------------------------------------
# Vision library (stages, inference adapter, OpenCV)
# -----------------------------------------------------------------------------
add_library(normitri_vision
  src/vision/frame_cv_utils.cpp
  src/vision/inference_backend.cpp
  src/vision/resize_stage.cpp
  src/vision/normalize_stage.cpp
  src/vision/color_convert_stage.cpp
  src/vision/defect_decoder.cpp
  src/vision/defect_detection_stage.cpp
  src/vision/mock_inference_backend.cpp
)
target_include_directories(normitri_vision
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vision"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(normitri_vision PUBLIC normitri_core ${OpenCV_LIBS})
target_compile_features(normitri_vision PUBLIC cxx_std_23)
normitri_enable_warnings(normitri_vision)

# -----------------------------------------------------------------------------
# App library (config, pipeline runner — shared by executables)
# -----------------------------------------------------------------------------
add_library(normitri_app_lib
  src/app/config.cpp
  src/app/pipeline_runner.cpp
)
target_include_directories(normitri_app_lib
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
)
target_link_libraries(normitri_app_lib PUBLIC normitri_vision)
target_compile_features(normitri_app_lib PUBLIC cxx_std_23)
normitri_enable_warnings(normitri_app_lib)

# -----------------------------------------------------------------------------
# Executable: normitri-cli
# -----------------------------------------------------------------------------
if(NORMITRI_BUILD_APP)
  add_executable(normitri_cli apps/normitri-cli/main.cpp)
  target_link_libraries(normitri_cli PRIVATE normitri_app_lib)
  target_compile_features(normitri_cli PRIVATE cxx_std_23)
  normitri_enable_warnings(normitri_cli)
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(NORMITRI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Install: libraries, headers, package config
# -----------------------------------------------------------------------------
set(NORMITRI_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Normitri" CACHE STRING "Install dir for NormitriConfig.cmake")

install(TARGETS normitri_core normitri_vision normitri_app_lib
  EXPORT NormitriTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/normitri
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)
if(NORMITRI_BUILD_APP)
  install(TARGETS normitri_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Package config for find_package(Normitri)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/NormitriConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormitriConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/NormitriConfig.cmake"
  INSTALL_DESTINATION ${NORMITRI_CMAKECONFIG_INSTALL_DIR}
  PATH_VARS NORMITRI_CMAKECONFIG_INSTALL_DIR
)
install(EXPORT NormitriTargets
  FILE NormitriTargets.cmake
  NAMESPACE Normitri::
  DESTINATION ${NORMITRI_CMAKECONFIG_INSTALL_DIR}
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/NormitriConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/NormitriConfigVersion.cmake"
  DESTINATION ${NORMITRI_CMAKECONFIG_INSTALL_DIR}
)
