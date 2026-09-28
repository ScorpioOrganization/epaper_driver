#pragma once

#include <cstdint>
#include <string>

#include "epaper/panels/ssd16xx.hpp"

namespace epaper {

/**
 * @brief Waveshare 2.9" e-Paper Module V1 (IL3820), 128x296 portrait, black/white.
 *
 * Supports full and partial refresh (no fast mode). Command sequences and LUTs follow Waveshare's reference
 * driver (EPD_2in9.c, MIT licensed), except that the charge pump is switched off after every refresh (like GxEPD2
 * does) instead of running until sleep(). A panel kept awake between frames is then not left under drive voltage.
 */
class Epd2in9V1 final : public Ssd16xxPanel {
public:
  static constexpr std::uint16_t kWidth = 128;
  static constexpr std::uint16_t kHeight = 296;

  explicit Epd2in9V1(PanelIo io);

  std::string name() const override;
  bool supports(RefreshMode mode) const noexcept override;
  void init() override;
  void display(const Framebuffer& framebuffer, RefreshMode mode) override;
  void sleep() override;
  bool resumes_after_sleep() const noexcept override;

private:
  enum class Lut {
    kNone,
    kFull,
    kPartial,
  };

  void initialize(Lut lut);
  void write_frame(const Framebuffer& framebuffer);
  void turn_on();

  bool _initialized = false;
  Lut _lut = Lut::kNone;
};

}  // namespace epaper
