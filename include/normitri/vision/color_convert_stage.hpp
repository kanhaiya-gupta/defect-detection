#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <normitri/core/error.hpp>
#include <expected>

namespace normitri::vision {

/// Converts between pixel formats (e.g. BGR -> RGB).
class ColorConvertStage : public normitri::core::IPipelineStage {
 public:
  explicit ColorConvertStage(normitri::core::PixelFormat output_format);

  [[nodiscard]] std::expected<normitri::core::StageOutput,
                              normitri::core::PipelineError>
  process(const normitri::core::Frame& input) override;

 private:
  normitri::core::PixelFormat output_format_;
};

}  // namespace normitri::vision
