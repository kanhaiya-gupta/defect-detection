#include <normitri/core/defect.hpp>
#include <gtest/gtest.h>

namespace nc = normitri::core;

TEST(Defect, DefaultValues) {
  nc::Defect d;
  EXPECT_EQ(d.kind, nc::DefectKind::ProcessError);
  EXPECT_EQ(d.confidence, 0.f);
  EXPECT_FALSE(d.product_id.has_value());
  EXPECT_FALSE(d.category.has_value());
}

TEST(Defect, BBox) {
  nc::BBox b{0.1f, 0.2f, 0.3f, 0.4f};
  EXPECT_EQ(b.x, 0.1f);
  EXPECT_EQ(b.y, 0.2f);
  EXPECT_EQ(b.w, 0.3f);
  EXPECT_EQ(b.h, 0.4f);
}

TEST(Defect, WithOptional) {
  nc::Defect d;
  d.kind = nc::DefectKind::WrongItem;
  d.confidence = 0.95f;
  d.product_id = 42;
  d.category = "produce";
  EXPECT_EQ(d.kind, nc::DefectKind::WrongItem);
  EXPECT_EQ(*d.product_id, 42u);
  EXPECT_EQ(*d.category, "produce");
}
