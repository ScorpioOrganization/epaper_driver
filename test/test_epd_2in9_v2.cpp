#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "epaper/errors.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/panels/epd_2in9_v2.hpp"
#include "fakes/fake_hal.hpp"

namespace epaper {
namespace {

using std::chrono::milliseconds;
using testing::Command;
using testing::FakeHardware;
using Bytes = std::vector<std::uint8_t>;

const Bytes kInitCodes = { 0x12, 0x01, 0x11, 0x44, 0x45, 0x21, 0x4E, 0x4F, 0x32, 0x3F, 0x03, 0x04, 0x2C };
const Bytes kFastInitCodes = { 0x12, 0x01, 0x11, 0x44, 0x45, 0x3C, 0x21, 0x4E, 0x4F, 0x32, 0x3F, 0x03, 0x04, 0x2C };
const Bytes kFullDisplayCodes = { 0x44, 0x45, 0x4E, 0x4F, 0x24, 0x4E, 0x4F, 0x26, 0x22, 0x20 };
const Bytes kPartialSetupCodes = { 0x32, 0x3F, 0x03, 0x04, 0x2C, 0x37, 0x3C };
const Bytes kPartialDisplayCodes = { 0x44, 0x45, 0x4E, 0x4F, 0x24, 0x22, 0x20 };
const Bytes kRestoreCodes = { 0x44, 0x45, 0x4E, 0x4F, 0x24, 0x4E, 0x4F, 0x26 };

Bytes concat(const Bytes& a, const Bytes& b) {
  Bytes result = a;
  result.insert(result.end(), b.begin(), b.end());
  return result;
}

Framebuffer pattern(std::uint8_t seed) {
  Framebuffer framebuffer(Epd2in9V2::kWidth, Epd2in9V2::kHeight);
  for (int i = 0; i < 200; ++i) {
    framebuffer.set((i * 7 + seed) % 128, (i * 13 + seed) % 296, Color::kBlack);
  }
  return framebuffer;
}

class Epd2in9V2Test : public ::testing::Test {
protected:
  FakeHardware hardware;
};

TEST_F(Epd2in9V2Test, Identity) {
  Epd2in9V2 panel(hardware.make_io());
  EXPECT_EQ(panel.width(), 128);
  EXPECT_EQ(panel.height(), 296);
  EXPECT_NE(panel.name().find("V2"), std::string::npos);
  EXPECT_TRUE(panel.supports(RefreshMode::kFull));
  EXPECT_TRUE(panel.supports(RefreshMode::kFast));
  EXPECT_TRUE(panel.supports(RefreshMode::kPartial));
  EXPECT_TRUE(panel.resumes_after_sleep());
}

TEST_F(Epd2in9V2Test, RequiresCompleteIo) {
  auto io = hardware.make_io();
  io.busy.reset();
  EXPECT_THROW(Epd2in9V2 panel(std::move(io)), std::invalid_argument);
  io = hardware.make_io();
  io.clock.reset();
  EXPECT_THROW(Epd2in9V2 panel(std::move(io)), std::invalid_argument);
}

TEST_F(Epd2in9V2Test, InitSequence) {
  Epd2in9V2 panel(hardware.make_io(true));
  panel.init();

  EXPECT_EQ(hardware.power_levels, std::vector<bool>{ true });
  EXPECT_EQ(hardware.reset_levels, (std::vector<bool>{ true, false, true }));
  ASSERT_EQ(hardware.command_codes(), kInitCodes);
  const auto commands = hardware.commands();
  EXPECT_TRUE(commands[0].data.empty());
  EXPECT_EQ(commands[1].data, (Bytes{ 0x27, 0x01, 0x00 }));
  EXPECT_EQ(commands[2].data, Bytes{ 0x03 });
  EXPECT_EQ(commands[3].data, (Bytes{ 0x00, 0x0F }));
  EXPECT_EQ(commands[4].data, (Bytes{ 0x00, 0x00, 0x27, 0x01 }));
  EXPECT_EQ(commands[5].data, (Bytes{ 0x00, 0x80 }));
  EXPECT_EQ(commands[6].data, Bytes{ 0x00 });
  EXPECT_EQ(commands[7].data, (Bytes{ 0x00, 0x00 }));
  // WS_20_30 waveform.
  ASSERT_EQ(commands[8].data.size(), 153u);
  EXPECT_EQ(commands[8].data[0], 0x80);
  EXPECT_EQ(commands[8].data[1], 0x66);
  EXPECT_EQ(commands[9].data, Bytes{ 0x22 });
  EXPECT_EQ(commands[10].data, Bytes{ 0x17 });
  EXPECT_EQ(commands[11].data, (Bytes{ 0x41, 0x00, 0x32 }));
  EXPECT_EQ(commands[12].data, Bytes{ 0x36 });

  // Reset pulse 10/2/10 ms and 100 ms settle. BUSY is low right away: no waiting after it drops.
  EXPECT_EQ(
    hardware.clock->sleeps,
    (std::vector<milliseconds>{ milliseconds{ 10 }, milliseconds{ 2 }, milliseconds{ 10 }, milliseconds{ 100 } }));
}

TEST_F(Epd2in9V2Test, WorksWithoutPowerPin) {
  Epd2in9V2 panel(hardware.make_io(false));
  panel.init();
  hardware.clear_log();
  panel.sleep();
  EXPECT_TRUE(hardware.power_levels.empty());
  // The controller stays powered: RST keeps it out of reset.
  EXPECT_TRUE(hardware.reset_levels.empty());
}

TEST_F(Epd2in9V2Test, FullRefreshWritesBothRams) {
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  hardware.clear_log();
  const auto framebuffer = pattern(3);
  panel.display(framebuffer, RefreshMode::kFull);

  ASSERT_EQ(hardware.command_codes(), kFullDisplayCodes);
  const auto commands = hardware.commands();
  EXPECT_EQ(commands[4].data, framebuffer.data());
  EXPECT_EQ(commands[7].data, framebuffer.data());
  EXPECT_EQ(commands[8].data, Bytes{ 0xC7 });
  EXPECT_TRUE(hardware.reset_levels.empty());
}

TEST_F(Epd2in9V2Test, FastRefreshLoadsItsWaveformOnce) {
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  hardware.clear_log();
  const auto framebuffer = pattern(5);

  panel.display(framebuffer, RefreshMode::kFast);
  ASSERT_EQ(hardware.command_codes(), concat(kFastInitCodes, kFullDisplayCodes));
  const auto commands = hardware.commands();
  EXPECT_EQ(commands[5].data, Bytes{ 0x05 });
  EXPECT_EQ(commands[9].data[0], 0x90);
  EXPECT_EQ(commands[11].data, Bytes{ 0x17 });
  EXPECT_EQ(commands[12].data, (Bytes{ 0x41, 0xAE, 0x32 }));
  EXPECT_EQ(commands[13].data, Bytes{ 0x38 });

  hardware.clear_log();
  panel.display(framebuffer, RefreshMode::kFast);
  EXPECT_EQ(hardware.command_codes(), kFullDisplayCodes);

  // Back to the regular full waveform.
  hardware.clear_log();
  panel.display(framebuffer, RefreshMode::kFull);
  EXPECT_EQ(hardware.command_codes(), concat(kInitCodes, kFullDisplayCodes));
}

TEST_F(Epd2in9V2Test, PartialRefresh) {
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  panel.display(pattern(1), RefreshMode::kFull);
  hardware.clear_log();

  // First partial refresh after a full one: Waveshare's setup (reset pulse, partial waveform with its voltages,
  // RAM ping-pong, border), then the frame goes into the "new" RAM bank.
  const auto framebuffer = pattern(2);
  panel.display(framebuffer, RefreshMode::kPartial);
  EXPECT_EQ(hardware.reset_levels, (std::vector<bool>{ true, false, true }));
  EXPECT_EQ(hardware.clock->sleeps, (std::vector<milliseconds>{ milliseconds{ 1 }, milliseconds{ 2 } }));
  ASSERT_EQ(hardware.command_codes(), concat(kPartialSetupCodes, kPartialDisplayCodes));
  auto commands = hardware.commands();
  ASSERT_EQ(commands[0].data.size(), 153u);
  EXPECT_EQ(commands[0].data[1], 0x40);
  EXPECT_EQ(commands[1].data, Bytes{ 0x22 });
  EXPECT_EQ(commands[2].data, Bytes{ 0x17 });
  EXPECT_EQ(commands[3].data, (Bytes{ 0x41, 0xB0, 0x32 }));
  EXPECT_EQ(commands[4].data, Bytes{ 0x36 });
  EXPECT_EQ(commands[5].data, (Bytes{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 }));
  EXPECT_EQ(commands[6].data, Bytes{ 0x80 });
  EXPECT_EQ(commands[11].data, framebuffer.data());
  EXPECT_EQ(commands[12].data, Bytes{ 0xCF });

  // Back to back partial refreshes: the setup is still in place, only the frame and the update are sent.
  hardware.clear_log();
  const auto next = pattern(7);
  panel.display(next, RefreshMode::kPartial);
  ASSERT_EQ(hardware.command_codes(), kPartialDisplayCodes);
  commands = hardware.commands();
  EXPECT_EQ(commands[4].data, next.data());
  EXPECT_EQ(commands[5].data, Bytes{ 0xCF });
  EXPECT_TRUE(hardware.reset_levels.empty());
  EXPECT_TRUE(hardware.clock->sleeps.empty());

  // The partial LUT replaced the full one: a full refresh re-initializes first.
  hardware.clear_log();
  panel.display(framebuffer, RefreshMode::kFull);
  EXPECT_EQ(hardware.command_codes(), concat(kInitCodes, kFullDisplayCodes));

  // And the next partial refresh sets itself up again.
  hardware.clear_log();
  panel.display(next, RefreshMode::kPartial);
  EXPECT_EQ(hardware.command_codes(), concat(kPartialSetupCodes, kPartialDisplayCodes));
}

TEST_F(Epd2in9V2Test, PartialBeforeAnyFrameIsFull) {
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  hardware.clear_log();
  const auto framebuffer = pattern(4);
  panel.display(framebuffer, RefreshMode::kPartial);
  ASSERT_EQ(hardware.command_codes(), kFullDisplayCodes);
  EXPECT_EQ(hardware.commands()[8].data, Bytes{ 0xC7 });

  hardware.clear_log();
  panel.display(pattern(5), RefreshMode::kPartial);
  EXPECT_EQ(hardware.command_codes(), concat(kPartialSetupCodes, kPartialDisplayCodes));
}

TEST_F(Epd2in9V2Test, PartialRefreshAfterWakingUp) {
  Epd2in9V2 panel(hardware.make_io(true));
  panel.init();
  panel.display(pattern(1), RefreshMode::kFull);
  const auto shown = pattern(2);
  panel.display(shown, RefreshMode::kPartial);
  panel.sleep();
  hardware.clear_log();

  // PWR was cut: the controller RAM is gone, the driver writes the frame on the panel back into both banks.
  panel.init();
  const auto framebuffer = pattern(3);
  panel.display(framebuffer, RefreshMode::kPartial);
  const auto wake_codes = concat(kInitCodes, kRestoreCodes);
  ASSERT_EQ(hardware.command_codes(), concat(wake_codes, concat(kPartialSetupCodes, kPartialDisplayCodes)));
  const auto commands = hardware.commands();
  const auto restore = kInitCodes.size();
  EXPECT_EQ(commands[restore + 4].data, shown.data());
  EXPECT_EQ(commands[restore + 7].data, shown.data());
  EXPECT_EQ(commands.end()[-3].data, framebuffer.data());
  EXPECT_EQ(commands.end()[-2].data, Bytes{ 0xCF });

  // Restored once, back to back partial refreshes follow.
  hardware.clear_log();
  panel.display(pattern(4), RefreshMode::kPartial);
  EXPECT_EQ(hardware.command_codes(), kPartialDisplayCodes);
}

TEST_F(Epd2in9V2Test, FailedRefreshForgetsTheFrameOnThePanel) {
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  panel.display(pattern(1), RefreshMode::kFull);
  panel.display(pattern(2), RefreshMode::kPartial);
  hardware.busy = [](std::size_t) { return true; };
  EXPECT_THROW(panel.display(pattern(3), RefreshMode::kPartial), TimeoutError);

  // Nothing is known about the image on the panel: the next refresh is a full one.
  hardware.busy = [](std::size_t) { return false; };
  hardware.clear_log();
  panel.display(pattern(4), RefreshMode::kPartial);
  ASSERT_EQ(hardware.command_codes(), concat(kInitCodes, kFullDisplayCodes));

  hardware.busy = [](std::size_t) { return true; };
  EXPECT_THROW(panel.display(pattern(5), RefreshMode::kFull), TimeoutError);
  hardware.busy = [](std::size_t) { return false; };
  hardware.clear_log();
  panel.display(pattern(6), RefreshMode::kPartial);
  EXPECT_EQ(hardware.command_codes(), kFullDisplayCodes);
}

TEST_F(Epd2in9V2Test, DisplayErrors) {
  Epd2in9V2 panel(hardware.make_io());
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), std::logic_error);
  panel.init();
  EXPECT_THROW(panel.display(Framebuffer(296, 128), RefreshMode::kFull), std::invalid_argument);
  EXPECT_THROW(panel.display(pattern(0), static_cast<RefreshMode>(9)), std::invalid_argument);
}

TEST_F(Epd2in9V2Test, SleepAndWake) {
  Epd2in9V2 panel(hardware.make_io(true));
  panel.init();
  hardware.clear_log();
  panel.sleep();
  ASSERT_EQ(hardware.command_codes(), Bytes{ 0x10 });
  EXPECT_EQ(hardware.commands()[0].data, Bytes{ 0x01 });
  EXPECT_EQ(hardware.power_levels, std::vector<bool>{ false });
  // RST and DC go low before PWR is cut, so they do not feed the unpowered controller.
  EXPECT_EQ(hardware.reset_levels, std::vector<bool>{ false });
  EXPECT_FALSE(hardware.data_command_level);
  EXPECT_EQ(hardware.clock->total_slept(), milliseconds{ 100 });
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), std::logic_error);

  hardware.clear_log();
  panel.init();
  panel.display(pattern(0), RefreshMode::kFull);
  EXPECT_EQ(hardware.command_codes(), concat(kInitCodes, kFullDisplayCodes));
}

TEST_F(Epd2in9V2Test, PollsBusyUntilReleased) {
  hardware.busy = [](std::size_t reads) { return reads < 3; };
  Epd2in9V2 panel(hardware.make_io());
  panel.init();
  EXPECT_GT(hardware.busy_reads, 3u);
  EXPECT_EQ(std::count(hardware.clock->sleeps.begin(), hardware.clock->sleeps.end(), milliseconds{ 10 }), 3 + 2);
}

TEST_F(Epd2in9V2Test, BusyTimeout) {
  hardware.busy = [](std::size_t) { return true; };
  Epd2in9V2 panel(hardware.make_io());
  try {
    panel.init();
    FAIL() << "expected TimeoutError";
  } catch (const TimeoutError& error) {
    EXPECT_NE(std::string(error.what()).find("BUSY"), std::string::npos);
  }
  EXPECT_GE(hardware.clock->total_slept(), milliseconds{ 3000 });
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), std::logic_error);
}

}  // namespace
}  // namespace epaper
