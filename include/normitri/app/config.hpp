#pragma once

#include <normitri/core/defect.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace normitri::app {

/// Inference backend type: mock (synthetic) or onnx (real model).
enum class InferenceBackendType {
  Mock,
  Onnx,
};

/// Pipeline configuration: model path, stages, thresholds.
struct PipelineConfig {
  std::string model_path;
  InferenceBackendType backend_type{InferenceBackendType::Mock};
  std::uint32_t resize_width{640};
  std::uint32_t resize_height{640};
  float normalize_mean{0.f};
  float normalize_scale{1.f};
  float confidence_threshold{0.5f};
  std::vector<std::string> high_value_categories;  // for alerting prioritisation
};

/// Load config from a simple key=value file (one per line) or use defaults.
PipelineConfig load_config(const std::string& path);

/// Default config when no file is provided.
PipelineConfig default_config();

}  // namespace normitri::app
