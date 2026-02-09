#include <normitri/core/defect_result.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace nc = normitri::core;

namespace {

class PassThroughStage : public nc::IPipelineStage {
 public:
  std::expected<nc::StageOutput, nc::PipelineError> process(
      const nc::Frame& input) override {
    std::vector<std::byte> buf(input.data().begin(), input.data().end());
    return nc::StageOutput{nc::Frame(input.width(), input.height(),
                                      input.format(), std::move(buf))};
  }
};

class EmitDefectResultStage : public nc::IPipelineStage {
 public:
  std::expected<nc::StageOutput, nc::PipelineError> process(
      const nc::Frame&) override {
    nc::DefectResult r;
    r.frame_id = 1;
    r.defects.push_back({nc::DefectKind::WrongItem, {}, 0.9f, std::nullopt, std::nullopt});
    return nc::StageOutput{std::move(r)};
  }
};

}  // namespace

TEST(Pipeline, EmptyPipelineReturnsError) {
  nc::Pipeline p;
  std::vector<std::byte> buf(10);
  nc::Frame f(1, 1, nc::PixelFormat::Grayscale8, std::move(buf));
  auto result = p.run(f);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), nc::PipelineError::InvalidConfig);
}

TEST(Pipeline, SingleStageEmitResult) {
  nc::Pipeline p;
  p.add_stage(std::make_unique<EmitDefectResultStage>());
  std::vector<std::byte> buf(10);
  nc::Frame f(1, 1, nc::PixelFormat::Grayscale8, std::move(buf));
  auto result = p.run(f);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->frame_id, 1u);
  EXPECT_EQ(result->defects.size(), 1u);
  EXPECT_EQ(result->defects[0].kind, nc::DefectKind::WrongItem);
  EXPECT_FLOAT_EQ(result->defects[0].confidence, 0.9f);
}

TEST(Pipeline, PassThroughThenEmit) {
  nc::Pipeline p;
  p.add_stage(std::make_unique<PassThroughStage>());
  p.add_stage(std::make_unique<EmitDefectResultStage>());
  std::vector<std::byte> buf(10);
  nc::Frame f(1, 1, nc::PixelFormat::Grayscale8, std::move(buf));
  auto result = p.run(f);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->defects.size(), 1u);
}
