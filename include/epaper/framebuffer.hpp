#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "epaper/types.hpp"

namespace epaper {

/**
 * @brief 1 bit per pixel image in the panel's native orientation.
 *
 * Rows are stored top to bottom, each row padded to a whole byte. Inside a byte the most significant bit is
 * the leftmost pixel. A set bit is white, a cleared bit is black - the layout SSD16xx controllers expect in
 * their RAM, so the buffer can be streamed to the panel as is. Padding bits are always kept white.
 */
class Framebuffer {
public:
  Framebuffer(std::uint16_t width, std::uint16_t height, Color fill_color = Color::kWhite);

  std::uint16_t width() const noexcept;
  std::uint16_t height() const noexcept;
  /// Bytes per row.
  std::size_t stride() const noexcept;

  void fill(Color color);
  /// Sets a pixel, coordinates outside of the buffer are ignored.
  void set(int x, int y, Color color) noexcept;
  /// Flips a pixel, coordinates outside of the buffer are ignored.
  void flip(int x, int y) noexcept;
  /// Throws std::out_of_range for coordinates outside of the buffer.
  Color get(int x, int y) const;
  bool contains(int x, int y) const noexcept;

  const std::vector<std::uint8_t>& data() const noexcept;

  /// Binary Netpbm (P4) image, handy for previews and golden files.
  std::string to_pbm() const;
  /// Parses a binary Netpbm (P4) image, throws epaper::Error when it is malformed.
  static Framebuffer from_pbm(const std::string& pbm);

  bool operator==(const Framebuffer& other) const noexcept;
  bool operator!=(const Framebuffer& other) const noexcept;

private:
  void mask_padding() noexcept;

  std::uint16_t _width;
  std::uint16_t _height;
  std::size_t _stride;
  std::vector<std::uint8_t> _data;
};

}  // namespace epaper
