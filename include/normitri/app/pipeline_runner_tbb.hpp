#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef NORMITRI_HAS_TBB

namespace normitri::app {

/// Callback for each DefectResult in the multi-camera TBB runner; receives result and unit_id (camera_id or customer_id).
/// May be invoked from TBB worker threads; must be thread-safe.
using DefectResultCallbackWithUnitId =
    std::function<void(const normitri::core::DefectResult&, const std::string& unit_id)>;

/// Runs pipelines on a batch of (unit_id, frame) work items in parallel using TBB.
///
/// Each work item is (unit_id, frame). For each item, the pipeline for that unit_id is run on the frame;
/// on success, the result's camera_id is set to unit_id and callback(result, unit_id) is invoked.
/// \p pipelines must contain an entry for every unit_id that appears in \p work_items.
///
/// **One pipeline per unit:** Use one Pipeline (and one inference backend) per camera or per customer.
/// Each pipeline is invoked from TBB tasks; if the same unit_id appears in multiple work items, the same
/// pipeline may be used from multiple threads concurrently. For non-thread-safe backends (e.g. ONNX,
/// TensorRT), submit at most one work item per unit_id per call, or use a thread-safe backend.
///
/// \param pipelines Map from unit_id (e.g. camera_id or customer_id) to pipeline. Caller keeps ownership.
/// \param work_items Flat list of (unit_id, frame) pairs. Frames are read only; not modified.
/// \param callback Invoked for each successful result with (result, unit_id). Must be thread-safe.
void run_pipeline_multi_camera_tbb(
    const std::unordered_map<std::string, normitri::core::Pipeline*>& pipelines,
    const std::vector<std::pair<std::string, normitri::core::Frame>>& work_items,
    DefectResultCallbackWithUnitId callback);

}  // namespace normitri::app

#endif  // NORMITRI_HAS_TBB
