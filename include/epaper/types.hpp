#pragma once

#include <cstdint>

namespace epaper {

/// Pixel color of a monochrome e-paper panel.
enum class Color : std::uint8_t {
  kBlack = 0,
  kWhite = 1,
};

/// Clockwise rotation applied when drawing on a Canvas.
enum class Rotation : std::uint8_t {
  k0,
  k90,
  k180,
  k270,
};

/// Waveform used for a panel refresh.
enum class RefreshMode : std::uint8_t {
  /// Slow (~2-3 s) full waveform with flashing, removes ghosting.
  kFull,
  /// Faster full waveform (panel dependent), still flashes.
  kFast,
  /// Partial waveform (~0.5 s), no flashing, accumulates ghosting over time.
  kPartial,
};

/// Returns the opposite color.
constexpr Color invert(Color color) noexcept {
  return color == Color::kBlack ? Color::kWhite : Color::kBlack;
}

/// Maps 0/90/180/270 degrees to a Rotation, throws std::invalid_argument for anything else.
Rotation rotation_from_degrees(int degrees);

/// Human readable name of a refresh mode ("full", "fast", "partial").
const char * to_string(RefreshMode mode) noexcept;

}  // namespace epaper
