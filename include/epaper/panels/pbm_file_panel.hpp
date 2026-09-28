#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "epaper/panel.hpp"

namespace epaper {

/**
 * @brief Virtual panel that writes every displayed frame to a binary PBM file.
 *
 * Lets applications run without hardware (development machines, simulation, tests). The file is replaced
 * atomically (written next to the target, then renamed), so it can be watched by an image viewer.
 */
class PbmFilePanel final : public Panel {
public:
  explicit PbmFilePanel(std::string path, std::uint16_t width = 128, std::uint16_t height = 296);

  std::string name() const override;
  std::uint16_t width() const noexcept override;
  std::uint16_t height() const noexcept override;
  bool supports(RefreshMode mode) const noexcept override;
  void init() override;
  void display(const Framebuffer& framebuffer, RefreshMode mode) override;
  void sleep() override;
  bool resumes_after_sleep() const noexcept override;

  const std::string& path() const noexcept;
  bool initialized() const noexcept;
  std::size_t init_count() const noexcept;
  std::size_t sleep_count() const noexcept;
  std::size_t display_count() const noexcept;
  std::optional<RefreshMode> last_mode() const noexcept;

private:
  std::string _path;
  std::uint16_t _width;
  std::uint16_t _height;
  bool _initialized = false;
  std::size_t _init_count = 0;
  std::size_t _sleep_count = 0;
  std::size_t _display_count = 0;
  std::optional<RefreshMode> _last_mode;
};

}  // namespace epaper
