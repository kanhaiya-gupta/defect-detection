#include <normitri/app/config.hpp>
#include <fstream>
#include <sstream>
#include <string_view>

namespace normitri::app {

namespace {

void trim(std::string& s) {
  const auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    s.clear();
    return;
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  s = s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}

bool parse_line(std::string_view line, std::string& key, std::string& value) {
  const auto pos = line.find('=');
  if (pos == std::string_view::npos) return false;
  key.assign(line.substr(0, pos));
  value.assign(line.substr(pos + 1));
  trim(key);
  trim(value);
  return !key.empty();
}

}  // namespace

PipelineConfig default_config() {
  PipelineConfig c;
  c.model_path = "";
  c.backend_type = InferenceBackendType::Mock;
  c.resize_width = 640;
  c.resize_height = 640;
  c.normalize_mean = 0.f;
  c.normalize_scale = 1.f / 255.f;
  c.confidence_threshold = 0.5f;
  return c;
}

PipelineConfig load_config(const std::string& path) {
  PipelineConfig c = default_config();
  std::ifstream f(path);
  if (!f) return c;

  std::string line;
  std::string key;
  std::string value;
  while (std::getline(f, line)) {
    trim(line);
    if (line.empty() || line[0] == '#') continue;
    if (!parse_line(line, key, value)) continue;

    if (key == "model_path") c.model_path = value;
    else if (key == "backend_type") {
      if (value == "onnx") c.backend_type = InferenceBackendType::Onnx;
      else if (value == "mock") c.backend_type = InferenceBackendType::Mock;
    }
    else if (key == "resize_width") c.resize_width = static_cast<std::uint32_t>(std::stoul(value));
    else if (key == "resize_height") c.resize_height = static_cast<std::uint32_t>(std::stoul(value));
    else if (key == "normalize_mean") c.normalize_mean = std::stof(value);
    else if (key == "normalize_scale") c.normalize_scale = std::stof(value);
    else if (key == "confidence_threshold") c.confidence_threshold = std::stof(value);
  }
  return c;
}

}  // namespace normitri::app
