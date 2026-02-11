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
# When ON (default), TensorRT backend is built if TensorRT and CUDA are found; when OFF, skip.
# Set to OFF to disable auto-detection (e.g. in CI without GPU).
option(NORMITRI_USE_TENSORRT "Build TensorRT backend when TensorRT/CUDA are available (auto-detect)" ON)
# When ON, look for TBB and build multi-camera/multi-tenant runner; when OFF, skip. Set OFF if TBB is not available.
option(NORMITRI_USE_TBB "Build TBB-based multi-camera runner when TBB is available (auto-detect)" ON)
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
find_package(onnxruntime REQUIRED)

# Auto-detect TensorRT and CUDA when NORMITRI_USE_TENSORRT is ON (default). If found, build
# TensorRT backend; if not found, build without it. Set NORMITRI_USE_TENSORRT=OFF to skip.
set(NORMITRI_TENSORRT_AVAILABLE OFF)
if(NORMITRI_USE_TENSORRT)
  find_path(TensorRT_INCLUDE_DIR NvInfer.h
    HINTS "$ENV{TensorRT_ROOT}" "$ENV{TENSORRT_ROOT}" "/usr" "/usr/local"
    PATH_SUFFIXES include include/x86_64-linux-gnu)
  find_library(TensorRT_nvinfer_LIBRARY nvinfer
    HINTS "$ENV{TensorRT_ROOT}" "$ENV{TENSORRT_ROOT}" "/usr" "/usr/local"
    PATH_SUFFIXES lib lib/x86_64-linux-gnu lib64)
  find_library(CUDA_cudart_LIBRARY cudart
    HINTS "$ENV{CUDA_HOME}" "$ENV{CUDA_PATH}" "/usr/local/cuda" "/usr"
    PATH_SUFFIXES lib lib64 lib/x64)
  if(TensorRT_INCLUDE_DIR AND TensorRT_nvinfer_LIBRARY AND CUDA_cudart_LIBRARY)
    set(NORMITRI_TENSORRT_AVAILABLE ON)
    message(STATUS "TensorRT/CUDA found: building TensorRT inference backend")
    message(STATUS "  TensorRT: ${TensorRT_INCLUDE_DIR}, ${TensorRT_nvinfer_LIBRARY}")
    message(STATUS "  CUDA cudart: ${CUDA_cudart_LIBRARY}")
  else()
    message(STATUS "TensorRT/CUDA not found; building without TensorRT backend (set NORMITRI_USE_TENSORRT=OFF to hide)")
  endif()
endif()

# Auto-detect TBB when NORMITRI_USE_TBB is ON. If found, build multi-camera runner; if not, skip.
# Try find_package(TBB) first (system or config); then try Conan-provided onetbb.
set(NORMITRI_TBB_AVAILABLE OFF)
if(NORMITRI_USE_TBB)
  find_package(TBB QUIET)
  if(TBB_FOUND AND TARGET TBB::tbb)
    set(NORMITRI_TBB_AVAILABLE ON)
    message(STATUS "TBB found: building multi-camera/multi-tenant runner")
  else()
    find_package(onetbb QUIET)
    if(onetbb_FOUND AND TARGET onetbb::onetbb)
      set(NORMITRI_TBB_AVAILABLE ON)
      add_library(TBB::tbb ALIAS onetbb::onetbb)
      message(STATUS "onetbb (TBB) found: building multi-camera/multi-tenant runner")
    else()
      message(STATUS "TBB not found; multi-camera TBB runner disabled (set NORMITRI_USE_TBB=OFF to hide)")
    endif()
  endif()
endif()

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
set(normitri_vision_sources
  src/vision/frame_cv_utils.cpp
  src/vision/load_image.cpp
  src/vision/inference_backend.cpp
  src/vision/resize_stage.cpp
  src/vision/normalize_stage.cpp
  src/vision/color_convert_stage.cpp
  src/vision/defect_decoder.cpp
  src/vision/defect_detection_stage.cpp
  src/vision/mock_inference_backend.cpp
  src/vision/onnx_inference_backend.cpp
)
if(NORMITRI_TENSORRT_AVAILABLE)
  list(APPEND normitri_vision_sources src/vision/tensorrt_inference_backend.cpp)
endif()
add_library(normitri_vision ${normitri_vision_sources})
target_include_directories(normitri_vision
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vision"
    ${OpenCV_INCLUDE_DIRS}
    $<$<BOOL:${NORMITRI_TENSORRT_AVAILABLE}>:${TensorRT_INCLUDE_DIR}>
)
target_link_libraries(normitri_vision PUBLIC normitri_core ${OpenCV_LIBS} onnxruntime::onnxruntime)
if(NORMITRI_TENSORRT_AVAILABLE)
  target_link_libraries(normitri_vision PRIVATE ${TensorRT_nvinfer_LIBRARY} ${CUDA_cudart_LIBRARY})
endif()
target_compile_features(normitri_vision PUBLIC cxx_std_23)
normitri_enable_warnings(normitri_vision)

# -----------------------------------------------------------------------------
# App library (config, pipeline runner — shared by executables)
# -----------------------------------------------------------------------------
set(normitri_app_sources
  src/app/config.cpp
  src/app/pipeline_runner.cpp
)
if(NORMITRI_TBB_AVAILABLE)
  list(APPEND normitri_app_sources src/app/pipeline_runner_tbb.cpp)
endif()
add_library(normitri_app_lib ${normitri_app_sources})
target_include_directories(normitri_app_lib
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
)
target_link_libraries(normitri_app_lib PUBLIC normitri_vision)
if(NORMITRI_TBB_AVAILABLE)
  target_link_libraries(normitri_app_lib PUBLIC TBB::tbb)
  target_compile_definitions(normitri_app_lib PUBLIC NORMITRI_HAS_TBB)
endif()
target_compile_features(normitri_app_lib PUBLIC cxx_std_23)
normitri_enable_warnings(normitri_app_lib)

# -----------------------------------------------------------------------------
# Executable: normitri-cli
# -----------------------------------------------------------------------------
if(NORMITRI_BUILD_APP)
  add_executable(normitri_cli apps/normitri-cli/main.cpp)
  target_link_libraries(normitri_cli PRIVATE normitri_app_lib)
  target_compile_features(normitri_cli PRIVATE cxx_std_23)
  if(NORMITRI_TENSORRT_AVAILABLE)
    target_compile_definitions(normitri_cli PRIVATE NORMITRI_HAS_TENSORRT)
  endif()
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
