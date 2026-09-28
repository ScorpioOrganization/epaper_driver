#include "epaper/panels/ssd16xx.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "epaper/errors.hpp"

namespace epaper {

Ssd16xxPanel::Ssd16xxPanel(PanelIo io, std::uint16_t width, std::uint16_t height, BusyTiming busy_timing)
: _io(std::move(io)), _width(width), _height(height), _busy_timing(busy_timing) {
  if (!_io.spi || !_io.reset || !_io.data_command || !_io.busy || !_io.clock) {
    throw std::invalid_argument("panel needs SPI, RST, DC, BUSY and a clock (PWR is optional)");
  }
}

std::uint16_t Ssd16xxPanel::width() const noexcept {
  return _width;
}

std::uint16_t Ssd16xxPanel::height() const noexcept {
  return _height;
}

void Ssd16xxPanel::command(std::uint8_t command) {
  _io.data_command->write(false);
  _io.spi->write(&command, 1);
}

void Ssd16xxPanel::data(std::uint8_t byte) {
  data(&byte, 1);
}

void Ssd16xxPanel::data(std::initializer_list<std::uint8_t> bytes) {
  data(bytes.begin(), bytes.size());
}

void Ssd16xxPanel::data(const std::uint8_t* bytes, std::size_t size) {
  _io.data_command->write(true);
  _io.spi->write(bytes, size);
}

void Ssd16xxPanel::hardware_reset(
  std::chrono::milliseconds high_before, std::chrono::milliseconds low, std::chrono::milliseconds high_after) {
  _io.reset->write(true);
  delay(high_before);
  _io.reset->write(false);
  delay(low);
  _io.reset->write(true);
  delay(high_after);
}

void Ssd16xxPanel::wait_busy(std::chrono::milliseconds timeout) {
  delay(_busy_timing.before);
  const auto deadline = _io.clock->now() + timeout;
  while (_io.busy->read()) {
    if (_io.clock->now() >= deadline) {
      throw TimeoutError(name() + ": BUSY still high after " + std::to_string(timeout.count()) + " ms");
    }
    _io.clock->sleep_for(_busy_timing.poll);
  }
  delay(_busy_timing.after);
}

void Ssd16xxPanel::delay(std::chrono::milliseconds duration) {
  if (duration.count() > 0) {
    _io.clock->sleep_for(duration);
  }
}

void Ssd16xxPanel::set_window(std::uint16_t x_start, std::uint16_t y_start, std::uint16_t x_end, std::uint16_t y_end) {
  command(0x44);
  data({ static_cast<std::uint8_t>((x_start >> 3) & 0xFF), static_cast<std::uint8_t>((x_end >> 3) & 0xFF) });
  command(0x45);
  data(
    {
      static_cast<std::uint8_t>(y_start & 0xFF), static_cast<std::uint8_t>((y_start >> 8) & 0xFF),
      static_cast<std::uint8_t>(y_end & 0xFF), static_cast<std::uint8_t>((y_end >> 8) & 0xFF),
    });
}

void Ssd16xxPanel::set_cursor(std::uint8_t x_byte, std::uint16_t y) {
  command(0x4E);
  data(x_byte);
  command(0x4F);
  data({ static_cast<std::uint8_t>(y & 0xFF), static_cast<std::uint8_t>((y >> 8) & 0xFF) });
}

void Ssd16xxPanel::write_ram(std::uint8_t ram_command, const Framebuffer& framebuffer) {
  command(ram_command);
  data(framebuffer.data().data(), framebuffer.data().size());
}

void Ssd16xxPanel::check_size(const Framebuffer& framebuffer) const {
  if (framebuffer.width() != _width || framebuffer.height() != _height) {
    throw std::invalid_argument(
            name() + " needs a " + std::to_string(_width) + "x" + std::to_string(_height) + " framebuffer, got " +
            std::to_string(framebuffer.width()) + "x" + std::to_string(framebuffer.height()));
  }
}

void Ssd16xxPanel::power_on() {
  if (_io.power) {
    _io.power->write(true);
  }
}

void Ssd16xxPanel::power_off() {
  if (_io.power) {
    // Same order as Waveshare's module_exit().
    _io.reset->write(false);
    _io.data_command->write(false);
    _io.power->write(false);
  }
}

}  // namespace epaper
