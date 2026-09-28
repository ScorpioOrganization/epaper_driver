#pragma once

#include <linux/gpio.h>
#include <linux/spi/spidev.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "epaper/linux/syscalls.hpp"

namespace epaper::testing {

/// In-memory kernel with spidev nodes and GPIO v2 character devices.
class FakeSyscalls final : public os::Syscalls {
public:
  struct Line {
    std::string chip;
    std::uint32_t offset = 0;
    std::uint64_t flags = 0;
    std::string consumer;
    bool value = false;
    std::vector<bool> writes;
  };

  struct SpiTransfer {
    int fd;
    std::vector<std::uint8_t> bytes;
    std::uint32_t speed_hz;
    std::uint8_t bits_per_word;
  };

  /// Adds /dev/gpiochipN style node with the given line names.
  void add_chip(const std::string& path, std::vector<std::string> line_names) {
    chips[path] = std::move(line_names);
  }

  void add_spidev(const std::string& path) {
    spidevs.insert(path);
  }

  /// Line state by chip path and offset, nullptr when not requested.
  Line * find_line(const std::string& chip, std::uint32_t offset) {
    for (auto& [fd, line] : lines) {
      if (line.chip == chip && line.offset == offset) {
        return &line;
      }
    }
    return nullptr;
  }

  int open(const std::string& path, int /*flags*/) override {
    if (fail_open.count(path) != 0 || (chips.count(path) == 0 && spidevs.count(path) == 0)) {
      errno = fail_open.count(path) != 0 ? EACCES : ENOENT;
      return -1;
    }
    const int fd = _next_fd++;
    open_files[fd] = path;
    return fd;
  }

  int close(int fd) override {
    closed.push_back(fd);
    open_files.erase(fd);
    lines.erase(fd);
    return 0;
  }

  int ioctl(int fd, os::IoctlRequest request, void* argument) override {
    ioctl_calls.push_back(request);
    const auto failure = fail_ioctl.find(request);
    if (failure != fail_ioctl.end()) {
      errno = failure->second;
      return -1;
    }
    if (request == SPI_IOC_WR_MODE) {
      spi_mode = *static_cast<std::uint8_t*>(argument);
      return 0;
    }
    if (request == SPI_IOC_WR_BITS_PER_WORD) {
      spi_bits_per_word = *static_cast<std::uint8_t*>(argument);
      return 0;
    }
    if (request == SPI_IOC_WR_MAX_SPEED_HZ) {
      spi_speed_hz = *static_cast<std::uint32_t*>(argument);
      return 0;
    }
    if (request == SPI_IOC_MESSAGE(1)) {
      const auto* transfer = static_cast<const spi_ioc_transfer*>(argument);
      const auto* bytes = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(transfer->tx_buf));
      spi_transfers.push_back(
        { fd, std::vector<std::uint8_t>(bytes, bytes + transfer->len), transfer->speed_hz,
          transfer->bits_per_word });
      return static_cast<int>(transfer->len);
    }
    if (request == GPIO_GET_CHIPINFO_IOCTL) {
      auto* info = static_cast<gpiochip_info*>(argument);
      const auto& path = open_files.at(fd);
      std::strncpy(info->name, path.c_str(), sizeof(info->name) - 1);
      info->lines = static_cast<std::uint32_t>(chips.at(path).size());
      return 0;
    }
    if (request == GPIO_V2_GET_LINEINFO_IOCTL) {
      auto* info = static_cast<gpio_v2_line_info*>(argument);
      const auto& name = chips.at(open_files.at(fd)).at(info->offset);
      // Mimic the kernel: names are NUL terminated only when shorter than the field.
      std::memcpy(info->name, name.c_str(), std::min(name.size() + 1, sizeof(info->name)));
      return 0;
    }
    if (request == GPIO_V2_GET_LINE_IOCTL) {
      auto* line_request = static_cast<gpio_v2_line_request*>(argument);
      Line line;
      line.chip = open_files.at(fd);
      line.offset = line_request->offsets[0];
      line.flags = line_request->config.flags;
      line.consumer = line_request->consumer;
      if (line_request->config.num_attrs > 0) {
        line.value = (line_request->config.attrs[0].attr.values & 1u) != 0;
      }
      line_request->fd = _next_fd++;
      lines[line_request->fd] = line;
      return 0;
    }
    if (request == GPIO_V2_LINE_SET_VALUES_IOCTL) {
      const auto* values = static_cast<const gpio_v2_line_values*>(argument);
      auto& line = lines.at(fd);
      line.value = (values->bits & 1u) != 0;
      line.writes.push_back(line.value);
      return 0;
    }
    if (request == GPIO_V2_LINE_GET_VALUES_IOCTL) {
      auto* values = static_cast<gpio_v2_line_values*>(argument);
      values->bits = lines.at(fd).value ? 1u : 0u;
      return 0;
    }
    errno = ENOTTY;
    return -1;
  }

  std::vector<std::string> list_gpiochips() override {
    std::vector<std::string> paths;
    for (const auto& [path, names] : chips) {
      paths.push_back(path);
    }
    return paths;
  }

  std::map<std::string, std::vector<std::string>> chips;
  std::set<std::string> spidevs;
  std::set<std::string> fail_open;
  std::map<os::IoctlRequest, int> fail_ioctl;
  std::map<int, std::string> open_files;
  std::map<int, Line> lines;
  std::vector<int> closed;
  std::vector<os::IoctlRequest> ioctl_calls;
  std::vector<SpiTransfer> spi_transfers;
  std::uint8_t spi_mode = 0xFF;
  std::uint8_t spi_bits_per_word = 0;
  std::uint32_t spi_speed_hz = 0;

private:
  int _next_fd = 3;
};

}  // namespace epaper::testing
