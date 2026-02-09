#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <expected>

namespace normitri::vision {

/// Normalizes pixel values (e.g. mean/scale for neural network input).
class NormalizeStage : public normitri::core::IPipelineStage {
 public:
  NormalizeStage(float mean, float scale);

  [[nodiscard]] std::expected<normitri::core::StageOutput,
                              normitri::core::PipelineError>
  process(const normitri::core::Frame& input) override;

 private:
  float mean_;
  float scale_;
};

}  // namespace normitri::vision
