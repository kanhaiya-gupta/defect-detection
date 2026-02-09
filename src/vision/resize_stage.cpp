#include <normitri/vision/resize_stage.hpp>
#include "frame_cv_utils.hpp"
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace normitri::vision {

ResizeStage::ResizeStage(std::uint32_t target_width,
                         std::uint32_t target_height)
    : target_width_(target_width), target_height_(target_height) {}

std::expected<normitri::core::StageOutput, normitri::core::PipelineError>
ResizeStage::process(const normitri::core::Frame& input) {
  if (input.empty()) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }

  using namespace normitri::core;

  auto mat_in = detail::frame_to_mat(input);
  if (!mat_in) {
    return std::unexpected(PipelineError::InvalidFrame);
  }

  if (input.width() == target_width_ && input.height() == target_height_) {
    std::vector<std::byte> buf(input.data().begin(), input.data().end());
    return StageOutput{
        Frame(input.width(), input.height(), input.format(), std::move(buf))};
  }

  cv::Mat mat_out;
  cv::resize(*mat_in, mat_out,
             cv::Size(static_cast<int>(target_width_),
                      static_cast<int>(target_height_)),
             0, 0, cv::INTER_LINEAR);

  Frame out = detail::mat_to_frame(mat_out, input.format());
  return StageOutput{std::move(out)};
}

}  // namespace normitri::vision
