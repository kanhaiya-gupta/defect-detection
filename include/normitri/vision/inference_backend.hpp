#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/inference_result.hpp>
#include <expected>
#include <span>
#include <vector>

namespace normitri::vision {

/// Expected input format and lifecycle: see docs/inference-contract.md.

/// Abstract inference backend: Frame -> InferenceResult.
/// Implement infer(); optionally override validate_input, infer_batch, warmup.
class IInferenceBackend {
 public:
  virtual ~IInferenceBackend() = default;

  /// Single-frame inference. Must be implemented.
  [[nodiscard]] virtual std::expected<InferenceResult, normitri::core::PipelineError>
  infer(const normitri::core::Frame& input) = 0;

  /// Optional: validate frame format/dimensions before infer. Default: accept.
  [[nodiscard]] virtual std::expected<void, normitri::core::PipelineError>
  validate_input(const normitri::core::Frame& /*input*/) const {
    return {};
  }

  /// Optional: batch inference. Default: loop over infer(). Override for GPU batching.
  [[nodiscard]] virtual std::expected<std::vector<InferenceResult>, normitri::core::PipelineError>
  infer_batch(std::span<const normitri::core::Frame> inputs);

  /// Optional: warmup run (e.g. dummy inference). Call once after construction. Default: no-op.
  virtual void warmup() {}
};

}  // namespace normitri::vision
