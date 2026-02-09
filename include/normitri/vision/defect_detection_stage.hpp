#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <normitri/vision/defect_decoder.hpp>
#include <normitri/vision/inference_backend.hpp>
#include <expected>
#include <memory>
#include <cstdint>

namespace normitri::vision {

/// Pipeline stage: run inference backend + decoder -> DefectResult.
class DefectDetectionStage : public normitri::core::IPipelineStage {
 public:
  DefectDetectionStage(std::unique_ptr<IInferenceBackend> backend,
                       DefectDecoder decoder,
                       std::uint64_t frame_id = 0);

  [[nodiscard]] std::expected<normitri::core::StageOutput,
                              normitri::core::PipelineError>
  process(const normitri::core::Frame& input) override;

  void set_frame_id(std::uint64_t id) noexcept { frame_id_ = id; }

 private:
  std::unique_ptr<IInferenceBackend> backend_;
  DefectDecoder decoder_;
  std::uint64_t frame_id_;
};

}  // namespace normitri::vision
