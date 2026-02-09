#pragma once

#include <cstdint>
#include <vector>

namespace normitri::vision {

/// Raw model output (boxes, class scores, etc.) before decoding to Defect list.
struct InferenceResult {
  std::vector<float> boxes;   // e.g. [x1,y1,x2,y2] per detection
  std::vector<float> scores;
  std::vector<std::int64_t> class_ids;
  std::uint32_t num_detections{0};
};

}  // namespace normitri::vision
