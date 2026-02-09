#include <normitri/vision/defect_decoder.hpp>
#include <normitri/core/defect.hpp>
#include <algorithm>
#include <cstddef>

namespace normitri::vision {

DefectDecoder::DefectDecoder(float confidence_threshold,
                             ClassToDefectKindMap class_to_kind)
    : confidence_threshold_(confidence_threshold),
      class_to_kind_(std::move(class_to_kind)) {}

std::vector<normitri::core::Defect> DefectDecoder::decode(
    const InferenceResult& result) const {
  std::vector<normitri::core::Defect> out;
  const std::size_t n = static_cast<std::size_t>(result.num_detections);
  const std::size_t max_class =
      class_to_kind_.empty() ? 0 : class_to_kind_.size() - 1;

  for (std::size_t i = 0; i < n; ++i) {
    const float score =
        i < result.scores.size() ? result.scores[i] : 0.f;
    if (score < confidence_threshold_) {
      continue;
    }

    normitri::core::Defect d;
    d.confidence = score;
    d.kind = normitri::core::DefectKind::ProcessError;
    if (i < result.class_ids.size()) {
      const auto cid = result.class_ids[i];
      if (cid >= 0 && static_cast<std::size_t>(cid) <= max_class) {
        d.kind = class_to_kind_[static_cast<std::size_t>(cid)];
      }
    }
    if (i * 4 + 3 < result.boxes.size()) {
      d.bbox.x = result.boxes[i * 4 + 0];
      d.bbox.y = result.boxes[i * 4 + 1];
      d.bbox.w = result.boxes[i * 4 + 2] - d.bbox.x;
      d.bbox.h = result.boxes[i * 4 + 3] - d.bbox.y;
    }
    out.push_back(std::move(d));
  }
  return out;
}

}  // namespace normitri::vision
