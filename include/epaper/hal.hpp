#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace epaper {

// Interfaces below follow C++ Core Guidelines C.35: the destructor is protected and non-virtual, so
// implementations are owned through std::shared_ptr (which remembers the concrete type) and can never be
// deleted through a base pointer.

/// Write-only SPI link to the panel. Chip select is handled by the implementation.
class SpiDevice {
public:
  virtual void write(const std::uint8_t* data, std::size_t size) = 0;

protected:
  ~SpiDevice() = default;
};

/// Digital output line (RST, DC, PWR).
class OutputPin {
public:
  virtual void write(bool high) = 0;

protected:
  ~OutputPin() = default;
};

/// Digital input line (BUSY).
class InputPin {
public:
  virtual bool read() = 0;

protected:
  ~InputPin() = default;
};

/// Time source used for reset pulses and BUSY polling, replaceable in tests.
class Clock {
public:
  virtual std::chrono::steady_clock::time_point now() = 0;
  virtual void sleep_for(std::chrono::milliseconds duration) = 0;

protected:
  ~Clock() = default;
};

/// Clock backed by std::chrono::steady_clock and std::this_thread::sleep_for.
class SystemClock final : public Clock {
public:
  std::chrono::steady_clock::time_point now() override;
  void sleep_for(std::chrono::milliseconds duration) override;
};

}  // namespace epaper
