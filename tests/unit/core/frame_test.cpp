#include <normitri/core/frame.hpp>
#include <gtest/gtest.h>
#include <vector>

namespace nc = normitri::core;

TEST(Frame, DefaultEmpty) {
  nc::Frame f;
  EXPECT_EQ(f.width(), 0u);
  EXPECT_EQ(f.height(), 0u);
  EXPECT_TRUE(f.empty());
  EXPECT_EQ(f.size_bytes(), 0u);
}

TEST(Frame, ConstructFromBuffer) {
  std::vector<std::byte> buf(100 * 100 * 3);
  nc::Frame f(100, 100, nc::PixelFormat::RGB8, std::move(buf));
  EXPECT_EQ(f.width(), 100u);
  EXPECT_EQ(f.height(), 100u);
  EXPECT_EQ(f.format(), nc::PixelFormat::RGB8);
  EXPECT_FALSE(f.empty());
  EXPECT_EQ(f.size_bytes(), 100u * 100 * 3);
  EXPECT_EQ(f.data().size(), 100u * 100 * 3);
}

TEST(Frame, MinBytes) {
  EXPECT_EQ(nc::Frame::min_bytes(10, 10, nc::PixelFormat::Grayscale8), 100u);
  EXPECT_EQ(nc::Frame::min_bytes(10, 10, nc::PixelFormat::RGB8), 300u);
  EXPECT_EQ(nc::Frame::min_bytes(10, 10, nc::PixelFormat::Float32Planar), 10u * 10 * 3 * 4);
}
