#include <normitri/core/frame.hpp>
#include <cstddef>

namespace normitri::core {

std::size_t Frame::min_bytes(std::uint32_t width,
                              std::uint32_t height,
                              PixelFormat format) {
  const std::size_t pixels = static_cast<std::size_t>(width) * height;
  switch (format) {
    case PixelFormat::Grayscale8:
      return pixels;
    case PixelFormat::RGB8:
    case PixelFormat::BGR8:
      return pixels * 3;
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8:
      return pixels * 4;
    case PixelFormat::Float32Planar:
      return pixels * 3 * sizeof(float);  // CHW, 3 channels
    case PixelFormat::Unknown:
    default:
      return 0;
  }
}

}  // namespace normitri::core
