#include "epaper/display.hpp"

#include <stdexcept>
#include <utility>

namespace epaper {

namespace {

// Function arguments are evaluated in unspecified order, so the null check cannot live next to the
// width()/height() calls in the member initializer.
Framebuffer make_framebuffer(const std::shared_ptr<Panel>& panel) {
  if (!panel) {
    throw std::invalid_argument("Display needs a panel");
  }
  return Framebuffer(panel->width(), panel->height());
}

}  // namespace

Display::Display(std::shared_ptr<Panel> panel, DisplayOptions options, std::shared_ptr<Clock> clock)
: _panel(std::move(panel)),
  _options(options),
  _clock(std::move(clock)),
  _framebuffer(make_framebuffer(_panel)),
  _canvas(_framebuffer, _options.rotation),
  _shown(_framebuffer) {
  if (!_clock) {
    throw std::invalid_argument("Display needs a clock");
  }
}

Canvas& Display::canvas() noexcept {
  return _canvas;
}

const Framebuffer& Display::framebuffer() const noexcept {
  return _framebuffer;
}

Panel& Display::panel() noexcept {
  return *_panel;
}

const DisplayOptions& Display::options() const noexcept {
  return _options;
}

void Display::begin() {
  wake();
  _needs_full = true;
}

void Display::wake() {
  _panel->init();
  _awake = true;
}

void Display::clear(Color color) {
  _canvas.clear(color);
  refresh(RefreshMode::kFull, true);
}

bool Display::refresh(RefreshMode preferred, bool force) {
  if (!force && _has_shown && _framebuffer == _shown) {
    return false;
  }
  if (!_awake) {
    wake();
  }
  const auto now = _clock->now();
  const auto mode = choose_mode(preferred, now);
  try {
    _panel->display(_framebuffer, mode);
  } catch (...) {
    // Unknown panel state: re-initialize and redraw everything on the next attempt.
    _awake = false;
    _needs_full = true;
    _has_shown = false;
    throw;
  }
  _shown = _framebuffer;
  _has_shown = true;
  if (mode == RefreshMode::kPartial) {
    ++_partials_since_full;
  } else {
    _partials_since_full = 0;
    _needs_full = false;
    _last_full = now;
  }
  return true;
}

void Display::sleep() {
  if (!_awake) {
    return;
  }
  _awake = false;
  // Stays true when sleep() throws: the panel is in an unknown state.
  const bool needed_full = std::exchange(_needs_full, true);
  _panel->sleep();
  _needs_full = needed_full || !_panel->resumes_after_sleep();
}

bool Display::awake() const noexcept {
  return _awake;
}

std::size_t Display::partial_refreshes_since_full() const noexcept {
  return _partials_since_full;
}

RefreshMode Display::choose_mode(RefreshMode preferred, std::chrono::steady_clock::time_point now) const {
  auto mode = preferred;
  if (!_panel->supports(mode)) {
    mode = RefreshMode::kFull;
  }
  if (mode == RefreshMode::kPartial) {
    const bool too_many = _options.full_refresh_every > 0 && _partials_since_full >= _options.full_refresh_every;
    const bool too_old = _options.full_refresh_period.count() > 0 && now - _last_full >= _options.full_refresh_period;
    if (_needs_full || too_many || too_old) {
      mode = RefreshMode::kFull;
    }
  }
  return mode;
}

}  // namespace epaper
