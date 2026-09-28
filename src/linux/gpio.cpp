#include "epaper/linux/gpio.hpp"

#include <fcntl.h>
#include <linux/gpio.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "epaper/errors.hpp"

namespace epaper::os {

namespace {

constexpr std::size_t kMaxOffsetDigits = 6;

bool is_digit(char c) noexcept {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

}  // namespace

LineSpec LineSpec::parse(const std::string& text) {
  if (text.empty()) {
    throw std::invalid_argument("empty GPIO line spec");
  }
  LineSpec spec;
  const auto colon = text.rfind(':');
  if (colon == std::string::npos) {
    spec.name = text;
    return spec;
  }
  const auto chip = text.substr(0, colon);
  const auto offset = text.substr(colon + 1);
  if (chip.empty() || offset.empty() || offset.size() > kMaxOffsetDigits ||
    !std::all_of(offset.begin(), offset.end(), is_digit)) {
    throw std::invalid_argument("invalid GPIO line spec '" + text + "', expected <line name> or <chip>:<offset>");
  }
  spec.chip = chip;
  spec.offset = std::stoi(offset);
  return spec;
}

std::string LineSpec::to_string() const {
  if (offset >= 0) {
    return chip + ":" + std::to_string(offset);
  }
  return chip.empty() ? name : name + "@" + chip;
}

std::string gpiochip_path(const std::string& chip) {
  if (chip.empty()) {
    throw std::invalid_argument("empty GPIO chip name");
  }
  return chip.front() == '/' ? chip : "/dev/" + chip;
}

LinuxGpioLine::LinuxGpioLine(
  std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, Direction direction, const std::string& consumer,
  bool initial_high)
: _syscalls(std::move(syscalls)) {
  if (!_syscalls) {
    throw std::invalid_argument("LinuxGpioLine needs a Syscalls instance");
  }

  FileDescriptor chip;
  if (spec.offset >= 0) {
    _chip_path = gpiochip_path(spec.chip);
    _offset = static_cast<std::uint32_t>(spec.offset);
    chip = open_or_throw(_syscalls, _chip_path, O_RDWR);
  } else {
    chip = find_by_name(spec);
  }
  _description = _chip_path + ":" + std::to_string(_offset) + " (" + spec.to_string() + ")";

  gpio_v2_line_request request{ };
  request.offsets[0] = _offset;
  request.num_lines = 1;
  consumer.copy(request.consumer, sizeof(request.consumer) - 1);
  if (direction == Direction::kOutput) {
    request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    request.config.num_attrs = 1;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    request.config.attrs[0].attr.values = initial_high ? 1u : 0u;
    request.config.attrs[0].mask = 1u;
  } else {
    request.config.flags = GPIO_V2_LINE_FLAG_INPUT;
  }
  ioctl_or_throw(*_syscalls, chip.get(), GPIO_V2_GET_LINE_IOCTL, &request, "cannot request GPIO line", _description);
  _line = FileDescriptor(_syscalls, request.fd);
}

bool LinuxGpioLine::get() {
  gpio_v2_line_values values{ };
  values.mask = 1u;
  ioctl_or_throw(*_syscalls, _line.get(), GPIO_V2_LINE_GET_VALUES_IOCTL, &values, "cannot read GPIO line",
      _description);
  return (values.bits & 1u) != 0;
}

void LinuxGpioLine::set(bool high) {
  gpio_v2_line_values values{ };
  values.mask = 1u;
  values.bits = high ? 1u : 0u;
  ioctl_or_throw(*_syscalls, _line.get(), GPIO_V2_LINE_SET_VALUES_IOCTL, &values, "cannot write GPIO line",
      _description);
}

const std::string& LinuxGpioLine::chip_path() const noexcept {
  return _chip_path;
}

std::uint32_t LinuxGpioLine::offset() const noexcept {
  return _offset;
}

FileDescriptor LinuxGpioLine::find_by_name(const LineSpec& spec) {
  const auto chips = spec.chip.empty() ?
    _syscalls->list_gpiochips() :
    std::vector<std::string>{ gpiochip_path(spec.chip) };
  for (const auto& path : chips) {
    auto chip = open_or_throw(_syscalls, path, O_RDWR);
    gpiochip_info chip_info{ };
    ioctl_or_throw(*_syscalls, chip.get(), GPIO_GET_CHIPINFO_IOCTL, &chip_info, "cannot read GPIO chip info of", path);
    for (std::uint32_t line = 0; line < chip_info.lines; ++line) {
      gpio_v2_line_info line_info{ };
      line_info.offset = line;
      ioctl_or_throw(*_syscalls,
          chip.get(), GPIO_V2_GET_LINEINFO_IOCTL, &line_info, "cannot read GPIO line info of", path);
      const std::string_view line_name(line_info.name, strnlen(line_info.name, sizeof(line_info.name)));
      if (line_name == spec.name) {
        _chip_path = path;
        _offset = line;
        return chip;
      }
    }
  }
  throw Error("GPIO line '" + spec.to_string() + "' not found");
}

LinuxOutputPin::LinuxOutputPin(
  std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, const std::string& consumer, bool initial_high)
: _line(std::move(syscalls), spec, LinuxGpioLine::Direction::kOutput, consumer, initial_high) { }

void LinuxOutputPin::write(bool high) {
  _line.set(high);
}

const LinuxGpioLine& LinuxOutputPin::line() const noexcept {
  return _line;
}

LinuxInputPin::LinuxInputPin(std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, const std::string& consumer)
: _line(std::move(syscalls), spec, LinuxGpioLine::Direction::kInput, consumer) { }

bool LinuxInputPin::read() {
  return _line.get();
}

const LinuxGpioLine& LinuxInputPin::line() const noexcept {
  return _line;
}

}  // namespace epaper::os
