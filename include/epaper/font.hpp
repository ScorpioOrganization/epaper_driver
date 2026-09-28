#pragma once

#include <cstdint>

namespace epaper {

/**
 * @brief Monospaced bitmap font.
 *
 * Glyphs are stored column by column (`width` bytes per glyph), bit 0 of a column byte is the top row,
 * so fonts up to 8 pixels tall are supported. Characters outside of [first, last] are drawn as `fallback`.
 */
struct Font {
  std::uint8_t width;
  std::uint8_t height;
  /// Empty columns between two glyphs.
  std::uint8_t spacing;
  char first;
  char last;
  char fallback;
  const std::uint8_t* glyphs;

  bool has_glyph(char c) const noexcept;
  /// Column data of `c`, or of `fallback` when the font has no glyph for `c`.
  const std::uint8_t * glyph(char c) const noexcept;
};

/// Classic 5x7 font covering printable ASCII (0x20 - 0x7E).
extern const Font kFont5x7;

}  // namespace epaper
