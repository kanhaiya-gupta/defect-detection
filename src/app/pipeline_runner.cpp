#include <normitri/app/pipeline_runner.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace normitri::app {

std::expected<normitri::core::DefectResult, normitri::core::PipelineError>
run_pipeline(normitri::core::Pipeline& pipeline,
             const normitri::core::Frame& frame,
             StageTimingCallback* timing_cb) {
  return pipeline.run(frame, timing_cb);
}

void run_pipeline_batch(normitri::core::Pipeline& pipeline,
                        const std::vector<normitri::core::Frame>& frames,
                        DefectResultCallback callback) {
  for (const auto& frame : frames) {
    auto result = pipeline.run(frame);
    if (result && callback) {
      callback(*result);
    }
  }
}

namespace {

std::size_t effective_workers(std::size_t num_workers) {
  if (num_workers > 0) return num_workers;
  const unsigned hw = std::thread::hardware_concurrency();
  return hw > 0 ? static_cast<std::size_t>(hw) : 1;
}

}  // namespace

void run_pipeline_batch_parallel(
    normitri::core::Pipeline& pipeline,
    const std::vector<normitri::core::Frame>& frames,
    DefectResultCallback callback,
    std::size_t num_workers) {
  const std::size_t n = frames.size();
  if (n == 0 || !callback) return;

  const std::size_t workers = std::min(effective_workers(num_workers), n);
  if (workers <= 1) {
    run_pipeline_batch(pipeline, frames, std::move(callback));
    return;
  }

  std::queue<std::size_t> index_queue;
  for (std::size_t i = 0; i < n; ++i) {
    index_queue.push(i);
  }

  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::atomic<bool> producer_done{false};

  auto worker = [&]() {
    while (true) {
      std::size_t idx;
      {
        std::unique_lock lock(queue_mutex);
        queue_cv.wait(lock, [&]() {
          return producer_done.load() || !index_queue.empty();
        });
        if (producer_done.load() && index_queue.empty()) break;
        if (index_queue.empty()) continue;
        idx = index_queue.front();
        index_queue.pop();
      }

      auto result = pipeline.run(frames[idx]);
      if (result) {
        callback(*result);
      }
    }
  };

  producer_done = true;
  std::vector<std::thread> threads;
  threads.reserve(workers);
  for (std::size_t i = 0; i < workers; ++i) {
    threads.emplace_back(worker);
  }
  queue_cv.notify_all();

  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace normitri::app
