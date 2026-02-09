#include <normitri/vision/mock_inference_backend.hpp>
#include <normitri/core/defect.hpp>
#include <normitri/core/error.hpp>
#include <cstdint>
#include <span>
#include <vector>

namespace normitri::vision {

void MockInferenceBackend::set_defects(
    std::vector<normitri::core::Defect> defects) {
  defects_to_return_ = std::move(defects);
}

static InferenceResult mock_to_result(
    const std::vector<normitri::core::Defect>& defects) {
  InferenceResult r;
  r.num_detections = static_cast<std::uint32_t>(defects.size());
  for (const auto& d : defects) {
    r.boxes.push_back(d.bbox.x);
    r.boxes.push_back(d.bbox.y);
    r.boxes.push_back(d.bbox.x + d.bbox.w);
    r.boxes.push_back(d.bbox.y + d.bbox.h);
    r.scores.push_back(d.confidence);
    r.class_ids.push_back(static_cast<std::int64_t>(static_cast<std::uint8_t>(d.kind)));
  }
  return r;
}

std::expected<InferenceResult, normitri::core::PipelineError>
MockInferenceBackend::infer(const normitri::core::Frame& /*input*/) {
  return mock_to_result(defects_to_return_);
}

std::expected<void, normitri::core::PipelineError>
MockInferenceBackend::validate_input(const normitri::core::Frame& input) const {
  if (input.empty()) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }
  return {};
}

std::expected<std::vector<InferenceResult>, normitri::core::PipelineError>
MockInferenceBackend::infer_batch(std::span<const normitri::core::Frame> inputs) {
  std::vector<InferenceResult> results;
  results.reserve(inputs.size());
  InferenceResult one = mock_to_result(defects_to_return_);
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    if (inputs[i].empty()) {
      return std::unexpected(normitri::core::PipelineError::InvalidFrame);
    }
    results.push_back(one);
  }
  return results;
}

}  // namespace normitri::vision
