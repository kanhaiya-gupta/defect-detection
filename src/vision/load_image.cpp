#include <normitri/vision/load_image.hpp>
#include "frame_cv_utils.hpp"
#include <normitri/core/frame.hpp>
#include <opencv2/imgcodecs.hpp>

namespace normitri::vision {

std::optional<normitri::core::Frame> load_frame_from_image(const std::string& path) {
  cv::Mat mat = cv::imread(path);
  if (mat.empty()) return std::nullopt;

  normitri::core::PixelFormat format = normitri::core::PixelFormat::BGR8;
  if (mat.channels() == 1) format = normitri::core::PixelFormat::Grayscale8;

  return detail::mat_to_frame(mat, format);
}

}  // namespace normitri::vision
