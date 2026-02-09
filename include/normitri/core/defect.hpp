#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace normitri::core {

/// Defect kind for shopping-item defect detection.
enum class DefectKind : std::uint8_t {
  WrongItem,
  WrongQuantity,
  ExpiredOrQuality,
  ProcessError,  // e.g. not scanned, barcode switch
};

/// Axis-aligned bounding box (normalized 0–1 or pixel coords).
struct BBox {
  float x{0.f};
  float y{0.f};
  float w{0.f};
  float h{0.f};
};

/// Single detected defect: kind, location, confidence, optional product/category.
struct Defect {
  DefectKind kind{DefectKind::ProcessError};
  BBox bbox{};
  float confidence{0.f};
  std::optional<std::uint64_t> product_id;
  std::optional<std::string> category;  // or product_group for reporting
};

}  // namespace normitri::core
