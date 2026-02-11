// Unit tests for TensorRTInferenceBackend.
// One test runs without an engine (constructor with missing file). The rest require a real
// .engine file: set NORMITRI_TEST_TENSORRT_ENGINE to the path. They are skipped if the env var
// is unset or the file is missing, so CI without a GPU/engine still passes.
// Engine-dependent tests assume 640x640 input (e.g. YOLOv10n-derived engine).
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/tensorrt_inference_backend.hpp>
#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace nv = normitri::vision;
namespace nc = normitri::core;

// Engine path for tests that require a real TensorRT engine. If unset or file missing, those tests are skipped.
static std::string get_test_engine_path() {
  const char* env = std::getenv("NORMITRI_TEST_TENSORRT_ENGINE");
  if (env && env[0] != '\0' && std::filesystem::exists(env)) {
    return env;
  }
  return "";
}

// Common detector input size (e.g. YOLOv10n). Used when we have a real engine.
static constexpr std::uint32_t kDefaultEngineHeight = 640u;
static constexpr std::uint32_t kDefaultEngineWidth = 640u;

static nc::Frame make_float_frame(std::uint32_t w, std::uint32_t h) {
  const std::size_t num_bytes =
      nc::Frame::min_bytes(w, h, nc::PixelFormat::Float32Planar);
  std::vector<std::byte> buffer(num_bytes, std::byte{0});
  return nc::Frame(w, h, nc::PixelFormat::Float32Planar, std::move(buffer));
}

// --- Tests that run without an engine ---

TEST(TensorRTInferenceBackend, ConstructorThrowsWhenFileMissing) {
  EXPECT_THROW(
      {
        nv::TensorRTInferenceBackend backend(
            "nonexistent_tensorrt_engine_12345_should_not_exist.engine");
      },
      std::runtime_error);
}

// --- Tests that require a real TensorRT engine (skip if NORMITRI_TEST_TENSORRT_ENGINE not set or missing) ---

TEST(TensorRTInferenceBackend, ValidateInputRejectsEmptyFrame) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  nc::Frame empty;
  auto valid = backend.validate_input(empty);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(TensorRTInferenceBackend, ValidateInputRejectsWrongFormat) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  std::vector<std::byte> buf(kDefaultEngineWidth * kDefaultEngineHeight * 3);
  nc::Frame f(kDefaultEngineWidth, kDefaultEngineHeight, nc::PixelFormat::RGB8,
              std::move(buf));
  auto valid = backend.validate_input(f);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(TensorRTInferenceBackend, ValidateInputRejectsWrongDimensions) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  nc::Frame f = make_float_frame(320, 240);  // wrong size for 640x640 engine
  auto valid = backend.validate_input(f);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(TensorRTInferenceBackend, ValidateInputAcceptsMatchingFrame) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  nc::Frame f = make_float_frame(kDefaultEngineWidth, kDefaultEngineHeight);
  auto valid = backend.validate_input(f);
  ASSERT_TRUE(valid.has_value());
}

TEST(TensorRTInferenceBackend, InferReturnsSaneResult) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  backend.warmup();
  nc::Frame f = make_float_frame(kDefaultEngineWidth, kDefaultEngineHeight);
  auto result = backend.infer(f);
  ASSERT_TRUE(result.has_value()) << "infer() should succeed with valid frame";
  EXPECT_EQ(result->boxes.size(), result->num_detections * 4u);
  EXPECT_EQ(result->scores.size(), result->num_detections);
  EXPECT_EQ(result->class_ids.size(), result->num_detections);
  for (std::uint32_t i = 0; i < result->num_detections; ++i) {
    EXPECT_GE(result->scores[i], 0.f);
    EXPECT_LE(result->scores[i], 1.f);
  }
}

TEST(TensorRTInferenceBackend, WarmupDoesNotThrow) {
  const std::string path = get_test_engine_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_TENSORRT_ENGINE to run (path to .engine file)";
  }
  nv::TensorRTInferenceBackend backend(path);
  EXPECT_NO_THROW(backend.warmup());
}
