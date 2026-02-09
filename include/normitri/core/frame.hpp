#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace normitri::core {

/// Memory: Frame owns a single contiguous buffer (std::vector<std::byte>);
/// move semantics and RAII throughout. Use data() for std::span views (non-owning).
/// Thread-safety: distinct Frame instances are independent; sharing one Frame
/// across threads requires external synchronization.

/// Pixel layout / format.
enum class PixelFormat : std::uint8_t {
  Unknown,
  Grayscale8,
  RGB8,
  BGR8,
  RGBA8,
  BGRA8,
  Float32Planar,  // e.g. CHW float for inference
};

/// Single image or video frame: dimensions, format, and buffer (owned or view).
class Frame {
 public:
  Frame() = default;

  Frame(std::uint32_t width,
        std::uint32_t height,
        PixelFormat format,
        std::vector<std::byte> buffer)
      : width_(width),
        height_(height),
        format_(format),
        buffer_(std::move(buffer)) {}

  [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
  [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
  [[nodiscard]] PixelFormat format() const noexcept { return format_; }

  /// Mutable view of the buffer (owned).
  [[nodiscard]] std::span<std::byte> data() noexcept {
    return std::span<std::byte>(buffer_.data(), buffer_.size());
  }
  [[nodiscard]] std::span<const std::byte> data() const noexcept {
    return std::span<const std::byte>(buffer_.data(), buffer_.size());
  }

  [[nodiscard]] bool empty() const noexcept { return buffer_.empty(); }
  [[nodiscard]] std::size_t size_bytes() const noexcept { return buffer_.size(); }

  /// Minimum bytes required for given dimensions and format (for validation).
  [[nodiscard]] static std::size_t min_bytes(std::uint32_t width,
                                             std::uint32_t height,
                                             PixelFormat format);

 private:
  std::uint32_t width_{0};
  std::uint32_t height_{0};
  PixelFormat format_{PixelFormat::Unknown};
  std::vector<std::byte> buffer_;
};

}  // namespace normitri::core
