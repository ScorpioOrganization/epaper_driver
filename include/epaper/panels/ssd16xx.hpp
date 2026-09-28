#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "epaper/panel.hpp"

namespace epaper {

/// BUSY polling behaviour of a controller.
struct BusyTiming {
  /// Wait before the first poll.
  std::chrono::milliseconds before{ 0 };
  std::chrono::milliseconds poll{ 10 };
  /// Wait after BUSY went low.
  std::chrono::milliseconds after{ 0 };
};

/**
 * @brief Shared plumbing of Solomon SSD16xx style controllers (SSD1680, IL3820, ...).
 *
 * Owns the PanelIo and implements the 4-wire SPI protocol: DC low for a command byte, DC high for its
 * parameters, BUSY high while the controller works.
 */
class Ssd16xxPanel : public Panel {
public:
  std::uint16_t width() const noexcept override;
  std::uint16_t height() const noexcept override;

protected:
  Ssd16xxPanel(PanelIo io, std::uint16_t width, std::uint16_t height, BusyTiming busy_timing);
  ~Ssd16xxPanel() = default;

  void command(std::uint8_t command);
  void data(std::uint8_t byte);
  void data(std::initializer_list<std::uint8_t> bytes);
  void data(const std::uint8_t* bytes, std::size_t size);

  /// RST high for `high_before`, low for `low`, high for `high_after`.
  void hardware_reset(
    std::chrono::milliseconds high_before, std::chrono::milliseconds low, std::chrono::milliseconds high_after);
  /// Blocks until BUSY is low, throws TimeoutError after `timeout`.
  void wait_busy(std::chrono::milliseconds timeout);
  void delay(std::chrono::milliseconds duration);

  /// Commands 0x44 / 0x45: RAM window in pixels (x is converted to bytes).
  void set_window(std::uint16_t x_start, std::uint16_t y_start, std::uint16_t x_end, std::uint16_t y_end);
  /// Commands 0x4E / 0x4F: RAM address counter.
  void set_cursor(std::uint8_t x_byte, std::uint16_t y);
  /// Streams the framebuffer after `ram_command` (0x24 / 0x26).
  void write_ram(std::uint8_t ram_command, const Framebuffer& framebuffer);
  /// Throws std::invalid_argument unless the framebuffer has the native size.
  void check_size(const Framebuffer& framebuffer) const;

  void power_on();
  /// Cuts PWR (when wired), RST and DC first so the unpowered controller is not fed through them.
  void power_off();

private:
  PanelIo _io;
  std::uint16_t _width;
  std::uint16_t _height;
  BusyTiming _busy_timing;
};

}  // namespace epaper
