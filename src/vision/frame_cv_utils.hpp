#pragma once

#include <normitri/core/frame.hpp>
#include <opencv2/core/mat.hpp>
#include <optional>

namespace normitri::vision::detail {

/// Convert Frame to cv::Mat (shared view or copy). Returns nullopt if format unsupported.
std::optional<cv::Mat> frame_to_mat(const normitri::core::Frame& frame);

/// Convert cv::Mat to Frame (copy).
normitri::core::Frame mat_to_frame(const cv::Mat& mat,
                                    normitri::core::PixelFormat format);

}  // namespace normitri::vision::detail
