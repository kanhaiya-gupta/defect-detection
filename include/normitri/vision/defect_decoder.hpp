#pragma once

#include <normitri/core/defect.hpp>
#include <normitri/vision/inference_result.hpp>
#include <cstdint>
#include <vector>

namespace normitri::vision {

/// Maps model class id to DefectKind.
using ClassToDefectKindMap = std::vector<normitri::core::DefectKind>;

/// Decodes InferenceResult -> vector<Defect> with confidence threshold and optional NMS.
class DefectDecoder {
 public:
  DefectDecoder(float confidence_threshold,
                ClassToDefectKindMap class_to_kind);

  [[nodiscard]] std::vector<normitri::core::Defect> decode(
      const InferenceResult& result) const;

  void set_confidence_threshold(float t) noexcept { confidence_threshold_ = t; }
  [[nodiscard]] float confidence_threshold() const noexcept {
    return confidence_threshold_;
  }

 private:
  float confidence_threshold_;
  ClassToDefectKindMap class_to_kind_;
};

}  // namespace normitri::vision
