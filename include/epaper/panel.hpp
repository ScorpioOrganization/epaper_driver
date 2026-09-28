#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "epaper/framebuffer.hpp"
#include "epaper/hal.hpp"
#include "epaper/types.hpp"

namespace epaper {

/// Hardware a SPI e-paper module is wired to.
struct PanelIo {
  std::shared_ptr<SpiDevice> spi;
  /// RST, active low.
  std::shared_ptr<OutputPin> reset;
  /// DC, low = command, high = data.
  std::shared_ptr<OutputPin> data_command;
  /// BUSY, high while the controller is working.
  std::shared_ptr<InputPin> busy;
  /// Optional PWR line of Waveshare Rev2.1+ boards, high while in use. Null when PWR is tied to 3.3 V (or absent).
  std::shared_ptr<OutputPin> power;
  std::shared_ptr<Clock> clock;
};

/**
 * @brief A physical (or virtual) panel.
 *
 * Framebuffers passed to display() must have the panel's native width() x height().
 * init() must be called before the first display() and again after sleep().
 */
class Panel {
public:
  virtual std::string name() const = 0;
  virtual std::uint16_t width() const noexcept = 0;
  virtual std::uint16_t height() const noexcept = 0;
  virtual bool supports(RefreshMode mode) const noexcept = 0;

  /// Powers up, resets and configures the controller.
  virtual void init() = 0;
  /// Shows `framebuffer`. Throws std::invalid_argument for a wrong size or an unsupported mode.
  virtual void display(const Framebuffer& framebuffer, RefreshMode mode) = 0;
  /// Deep sleep: the image stays visible without power, init() is needed before the next display().
  virtual void sleep() = 0;
  /**
   * @brief The first refresh after sleep() + init() may be partial.
   *
   * A partial refresh is computed against the previous frame, which most controllers lose in deep sleep (and
   * every controller loses when its supply is cut). Panels returning false need a full refresh after waking up.
   */
  virtual bool resumes_after_sleep() const noexcept = 0;

protected:
  /// Owned through std::shared_ptr, see hal.hpp.
  ~Panel() = default;
};

}  // namespace epaper
