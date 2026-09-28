#pragma once

#include <cstdint>
#include <string_view>

#include "epaper/font.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/types.hpp"

namespace epaper {

/// Geometry of seven segment digits drawn by Canvas::draw_seven_segment.
struct SevenSegmentStyle {
  int digit_width;
  int digit_height;
  int thickness;
  /// Gap between two characters.
  int spacing;
};

/**
 * @brief Drawing surface over a Framebuffer.
 *
 * Coordinates are logical: (0, 0) is the top left corner after applying the rotation, so a portrait
 * 128x296 panel used with Rotation::k90 becomes a 296x128 landscape canvas. Everything is clipped, drawing
 * outside of the canvas is never an error.
 */
class Canvas {
public:
  explicit Canvas(Framebuffer& framebuffer, Rotation rotation = Rotation::k0);

  int width() const noexcept;
  int height() const noexcept;
  Rotation rotation() const noexcept;
  Framebuffer& framebuffer() noexcept;

  void clear(Color color = Color::kWhite);
  void draw_pixel(int x, int y, Color color);
  /// Throws std::out_of_range for coordinates outside of the canvas.
  Color get_pixel(int x, int y) const;

  void draw_hline(int x, int y, int length, Color color);
  void draw_vline(int x, int y, int length, Color color);
  void draw_line(int x0, int y0, int x1, int y1, Color color);
  void draw_rect(int x, int y, int width, int height, Color color);
  void fill_rect(int x, int y, int width, int height, Color color);
  void invert_rect(int x, int y, int width, int height);

  /**
   * @brief Draws `text` with its top left corner at (x, y), every font pixel scaled to `scale` x `scale`.
   * @return x coordinate right after the last glyph.
   */
  int draw_text(
    int x, int y, std::string_view text, const Font& font = kFont5x7, int scale = 1,
    Color color = Color::kBlack);
  static int text_width(std::string_view text, const Font& font = kFont5x7, int scale = 1) noexcept;
  static int text_height(const Font& font = kFont5x7, int scale = 1) noexcept;

  /// Draws the set bits of a row major, MSB first, byte padded bitmap in `color`.
  void draw_bitmap(int x, int y, int width, int height, const std::uint8_t* bits, Color color);

  /**
   * @brief Draws `text` as seven segment characters (0-9, '-', ' ', '.', ':'). Other characters are left blank.
   * @return x coordinate right after the last character.
   */
  int draw_seven_segment(int x, int y, std::string_view text, const SevenSegmentStyle& style, Color color);
  static int seven_segment_width(std::string_view text, const SevenSegmentStyle& style) noexcept;

private:
  bool map(int x, int y, int& native_x, int& native_y) const noexcept;
  void flip_pixel(int x, int y);

  Framebuffer& _framebuffer;
  Rotation _rotation;
};

}  // namespace epaper
