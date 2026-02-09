#include <normitri/vision/defect_detection_stage.hpp>
#include <normitri/core/defect_result.hpp>

namespace normitri::vision {

DefectDetectionStage::DefectDetectionStage(
    std::unique_ptr<IInferenceBackend> backend,
    DefectDecoder decoder,
    std::uint64_t frame_id)
    : backend_(std::move(backend)),
      decoder_(std::move(decoder)),
      frame_id_(frame_id) {}

std::expected<normitri::core::StageOutput, normitri::core::PipelineError>
DefectDetectionStage::process(const normitri::core::Frame& input) {
  auto valid = backend_->validate_input(input);
  if (!valid) {
    return std::unexpected(valid.error());
  }
  auto result = backend_->infer(input);
  if (!result) {
    return std::unexpected(result.error());
  }

  normitri::core::DefectResult out;
  out.frame_id = frame_id_;
  out.defects = decoder_.decode(*result);
  return normitri::core::StageOutput{std::move(out)};
}

}  // namespace normitri::vision
