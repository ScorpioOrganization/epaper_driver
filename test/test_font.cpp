#include <gtest/gtest.h>

#include <cstdint>

#include "epaper/font.hpp"

namespace epaper {
namespace {

TEST(Font, Geometry) {
  EXPECT_EQ(kFont5x7.width, 5);
  EXPECT_EQ(kFont5x7.height, 7);
  EXPECT_EQ(kFont5x7.spacing, 1);
  EXPECT_EQ(kFont5x7.first, ' ');
  EXPECT_EQ(kFont5x7.last, '~');
}

TEST(Font, HasGlyph) {
  EXPECT_TRUE(kFont5x7.has_glyph(' '));
  EXPECT_TRUE(kFont5x7.has_glyph('A'));
  EXPECT_TRUE(kFont5x7.has_glyph('~'));
  EXPECT_FALSE(kFont5x7.has_glyph('\n'));
  EXPECT_FALSE(kFont5x7.has_glyph(static_cast<char>(0x7F)));
}

TEST(Font, GlyphLookup) {
  const std::uint8_t* space = kFont5x7.glyph(' ');
  for (int column = 0; column < kFont5x7.width; ++column) {
    EXPECT_EQ(space[column], 0x00);
  }
  // '1': a single full height stem in the middle column.
  EXPECT_EQ(kFont5x7.glyph('1')[2], 0x7F);
  // '-': middle row in every column.
  for (int column = 0; column < kFont5x7.width; ++column) {
    EXPECT_EQ(kFont5x7.glyph('-')[column], 0x08);
  }
  // Glyphs never use the 8th row of the byte.
  for (char c = ' '; c < '~'; ++c) {
    for (int column = 0; column < kFont5x7.width; ++column) {
      EXPECT_EQ(kFont5x7.glyph(c)[column] & 0x80, 0) << c;
    }
  }
}

TEST(Font, MissingGlyphUsesFallback) {
  EXPECT_EQ(kFont5x7.glyph('\t'), kFont5x7.glyph('?'));
  EXPECT_EQ(kFont5x7.glyph(static_cast<char>(0xB0)), kFont5x7.glyph('?'));
}

}  // namespace
}  // namespace epaper
