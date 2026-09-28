#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "epaper/hal.hpp"
#include "epaper/linux/syscalls.hpp"

namespace epaper::os {

struct SpiConfig {
  std::string path = "/dev/spidev0.0";
  std::uint32_t speed_hz = 4000000;
  /// SPI mode 0-3, e-paper controllers use mode 0.
  std::uint8_t mode = 0;
  /// spidev rejects transfers above its `bufsiz` module parameter (4096 by default), longer writes are split.
  std::size_t max_transfer_size = 4096;
};

/// SPI through the spidev character device. Chip select is the hardware CS line of the device node.
class LinuxSpiDevice final : public SpiDevice {
public:
  LinuxSpiDevice(std::shared_ptr<Syscalls> syscalls, SpiConfig config);

  void write(const std::uint8_t* data, std::size_t size) override;
  const SpiConfig& config() const noexcept;

private:
  std::shared_ptr<Syscalls> _syscalls;
  SpiConfig _config;
  FileDescriptor _fd;
};

}  // namespace epaper::os
