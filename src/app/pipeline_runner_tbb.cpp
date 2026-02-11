#include <normitri/app/pipeline_runner_tbb.hpp>
#include <normitri/core/defect_result.hpp>
#include <normitri/core/pipeline.hpp>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef NORMITRI_HAS_TBB

namespace normitri::app {

void run_pipeline_multi_camera_tbb(
    const std::unordered_map<std::string, normitri::core::Pipeline*>& pipelines,
    const std::vector<std::pair<std::string, normitri::core::Frame>>& work_items,
    DefectResultCallbackWithUnitId callback) {
  if (work_items.empty() || !callback) return;

  const std::size_t n = work_items.size();
  tbb::parallel_for(
      tbb::blocked_range<std::size_t>(0, n),
      [&pipelines, &work_items, &callback](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
          const std::string& unit_id = work_items[i].first;
          const normitri::core::Frame& frame = work_items[i].second;
          auto it = pipelines.find(unit_id);
          if (it == pipelines.end()) continue;
          normitri::core::Pipeline* pipeline = it->second;
          auto result = pipeline->run(frame);
          if (result) {
            result->camera_id = unit_id;
            callback(*result, unit_id);
          }
        }
      });
}

}  // namespace normitri::app

#endif  // NORMITRI_HAS_TBB
