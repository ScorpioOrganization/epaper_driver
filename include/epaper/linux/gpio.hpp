#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "epaper/hal.hpp"
#include "epaper/linux/syscalls.hpp"

namespace epaper::os {

/**
 * @brief Which GPIO line to use.
 *
 * Either a line name searched on every GPIO chip ("PR.04" on Jetson, "GPIO17" on Raspberry Pi 5 - see
 * `gpioinfo`), or an explicit chip and offset ("gpiochip0:112" or "/dev/gpiochip0:112").
 */
struct LineSpec {
  /// Chip name ("gpiochip0") or path. Required with `offset`, optional with `name` (empty = all chips).
  std::string chip;
  std::string name;
  /// Line offset on `chip`, negative to look the line up by `name`.
  int offset = -1;

  /// Parses "<name>" or "<chip>:<offset>", throws std::invalid_argument when malformed.
  static LineSpec parse(const std::string& text);
  std::string to_string() const;
};

/// "/dev/<chip>" for a bare chip name, the argument itself for a path.
std::string gpiochip_path(const std::string& chip);

/// One line requested through the GPIO v2 character device API (Linux 5.10+).
class LinuxGpioLine {
public:
  enum class Direction {
    kInput,
    kOutput,
  };

  LinuxGpioLine(
    std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, Direction direction, const std::string& consumer,
    bool initial_high = false);

  bool get();
  void set(bool high);

  const std::string& chip_path() const noexcept;
  std::uint32_t offset() const noexcept;

private:
  FileDescriptor find_by_name(const LineSpec& spec);

  std::shared_ptr<Syscalls> _syscalls;
  std::string _chip_path;
  std::uint32_t _offset = 0;
  std::string _description;
  FileDescriptor _line;
};

class LinuxOutputPin final : public OutputPin {
public:
  LinuxOutputPin(
    std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, const std::string& consumer,
    bool initial_high = false);

  void write(bool high) override;
  const LinuxGpioLine& line() const noexcept;

private:
  LinuxGpioLine _line;
};

class LinuxInputPin final : public InputPin {
public:
  LinuxInputPin(std::shared_ptr<Syscalls> syscalls, const LineSpec& spec, const std::string& consumer);

  bool read() override;
  const LinuxGpioLine& line() const noexcept;

private:
  LinuxGpioLine _line;
};

}  // namespace epaper::os
