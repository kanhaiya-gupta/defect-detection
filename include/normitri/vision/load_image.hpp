#pragma once

#include <normitri/core/frame.hpp>
#include <optional>
#include <string>

namespace normitri::vision {

/// Load an image file into a Frame (BGR8 or Grayscale8). Returns nullopt on failure.
std::optional<normitri::core::Frame> load_frame_from_image(const std::string& path);

}  // namespace normitri::vision
