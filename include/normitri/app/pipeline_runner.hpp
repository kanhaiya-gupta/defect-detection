#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
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
/// If camera_id / customer_id are provided, they are set on the returned DefectResult for traceability.
[[nodiscard]] std::expected<normitri::core::DefectResult,
                            normitri::core::PipelineError>
run_pipeline(normitri::core::Pipeline& pipeline,
             const normitri::core::Frame& frame,
             StageTimingCallback* timing_cb = nullptr,
             std::optional<std::string> camera_id = std::nullopt,
             std::optional<std::string> customer_id = std::nullopt);

/// Runs pipeline on multiple frames sequentially; calls callback for each result.
/// If camera_ids / customer_ids are provided (same size as frames), each result is tagged with the corresponding id before callback; empty string = leave unset.
void run_pipeline_batch(normitri::core::Pipeline& pipeline,
                        const std::vector<normitri::core::Frame>& frames,
                        DefectResultCallback callback,
                        const std::vector<std::string>* camera_ids = nullptr,
                        const std::vector<std::string>* customer_ids = nullptr);

/// Runs pipeline on multiple frames in parallel using a thread pool.
/// Pipeline::run() is called from worker threads; callback may be invoked
/// from any worker (must be thread-safe). num_workers 0 = use hardware concurrency.
/// If camera_ids / customer_ids are provided (same size as frames), each result is tagged with the corresponding id before callback; empty string = leave unset.
void run_pipeline_batch_parallel(
    normitri::core::Pipeline& pipeline,
    const std::vector<normitri::core::Frame>& frames,
    DefectResultCallback callback,
    std::size_t num_workers = 0,
    const std::vector<std::string>* camera_ids = nullptr,
    const std::vector<std::string>* customer_ids = nullptr);

}  // namespace normitri::app
