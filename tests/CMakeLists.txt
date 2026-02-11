cmake_minimum_required(VERSION 3.21)

# GTest is required (found in root CMakeLists.txt when NORMITRI_BUILD_TESTS=ON)
include(GoogleTest)

# Unit tests: core
add_executable(normitri_core_tests
  unit/core/frame_test.cpp
  unit/core/defect_test.cpp
  unit/core/pipeline_test.cpp
)
target_link_libraries(normitri_core_tests PRIVATE
  normitri_core
  GTest::gtest_main
)
target_include_directories(normitri_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
normitri_enable_warnings(normitri_core_tests)
include(GoogleTest)
gtest_discover_tests(normitri_core_tests)

# Unit tests: vision
set(normitri_vision_test_sources
  unit/vision/defect_decoder_test.cpp
  unit/vision/mock_inference_backend_test.cpp
  unit/vision/onnx_inference_backend_test.cpp
)
if(NORMITRI_TENSORRT_AVAILABLE)
  list(APPEND normitri_vision_test_sources unit/vision/tensorrt_inference_backend_test.cpp)
endif()
add_executable(normitri_vision_tests ${normitri_vision_test_sources})
target_link_libraries(normitri_vision_tests PRIVATE
  normitri_vision
  GTest::gtest_main
)
target_include_directories(normitri_vision_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
normitri_enable_warnings(normitri_vision_tests)
gtest_discover_tests(normitri_vision_tests)

# Unit tests: app (TBB multi-camera runner), only when TBB is available
if(NORMITRI_TBB_AVAILABLE)
  add_executable(normitri_app_tests
    unit/app/pipeline_runner_tbb_test.cpp
  )
  target_link_libraries(normitri_app_tests PRIVATE
    normitri_app_lib
    GTest::gtest_main
  )
  target_include_directories(normitri_app_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
  normitri_enable_warnings(normitri_app_tests)
  gtest_discover_tests(normitri_app_tests)
endif()

# Integration: full pipeline
add_executable(normitri_integration_tests
  integration/full_pipeline_test.cpp
)
target_link_libraries(normitri_integration_tests PRIVATE
  normitri_app_lib
  GTest::gtest_main
)
target_include_directories(normitri_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
normitri_enable_warnings(normitri_integration_tests)
gtest_discover_tests(normitri_integration_tests)
