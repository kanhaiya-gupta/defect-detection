#pragma once

#include <normitri/core/defect_result.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <expected>
#include <memory>
#include <variant>

namespace normitri::core {

/// Output of a pipeline stage: either pass-through Frame or final DefectResult.
using StageOutput = std::variant<Frame, DefectResult>;

/// Abstract pipeline stage: process one Frame, return Frame (continue) or DefectResult (done).
class IPipelineStage {
 public:
  virtual ~IPipelineStage() = default;

  [[nodiscard]] virtual std::expected<StageOutput, PipelineError> process(
      const Frame& input) = 0;
};

}  // namespace normitri::core
