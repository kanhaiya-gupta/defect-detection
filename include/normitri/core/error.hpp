#pragma once

namespace normitri::core {

/// Pipeline error codes; used with std::expected for recoverable failures.
enum class PipelineError {
  None = 0,
  InvalidFrame,
  LoadFailed,
  InferenceFailed,
  InvalidConfig,
  DecoderError,
};

}  // namespace normitri::core
