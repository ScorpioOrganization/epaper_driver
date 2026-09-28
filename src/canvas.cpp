#include "epaper/canvas.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace epaper {

namespace {

constexpr std::uint8_t kSegmentA = 1u << 0;  // top
constexpr std::uint8_t kSegmentB = 1u << 1;  // top right
constexpr std::uint8_t kSegmentC = 1u << 2;  // bottom right
constexpr std::uint8_t kSegmentD = 1u << 3;  // bottom
constexpr std::uint8_t kSegmentE = 1u << 4;  // bottom left
constexpr std::uint8_t kSegmentF = 1u << 5;  // top left
constexpr std::uint8_t kSegmentG = 1u << 6;  // middle

constexpr std::array<std::uint8_t, 10> kDigitSegments = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

std::uint8_t segments_for(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return kDigitSegments[static_cast<std::size_t>(c - '0')];
  }
  return c == '-' ? kSegmentG : 0;
}

int seven_segment_char_width(char c, const SevenSegmentStyle& style) noexcept {
  return (c == '.' || c == ':') ? style.thickness : style.digit_width;
}

}  // namespace

Canvas::Canvas(Framebuffer& framebuffer, Rotation rotation)
: _framebuffer(framebuffer), _rotation(rotation) { }

int Canvas::width() const noexcept {
  return (_rotation == Rotation::k90 || _rotation == Rotation::k270) ? _framebuffer.height() : _framebuffer.width();
}

int Canvas::height() const noexcept {
  return (_rotation == Rotation::k90 || _rotation == Rotation::k270) ? _framebuffer.width() : _framebuffer.height();
}

Rotation Canvas::rotation() const noexcept {
  return _rotation;
}

Framebuffer& Canvas::framebuffer() noexcept {
  return _framebuffer;
}

void Canvas::clear(Color color) {
  _framebuffer.fill(color);
}

void Canvas::draw_pixel(int x, int y, Color color) {
  int native_x = 0;
  int native_y = 0;
  if (map(x, y, native_x, native_y)) {
    _framebuffer.set(native_x, native_y, color);
  }
}

Color Canvas::get_pixel(int x, int y) const {
  int native_x = 0;
  int native_y = 0;
  if (!map(x, y, native_x, native_y)) {
    throw std::out_of_range("pixel outside of the canvas");
  }
  return _framebuffer.get(native_x, native_y);
}

void Canvas::draw_hline(int x, int y, int length, Color color) {
  fill_rect(x, y, length, 1, color);
}

void Canvas::draw_vline(int x, int y, int length, Color color) {
  fill_rect(x, y, 1, length, color);
}

void Canvas::draw_line(int x0, int y0, int x1, int y1, Color color) {
  // Bresenham, all octants.
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int step_x = x0 < x1 ? 1 : -1;
  const int step_y = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      return;
    }
    const int doubled = 2 * error;
    if (doubled >= dy) {
      error += dy;
      x0 += step_x;
    }
    if (doubled <= dx) {
      error += dx;
      y0 += step_y;
    }
  }
}

void Canvas::draw_rect(int x, int y, int width, int height, Color color) {
  if (width <= 0 || height <= 0) {
    return;
  }
  draw_hline(x, y, width, color);
  draw_hline(x, y + height - 1, width, color);
  draw_vline(x, y, height, color);
  draw_vline(x + width - 1, y, height, color);
}

void Canvas::fill_rect(int x, int y, int width, int height, Color color) {
  const int left = std::max(x, 0);
  const int top = std::max(y, 0);
  const int right = std::min(x + width, this->width());
  const int bottom = std::min(y + height, this->height());
  for (int row = top; row < bottom; ++row) {
    for (int column = left; column < right; ++column) {
      draw_pixel(column, row, color);
    }
  }
}

void Canvas::invert_rect(int x, int y, int width, int height) {
  const int left = std::max(x, 0);
  const int top = std::max(y, 0);
  const int right = std::min(x + width, this->width());
  const int bottom = std::min(y + height, this->height());
  for (int row = top; row < bottom; ++row) {
    for (int column = left; column < right; ++column) {
      flip_pixel(column, row);
    }
  }
}

int Canvas::draw_text(int x, int y, std::string_view text, const Font& font, int scale, Color color) {
  if (scale < 1) {
    return x;
  }
  int cursor = x;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i != 0) {
      cursor += font.spacing * scale;
    }
    const auto* columns = font.glyph(text[i]);
    for (int column = 0; column < font.width; ++column) {
      for (int row = 0; row < font.height; ++row) {
        if (((columns[column] >> row) & 1u) != 0) {
          fill_rect(cursor + column * scale, y + row * scale, scale, scale, color);
        }
      }
    }
    cursor += font.width * scale;
  }
  return cursor;
}

int Canvas::text_width(std::string_view text, const Font& font, int scale) noexcept {
  if (text.empty() || scale < 1) {
    return 0;
  }
  const auto count = static_cast<int>(text.size());
  return (count * font.width + (count - 1) * font.spacing) * scale;
}

int Canvas::text_height(const Font& font, int scale) noexcept {
  return scale < 1 ? 0 : font.height* scale;
}

void Canvas::draw_bitmap(int x, int y, int width, int height, const std::uint8_t* bits, Color color) {
  if (bits == nullptr || width <= 0 || height <= 0) {
    return;
  }
  const auto stride = static_cast<std::size_t>((width + 7) / 8);
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const auto byte = bits[static_cast<std::size_t>(row) * stride + static_cast<std::size_t>(column / 8)];
      if ((byte & (0x80u >> (column % 8))) != 0) {
        draw_pixel(x + column, y + row, color);
      }
    }
  }
}

int Canvas::draw_seven_segment(int x, int y, std::string_view text, const SevenSegmentStyle& style, Color color) {
  const int w = style.digit_width;
  const int h = style.digit_height;
  const int t = style.thickness;
  // Vertical segments overlap the middle bar so that the corners stay filled.
  const int half = (h + t) / 2;
  int cursor = x;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i != 0) {
      cursor += style.spacing;
    }
    const char c = text[i];
    if (c == '.') {
      fill_rect(cursor, y + h - t, t, t, color);
    } else if (c == ':') {
      fill_rect(cursor, y + h / 3 - t / 2, t, t, color);
      fill_rect(cursor, y + 2 * h / 3 - t / 2, t, t, color);
    } else {
      const auto segments = segments_for(c);
      if ((segments & kSegmentA) != 0) {
        fill_rect(cursor, y, w, t, color);
      }
      if ((segments & kSegmentB) != 0) {
        fill_rect(cursor + w - t, y, t, half, color);
      }
      if ((segments & kSegmentC) != 0) {
        fill_rect(cursor + w - t, y + h - half, t, half, color);
      }
      if ((segments & kSegmentD) != 0) {
        fill_rect(cursor, y + h - t, w, t, color);
      }
      if ((segments & kSegmentE) != 0) {
        fill_rect(cursor, y + h - half, t, half, color);
      }
      if ((segments & kSegmentF) != 0) {
        fill_rect(cursor, y, t, half, color);
      }
      if ((segments & kSegmentG) != 0) {
        fill_rect(cursor, y + (h - t) / 2, w, t, color);
      }
    }
    cursor += seven_segment_char_width(c, style);
  }
  return cursor;
}

int Canvas::seven_segment_width(std::string_view text, const SevenSegmentStyle& style) noexcept {
  int width = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    width += seven_segment_char_width(text[i], style) + (i != 0 ? style.spacing : 0);
  }
  return width;
}

bool Canvas::map(int x, int y, int& native_x, int& native_y) const noexcept {
  if (x < 0 || y < 0 || x >= width() || y >= height()) {
    return false;
  }
  const int native_width = _framebuffer.width();
  const int native_height = _framebuffer.height();
  switch (_rotation) {
    case Rotation::k90:
      native_x = native_width - 1 - y;
      native_y = x;
      break;
    case Rotation::k180:
      native_x = native_width - 1 - x;
      native_y = native_height - 1 - y;
      break;
    case Rotation::k270:
      native_x = y;
      native_y = native_height - 1 - x;
      break;
    default:
      native_x = x;
      native_y = y;
      break;
  }
  return true;
}

void Canvas::flip_pixel(int x, int y) {
  int native_x = 0;
  int native_y = 0;
  if (map(x, y, native_x, native_y)) {
    _framebuffer.flip(native_x, native_y);
  }
}

}  // namespace epaper
