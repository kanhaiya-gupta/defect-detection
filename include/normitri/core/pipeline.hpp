#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline_stage.hpp>
#include <expected>
#include <functional>
#include <memory>
#include <vector>

namespace normitri::core {

/// Callback for per-stage timing: (stage_index, duration_ms). Optional; pass to run().
using StageTimingCallback = std::function<void(std::size_t stage_index, double duration_ms)>;

/// Runs a sequence of stages; passes Frame through until a stage returns DefectResult.
class Pipeline {
 public:
  Pipeline() = default;

  void add_stage(std::unique_ptr<IPipelineStage> stage);

  /// Run pipeline on one frame; returns first DefectResult or error.
  /// If timing_cb is non-null, it is called after each stage with (stage_index, duration_ms).
  /// Thread-safe: safe to call run() from multiple threads concurrently
  /// (stages are not modified during process()).
  [[nodiscard]] std::expected<DefectResult, PipelineError> run(
      const Frame& input,
      StageTimingCallback* timing_cb = nullptr);

  [[nodiscard]] std::size_t stage_count() const noexcept {
    return stages_.size();
  }

 private:
  std::vector<std::unique_ptr<IPipelineStage>> stages_;
};

}  // namespace normitri::core
