#include "epaper/panels/epd_2in9_v1.hpp"

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace epaper {

namespace {

using std::chrono::milliseconds;

constexpr milliseconds kCommandTimeout{ 3000 };
constexpr milliseconds kRefreshTimeout{ 10000 };

// LUTs from Waveshare's EPD_2in9.c (MIT), loaded with command 0x32.
constexpr std::uint8_t kLutFull[] = {
  0x50, 0xAA, 0x55, 0xAA, 0x11, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFF, 0xFF, 0x1F, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

constexpr std::uint8_t kLutPartial[] = {
  0x10, 0x18, 0x18, 0x08, 0x18, 0x18,
  0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x13, 0x14, 0x44, 0x12,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static_assert(sizeof(kLutFull) == 30 && sizeof(kLutPartial) == 30, "IL3820 LUT is 30 bytes");

}  // namespace

Epd2in9V1::Epd2in9V1(PanelIo io)
: Ssd16xxPanel(std::move(io), kWidth, kHeight,
    BusyTiming{ milliseconds{ 100 }, milliseconds{ 10 }, milliseconds{ 0 } }) {
}

std::string Epd2in9V1::name() const {
  return "Waveshare 2.9in V1 (IL3820)";
}

bool Epd2in9V1::supports(RefreshMode mode) const noexcept {
  return mode == RefreshMode::kFull || mode == RefreshMode::kPartial;
}

bool Epd2in9V1::resumes_after_sleep() const noexcept {
  // Which of the two alternating frame buffers survives deep sleep is not documented.
  return false;
}

void Epd2in9V1::init() {
  power_on();
  initialize(Lut::kFull);
  _initialized = true;
}

void Epd2in9V1::display(const Framebuffer& framebuffer, RefreshMode mode) {
  check_size(framebuffer);
  if (!_initialized) {
    throw std::logic_error(name() + ": init() must be called before display()");
  }
  if (mode == RefreshMode::kFull) {
    if (_lut != Lut::kFull) {
      initialize(Lut::kFull);
    }
    write_frame(framebuffer);
    turn_on();
    return;
  }
  if (mode == RefreshMode::kPartial) {
    if (_lut != Lut::kPartial) {
      initialize(Lut::kPartial);
    }
    write_frame(framebuffer);
    turn_on();
    // The controller swaps its two frame buffers on every update, keep both in sync with the new image.
    write_frame(framebuffer);
    return;
  }
  throw std::invalid_argument(name() + " does not support " + to_string(mode) + " refresh");
}

void Epd2in9V1::sleep() {
  command(0x10);  // deep sleep
  data(0x01);
  power_off();
  _initialized = false;
  _lut = Lut::kNone;
}

void Epd2in9V1::initialize(Lut lut) {
  hardware_reset(milliseconds{ 200 }, milliseconds{ 2 }, milliseconds{ 200 });
  command(0x01);  // driver output control: 296 gate lines
  data({ 0x27, 0x01, 0x00 });
  command(0x0C);  // booster soft start
  data({ 0xD7, 0xD6, 0x9D });
  command(0x2C);  // VCOM
  data(0xA8);
  command(0x3A);  // dummy line period
  data(0x1A);
  command(0x3B);  // gate line width
  data(0x08);
  command(0x3C);  // border waveform
  data(0x03);
  command(0x11);  // data entry mode: x and y increment
  data(0x03);
  command(0x32);  // LUT
  data(lut == Lut::kPartial ? kLutPartial : kLutFull, sizeof(kLutFull));
  _lut = lut;
}

void Epd2in9V1::write_frame(const Framebuffer& framebuffer) {
  set_window(0, 0, kWidth - 1, kHeight - 1);
  set_cursor(0, 0);
  write_ram(0x24, framebuffer);
}

void Epd2in9V1::turn_on() {
  command(0x22);  // display update control 2
  data(0xC4);
  command(0x20);  // master activation
  command(0xFF);  // terminate frame read/write
  wait_busy(kRefreshTimeout);
  // 0xC4 leaves the charge pump running, which fades and wears the panel while it waits for the next frame.
  command(0x22);  // clock + charge pump on, charge pump + clock off
  data(0xC3);
  command(0x20);
  wait_busy(kCommandTimeout);
}

}  // namespace epaper
