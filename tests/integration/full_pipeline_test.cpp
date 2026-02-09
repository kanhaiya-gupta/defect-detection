#include <normitri/app/config.hpp>
#include <normitri/app/pipeline_runner.hpp>
#include <normitri/core/defect.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <normitri/vision/defect_decoder.hpp>
#include <normitri/vision/defect_detection_stage.hpp>
#include <normitri/vision/mock_inference_backend.hpp>
#include <normitri/vision/normalize_stage.hpp>
#include <normitri/vision/resize_stage.hpp>
#include <gtest/gtest.h>
#include <mutex>
#include <memory>
#include <vector>

namespace {

using namespace normitri::core;
using namespace normitri::vision;
using namespace normitri::app;

Pipeline build_demo_pipeline() {
  Pipeline p;
  p.add_stage(std::make_unique<ResizeStage>(64, 64));
  p.add_stage(std::make_unique<NormalizeStage>(0.f, 1.f / 255.f));
  ClassToDefectKindMap map = {
      DefectKind::WrongItem,
      DefectKind::WrongQuantity,
      DefectKind::ExpiredOrQuality,
      DefectKind::ProcessError,
  };
  DefectDecoder decoder(0.3f, std::move(map));
  auto mock = std::make_unique<MockInferenceBackend>();
  mock->set_defects({{DefectKind::WrongItem, {}, 0.99f, std::nullopt, std::nullopt}});
  p.add_stage(std::make_unique<DefectDetectionStage>(
      std::move(mock), std::move(decoder), 42));
  return p;
}

}  // namespace

TEST(FullPipeline, ResizeNormalizeDefectDetection) {
  Pipeline pipeline = build_demo_pipeline();
  std::vector<std::byte> buf(320 * 240 * 3);
  Frame frame(320, 240, PixelFormat::RGB8, std::move(buf));

  auto result = run_pipeline(pipeline, frame);
  ASSERT_TRUE(result.has_value()) << "Pipeline run failed";
  EXPECT_EQ(result->frame_id, 42u);
  ASSERT_EQ(result->defects.size(), 1u);
  EXPECT_EQ(result->defects[0].kind, DefectKind::WrongItem);
  EXPECT_FLOAT_EQ(result->defects[0].confidence, 0.99f);
}

TEST(FullPipeline, BatchParallel) {
  Pipeline pipeline = build_demo_pipeline();
  std::vector<Frame> frames;
  for (int i = 0; i < 8; ++i) {
    std::vector<std::byte> buf(64 * 64 * 3);
    frames.emplace_back(64, 64, PixelFormat::RGB8, std::move(buf));
  }

  std::vector<DefectResult> results;
  std::mutex results_mutex;
  run_pipeline_batch_parallel(
      pipeline, frames,
      [&](const DefectResult& r) {
        std::lock_guard lock(results_mutex);
        results.push_back(r);
      },
      2);

  EXPECT_EQ(results.size(), 8u);
  for (const auto& r : results) {
    EXPECT_EQ(r.defects.size(), 1u);
    EXPECT_EQ(r.defects[0].kind, DefectKind::WrongItem);
  }
}

TEST(FullPipeline, StageTimingCallbackInvoked) {
  Pipeline pipeline = build_demo_pipeline();
  std::vector<std::byte> buf(64 * 64 * 3);
  Frame frame(64, 64, PixelFormat::RGB8, std::move(buf));

  std::vector<std::pair<std::size_t, double>> timings;
  StageTimingCallback timing_cb = [&](std::size_t idx, double ms) {
    timings.emplace_back(idx, ms);
  };

  auto result = run_pipeline(pipeline, frame, &timing_cb);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(timings.size(), 3u);  // resize, normalize, defect_detection
  for (std::size_t i = 0; i < timings.size(); ++i) {
    EXPECT_EQ(timings[i].first, i);
    EXPECT_GE(timings[i].second, 0.0);
  }
}
