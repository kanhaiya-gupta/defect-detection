/**
 * normitri-cli — Run defect-detection pipeline on image(s); output defects.
 * Build: cmake -B build && cmake --build build
 * Run:   ./build/apps/normitri-cli/normitri_cli [--config path] [--input path]
 * With --input: also writes results to output/<basename>.txt (same content as terminal).
 */

#include <normitri/app/config.hpp>
#include <normitri/app/pipeline_runner.hpp>
#include <normitri/core/defect.hpp>
#include <normitri/core/defect_result.hpp>
#include <normitri/core/frame.hpp>
#include <normitri/core/pipeline.hpp>
#include <normitri/vision/defect_decoder.hpp>
#include <normitri/vision/defect_detection_stage.hpp>
#include <normitri/vision/load_image.hpp>
#include <normitri/vision/mock_inference_backend.hpp>
#include <normitri/vision/normalize_stage.hpp>
#include <normitri/vision/onnx_inference_backend.hpp>
#include <normitri/vision/resize_stage.hpp>
#ifdef NORMITRI_HAS_TENSORRT
#include <normitri/vision/tensorrt_inference_backend.hpp>
#endif

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string defect_kind_str(normitri::core::DefectKind k) {
  switch (k) {
  case normitri::core::DefectKind::WrongItem:
    return "WrongItem";
  case normitri::core::DefectKind::WrongQuantity:
    return "WrongQuantity";
  case normitri::core::DefectKind::ExpiredOrQuality:
    return "ExpiredOrQuality";
  case normitri::core::DefectKind::ProcessError:
    return "ProcessError";
  default:
    return "Unknown";
  }
}

normitri::core::Pipeline build_pipeline(const normitri::app::PipelineConfig &cfg) {
  using namespace normitri::core;
  using namespace normitri::vision;

  normitri::core::Pipeline pipeline;

  pipeline.add_stage(std::make_unique<ResizeStage>(cfg.resize_width, cfg.resize_height));
  pipeline.add_stage(std::make_unique<NormalizeStage>(cfg.normalize_mean, cfg.normalize_scale));

  ClassToDefectKindMap class_to_kind = {
      DefectKind::WrongItem,
      DefectKind::WrongQuantity,
      DefectKind::ExpiredOrQuality,
      DefectKind::ProcessError,
  };
  DefectDecoder decoder(cfg.confidence_threshold, std::move(class_to_kind));

  std::unique_ptr<IInferenceBackend> backend;
  if (cfg.backend_type == normitri::app::InferenceBackendType::Onnx) {
    if (cfg.model_path.empty()) {
      throw std::runtime_error("backend_type=onnx requires model_path to be set in config");
    }
    auto onnx = std::make_unique<OnnxInferenceBackend>(cfg.model_path);
    onnx->warmup();
    backend = std::move(onnx);
  }
#ifdef NORMITRI_HAS_TENSORRT
  else if (cfg.backend_type == normitri::app::InferenceBackendType::TensorRT) {
    if (cfg.model_path.empty()) {
      throw std::runtime_error("backend_type=tensorrt requires model_path (engine file) to be set in config");
    }
    auto trt = std::make_unique<TensorRTInferenceBackend>(cfg.model_path);
    trt->warmup();
    backend = std::move(trt);
  }
#endif
  else {
    auto mock = std::make_unique<MockInferenceBackend>();
    mock->set_defects({
        {DefectKind::WrongItem, {0.1f, 0.2f, 0.3f, 0.4f}, 0.95f, std::nullopt, std::nullopt},
    });
    backend = std::move(mock);
  }

  pipeline.add_stage(
      std::make_unique<DefectDetectionStage>(std::move(backend), std::move(decoder), 0));

  return pipeline;
}

normitri::core::Frame make_dummy_frame(std::uint32_t w, std::uint32_t h) {
  const std::size_t bytes = static_cast<std::size_t>(w) * h * 3;
  std::vector<std::byte> buffer(bytes, std::byte{0});
  return normitri::core::Frame(w, h, normitri::core::PixelFormat::RGB8, std::move(buffer));
}

} // namespace

int main(int argc, char *argv[]) {
  std::string config_path;
  std::string input_path;
  std::string backend_override;  // "mock", "onnx", or "tensorrt"
  std::string model_override;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (arg == "--input" && i + 1 < argc) {
      input_path = argv[++i];
    } else if (arg == "--backend" && i + 1 < argc) {
      backend_override = argv[++i];
    } else if (arg == "--model" && i + 1 < argc) {
      model_override = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: normitri_cli [options] [--input <path>]\n"
                << "  --config <path>   Pipeline config (key=value file); default: built-in (mock)\n"
                << "  --backend <type>  Override backend: mock | onnx | tensorrt (default from config)\n"
                << "  --model <path>    Override model path (required for --backend onnx or tensorrt)\n"
                << "  --input <path>    Image path (optional; demo uses synthetic frame)\n"
                << "\nBackend selection: config file (backend_type=, model_path=) or --backend/--model.\n";
      return 0;
    }
  }

  normitri::app::PipelineConfig cfg = config_path.empty() ? normitri::app::default_config()
                                                          : normitri::app::load_config(config_path);

  if (!backend_override.empty()) {
    if (backend_override == "mock") {
      cfg.backend_type = normitri::app::InferenceBackendType::Mock;
    } else if (backend_override == "onnx") {
      cfg.backend_type = normitri::app::InferenceBackendType::Onnx;
    } else if (backend_override == "tensorrt") {
#ifdef NORMITRI_HAS_TENSORRT
      cfg.backend_type = normitri::app::InferenceBackendType::TensorRT;
#else
      std::cerr << "TensorRT backend not available (build with -DNORMITRI_USE_TENSORRT=ON and TensorRT/CUDA)\n";
      return 1;
#endif
    } else {
      std::cerr << "Unknown --backend " << backend_override << " (use mock, onnx, or tensorrt)\n";
      return 1;
    }
  }
  if (!model_override.empty()) {
    cfg.model_path = model_override;
  }

  normitri::core::Pipeline pipeline = build_pipeline(cfg);

  normitri::core::Frame frame;
  if (!input_path.empty()) {
    auto loaded = normitri::vision::load_frame_from_image(input_path);
    if (!loaded) {
      std::cerr << "Failed to load image: " << input_path << "\n";
      return 1;
    }
    frame = std::move(*loaded);
  } else {
    frame = make_dummy_frame(320, 240);
  }
  auto result = normitri::app::run_pipeline(pipeline, frame);

  if (!result) {
    std::cerr << "Pipeline error: " << static_cast<int>(result.error()) << "\n";
    return 1;
  }

  std::ostringstream out;
  out << "frame_id=" << result->frame_id << " defects=" << result->defects.size();
  if (result->camera_id.has_value()) out << " camera_id=" << *result->camera_id;
  if (result->customer_id.has_value()) out << " customer_id=" << *result->customer_id;
  out << "\n";
  for (const auto &d : result->defects) {
    out << "  " << defect_kind_str(d.kind) << " confidence=" << d.confidence << " bbox=("
        << d.bbox.x << "," << d.bbox.y << "," << d.bbox.w << "," << d.bbox.h << ")\n";
  }
  std::string text = out.str();
  std::cout << text;

  if (!input_path.empty()) {
    std::filesystem::path p(input_path);
    std::filesystem::path out_dir("output");
    std::filesystem::create_directories(out_dir);
    std::filesystem::path out_file = out_dir / (p.stem().string() + ".txt");
    std::ofstream f(out_file);
    if (f) {
      f << text;
    } else {
      std::cerr << "Warning: could not write " << out_file << "\n";
    }
  }
  return 0;
}
