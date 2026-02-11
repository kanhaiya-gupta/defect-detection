#include <normitri/vision/tensorrt_inference_backend.hpp>
#include <normitri/core/error.hpp>
#include <normitri/core/frame.hpp>
#include <NvInfer.h>
#include <NvInferRuntime.h>
#include <cuda_runtime.h>
#include <cstdint>
#include <expected>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace normitri::vision {

namespace {

constexpr int kNumChannels = 3;

/// Copy HWC float buffer to NCHW.
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

class Logger : public nvinfer1::ILogger {
 public:
  void log(Severity severity, const char* msg) noexcept override {
    (void)severity;
    (void)msg;
    // Optionally forward to application logging
  }
};

}  // namespace

struct TensorRTInferenceBackend::Impl {
  Logger logger;
  std::unique_ptr<nvinfer1::IRuntime> runtime;
  std::unique_ptr<nvinfer1::ICudaEngine> engine;
  std::unique_ptr<nvinfer1::IExecutionContext> context;

  std::uint32_t input_height{0};
  std::uint32_t input_width{0};
  std::size_t input_num_floats{0};

  std::vector<float> nchw_buffer;
  std::vector<void*> device_buffers;
  std::vector<std::vector<std::byte>> host_output_buffers;
  std::vector<std::size_t> output_num_elements;
  std::vector<nvinfer1::DataType> output_dtypes;

  bool use_yolo_single_output{false};
  int num_io_tensors{0};
  int input_io_index{0};
  std::vector<std::string> io_tensor_names;
};

TensorRTInferenceBackend::TensorRTInferenceBackend(std::string engine_path) : impl_(std::make_unique<Impl>()) {
  std::ifstream f(engine_path, std::ios::binary | std::ios::ate);
  if (!f) {
    throw std::runtime_error("TensorRTInferenceBackend: cannot open engine file: " + engine_path);
  }
  const std::streamsize size = f.tellg();
  f.seekg(0);
  std::vector<char> blob(static_cast<std::size_t>(size));
  if (!f.read(blob.data(), size)) {
    throw std::runtime_error("TensorRTInferenceBackend: failed to read engine file");
  }

  impl_->runtime.reset(nvinfer1::createInferRuntime(impl_->logger));
  if (!impl_->runtime) {
    throw std::runtime_error("TensorRTInferenceBackend: createInferRuntime failed");
  }
  impl_->engine.reset(impl_->runtime->deserializeCudaEngine(blob.data(), static_cast<std::size_t>(size)));
  if (!impl_->engine) {
    throw std::runtime_error("TensorRTInferenceBackend: deserializeCudaEngine failed");
  }
  impl_->context.reset(impl_->engine->createExecutionContext());
  if (!impl_->context) {
    throw std::runtime_error("TensorRTInferenceBackend: createExecutionContext failed");
  }

  impl_->num_io_tensors = impl_->engine->getNbIOTensors();
  if (impl_->num_io_tensors < 1) {
    throw std::runtime_error("TensorRTInferenceBackend: engine has no IO tensors");
  }

  impl_->io_tensor_names.resize(static_cast<std::size_t>(impl_->num_io_tensors));
  for (int i = 0; i < impl_->num_io_tensors; ++i) {
    const char* name = impl_->engine->getIOTensorName(i);
    impl_->io_tensor_names[static_cast<std::size_t>(i)] = (name != nullptr) ? name : "";
  }

  // Find input tensor (first IO tensor that is input)
  for (int i = 0; i < impl_->num_io_tensors; ++i) {
    if (impl_->engine->getTensorIOMode(impl_->io_tensor_names[static_cast<std::size_t>(i)].c_str()) ==
        nvinfer1::TensorIOMode::kINPUT) {
      impl_->input_io_index = i;
      break;
    }
  }

  const std::string& input_name = impl_->io_tensor_names[static_cast<std::size_t>(impl_->input_io_index)];
  nvinfer1::Dims dims = impl_->engine->getTensorShape(input_name.c_str());
  if (dims.nbDims != 4) {
    throw std::runtime_error("TensorRTInferenceBackend: expected 4D input (NCHW)");
  }
  // NCHW: dims.d[0]=N, d[1]=C, d[2]=H, d[3]=W (TensorRT 10 may use int64 in Dims; cast for our use)
  const int n = (dims.d[0] <= 0) ? 1 : static_cast<int>(dims.d[0]);
  const int c = (dims.d[1] <= 0) ? kNumChannels : static_cast<int>(dims.d[1]);
  const int h = (dims.d[2] <= 0) ? 640 : static_cast<int>(dims.d[2]);
  const int w = (dims.d[3] <= 0) ? 640 : static_cast<int>(dims.d[3]);
  if (c != kNumChannels) {
    throw std::runtime_error("TensorRTInferenceBackend: expected 3-channel input");
  }
  impl_->input_height = static_cast<std::uint32_t>(h);
  impl_->input_width = static_cast<std::uint32_t>(w);
  impl_->input_num_floats = static_cast<std::size_t>(n) * static_cast<std::size_t>(c) * static_cast<std::size_t>(h) *
                            static_cast<std::size_t>(w);

  impl_->device_buffers.resize(static_cast<std::size_t>(impl_->num_io_tensors), nullptr);
  impl_->host_output_buffers.resize(static_cast<std::size_t>(impl_->num_io_tensors));
  impl_->output_num_elements.resize(static_cast<std::size_t>(impl_->num_io_tensors), 0);
  impl_->output_dtypes.resize(static_cast<std::size_t>(impl_->num_io_tensors), nvinfer1::DataType::kFLOAT);

  for (int i = 0; i < impl_->num_io_tensors; ++i) {
    const std::string& name = impl_->io_tensor_names[static_cast<std::size_t>(i)];
    nvinfer1::Dims d = impl_->engine->getTensorShape(name.c_str());
    nvinfer1::DataType dt = impl_->engine->getTensorDataType(name.c_str());
    std::size_t element_size = (dt == nvinfer1::DataType::kFLOAT) ? sizeof(float) : sizeof(int64_t);
    std::size_t num = 1;
    for (int j = 0; j < d.nbDims; ++j) {
      const int64_t dim_val = (d.d[j] > 0) ? d.d[j] : 1;
      num *= static_cast<std::size_t>(dim_val);
    }
    impl_->output_num_elements[static_cast<std::size_t>(i)] = num;
    impl_->output_dtypes[static_cast<std::size_t>(i)] = dt;
    std::size_t bytes = num * element_size;
    void* ptr = nullptr;
    cudaError_t err = cudaMalloc(&ptr, bytes);
    if (err != cudaSuccess) {
      throw std::runtime_error("TensorRTInferenceBackend: cudaMalloc failed for tensor " + name);
    }
    impl_->device_buffers[static_cast<std::size_t>(i)] = ptr;
    if (impl_->engine->getTensorIOMode(name.c_str()) != nvinfer1::TensorIOMode::kINPUT) {
      impl_->host_output_buffers[static_cast<std::size_t>(i)].resize(bytes);
    }
  }

  impl_->use_yolo_single_output = (impl_->num_io_tensors == 2);
  impl_->nchw_buffer.resize(impl_->input_num_floats);
}

TensorRTInferenceBackend::~TensorRTInferenceBackend() {
  if (!impl_) return;
  for (void* ptr : impl_->device_buffers) {
    if (ptr) {
      cudaFree(ptr);
    }
  }
}

std::expected<void, normitri::core::PipelineError>
TensorRTInferenceBackend::validate_input(const normitri::core::Frame& input) const {
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
TensorRTInferenceBackend::infer(const normitri::core::Frame& input) {
  auto valid = validate_input(input);
  if (!valid) {
    return std::unexpected(valid.error());
  }

  const std::uint32_t h = input.height();
  const std::uint32_t w = input.width();
  const float* src = reinterpret_cast<const float*>(input.data().data());
  HwcToNchw(src, h, w, impl_->nchw_buffer.data());

  cudaError_t err = cudaMemcpy(impl_->device_buffers[static_cast<std::size_t>(impl_->input_io_index)],
                              impl_->nchw_buffer.data(),
                              impl_->input_num_floats * sizeof(float),
                              cudaMemcpyHostToDevice);
  if (err != cudaSuccess) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }

  std::vector<void*> bindings(impl_->device_buffers.begin(), impl_->device_buffers.end());
  bool ok = impl_->context->executeV2(bindings.data());
  if (!ok) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }

  // Copy outputs back to host
  for (int i = 0; i < impl_->num_io_tensors; ++i) {
    if (impl_->engine->getTensorIOMode(impl_->io_tensor_names[static_cast<std::size_t>(i)].c_str()) ==
        nvinfer1::TensorIOMode::kINPUT) {
      continue;
    }
    std::size_t idx = static_cast<std::size_t>(i);
    std::size_t num = impl_->output_num_elements[idx];
    nvinfer1::DataType dt = impl_->output_dtypes[idx];
    std::size_t bytes = num * (dt == nvinfer1::DataType::kFLOAT ? sizeof(float) : sizeof(int64_t));
    err = cudaMemcpy(impl_->host_output_buffers[idx].data(), impl_->device_buffers[idx], bytes, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
      return std::unexpected(normitri::core::PipelineError::InferenceFailed);
    }
  }

  InferenceResult result;
  const int output_io_index = impl_->input_io_index == 0 ? 1 : 0;
  const std::size_t out_idx = static_cast<std::size_t>(output_io_index);

  if (impl_->use_yolo_single_output) {
    const float* data = reinterpret_cast<const float*>(impl_->host_output_buffers[out_idx].data());
    std::size_t n = impl_->output_num_elements[out_idx];
    if (n < 6) {
      return result;
    }
    std::size_t num_det = n / 6;
    bool rows_are_n6 = true;
    nvinfer1::Dims d =
        impl_->engine->getTensorShape(impl_->io_tensor_names[out_idx].c_str());
    if (d.nbDims == 3 && d.d[1] == 6) {
      num_det = static_cast<std::size_t>(d.d[2]);
      rows_are_n6 = false;
    } else if (d.nbDims == 3 && d.d[2] == 6) {
      num_det = static_cast<std::size_t>(d.d[1]);
    }
    result.num_detections = static_cast<std::uint32_t>(num_det);
    result.boxes.reserve(num_det * 4);
    result.scores.reserve(num_det);
    result.class_ids.reserve(num_det);
    const std::size_t stride_sz = rows_are_n6 ? 6 : num_det;
    for (std::size_t i = 0; i < num_det; ++i) {
      if (rows_are_n6) {
        const float* row = data + i * 6;
        result.boxes.push_back(row[0]);
        result.boxes.push_back(row[1]);
        result.boxes.push_back(row[2]);
        result.boxes.push_back(row[3]);
        result.scores.push_back(row[4]);
        result.class_ids.push_back(static_cast<std::int64_t>(row[5]));
      } else {
        result.boxes.push_back(data[0 * stride_sz + i]);
        result.boxes.push_back(data[1 * stride_sz + i]);
        result.boxes.push_back(data[2 * stride_sz + i]);
        result.boxes.push_back(data[3 * stride_sz + i]);
        result.scores.push_back(data[4 * stride_sz + i]);
        result.class_ids.push_back(static_cast<std::int64_t>(data[5 * stride_sz + i]));
      }
    }
    return result;
  }

  // Three-output path: assume IO indices 1, 2, 3 are boxes, scores, class_ids
  if (impl_->num_io_tensors < 4) {
    return std::unexpected(normitri::core::PipelineError::InferenceFailed);
  }
  const float* boxes_data = reinterpret_cast<const float*>(impl_->host_output_buffers[1].data());
  const float* scores_data = reinterpret_cast<const float*>(impl_->host_output_buffers[2].data());
  const std::size_t n_boxes = impl_->output_num_elements[1] / 4;
  const std::size_t n_scores = impl_->output_num_elements[2];
  const std::size_t n_classes = impl_->output_num_elements[3];
  std::size_t n = (std::min)((std::min)(n_boxes, n_scores), n_classes);
  if (n == 0) return result;

  result.num_detections = static_cast<std::uint32_t>(n);
  result.boxes.reserve(n * 4);
  result.scores.reserve(n);
  result.class_ids.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    result.boxes.push_back(boxes_data[i * 4 + 0]);
    result.boxes.push_back(boxes_data[i * 4 + 1]);
    result.boxes.push_back(boxes_data[i * 4 + 2]);
    result.boxes.push_back(boxes_data[i * 4 + 3]);
    result.scores.push_back(scores_data[i]);
  }
  if (impl_->output_dtypes[3] == nvinfer1::DataType::kFLOAT) {
    const float* classes_f = reinterpret_cast<const float*>(impl_->host_output_buffers[3].data());
    for (std::size_t i = 0; i < n; ++i) {
      result.class_ids.push_back(static_cast<std::int64_t>(classes_f[i]));
    }
  } else {
    const int64_t* classes_i = reinterpret_cast<const int64_t*>(impl_->host_output_buffers[3].data());
    for (std::size_t i = 0; i < n; ++i) {
      result.class_ids.push_back(classes_i[i]);
    }
  }
  return result;
}

void TensorRTInferenceBackend::warmup() {
  const std::size_t num_bytes = impl_->input_num_floats * sizeof(float);
  std::vector<std::byte> buffer(num_bytes, std::byte{0});
  normitri::core::Frame frame(impl_->input_width, impl_->input_height,
                              normitri::core::PixelFormat::Float32Planar, std::move(buffer));
  (void)infer(frame);
}

}  // namespace normitri::vision
