#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/inference_backend.hpp>
#include <normitri/vision/inference_result.hpp>
#include <memory>
#include <string>

namespace normitri::vision {

/// TensorRT inference backend: loads a serialized engine (.engine) and implements IInferenceBackend.
///
/// Expected engine: built from a detection model with one input (float image, NCHW) and either:
/// - **One output (YOLO-style)**: [1, N, 6] or [1, 6, N] (xmin, ymin, xmax, ymax, score, class_id).
/// - **Three outputs**: boxes [1,N,4], scores [1,N], class_ids [1,N] (or equivalent layouts).
/// Input contract: Frame must be Float32Planar, HWC, dimensions matching the engine input
/// (e.g. 640×640). See docs/inference-contract.md. The backend copies HWC to NCHW and to GPU.
///
/// Requires CUDA and TensorRT at build time; build with -DNORMITRI_USE_TENSORRT=ON and
/// TensorRT/CUDA installed. At runtime, a GPU is required.
class TensorRTInferenceBackend : public IInferenceBackend {
 public:
  /// \param engine_path Path to the serialized TensorRT engine file (.engine).
  explicit TensorRTInferenceBackend(std::string engine_path);

  ~TensorRTInferenceBackend() override;

  TensorRTInferenceBackend(const TensorRTInferenceBackend&) = delete;
  TensorRTInferenceBackend& operator=(const TensorRTInferenceBackend&) = delete;

  [[nodiscard]] std::expected<InferenceResult, normitri::core::PipelineError>
  infer(const normitri::core::Frame& input) override;

  [[nodiscard]] std::expected<void, normitri::core::PipelineError>
  validate_input(const normitri::core::Frame& input) const override;

  void warmup() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace normitri::vision
