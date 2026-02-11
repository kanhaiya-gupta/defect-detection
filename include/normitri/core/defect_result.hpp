#pragma once

#include <normitri/core/defect.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace normitri::core {

/// Result of the defect-detection pipeline for one or more frames.
/// Optional camera_id and customer_id identify which camera and customer this result belongs to
/// (set by the application or by run_pipeline when context is provided).
struct DefectResult {
  std::uint64_t frame_id{0};
  std::vector<Defect> defects;
  std::string metadata;  // optional JSON or free-form

  /// Which camera produced this frame (e.g. "cam_1", "lane_3"). Set by app or pipeline runner.
  std::optional<std::string> camera_id;
  /// Which customer this result relates to (e.g. "cust_42", checkout lane id). Set by app or pipeline runner.
  std::optional<std::string> customer_id;
};

}  // namespace normitri::core
