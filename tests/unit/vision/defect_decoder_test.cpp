#include <normitri/core/defect.hpp>
#include <normitri/vision/defect_decoder.hpp>
#include <normitri/vision/inference_result.hpp>
#include <gtest/gtest.h>
#include <vector>

namespace nv = normitri::vision;
namespace nc = normitri::core;

TEST(DefectDecoder, EmptyResult) {
  nv::InferenceResult r;
  r.num_detections = 0;
  nv::DefectDecoder dec(0.5f, {nc::DefectKind::WrongItem, nc::DefectKind::WrongQuantity});
  auto out = dec.decode(r);
  EXPECT_TRUE(out.empty());
}

TEST(DefectDecoder, BelowThresholdFiltered) {
  nv::InferenceResult r;
  r.num_detections = 1;
  r.boxes = {0.f, 0.f, 0.1f, 0.1f};
  r.scores = {0.3f};
  r.class_ids = {0};
  nv::DefectDecoder dec(0.5f, {nc::DefectKind::WrongItem});
  auto out = dec.decode(r);
  EXPECT_TRUE(out.empty());
}

TEST(DefectDecoder, AboveThresholdDecoded) {
  nv::InferenceResult r;
  r.num_detections = 1;
  r.boxes = {0.1f, 0.2f, 0.4f, 0.5f};
  r.scores = {0.9f};
  r.class_ids = {0};
  nv::DefectDecoder dec(0.5f, {nc::DefectKind::WrongItem, nc::DefectKind::ProcessError});
  auto out = dec.decode(r);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].kind, nc::DefectKind::WrongItem);
  EXPECT_FLOAT_EQ(out[0].confidence, 0.9f);
  EXPECT_FLOAT_EQ(out[0].bbox.x, 0.1f);
  EXPECT_FLOAT_EQ(out[0].bbox.y, 0.2f);
  EXPECT_FLOAT_EQ(out[0].bbox.w, 0.3f);
  EXPECT_FLOAT_EQ(out[0].bbox.h, 0.3f);
}
