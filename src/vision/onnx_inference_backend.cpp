#include <normitri/vision/onnx_inference_backend.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <onnxruntime_cxx_api.h>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace normitri::vision {

namespace {

constexpr int64_t kNumChannels = 3;

Ort::MemoryInfo CpuMemoryInfo() {
  return Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
}

/// Copy HWC (height, width, channels) float buffer to NCHW (batch, channels, height, width).
void HwcToNchw(const float* hwc, std::uint32_t h, std::uint32_t w, float* nchw) {
  const std::size_t hw = static_cast<std::size_t>(h) * w;
  for (std::uint32_t y = 0; y < h; ++y) {
    for (std::uint32_t x = 0; x < w; ++x) {
      const std::size_t src_idx = (static_cast<std::size_t>(y) * w + x) * kNumChannels;
      nchw[0 * hw + y * w + x] = hwc[src_idx + 0];
      nchw[1 * hw + y * w + x] = hwc[src_idx + 1];
      nchw[2 * hw + y * w + x] = hwc[src_idx + 2];
    }
  }
}

}  // namespace

struct OnnxInferenceBackend::Impl {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "normitri"};
  Ort::SessionOptions session_options;
  Ort::Session session{nullptr};

  std::string input_name;
  std::array<std::string, 3> output_names;
  std::vector<const char*> output_name_ptrs;

  std::uint32_t input_height{0};
  std::uint32_t input_width{0};
  bool input_is_nchw{true};
  /// True if model has a single output with [1, N, 6] or [1, 6, N] (xmin, ymin, xmax, ymax, score, class_id) — e.g. YOLOv10.
  bool use_yolo_single_output{false};

  std::vector<float> nchw_buffer;  // scratch for HWC -> NCHW

  Impl() {
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
  }
};

OnnxInferenceBackend::OnnxInferenceBackend(std::string model_path,
                                           std::string input_name,
                                           std::array<std::string, 3> output_names)
    : impl_(std::make_unique<Impl>()) {
  impl_->session_options.SetIntraOpNumThreads(1);
  impl_->session = Ort::Session(impl_->env, model_path.c_str(), impl_->session_options);

  Ort::AllocatorWithDefaultOptions allocator;
  const size_t num_inputs = impl_->session.GetInputCount();
  if (num_inputs == 0) {
    throw std::runtime_error("OnnxInferenceBackend: model has no inputs");
  }
  if (input_name.empty()) {
    impl_->input_name = impl_->session.GetInputNameAllocated(0, allocator).get();
  } else {
    impl_->input_name = std::move(input_name);
  }

  Ort::TypeInfo input_type = impl_->session.GetInputTypeInfo(0);
  const auto shape_info = input_type.GetTensorTypeAndShapeInfo();
  std::vector<int64_t> dims = shape_info.GetShape();
  if (dims.size() == 4u) {
    // NCHW: [1, C, H, W] or NHWC: [1, H, W, C]
    if (dims[1] == kNumChannels) {
      impl_->input_is_nchw = true;
      impl_->input_height = static_cast<std::uint32_t>(dims[2]);
      impl_->input_width = static_cast<std::uint32_t>(dims[3]);
    } else if (dims[3] == kNumChannels) {
      impl_->input_is_nchw = false;
      impl_->input_height = static_cast<std::uint32_t>(dims[1]);
      impl_->input_width = static_cast<std::uint32_t>(dims[2]);
    } else {
      throw std::runtime_error("OnnxInferenceBackend: expected input shape [1,3,H,W] or [1,H,W,3]");
    }
  } else {
    throw std::runtime_error("OnnxInferenceBackend: expected 4D input");
  }

  const size_t num_outputs = impl_->session.GetOutputCount();
  if (num_outputs == 0) {
    throw std::runtime_error("OnnxInferenceBackend: model has no outputs");
  }
  if (num_outputs == 1u) {
    // YOLO-style single output: [1, N, 6] or [1, 6, N] (xmin, ymin, xmax, ymax, score, class_id)
    impl_->use_yolo_single_output = true;
    impl_->output_names[0] = impl_->session.GetOutputNameAllocated(0, allocator).get();
    impl_->output_name_ptrs.push_back(impl_->output_names[0].c_str());
  } else if (num_outputs >= 3u) {
    for (std::size_t i = 0; i < 3u; ++i) {
      if (output_names[i].empty()) {
        impl_->output_names[i] = impl_->session.GetOutputNameAllocated(static_cast<size_t>(i), allocator).get();
      } else {
        impl_->output_names[i] = output_names[i];
      }
      impl_->output_name_ptrs.push_back(impl_->output_names[i].c_str());
    }
  } else {
    throw std::runtime_error("OnnxInferenceBackend: model must have 1 output (YOLO-style) or at least 3 outputs (boxes, scores, class_ids)");
  }
}

OnnxInferenceBackend::~OnnxInferenceBackend() = default;

std::expected<void, normitri::core::PipelineError>
OnnxInferenceBackend::validate_input(const normitri::core::Frame& input) const {
  if (input.empty()) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }
  if (input.format() != normitri::core::PixelFormat::Float32Planar) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }
  if (input.width() != impl_->input_width || input.height() != impl_->input_height) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }
  const std::size_t expected_bytes =
      static_cast<std::size_t>(impl_->input_height) * impl_->input_width * kNumChannels * sizeof(float);
  if (input.size_bytes() < expected_bytes) {
    return std::unexpected(normitri::core::PipelineError::InvalidFrame);
  }
  return {};
}

std::expected<InferenceResult, normitri::core::PipelineError>
OnnxInferenceBackend::infer(const normitri::core::Frame& input) {
  auto valid = validate_input(input);
  if (!valid) {
    return std::unexpected(valid.error());
  }

  const std::uint32_t h = input.height();
  const std::uint32_t w = input.width();
  const float* src = reinterpret_cast<const float*>(input.data().data());

  Ort::MemoryInfo mem_info = CpuMemoryInfo();
  Ort::Value input_tensor{nullptr};

  if (impl_->input_is_nchw) {
    const std::size_t num_floats = static_cast<std::size_t>(1) * kNumChannels * h * w;
    impl_->nchw_buffer.resize(num_floats);
    HwcToNchw(src, h, w, impl_->nchw_buffer.data());
    const std::array<int64_t, 4> shape{1, static_cast<int64_t>(kNumChannels),
                                        static_cast<int64_t>(h), static_cast<int64_t>(w)};
    input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, impl_->nchw_buffer.data(), num_floats * sizeof(float),
        shape.data(), shape.size());
  } else {
    const std::size_t num_floats = static_cast<std::size_t>(1) * h * w * kNumChannels;
    const std::array<int64_t, 4> shape{1, static_cast<int64_t>(h), static_cast<int64_t>(w),
                                        static_cast<int64_t>(kNumChannels)};
    input_tensor = Ort::Value::CreateTensor<float>(
        mem_info, const_cast<float*>(src), num_floats * sizeof(float),
        shape.data(), shape.size());
  }

  const char* input_names_c[] = {impl_->input_name.c_str()};
  Ort::RunOptions run_options;

  std::vector<Ort::Value> outputs;
  try {
    outputs = impl_->session.Run(
        run_options,
        input_names_c, &input_tensor, 1,
        impl_->output_name_ptrs.data(), impl_->output_name_ptrs.size());
  } catch (const Ort::Exception& e) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }

  InferenceResult result;

  if (impl_->use_yolo_single_output) {
    // Single output: [1, N, 6] or [1, 6, N] — (xmin, ymin, xmax, ymax, score, class_id)
    if (outputs.size() != 1u) {
      return std::unexpected(normitri::core::PipelineError::InferenceFailed);
    }
    Ort::Value& out = outputs[0];
    const auto shape = out.GetTensorTypeAndShapeInfo().GetShape();
    const float* data = out.GetTensorData<float>();
    int64_t n = 0;
    bool rows_are_n6 = false;  // true: [1, N, 6]; false: [1, 6, N]
    if (shape.size() == 3u && shape[0] == 1 && shape[2] == 6) {
      n = shape[1];
      rows_are_n6 = true;
    } else if (shape.size() == 3u && shape[0] == 1 && shape[1] == 6) {
      n = shape[2];
      rows_are_n6 = false;
    }
    if (n <= 0) {
      return std::unexpected(normitri::core::PipelineError::InferenceFailed);
    }
    result.num_detections = static_cast<std::uint32_t>(n);
    result.boxes.reserve(static_cast<std::size_t>(n) * 4u);
    result.scores.reserve(static_cast<std::size_t>(n));
    result.class_ids.reserve(static_cast<std::size_t>(n));
    const int64_t stride = rows_are_n6 ? 6 : static_cast<int64_t>(n);
    for (int64_t i = 0; i < n; ++i) {
      if (rows_are_n6) {
        const float* row = data + i * 6;
        result.boxes.push_back(row[0]);
        result.boxes.push_back(row[1]);
        result.boxes.push_back(row[2]);
        result.boxes.push_back(row[3]);
        result.scores.push_back(row[4]);
        result.class_ids.push_back(static_cast<int64_t>(row[5]));
      } else {
        result.boxes.push_back(data[0 * stride + i]);
        result.boxes.push_back(data[1 * stride + i]);
        result.boxes.push_back(data[2 * stride + i]);
        result.boxes.push_back(data[3 * stride + i]);
        result.scores.push_back(data[4 * stride + i]);
        result.class_ids.push_back(static_cast<int64_t>(data[5 * stride + i]));
      }
    }
    return result;
  }

  // Three-output path: boxes, scores, class_ids
  if (outputs.size() < 3u) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }
  Ort::Value& boxes_val = outputs[0];
  Ort::Value& scores_val = outputs[1];
  Ort::Value& classes_val = outputs[2];

  auto boxes_info = boxes_val.GetTensorTypeAndShapeInfo();
  auto scores_info = scores_val.GetTensorTypeAndShapeInfo();
  auto classes_info = classes_val.GetTensorTypeAndShapeInfo();

  const std::vector<int64_t> boxes_shape = boxes_info.GetShape();
  const std::vector<int64_t> scores_shape = scores_info.GetShape();
  const std::vector<int64_t> classes_shape = classes_info.GetShape();

  // Support [1, N, 4], [1, 4, N], or [N, 4]; similarly [1, N] or [N] for scores/classes.
  int64_t n = 0;
  if (boxes_shape.size() == 3u && boxes_shape[0] == 1) {
    n = boxes_shape[1];
    if (boxes_shape[2] != 4) n = 0;
  } else if (boxes_shape.size() == 3u && boxes_shape[2] == 1 && boxes_shape[1] == 4) {
    n = boxes_shape[0];
  } else if (boxes_shape.size() == 2u && boxes_shape[1] == 4) {
    n = boxes_shape[0];
  }
  if (n <= 0) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }

  const float* boxes_data = boxes_val.GetTensorData<float>();
  const float* scores_data = scores_val.GetTensorData<float>();
  const int64_t* classes_data = classes_val.GetTensorData<int64_t>();

  result.num_detections = static_cast<std::uint32_t>(n);
  result.boxes.reserve(static_cast<std::size_t>(n) * 4u);
  result.scores.reserve(static_cast<std::size_t>(n));
  result.class_ids.reserve(static_cast<std::size_t>(n));

  const bool boxes_is_n4 = (boxes_shape.size() == 3u && boxes_shape[2] == 4) ||
                           (boxes_shape.size() == 2u && boxes_shape[1] == 4);
  const std::int64_t boxes_stride = boxes_is_n4 ? 4 : static_cast<std::int64_t>(n);

  for (int64_t i = 0; i < n; ++i) {
    if (boxes_is_n4) {
      const float* row = boxes_data + i * 4;
      result.boxes.push_back(row[0]);
      result.boxes.push_back(row[1]);
      result.boxes.push_back(row[2]);
      result.boxes.push_back(row[3]);
    } else {
      result.boxes.push_back(boxes_data[0 * boxes_stride + i]);
      result.boxes.push_back(boxes_data[1 * boxes_stride + i]);
      result.boxes.push_back(boxes_data[2 * boxes_stride + i]);
      result.boxes.push_back(boxes_data[3 * boxes_stride + i]);
    }
    result.scores.push_back(scores_data[i]);
    result.class_ids.push_back(classes_data[i]);
  }

  return result;
}

void OnnxInferenceBackend::warmup() {
  const std::size_t num_bytes =
      static_cast<std::size_t>(impl_->input_height) * impl_->input_width * kNumChannels * sizeof(float);
  std::vector<std::byte> buffer(num_bytes, std::byte{0});
  normitri::core::Frame frame(impl_->input_width, impl_->input_height,
                              normitri::core::PixelFormat::Float32Planar, std::move(buffer));
  (void)infer(frame);
}

}  // namespace normitri::vision
