#include <normitri/vision/color_convert_stage.hpp>
#include "frame_cv_utils.hpp"
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace normitri::vision {

ColorConvertStage::ColorConvertStage(
    normitri::core::PixelFormat output_format)
    : output_format_(output_format) {}

std::expected<normitri::core::StageOutput, normitri::core::PipelineError>
ColorConvertStage::process(const normitri::core::Frame& input) {
  if (input.empty()) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }

  using namespace normitri::core;

  auto mat_in = detail::frame_to_mat(input);
  if (!mat_in) {
    return std::unexpected(PipelineError::InvalidFrame);
  }

  if (input.format() == output_format_) {
    std::vector<std::byte> buf(input.data().begin(), input.data().end());
    return StageOutput{
        Frame(input.width(), input.height(), output_format_, std::move(buf))};
  }

  cv::Mat mat_out;
  int code = -1;
  if (input.format() == PixelFormat::BGR8 && output_format_ == PixelFormat::RGB8) {
    code = cv::COLOR_BGR2RGB;
  } else if (input.format() == PixelFormat::RGB8 && output_format_ == PixelFormat::BGR8) {
    code = cv::COLOR_RGB2BGR;
  } else if (input.format() == PixelFormat::BGRA8 && output_format_ == PixelFormat::RGBA8) {
    code = cv::COLOR_BGRA2RGBA;
  } else if (input.format() == PixelFormat::RGBA8 && output_format_ == PixelFormat::BGRA8) {
    code = cv::COLOR_RGBA2BGRA;
  } else if (input.format() == PixelFormat::Grayscale8 && output_format_ == PixelFormat::RGB8) {
    code = cv::COLOR_GRAY2RGB;
  } else if (input.format() == PixelFormat::RGB8 && output_format_ == PixelFormat::Grayscale8) {
    code = cv::COLOR_RGB2GRAY;
  } else if (input.format() == PixelFormat::BGR8 && output_format_ == PixelFormat::Grayscale8) {
    code = cv::COLOR_BGR2GRAY;
  } else if (input.format() == PixelFormat::Grayscale8 && output_format_ == PixelFormat::BGR8) {
    code = cv::COLOR_GRAY2BGR;
  }

  if (code >= 0) {
    cv::cvtColor(*mat_in, mat_out, code);
    Frame out = detail::mat_to_frame(mat_out, output_format_);
    return StageOutput{std::move(out)};
  }

  std::vector<std::byte> buffer(input.data().begin(), input.data().end());
  return StageOutput{
      Frame(input.width(), input.height(), output_format_, std::move(buffer))};
}

}  // namespace normitri::vision
