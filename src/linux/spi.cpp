#include "epaper/linux/spi.hpp"

#include <fcntl.h>
#include <linux/spi/spidev.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace epaper::os {

namespace {

constexpr std::uint8_t kMaxSpiMode = 3;
constexpr std::uint8_t kBitsPerWord = 8;

}  // namespace

LinuxSpiDevice::LinuxSpiDevice(std::shared_ptr<Syscalls> syscalls, SpiConfig config)
: _syscalls(std::move(syscalls)), _config(std::move(config)) {
  if (!_syscalls) {
    throw std::invalid_argument("LinuxSpiDevice needs a Syscalls instance");
  }
  if (_config.speed_hz == 0 || _config.max_transfer_size == 0 || _config.mode > kMaxSpiMode) {
    throw std::invalid_argument("invalid SPI configuration for " + _config.path);
  }
  _fd = open_or_throw(_syscalls, _config.path, O_RDWR);

  std::uint8_t mode = _config.mode;
  ioctl_or_throw(*_syscalls, _fd.get(), SPI_IOC_WR_MODE, &mode, "cannot set SPI mode on", _config.path);
  std::uint8_t bits = kBitsPerWord;
  ioctl_or_throw(*_syscalls, _fd.get(), SPI_IOC_WR_BITS_PER_WORD, &bits, "cannot set SPI word size on", _config.path);
  std::uint32_t speed = _config.speed_hz;
  ioctl_or_throw(*_syscalls, _fd.get(), SPI_IOC_WR_MAX_SPEED_HZ, &speed, "cannot set SPI speed on", _config.path);
}

void LinuxSpiDevice::write(const std::uint8_t* data, std::size_t size) {
  std::size_t offset = 0;
  while (offset < size) {
    const auto chunk = std::min(size - offset, _config.max_transfer_size);
    spi_ioc_transfer transfer{ };
    transfer.tx_buf = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data + offset));
    transfer.len = static_cast<std::uint32_t>(chunk);
    transfer.speed_hz = _config.speed_hz;
    transfer.bits_per_word = kBitsPerWord;
    ioctl_or_throw(*_syscalls, _fd.get(), SPI_IOC_MESSAGE(1), &transfer, "SPI transfer failed on", _config.path);
    offset += chunk;
  }
}

const SpiConfig& LinuxSpiDevice::config() const noexcept {
  return _config;
}

}  // namespace epaper::os
