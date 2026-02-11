#pragma once

#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/vision/inference_backend.hpp>
#include <normitri/vision/inference_result.hpp>
#include <array>
#include <memory>
#include <string>

namespace normitri::vision {

/// ONNX Runtime inference backend: loads an ONNX model and implements IInferenceBackend.
///
/// Expected model: detection model with one input (float image) and either:
/// - **Three outputs**: boxes [1,N,4], scores [1,N], class_ids [1,N] (e.g. TF OD API / SSD style),
///   or layouts [1,4,N] / [1,N] for boxes.
/// - **One output (YOLO-style)**: single tensor [1, N, 6] or [1, 6, N] with (xmin, ymin, xmax, ymax, score, class_id)
///   per detection (e.g. onnx-community YOLOv10).
/// Output names are configurable via constructor; if empty, the first output(s) are used.
///
/// Input contract: Frame must be Float32Planar, HWC, dimensions matching model input
/// (e.g. 640x640). See docs/inference-contract.md. If the model expects NCHW, the
/// backend transposes from HWC when copying to the input tensor.
class OnnxInferenceBackend : public IInferenceBackend {
 public:
  /// \param model_path Path to the .onnx model file.
  /// \param input_name Optional input tensor name; if empty, the first input is used.
  /// \param output_names Optional {boxes, scores, class_ids}; if any empty, names are
  ///        inferred from the model (first three outputs in order).
  OnnxInferenceBackend(std::string model_path,
                       std::string input_name = {},
                       std::array<std::string, 3> output_names = {});

  ~OnnxInferenceBackend() override;

  OnnxInferenceBackend(const OnnxInferenceBackend&) = delete;
  OnnxInferenceBackend& operator=(const OnnxInferenceBackend&) = delete;

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
