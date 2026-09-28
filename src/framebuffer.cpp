#include "epaper/framebuffer.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "epaper/errors.hpp"

namespace epaper {

namespace {

constexpr std::uint8_t kWhiteByte = 0xFF;
constexpr std::uint8_t kBlackByte = 0x00;
constexpr std::size_t kMaxDimensionDigits = 5;
constexpr unsigned long kMaxDimension = 0xFFFF;  // NOLINT(runtime/int)

std::uint8_t bit_mask(int x) noexcept {
  return static_cast<std::uint8_t>(0x80u >> (static_cast<unsigned>(x) & 7u));
}

bool is_space(char c) noexcept {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool is_digit(char c) noexcept {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

std::uint16_t parse_dimension(const std::string& token) {
  if (token.empty() || token.size() > kMaxDimensionDigits || !std::all_of(token.begin(), token.end(), is_digit)) {
    throw Error("invalid PBM dimension '" + token + "'");
  }
  const auto value = std::stoul(token);
  if (value == 0 || value > kMaxDimension) {
    throw Error("PBM dimension out of range: " + token);
  }
  return static_cast<std::uint16_t>(value);
}

}  // namespace

Framebuffer::Framebuffer(std::uint16_t width, std::uint16_t height, Color fill_color)
: _width(width),
  _height(height),
  _stride((static_cast<std::size_t>(width) + 7) / 8),
  _data(_stride * height, kWhiteByte) {
  if (width == 0 || height == 0) {
    throw std::invalid_argument("framebuffer dimensions must be non-zero");
  }
  fill(fill_color);
}

std::uint16_t Framebuffer::width() const noexcept {
  return _width;
}

std::uint16_t Framebuffer::height() const noexcept {
  return _height;
}

std::size_t Framebuffer::stride() const noexcept {
  return _stride;
}

void Framebuffer::fill(Color color) {
  std::fill(_data.begin(), _data.end(), color == Color::kWhite ? kWhiteByte : kBlackByte);
  mask_padding();
}

void Framebuffer::set(int x, int y, Color color) noexcept {
  if (!contains(x, y)) {
    return;
  }
  auto& byte = _data[static_cast<std::size_t>(y) * _stride + static_cast<std::size_t>(x) / 8];
  if (color == Color::kWhite) {
    byte = static_cast<std::uint8_t>(byte | bit_mask(x));
  } else {
    byte = static_cast<std::uint8_t>(byte & ~bit_mask(x));
  }
}

void Framebuffer::flip(int x, int y) noexcept {
  if (!contains(x, y)) {
    return;
  }
  auto& byte = _data[static_cast<std::size_t>(y) * _stride + static_cast<std::size_t>(x) / 8];
  byte = static_cast<std::uint8_t>(byte ^ bit_mask(x));
}

Color Framebuffer::get(int x, int y) const {
  if (!contains(x, y)) {
    throw std::out_of_range(
            "pixel (" + std::to_string(x) + ", " + std::to_string(y) + ") outside of " +
            std::to_string(_width) + "x" + std::to_string(_height) + " framebuffer");
  }
  const auto byte = _data[static_cast<std::size_t>(y) * _stride + static_cast<std::size_t>(x) / 8];
  return (byte & bit_mask(x)) != 0 ? Color::kWhite : Color::kBlack;
}

bool Framebuffer::contains(int x, int y) const noexcept {
  return x >= 0 && y >= 0 && x < _width && y < _height;
}

const std::vector<std::uint8_t>& Framebuffer::data() const noexcept {
  return _data;
}

std::string Framebuffer::to_pbm() const {
  const std::string header = "P4\n" + std::to_string(_width) + " " + std::to_string(_height) + "\n";
  std::string pbm(header.size() + _data.size(), '\0');
  std::copy(header.begin(), header.end(), pbm.begin());
  // PBM uses 1 for black, the panel uses 1 for white.
  std::transform(
    _data.begin(), _data.end(), pbm.begin() + static_cast<std::ptrdiff_t>(header.size()),
    [](std::uint8_t byte) { return static_cast<char>(static_cast<std::uint8_t>(~byte)); });
  return pbm;
}

Framebuffer Framebuffer::from_pbm(const std::string& pbm) {
  std::size_t pos = 0;
  const auto next_token = [&pbm, &pos]() {
      while (pos < pbm.size()) {
        if (is_space(pbm[pos])) {
          ++pos;
        } else if (pbm[pos] == '#') {
          while (pos < pbm.size() && pbm[pos] != '\n') {
            ++pos;
          }
        } else {
          break;
        }
      }
      const auto start = pos;
      while (pos < pbm.size() && !is_space(pbm[pos]) && pbm[pos] != '#') {
        ++pos;
      }
      return pbm.substr(start, pos - start);
    };

  if (next_token() != "P4") {
    throw Error("not a binary PBM (P4) image");
  }
  const auto width = parse_dimension(next_token());
  const auto height = parse_dimension(next_token());
  // Exactly one whitespace character separates the header from the pixel data.
  if (pos >= pbm.size() || !is_space(pbm[pos])) {
    throw Error("truncated PBM header");
  }
  ++pos;

  Framebuffer framebuffer(width, height);
  if (pbm.size() - pos != framebuffer._data.size()) {
    throw Error(
            "PBM pixel data has " + std::to_string(pbm.size() - pos) + " bytes, expected " +
            std::to_string(framebuffer._data.size()));
  }
  for (std::size_t i = 0; i < framebuffer._data.size(); ++i) {
    framebuffer._data[i] = static_cast<std::uint8_t>(~static_cast<std::uint8_t>(pbm[pos + i]));
  }
  framebuffer.mask_padding();
  return framebuffer;
}

bool Framebuffer::operator==(const Framebuffer& other) const noexcept {
  return _width == other._width && _height == other._height && _data == other._data;
}

bool Framebuffer::operator!=(const Framebuffer& other) const noexcept {
  return !(*this == other);
}

void Framebuffer::mask_padding() noexcept {
  const auto used_bits = static_cast<unsigned>(_width % 8);
  if (used_bits == 0) {
    return;
  }
  const auto padding = static_cast<std::uint8_t>(0xFFu >> used_bits);
  for (std::size_t row = 0; row < _height; ++row) {
    auto& byte = _data[row * _stride + _stride - 1];
    byte = static_cast<std::uint8_t>(byte | padding);
  }
}

}  // namespace epaper
