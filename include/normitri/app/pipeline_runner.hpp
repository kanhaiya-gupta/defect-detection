#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <cstddef>
#include <expected>
#include <functional>
#include <vector>

namespace normitri::app {

/// Callback for each DefectResult; may be invoked from worker threads.
/// Must be thread-safe if using run_pipeline_batch_parallel.
using DefectResultCallback =
    std::function<void(const normitri::core::DefectResult&)>;

/// Optional per-stage timing: (stage_index, duration_ms). Pass to run_pipeline to get timings.
using StageTimingCallback = normitri::core::StageTimingCallback;

/// Runs pipeline on a single frame. No threading; direct call.
/// If timing_cb is non-null, it is invoked for each stage with (stage_index, duration_ms).
[[nodiscard]] std::expected<normitri::core::DefectResult,
                            normitri::core::PipelineError>
run_pipeline(normitri::core::Pipeline& pipeline,
             const normitri::core::Frame& frame,
             StageTimingCallback* timing_cb = nullptr);

/// Runs pipeline on multiple frames sequentially; calls callback for each result.
void run_pipeline_batch(normitri::core::Pipeline& pipeline,
                        const std::vector<normitri::core::Frame>& frames,
                        DefectResultCallback callback);

/// Runs pipeline on multiple frames in parallel using a thread pool.
/// Pipeline::run() is called from worker threads; callback may be invoked
/// from any worker (must be thread-safe). num_workers 0 = use hardware concurrency.
void run_pipeline_batch_parallel(
    normitri::core::Pipeline& pipeline,
    const std::vector<normitri::core::Frame>& frames,
    DefectResultCallback callback,
    std::size_t num_workers = 0);

}  // namespace normitri::app
