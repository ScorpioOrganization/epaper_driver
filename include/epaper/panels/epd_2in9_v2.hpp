#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "epaper/panels/ssd16xx.hpp"

namespace epaper {

/**
 * @brief Waveshare 2.9" e-Paper Module V2 / V3 / Rev2.x (SSD1680), 128x296 portrait, black/white.
 *
 * Supports full (~3 s), fast (~1.5 s) and partial (~0.5 s) refresh. Command sequences and waveform tables
 * follow Waveshare's reference drivers (EPD_2in9_V2.c, epd2in9_V2.py, MIT licensed), with two additions:
 * - Back to back partial refreshes skip the reset, waveform upload and separate power up the reference repeats
 *   every time: only the new frame is sent, then one update that powers the analog part up and down again.
 * - The driver keeps a copy of the last frame. After sleep() + init() (even with PWR cut) it writes that frame
 *   back into both controller RAM banks, so the first refresh after waking up can still be partial.
 */
class Epd2in9V2 final : public Ssd16xxPanel {
public:
  static constexpr std::uint16_t kWidth = 128;
  static constexpr std::uint16_t kHeight = 296;

  explicit Epd2in9V2(PanelIo io);

  std::string name() const override;
  bool supports(RefreshMode mode) const noexcept override;
  void init() override;
  /// A partial refresh before any frame was shown has nothing to be computed against: it is done as a full one.
  void display(const Framebuffer& framebuffer, RefreshMode mode) override;
  void sleep() override;
  bool resumes_after_sleep() const noexcept override;

private:
  /// Waveform currently loaded into the controller.
  enum class Waveform {
    kNone,
    kFull,
    kFast,
    kPartial,
  };

  void initialize(Waveform waveform);
  void load_waveform(const std::uint8_t* table);
  void write_both_rams(const Framebuffer& framebuffer);
  void display_full(const Framebuffer& framebuffer, Waveform waveform);
  void display_partial(const Framebuffer& framebuffer);

  bool _initialized = false;
  Waveform _waveform = Waveform::kNone;
  /// The controller RAM holds _last_frame. Cleared by init(): the RAM may not have survived sleep().
  bool _rams_hold_last_frame = false;
  /// Frame on the panel, the "previous" image of the next partial refresh.
  std::optional<Framebuffer> _last_frame;
};

}  // namespace epaper
