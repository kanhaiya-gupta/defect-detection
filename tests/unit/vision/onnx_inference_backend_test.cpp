// Unit tests for OnnxInferenceBackend.
// One test runs without a model (constructor with missing file). The rest require a real
// .onnx model: set NORMITRI_TEST_ONNX_MODEL to the path (e.g. models/onnx-community__yolov10n/onnx/model.onnx).
// They are skipped if the env var is unset or the file is missing, so CI without a model still passes.
// Model-dependent tests assume 640x640 input (e.g. YOLOv10n).
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/onnx_inference_backend.hpp>
#include <onnxruntime_cxx_api.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace nv = normitri::vision;
namespace nc = normitri::core;

// Model path for tests that require a real ONNX model. If unset or file missing, those tests are skipped.
static std::string get_test_model_path() {
  const char* env = std::getenv("NORMITRI_TEST_ONNX_MODEL");
  if (env && env[0] != '\0' && std::filesystem::exists(env)) {
    return env;
  }
  return "";
}

// YOLOv10n (and many detectors) use 640x640 input. Used when we have a real model.
static constexpr std::uint32_t kDefaultModelHeight = 640u;
static constexpr std::uint32_t kDefaultModelWidth = 640u;

static nc::Frame make_float_frame(std::uint32_t w, std::uint32_t h) {
  const std::size_t num_bytes =
      nc::Frame::min_bytes(w, h, nc::PixelFormat::Float32Planar);
  std::vector<std::byte> buffer(num_bytes, std::byte{0});
  return nc::Frame(w, h, nc::PixelFormat::Float32Planar, std::move(buffer));
}

// --- Tests that run without a model ---

TEST(OnnxInferenceBackend, ConstructorThrowsWhenFileMissing) {
  // ONNX Runtime throws Ort::Exception when the model file does not exist.
  EXPECT_THROW(
      {
        nv::OnnxInferenceBackend backend(
            "nonexistent_onnx_model_12345_should_not_exist.onnx");
      },
      Ort::Exception);
}

// --- Tests that require a real ONNX model (skip if NORMITRI_TEST_ONNX_MODEL not set or missing) ---

TEST(OnnxInferenceBackend, ValidateInputRejectsEmptyFrame) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  nc::Frame empty;
  auto valid = backend.validate_input(empty);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(OnnxInferenceBackend, ValidateInputRejectsWrongFormat) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  std::vector<std::byte> buf(kDefaultModelWidth * kDefaultModelHeight * 3);
  nc::Frame f(kDefaultModelWidth, kDefaultModelHeight, nc::PixelFormat::RGB8,
              std::move(buf));
  auto valid = backend.validate_input(f);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(OnnxInferenceBackend, ValidateInputRejectsWrongDimensions) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  nc::Frame f = make_float_frame(320, 240);  // wrong size for 640x640 model
  auto valid = backend.validate_input(f);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(OnnxInferenceBackend, ValidateInputAcceptsMatchingFrame) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  nc::Frame f = make_float_frame(kDefaultModelWidth, kDefaultModelHeight);
  auto valid = backend.validate_input(f);
  ASSERT_TRUE(valid.has_value());
}

TEST(OnnxInferenceBackend, InferReturnsSaneResult) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  backend.warmup();
  nc::Frame f = make_float_frame(kDefaultModelWidth, kDefaultModelHeight);
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

TEST(OnnxInferenceBackend, WarmupDoesNotThrow) {
  const std::string path = get_test_model_path();
  if (path.empty()) {
    GTEST_SKIP() << "Set NORMITRI_TEST_ONNX_MODEL to run (path to .onnx file)";
  }
  nv::OnnxInferenceBackend backend(path);
  EXPECT_NO_THROW(backend.warmup());
}
