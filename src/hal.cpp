#include "epaper/hal.hpp"

#include <thread>

namespace epaper {

std::chrono::steady_clock::time_point SystemClock::now() {
  return std::chrono::steady_clock::now();
}

void SystemClock::sleep_for(std::chrono::milliseconds duration) {
  std::this_thread::sleep_for(duration);
}

}  // namespace epaper
