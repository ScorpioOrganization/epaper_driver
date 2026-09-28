#include "epaper/panels/pbm_file_panel.hpp"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "epaper/errors.hpp"

namespace epaper {

PbmFilePanel::PbmFilePanel(std::string path, std::uint16_t width, std::uint16_t height)
: _path(std::move(path)), _width(width), _height(height) {
  if (_path.empty() || width == 0 || height == 0) {
    throw std::invalid_argument("PbmFilePanel needs a path and a non-zero size");
  }
}

std::string PbmFilePanel::name() const {
  return "PBM file " + _path;
}

std::uint16_t PbmFilePanel::width() const noexcept {
  return _width;
}

std::uint16_t PbmFilePanel::height() const noexcept {
  return _height;
}

bool PbmFilePanel::supports(RefreshMode /*mode*/) const noexcept {
  return true;
}

void PbmFilePanel::init() {
  _initialized = true;
  ++_init_count;
}

void PbmFilePanel::display(const Framebuffer& framebuffer, RefreshMode mode) {
  if (framebuffer.width() != _width || framebuffer.height() != _height) {
    throw std::invalid_argument(name() + ": framebuffer size does not match the panel");
  }
  if (!_initialized) {
    throw std::logic_error(name() + ": init() must be called before display()");
  }
  const auto temporary = _path + ".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << framebuffer.to_pbm();
    file.close();
    if (!file) {
      throw Error("cannot write " + temporary);
    }
  }
  if (std::rename(temporary.c_str(), _path.c_str()) != 0) {
    const int error_number = errno;
    std::remove(temporary.c_str());
    throw SystemError("cannot replace " + _path, error_number);
  }
  ++_display_count;
  _last_mode = mode;
}

void PbmFilePanel::sleep() {
  _initialized = false;
  ++_sleep_count;
}

bool PbmFilePanel::resumes_after_sleep() const noexcept {
  return true;
}

const std::string& PbmFilePanel::path() const noexcept {
  return _path;
}

bool PbmFilePanel::initialized() const noexcept {
  return _initialized;
}

std::size_t PbmFilePanel::init_count() const noexcept {
  return _init_count;
}

std::size_t PbmFilePanel::sleep_count() const noexcept {
  return _sleep_count;
}

std::size_t PbmFilePanel::display_count() const noexcept {
  return _display_count;
}

std::optional<RefreshMode> PbmFilePanel::last_mode() const noexcept {
  return _last_mode;
}

}  // namespace epaper
