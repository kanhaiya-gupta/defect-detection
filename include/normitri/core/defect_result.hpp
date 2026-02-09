#pragma once

#include <normitri/core/defect.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace normitri::core {

/// Result of the defect-detection pipeline for one or more frames.
struct DefectResult {
  std::uint64_t frame_id{0};
  std::vector<Defect> defects;
  std::string metadata;  // optional JSON or free-form
};

}  // namespace normitri::core
