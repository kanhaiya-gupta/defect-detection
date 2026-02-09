#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <cstdint>
#include <expected>
#include <memory>

namespace normitri::vision {

/// Resizes input frame to a fixed size (e.g. inference input size).
class ResizeStage : public normitri::core::IPipelineStage {
 public:
  ResizeStage(std::uint32_t target_width, std::uint32_t target_height);

  [[nodiscard]] std::expected<normitri::core::StageOutput,
                              normitri::core::PipelineError>
  process(const normitri::core::Frame& input) override;

 private:
  std::uint32_t target_width_;
  std::uint32_t target_height_;
};

}  // namespace normitri::vision
