#pragma once

#include <chrono>
#include <cstddef>
#include <memory>

#include "epaper/canvas.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/hal.hpp"
#include "epaper/panel.hpp"
#include "epaper/types.hpp"

namespace epaper {

struct DisplayOptions {
  Rotation rotation = Rotation::k0;
  /// Partial refreshes allowed before a full one is forced to clear ghosting, 0 = never forced.
  std::size_t full_refresh_every = 50;
  /// Longest time between two full refreshes, 0 = no limit.
  std::chrono::seconds full_refresh_period{ 600 };
};

/**
 * @brief High level API: draw on canvas(), then refresh().
 *
 * Keeps track of what the panel shows, so refresh() is a no-op while nothing changed, and decides when a
 * partial refresh has to be promoted to a full one (first frame after begin(), first frame after waking up a
 * panel that does not resume after sleep, periodic ghosting cleanup, panels without partial support). A
 * sleeping panel is woken up automatically by the next refresh().
 */
class Display {
public:
  explicit Display(
    std::shared_ptr<Panel> panel, DisplayOptions options = { },
    std::shared_ptr<Clock> clock = std::make_shared<SystemClock>());
  Display(const Display&) = delete;
  Display& operator=(const Display&) = delete;

  Canvas& canvas() noexcept;
  const Framebuffer& framebuffer() const noexcept;
  Panel& panel() noexcept;
  const DisplayOptions& options() const noexcept;

  /// Initializes the panel. The next refresh is a full one.
  void begin();
  /// Fills the canvas with `color` and shows it with a full refresh.
  void clear(Color color = Color::kWhite);
  /**
   * @brief Pushes the canvas to the panel.
   * @param preferred refresh mode to use when possible.
   * @param force refresh even when the canvas did not change since the last refresh.
   * @return false when nothing had to be done.
   */
  bool refresh(RefreshMode preferred = RefreshMode::kPartial, bool force = false);
  /**
   * @brief Puts the panel into deep sleep, the image stays visible.
   *
   * The next refresh() wakes it up and stays partial when the panel resumes after sleep (Panel::resumes_after_sleep).
   */
  void sleep();

  bool awake() const noexcept;
  std::size_t partial_refreshes_since_full() const noexcept;

private:
  void wake();
  RefreshMode choose_mode(RefreshMode preferred, std::chrono::steady_clock::time_point now) const;

  std::shared_ptr<Panel> _panel;
  DisplayOptions _options;
  std::shared_ptr<Clock> _clock;
  Framebuffer _framebuffer;
  Canvas _canvas;
  Framebuffer _shown;
  bool _has_shown = false;
  bool _awake = false;
  bool _needs_full = true;
  std::size_t _partials_since_full = 0;
  std::chrono::steady_clock::time_point _last_full{ };
};

}  // namespace epaper
