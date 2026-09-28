#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "epaper/canvas.hpp"
#include "epaper/font.hpp"
#include "epaper/framebuffer.hpp"

namespace epaper {
namespace {

int count_black(const Canvas& canvas) {
  int count = 0;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      count += canvas.get_pixel(x, y) == Color::kBlack ? 1 : 0;
    }
  }
  return count;
}

/// Rows of '#' (black) and '.' (white), handy in failure messages.
std::string ascii(const Canvas& canvas, int x, int y, int width, int height) {
  std::string out;
  for (int row = y; row < y + height; ++row) {
    for (int column = x; column < x + width; ++column) {
      out += canvas.get_pixel(column, row) == Color::kBlack ? '#' : '.';
    }
    out += '\n';
  }
  return out;
}

TEST(Canvas, SizeFollowsRotation) {
  Framebuffer framebuffer(128, 296);
  EXPECT_EQ(Canvas(framebuffer).width(), 128);
  EXPECT_EQ(Canvas(framebuffer).height(), 296);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k90).width(), 296);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k90).height(), 128);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k180).width(), 128);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k270).width(), 296);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k270).height(), 128);
  EXPECT_EQ(Canvas(framebuffer, Rotation::k270).rotation(), Rotation::k270);
  Canvas canvas(framebuffer);
  EXPECT_EQ(&canvas.framebuffer(), &framebuffer);
}

TEST(Canvas, RotationMapping) {
  Framebuffer framebuffer(8, 16);
  // Logical top left corner lands on a different native corner for every rotation.
  Canvas(framebuffer, Rotation::k0).draw_pixel(0, 0, Color::kBlack);
  EXPECT_EQ(framebuffer.get(0, 0), Color::kBlack);
  framebuffer.fill(Color::kWhite);

  Canvas(framebuffer, Rotation::k90).draw_pixel(0, 0, Color::kBlack);
  EXPECT_EQ(framebuffer.get(7, 0), Color::kBlack);
  framebuffer.fill(Color::kWhite);

  Canvas(framebuffer, Rotation::k180).draw_pixel(0, 0, Color::kBlack);
  EXPECT_EQ(framebuffer.get(7, 15), Color::kBlack);
  framebuffer.fill(Color::kWhite);

  Canvas(framebuffer, Rotation::k270).draw_pixel(0, 0, Color::kBlack);
  EXPECT_EQ(framebuffer.get(0, 15), Color::kBlack);
  framebuffer.fill(Color::kWhite);

  // Moving right on a 90 degree canvas moves down on the panel.
  Canvas(framebuffer, Rotation::k90).draw_pixel(5, 2, Color::kBlack);
  EXPECT_EQ(framebuffer.get(5, 5), Color::kBlack);
}

TEST(Canvas, ClippingAndGetPixel) {
  Framebuffer framebuffer(8, 8);
  Canvas canvas(framebuffer, Rotation::k90);
  canvas.draw_pixel(-1, 0, Color::kBlack);
  canvas.draw_pixel(0, 8, Color::kBlack);
  canvas.draw_pixel(8, 0, Color::kBlack);
  canvas.fill_rect(-10, -10, 5, 5, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 0);
  EXPECT_THROW(canvas.get_pixel(8, 0), std::out_of_range);
  EXPECT_THROW(canvas.get_pixel(0, -1), std::out_of_range);

  canvas.fill_rect(-2, -2, 4, 4, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 4);
  canvas.fill_rect(6, 6, 10, 10, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 8);
}

TEST(Canvas, ClearFillsEverything) {
  Framebuffer framebuffer(8, 4);
  Canvas canvas(framebuffer);
  canvas.clear(Color::kBlack);
  EXPECT_EQ(count_black(canvas), 32);
  canvas.clear();
  EXPECT_EQ(count_black(canvas), 0);
}

TEST(Canvas, Lines) {
  Framebuffer framebuffer(16, 16);
  Canvas canvas(framebuffer);
  canvas.draw_hline(2, 3, 5, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 5);
  canvas.draw_vline(0, 0, 4, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 9);
  canvas.draw_hline(0, 10, 0, Color::kBlack);
  canvas.draw_vline(0, 10, -3, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 9);

  canvas.clear();
  canvas.draw_line(0, 0, 7, 7, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 8);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(canvas.get_pixel(i, i), Color::kBlack);
  }

  // Reversed direction, steep slope.
  canvas.clear();
  canvas.draw_line(3, 12, 1, 2, Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(3, 12), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(1, 2), Color::kBlack);
  EXPECT_EQ(count_black(canvas), 11);

  // Single point.
  canvas.clear();
  canvas.draw_line(4, 4, 4, 4, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 1);
}

TEST(Canvas, Rectangles) {
  Framebuffer framebuffer(16, 16);
  Canvas canvas(framebuffer);
  canvas.draw_rect(1, 1, 5, 4, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 14);
  EXPECT_EQ(canvas.get_pixel(3, 2), Color::kWhite);
  canvas.draw_rect(0, 0, 0, 5, Color::kBlack);
  canvas.draw_rect(0, 0, 5, -1, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 14);

  canvas.clear();
  canvas.fill_rect(1, 1, 5, 4, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 20);
  canvas.fill_rect(2, 2, 1, 1, Color::kWhite);
  EXPECT_EQ(count_black(canvas), 19);

  canvas.clear();
  canvas.invert_rect(0, 0, 4, 4);
  EXPECT_EQ(count_black(canvas), 16);
  // 14x14 clipped region, its 2x2 overlap with the first one flips back to white.
  canvas.invert_rect(2, 2, 20, 20);
  EXPECT_EQ(count_black(canvas), 16 - 4 + (14 * 14 - 4));
}

TEST(Canvas, TextMetrics) {
  EXPECT_EQ(Canvas::text_width(""), 0);
  EXPECT_EQ(Canvas::text_width("A"), 5);
  EXPECT_EQ(Canvas::text_width("AB"), 11);
  EXPECT_EQ(Canvas::text_width("AB", kFont5x7, 2), 22);
  EXPECT_EQ(Canvas::text_width("AB", kFont5x7, 0), 0);
  EXPECT_EQ(Canvas::text_height(), 7);
  EXPECT_EQ(Canvas::text_height(kFont5x7, 3), 21);
  EXPECT_EQ(Canvas::text_height(kFont5x7, 0), 0);
}

TEST(Canvas, DrawText) {
  Framebuffer framebuffer(32, 16);
  Canvas canvas(framebuffer);
  EXPECT_EQ(canvas.draw_text(1, 1, "1"), 6);
  // '1' is a full height stem in its middle column plus a foot and a flag.
  for (int row = 1; row < 8; ++row) {
    EXPECT_EQ(canvas.get_pixel(3, row), Color::kBlack) << ascii(canvas, 0, 0, 8, 9);
  }
  EXPECT_EQ(count_black(canvas), 7 + 1 + 1 + 1);

  canvas.clear();
  EXPECT_EQ(canvas.draw_text(0, 0, "--", kFont5x7, 2), 22);
  // Two scaled dashes: 2 x (5 columns * 2) wide, 2 rows tall, separated by 2 blank columns.
  EXPECT_EQ(count_black(canvas), 2 * 10 * 2);
  EXPECT_EQ(canvas.get_pixel(10, 6), Color::kWhite);
  EXPECT_EQ(canvas.get_pixel(12, 6), Color::kBlack);

  canvas.clear();
  EXPECT_EQ(canvas.draw_text(3, 0, "X", kFont5x7, 0), 3);
  EXPECT_EQ(count_black(canvas), 0);

  canvas.clear(Color::kBlack);
  canvas.draw_text(0, 0, "-", kFont5x7, 1, Color::kWhite);
  EXPECT_EQ(canvas.get_pixel(0, 3), Color::kWhite);
}

TEST(Canvas, DrawBitmap) {
  Framebuffer framebuffer(16, 8);
  Canvas canvas(framebuffer);
  const std::uint8_t arrow[] = {
    0b10000000, 0b01000000,
    0b11111111, 0b11000000,
    0b10000000, 0b01000000,
  };
  canvas.draw_bitmap(2, 1, 10, 3, arrow, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 2 + 10 + 2) << ascii(canvas, 0, 0, 16, 8);
  EXPECT_EQ(canvas.get_pixel(2, 1), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(11, 1), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(3, 1), Color::kWhite);

  canvas.clear();
  canvas.draw_bitmap(0, 0, 10, 3, nullptr, Color::kBlack);
  canvas.draw_bitmap(0, 0, 0, 3, arrow, Color::kBlack);
  canvas.draw_bitmap(0, 0, 10, 0, arrow, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 0);
}

TEST(Canvas, SevenSegment) {
  const SevenSegmentStyle style{ 10, 20, 2, 3 };
  EXPECT_EQ(Canvas::seven_segment_width("", style), 0);
  EXPECT_EQ(Canvas::seven_segment_width("8", style), 10);
  EXPECT_EQ(Canvas::seven_segment_width("25.2", style), 10 + 3 + 10 + 3 + 2 + 3 + 10);
  EXPECT_EQ(Canvas::seven_segment_width("1:2", style), 10 + 3 + 2 + 3 + 10);

  Framebuffer framebuffer(64, 32);
  Canvas canvas(framebuffer);
  EXPECT_EQ(canvas.draw_seven_segment(1, 1, "8", style, Color::kBlack), 11);
  // "8" lights every segment: the outline and the middle bar, but not the two holes.
  EXPECT_EQ(canvas.get_pixel(1, 1), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(10, 20), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(5, 10), Color::kBlack) << ascii(canvas, 0, 0, 12, 22);
  EXPECT_EQ(canvas.get_pixel(5, 5), Color::kWhite);
  EXPECT_EQ(canvas.get_pixel(5, 15), Color::kWhite);

  canvas.clear();
  canvas.draw_seven_segment(0, 0, "-", style, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 10 * 2);

  canvas.clear();
  canvas.draw_seven_segment(0, 0, "1", style, Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(9, 0), Color::kBlack);
  EXPECT_EQ(canvas.get_pixel(0, 0), Color::kWhite);

  canvas.clear();
  canvas.draw_seven_segment(0, 0, " x", style, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 0);

  canvas.clear();
  EXPECT_EQ(canvas.draw_seven_segment(0, 0, ".", style, Color::kBlack), 2);
  EXPECT_EQ(count_black(canvas), 4);
  EXPECT_EQ(canvas.get_pixel(0, 19), Color::kBlack);

  canvas.clear();
  canvas.draw_seven_segment(0, 0, ":", style, Color::kBlack);
  EXPECT_EQ(count_black(canvas), 8);

  // Every digit lights a distinct segment combination.
  std::string previous;
  for (char digit = '0'; digit <= '9'; ++digit) {
    canvas.clear();
    canvas.draw_seven_segment(0, 0, std::string(1, digit), style, Color::kBlack);
    const auto image = ascii(canvas, 0, 0, 10, 20);
    EXPECT_NE(image, previous) << digit;
    previous = image;
  }
}

}  // namespace
}  // namespace epaper
