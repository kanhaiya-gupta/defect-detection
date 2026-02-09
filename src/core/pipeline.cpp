#include <normitri/core/pipeline.hpp>
#include <chrono>

namespace normitri::core {

void Pipeline::add_stage(std::unique_ptr<IPipelineStage> stage) {
  if (stage) {
    stages_.push_back(std::move(stage));
  }
}

std::expected<DefectResult, PipelineError> Pipeline::run(
    const Frame& input,
    StageTimingCallback* timing_cb) {
  std::variant<Frame, DefectResult> current = input;

  for (std::size_t i = 0; i < stages_.size(); ++i) {
    const Frame* frame_ptr = std::get_if<Frame>(&current);
    if (!frame_ptr) {
      return std::get<DefectResult>(current);
    }

    const auto stage_start = std::chrono::steady_clock::now();
    auto result = stages_[i]->process(*frame_ptr);
    if (timing_cb) {
      const auto stage_end = std::chrono::steady_clock::now();
      const double ms = 1e-6 * static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(stage_end - stage_start).count());
      (*timing_cb)(i, ms);
    }

    if (!result) {
      return std::unexpected(result.error());
    }

    current = std::move(*result);
  }

  if (const auto* result = std::get_if<DefectResult>(&current)) {
    return *result;
  }
  return std::unexpected(PipelineError::InvalidConfig);
}

}  // namespace normitri::core
