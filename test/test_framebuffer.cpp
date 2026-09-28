#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "epaper/errors.hpp"
#include "epaper/framebuffer.hpp"

namespace epaper {
namespace {

TEST(Framebuffer, GeometryAndDefaultFill) {
  const Framebuffer framebuffer(128, 296);
  EXPECT_EQ(framebuffer.width(), 128);
  EXPECT_EQ(framebuffer.height(), 296);
  EXPECT_EQ(framebuffer.stride(), 16u);
  ASSERT_EQ(framebuffer.data().size(), 16u * 296u);
  for (const auto byte : framebuffer.data()) {
    ASSERT_EQ(byte, 0xFF);
  }
}

TEST(Framebuffer, RejectsEmptySize) {
  EXPECT_THROW(Framebuffer(0, 10), std::invalid_argument);
  EXPECT_THROW(Framebuffer(10, 0), std::invalid_argument);
}

TEST(Framebuffer, SetGetAndBitLayout) {
  Framebuffer framebuffer(16, 2);
  framebuffer.set(0, 0, Color::kBlack);
  framebuffer.set(9, 1, Color::kBlack);
  // MSB is the leftmost pixel, 0 is black.
  EXPECT_EQ(framebuffer.data()[0], 0x7F);
  EXPECT_EQ(framebuffer.data()[3], 0xBF);
  EXPECT_EQ(framebuffer.get(0, 0), Color::kBlack);
  EXPECT_EQ(framebuffer.get(1, 0), Color::kWhite);
  EXPECT_EQ(framebuffer.get(9, 1), Color::kBlack);

  framebuffer.set(0, 0, Color::kWhite);
  EXPECT_EQ(framebuffer.get(0, 0), Color::kWhite);
  EXPECT_EQ(framebuffer.data()[0], 0xFF);
}

TEST(Framebuffer, OutOfRangeAccess) {
  Framebuffer framebuffer(8, 8);
  const auto before = framebuffer;
  framebuffer.set(-1, 0, Color::kBlack);
  framebuffer.set(0, -1, Color::kBlack);
  framebuffer.set(8, 0, Color::kBlack);
  framebuffer.set(0, 8, Color::kBlack);
  framebuffer.flip(8, 8);
  EXPECT_EQ(framebuffer, before);
  EXPECT_THROW(framebuffer.get(8, 0), std::out_of_range);
  EXPECT_THROW(framebuffer.get(0, -1), std::out_of_range);
  EXPECT_FALSE(framebuffer.contains(-1, 3));
  EXPECT_TRUE(framebuffer.contains(7, 7));
}

TEST(Framebuffer, FlipAndFill) {
  Framebuffer framebuffer(8, 1);
  framebuffer.flip(3, 0);
  EXPECT_EQ(framebuffer.get(3, 0), Color::kBlack);
  framebuffer.flip(3, 0);
  EXPECT_EQ(framebuffer.get(3, 0), Color::kWhite);
  framebuffer.fill(Color::kBlack);
  EXPECT_EQ(framebuffer.data()[0], 0x00);
  framebuffer.fill(Color::kWhite);
  EXPECT_EQ(framebuffer.data()[0], 0xFF);
}

TEST(Framebuffer, PaddingStaysWhite) {
  Framebuffer framebuffer(10, 2, Color::kBlack);
  ASSERT_EQ(framebuffer.stride(), 2u);
  // Only the two leftmost bits of the second byte are pixels, the rest is padding.
  EXPECT_EQ(framebuffer.data()[1], 0x3F);
  EXPECT_EQ(framebuffer.data()[3], 0x3F);
  EXPECT_EQ(framebuffer, Framebuffer(10, 2, Color::kBlack));
}

TEST(Framebuffer, Equality) {
  Framebuffer a(8, 8);
  Framebuffer b(8, 8);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);
  b.set(1, 1, Color::kBlack);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a != b);
  EXPECT_NE(Framebuffer(8, 8), Framebuffer(8, 16));
  EXPECT_NE(Framebuffer(8, 8), Framebuffer(16, 4));
}

TEST(Framebuffer, PbmRoundTrip) {
  Framebuffer framebuffer(10, 3);
  framebuffer.set(0, 0, Color::kBlack);
  framebuffer.set(9, 2, Color::kBlack);
  const auto pbm = framebuffer.to_pbm();
  ASSERT_EQ(pbm.substr(0, 8), "P4\n10 3\n");
  // PBM: 1 is black.
  EXPECT_EQ(static_cast<unsigned char>(pbm[8]), 0x80);
  EXPECT_EQ(Framebuffer::from_pbm(pbm), framebuffer);
}

TEST(Framebuffer, PbmHeaderWithComments) {
  const std::string pbm = std::string("P4 # made by hand\n# another comment\n8\t1 ") + static_cast<char>(0x01);
  const auto framebuffer = Framebuffer::from_pbm(pbm);
  EXPECT_EQ(framebuffer.width(), 8);
  EXPECT_EQ(framebuffer.height(), 1);
  EXPECT_EQ(framebuffer.get(7, 0), Color::kBlack);
  EXPECT_EQ(framebuffer.get(6, 0), Color::kWhite);
}

TEST(Framebuffer, MalformedPbm) {
  EXPECT_THROW(Framebuffer::from_pbm(""), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P1\n8 1\n0"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\nx 1\n"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n8\n"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n0 1\n"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n70000 1\n"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n123456 1\n"), Error);
  // Header must be followed by exactly one whitespace character.
  EXPECT_THROW(Framebuffer::from_pbm("P4\n8 1"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n8 1#comment\n"), Error);
  // Pixel data too short / too long.
  EXPECT_THROW(Framebuffer::from_pbm("P4\n16 1\nx"), Error);
  EXPECT_THROW(Framebuffer::from_pbm("P4\n8 1\nxx"), Error);
}

}  // namespace
}  // namespace epaper
