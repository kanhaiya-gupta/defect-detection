#include "frame_cv_utils.hpp"
#include <normitri/core/frame.hpp>
#include <opencv2/core.hpp>
#include <cstddef>
#include <vector>

namespace normitri::vision::detail {

namespace nc = normitri::core;

std::optional<cv::Mat> frame_to_mat(const nc::Frame& frame) {
  if (frame.empty()) return std::nullopt;

  const int w = static_cast<int>(frame.width());
  const int h = static_cast<int>(frame.height());
  const std::size_t step = frame.size_bytes() / static_cast<std::size_t>(h);

  switch (frame.format()) {
    case nc::PixelFormat::Grayscale8:
      return cv::Mat(h, w, CV_8UC1, const_cast<std::byte*>(frame.data().data()),
                     step);
    case nc::PixelFormat::RGB8:
      return cv::Mat(h, w, CV_8UC3, const_cast<std::byte*>(frame.data().data()),
                     step);
    case nc::PixelFormat::BGR8:
      return cv::Mat(h, w, CV_8UC3, const_cast<std::byte*>(frame.data().data()),
                     step);
    case nc::PixelFormat::RGBA8:
      return cv::Mat(h, w, CV_8UC4, const_cast<std::byte*>(frame.data().data()),
                     step);
    case nc::PixelFormat::BGRA8:
      return cv::Mat(h, w, CV_8UC4, const_cast<std::byte*>(frame.data().data()),
                     step);
    case nc::PixelFormat::Float32Planar:
    case nc::PixelFormat::Unknown:
    default:
      return std::nullopt;
  }
}

nc::Frame mat_to_frame(const cv::Mat& mat, nc::PixelFormat format) {
  if (mat.empty()) return nc::Frame();

  const std::uint32_t w = static_cast<std::uint32_t>(mat.cols);
  const std::uint32_t h = static_cast<std::uint32_t>(mat.rows);
  const std::size_t len = mat.total() * mat.elemSize();
  std::vector<std::byte> buffer(len);
  std::memcpy(buffer.data(), mat.ptr(), len);
  return nc::Frame(w, h, format, std::move(buffer));
}

}  // namespace normitri::vision::detail
