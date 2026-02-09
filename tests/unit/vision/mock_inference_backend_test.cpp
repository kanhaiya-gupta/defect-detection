#include <normitri/core/defect.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/mock_inference_backend.hpp>
#include <gtest/gtest.h>
#include <vector>

namespace nv = normitri::vision;
namespace nc = normitri::core;

TEST(MockInferenceBackend, ReturnsSetDefects) {
  nv::MockInferenceBackend mock;
  mock.set_defects({
      {nc::DefectKind::WrongItem, {0.1f, 0.2f, 0.3f, 0.4f}, 0.95f, std::nullopt, std::nullopt},
      {nc::DefectKind::WrongQuantity, {}, 0.8f, 123, std::nullopt},
  });
  std::vector<std::byte> buf(100);
  nc::Frame f(10, 10, nc::PixelFormat::RGB8, std::move(buf));
  auto result = mock.infer(f);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->num_detections, 2u);
  EXPECT_EQ(result->scores.size(), 2u);
  EXPECT_EQ(result->boxes.size(), 8u);
  EXPECT_FLOAT_EQ(result->scores[0], 0.95f);
  EXPECT_FLOAT_EQ(result->scores[1], 0.8f);
}

TEST(MockInferenceBackend, ValidateInputRejectsEmptyFrame) {
  nv::MockInferenceBackend mock;
  nc::Frame empty;
  auto valid = mock.validate_input(empty);
  ASSERT_FALSE(valid.has_value());
  EXPECT_EQ(valid.error(), nc::PipelineError::InvalidFrame);
}

TEST(MockInferenceBackend, ValidateInputAcceptsNonEmptyFrame) {
  nv::MockInferenceBackend mock;
  std::vector<std::byte> buf(10);
  nc::Frame f(1, 10, nc::PixelFormat::RGB8, std::move(buf));
  auto valid = mock.validate_input(f);
  ASSERT_TRUE(valid.has_value());
}

TEST(MockInferenceBackend, InferBatchReturnsOneResultPerFrame) {
  nv::MockInferenceBackend mock;
  mock.set_defects({{nc::DefectKind::WrongItem, {}, 0.9f, std::nullopt, std::nullopt}});
  std::vector<nc::Frame> frames;
  for (int i = 0; i < 3; ++i) {
    std::vector<std::byte> buf(64);
    frames.emplace_back(8, 8, nc::PixelFormat::RGB8, std::move(buf));
  }
  auto results = mock.infer_batch(frames);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(results->size(), 3u);
  for (const auto& r : *results) {
    EXPECT_EQ(r.num_detections, 1u);
    EXPECT_FLOAT_EQ(r.scores[0], 0.9f);
  }
}

TEST(MockInferenceBackend, InferBatchFailsOnEmptyFrame) {
  nv::MockInferenceBackend mock;
  mock.set_defects({});
  std::vector<nc::Frame> frames;
  frames.push_back(nc::Frame{});  // empty
  auto results = mock.infer_batch(frames);
  ASSERT_FALSE(results.has_value());
  EXPECT_EQ(results.error(), nc::PipelineError::InvalidFrame);
}
