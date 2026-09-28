#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "epaper/hal.hpp"
#include "epaper/panel.hpp"

namespace epaper::testing {

/// Clock whose time only moves when somebody sleeps.
class FakeClock final : public Clock {
public:
  std::chrono::steady_clock::time_point now() override {
    return _now;
  }

  void sleep_for(std::chrono::milliseconds duration) override {
    sleeps.push_back(duration);
    _now += duration;
  }

  void advance(std::chrono::milliseconds duration) {
    _now += duration;
  }

  std::chrono::milliseconds total_slept() const {
    std::chrono::milliseconds total{ 0 };
    for (const auto sleep : sleeps) {
      total += sleep;
    }
    return total;
  }

  std::vector<std::chrono::milliseconds> sleeps;

private:
  std::chrono::steady_clock::time_point _now{ };
};

/// One command byte and the data bytes sent after it.
struct Command {
  std::uint8_t code;
  std::vector<std::uint8_t> data;
};

/// Records everything a panel driver does with its pins and SPI bus.
class FakeHardware {
public:
  struct Transfer {
    bool data_command;
    std::vector<std::uint8_t> bytes;
  };

  FakeHardware()
  : clock(std::make_shared<FakeClock>()) { }

  PanelIo make_io(bool with_power = false) {
    PanelIo io;
    io.spi = std::make_shared<Spi>(*this);
    io.reset = std::make_shared<Pin>(reset_levels);
    io.data_command = std::make_shared<DataCommandPin>(*this);
    io.busy = std::make_shared<BusyPin>(*this);
    if (with_power) {
      io.power = std::make_shared<Pin>(power_levels);
    }
    io.clock = clock;
    return io;
  }

  /// Transfers grouped as command + data. A data transfer without a preceding command throws.
  std::vector<Command> commands() const {
    std::vector<Command> result;
    for (const auto& transfer : transfers) {
      if (!transfer.data_command) {
        for (const auto byte : transfer.bytes) {
          result.push_back({ byte, { } });
        }
      } else {
        if (result.empty()) {
          throw std::logic_error("data sent before any command");
        }
        auto& data = result.back().data;
        data.insert(data.end(), transfer.bytes.begin(), transfer.bytes.end());
      }
    }
    return result;
  }

  std::vector<std::uint8_t> command_codes() const {
    std::vector<std::uint8_t> codes;
    for (const auto& command : commands()) {
      codes.push_back(command.code);
    }
    return codes;
  }

  void clear_log() {
    transfers.clear();
    reset_levels.clear();
    power_levels.clear();
    clock->sleeps.clear();
  }

  std::vector<Transfer> transfers;
  std::vector<bool> reset_levels;
  std::vector<bool> power_levels;
  bool data_command_level = false;
  /// BUSY level, called on every read with the number of reads so far.
  std::function<bool(std::size_t)> busy = [](std::size_t) { return false; };
  std::size_t busy_reads = 0;
  std::shared_ptr<FakeClock> clock;

private:
  class Spi final : public SpiDevice {
public:
    explicit Spi(FakeHardware& hardware)
    : _hardware(hardware) { }

    void write(const std::uint8_t* data, std::size_t size) override {
      _hardware.transfers.push_back({ _hardware.data_command_level, std::vector<std::uint8_t>(data, data + size) });
    }

private:
    FakeHardware& _hardware;
  };

  class Pin final : public OutputPin {
public:
    explicit Pin(std::vector<bool>& levels)
    : _levels(levels) { }

    void write(bool high) override {
      _levels.push_back(high);
    }

private:
    std::vector<bool>& _levels;
  };

  class DataCommandPin final : public OutputPin {
public:
    explicit DataCommandPin(FakeHardware& hardware)
    : _hardware(hardware) { }

    void write(bool high) override {
      _hardware.data_command_level = high;
    }

private:
    FakeHardware& _hardware;
  };

  class BusyPin final : public InputPin {
public:
    explicit BusyPin(FakeHardware& hardware)
    : _hardware(hardware) { }

    bool read() override {
      return _hardware.busy(_hardware.busy_reads++);
    }

private:
    FakeHardware& _hardware;
  };
};

}  // namespace epaper::testing
