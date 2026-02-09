#include <normitri/vision/inference_backend.hpp>
#include <normitri/core/error.hpp>
#include <span>
#include <vector>

namespace normitri::vision {

std::expected<std::vector<InferenceResult>, normitri::core::PipelineError>
IInferenceBackend::infer_batch(std::span<const normitri::core::Frame> inputs) {
  std::vector<InferenceResult> results;
  results.reserve(inputs.size());
  for (const auto& frame : inputs) {
    auto single = infer(frame);
    if (!single) {
      return std::unexpected(single.error());
    }
    results.push_back(std::move(*single));
  }
  return results;
}

}  // namespace normitri::vision
