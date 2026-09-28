#include "epaper/types.hpp"

#include <stdexcept>
#include <string>

namespace epaper {

Rotation rotation_from_degrees(int degrees) {
  switch (degrees) {
    case 0:
      return Rotation::k0;
    case 90:
      return Rotation::k90;
    case 180:
      return Rotation::k180;
    case 270:
      return Rotation::k270;
    default:
      throw std::invalid_argument("rotation must be 0, 90, 180 or 270 degrees, got " + std::to_string(degrees));
  }
}

const char * to_string(RefreshMode mode) noexcept {
  switch (mode) {
    case RefreshMode::kFull:
      return "full";
    case RefreshMode::kFast:
      return "fast";
    case RefreshMode::kPartial:
      return "partial";
  }
  return "unknown";
}

}  // namespace epaper
