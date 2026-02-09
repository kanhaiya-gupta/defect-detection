#pragma once

#include <normitri/vision/inference_backend.hpp>
#include <normitri/core/defect.hpp>
#include <vector>

namespace normitri::vision {

/// Mock backend that returns configurable synthetic defects (for tests/demo).
class MockInferenceBackend : public IInferenceBackend {
 public:
  /// Set defects to return on next infer() / infer_batch() call(s).
  void set_defects(std::vector<normitri::core::Defect> defects);

  [[nodiscard]] std::expected<InferenceResult, normitri::core::PipelineError>
  infer(const normitri::core::Frame& input) override;

  [[nodiscard]] std::expected<void, normitri::core::PipelineError>
  validate_input(const normitri::core::Frame& input) const override;

  [[nodiscard]] std::expected<std::vector<InferenceResult>, normitri::core::PipelineError>
  infer_batch(std::span<const normitri::core::Frame> inputs) override;

 private:
  std::vector<normitri::core::Defect> defects_to_return_;
};

}  // namespace normitri::vision
