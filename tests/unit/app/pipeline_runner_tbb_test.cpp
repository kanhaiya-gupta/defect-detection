#ifdef NORMITRI_HAS_TBB

#include <normitri/app/pipeline_runner_tbb.hpp>
#include <normitri/core/defect.hpp>
#include <normitri/core/defect_result.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <normitri/vision/defect_decoder.hpp>
#include <normitri/vision/defect_detection_stage.hpp>
#include <normitri/vision/mock_inference_backend.hpp>
#include <normitri/vision/normalize_stage.hpp>
#include <normitri/vision/resize_stage.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

normitri::core::Pipeline make_mock_pipeline() {
  normitri::core::Pipeline p;
  p.add_stage(std::make_unique<normitri::vision::ResizeStage>(64, 64));
  p.add_stage(std::make_unique<normitri::vision::NormalizeStage>(0.f, 1.f));
  normitri::vision::ClassToDefectKindMap map = {
      normitri::core::DefectKind::WrongItem,
      normitri::core::DefectKind::WrongQuantity,
      normitri::core::DefectKind::ExpiredOrQuality,
      normitri::core::DefectKind::ProcessError,
  };
  auto decoder = std::make_unique<normitri::vision::DefectDecoder>(0.5f, std::move(map));
  auto mock = std::make_unique<normitri::vision::MockInferenceBackend>();
  mock->set_defects({{normitri::core::DefectKind::WrongItem, {0.1f, 0.2f, 0.3f, 0.4f}, 0.9f, std::nullopt, std::nullopt}});
  p.add_stage(std::make_unique<normitri::vision::DefectDetectionStage>(std::move(mock), std::move(decoder), 0));
  return p;
}

normitri::core::Frame make_dummy_frame(std::uint32_t w = 64, std::uint32_t h = 64) {
  const std::size_t bytes = static_cast<std::size_t>(w) * h * 3;
  std::vector<std::byte> buffer(bytes, std::byte{0});
  return normitri::core::Frame(w, h, normitri::core::PixelFormat::RGB8, std::move(buffer));
}

}  // namespace

TEST(PipelineRunnerTbbTest, MultiCameraRunsCallbackPerWorkItem) {
  normitri::core::Pipeline pipeline1 = make_mock_pipeline();
  normitri::core::Pipeline pipeline2 = make_mock_pipeline();
  std::unordered_map<std::string, normitri::core::Pipeline*> pipelines;
  pipelines["cam_1"] = &pipeline1;
  pipelines["cam_2"] = &pipeline2;

  normitri::core::Frame frame1 = make_dummy_frame();
  normitri::core::Frame frame2 = make_dummy_frame();
  std::vector<std::pair<std::string, normitri::core::Frame>> work_items;
  work_items.emplace_back("cam_1", std::move(frame1));
  work_items.emplace_back("cam_2", std::move(frame2));

  std::atomic<std::size_t> call_count{0};
  std::vector<std::string> unit_ids;
  std::mutex mutex;
  normitri::app::run_pipeline_multi_camera_tbb(
      pipelines, work_items,
      [&call_count, &unit_ids, &mutex](const normitri::core::DefectResult& r, const std::string& unit_id) {
        call_count++;
        EXPECT_EQ(r.camera_id.has_value() ? *r.camera_id : "", unit_id);
        std::lock_guard lock(mutex);
        unit_ids.push_back(unit_id);
      });
  EXPECT_EQ(call_count.load(), 2u);
  EXPECT_EQ(unit_ids.size(), 2u);
  EXPECT_TRUE(std::find(unit_ids.begin(), unit_ids.end(), "cam_1") != unit_ids.end());
  EXPECT_TRUE(std::find(unit_ids.begin(), unit_ids.end(), "cam_2") != unit_ids.end());
}

TEST(PipelineRunnerTbbTest, EmptyWorkItemsDoesNotCallCallback) {
  normitri::core::Pipeline p = make_mock_pipeline();
  std::unordered_map<std::string, normitri::core::Pipeline*> pipelines;
  pipelines["cam_1"] = &p;
  std::vector<std::pair<std::string, normitri::core::Frame>> work_items;
  std::atomic<std::size_t> calls{0};
  normitri::app::run_pipeline_multi_camera_tbb(
      pipelines, work_items,
      [&calls](const normitri::core::DefectResult&, const std::string&) { calls++; });
  EXPECT_EQ(calls.load(), 0u);
}

#endif  // NORMITRI_HAS_TBB
