#include <normitri/vision/normalize_stage.hpp>
#include "frame_cv_utils.hpp"
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace normitri::vision {

NormalizeStage::NormalizeStage(float mean, float scale)
    : mean_(mean), scale_(scale) {}

std::expected<normitri::core::StageOutput, normitri::core::PipelineError>
NormalizeStage::process(const normitri::core::Frame& input) {
  if (input.empty()) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }

  using namespace normitri::core;

  auto mat_in = detail::frame_to_mat(input);
  if (!mat_in) {
    return std::unexpected(PipelineError::InvalidFrame);
  }

  cv::Mat mat_float;
  mat_in->convertTo(mat_float, CV_32FC(mat_in->channels()), scale_, -mean_ * scale_);

  const std::size_t len = mat_float.total() * mat_float.elemSize();
  std::vector<std::byte> buffer(len);
  std::memcpy(buffer.data(), mat_float.ptr(), len);
  Frame out(static_cast<std::uint32_t>(mat_float.cols),
            static_cast<std::uint32_t>(mat_float.rows),
            PixelFormat::Float32Planar,
            std::move(buffer));
  return StageOutput{std::move(out)};
}

}  // namespace normitri::vision
