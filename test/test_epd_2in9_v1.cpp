#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "epaper/errors.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/panels/epd_2in9_v1.hpp"
#include "fakes/fake_hal.hpp"

namespace epaper {
namespace {

using std::chrono::milliseconds;
using testing::FakeHardware;
using Bytes = std::vector<std::uint8_t>;

const Bytes kInitCodes = { 0x01, 0x0C, 0x2C, 0x3A, 0x3B, 0x3C, 0x11, 0x32 };
const Bytes kWriteFrameCodes = { 0x44, 0x45, 0x4E, 0x4F, 0x24 };
const Bytes kTurnOnCodes = { 0x22, 0x20, 0xFF, 0x22, 0x20 };

Bytes concat(std::initializer_list<Bytes> parts) {
  Bytes result;
  for (const auto& part : parts) {
    result.insert(result.end(), part.begin(), part.end());
  }
  return result;
}

Framebuffer pattern(std::uint8_t seed) {
  Framebuffer framebuffer(Epd2in9V1::kWidth, Epd2in9V1::kHeight);
  for (int i = 0; i < 100; ++i) {
    framebuffer.set((i * 11 + seed) % 128, (i * 3 + seed) % 296, Color::kBlack);
  }
  return framebuffer;
}

class Epd2in9V1Test : public ::testing::Test {
protected:
  FakeHardware hardware;
};

TEST_F(Epd2in9V1Test, Identity) {
  Epd2in9V1 panel(hardware.make_io());
  EXPECT_EQ(panel.width(), 128);
  EXPECT_EQ(panel.height(), 296);
  EXPECT_NE(panel.name().find("V1"), std::string::npos);
  EXPECT_TRUE(panel.supports(RefreshMode::kFull));
  EXPECT_TRUE(panel.supports(RefreshMode::kPartial));
  EXPECT_FALSE(panel.supports(RefreshMode::kFast));
  EXPECT_FALSE(panel.resumes_after_sleep());
}

TEST_F(Epd2in9V1Test, InitSequence) {
  Epd2in9V1 panel(hardware.make_io(true));
  panel.init();
  EXPECT_EQ(hardware.power_levels, std::vector<bool>{ true });
  EXPECT_EQ(hardware.reset_levels, (std::vector<bool>{ true, false, true }));
  EXPECT_EQ(hardware.clock->sleeps,
    (std::vector<milliseconds>{ milliseconds{ 200 }, milliseconds{ 2 }, milliseconds{ 200 } }));
  ASSERT_EQ(hardware.command_codes(), kInitCodes);
  const auto commands = hardware.commands();
  EXPECT_EQ(commands[0].data, (Bytes{ 0x27, 0x01, 0x00 }));
  EXPECT_EQ(commands[1].data, (Bytes{ 0xD7, 0xD6, 0x9D }));
  EXPECT_EQ(commands[2].data, Bytes{ 0xA8 });
  EXPECT_EQ(commands[3].data, Bytes{ 0x1A });
  EXPECT_EQ(commands[4].data, Bytes{ 0x08 });
  EXPECT_EQ(commands[5].data, Bytes{ 0x03 });
  EXPECT_EQ(commands[6].data, Bytes{ 0x03 });
  ASSERT_EQ(commands[7].data.size(), 30u);
  EXPECT_EQ(commands[7].data[0], 0x50);
}

TEST_F(Epd2in9V1Test, FullRefresh) {
  Epd2in9V1 panel(hardware.make_io());
  panel.init();
  hardware.clear_log();
  const auto framebuffer = pattern(4);
  panel.display(framebuffer, RefreshMode::kFull);
  ASSERT_EQ(hardware.command_codes(), concat({ kWriteFrameCodes, kTurnOnCodes }));
  const auto commands = hardware.commands();
  EXPECT_EQ(commands[0].data, (Bytes{ 0x00, 0x0F }));
  EXPECT_EQ(commands[1].data, (Bytes{ 0x00, 0x00, 0x27, 0x01 }));
  EXPECT_EQ(commands[4].data, framebuffer.data());
  EXPECT_EQ(commands[5].data, Bytes{ 0xC4 });
  // The charge pump is switched off again once the update is done.
  EXPECT_EQ(commands[8].data, Bytes{ 0xC3 });
  // BUSY is first checked 100 ms after the update and after the power down started.
  EXPECT_EQ(hardware.clock->sleeps, (std::vector<milliseconds>{ milliseconds{ 100 }, milliseconds{ 100 } }));
}

TEST_F(Epd2in9V1Test, PartialRefreshSwitchesLut) {
  Epd2in9V1 panel(hardware.make_io());
  panel.init();
  hardware.clear_log();
  const auto framebuffer = pattern(9);

  panel.display(framebuffer, RefreshMode::kPartial);
  ASSERT_EQ(hardware.command_codes(), concat({ kInitCodes, kWriteFrameCodes, kTurnOnCodes, kWriteFrameCodes }));
  const auto commands = hardware.commands();
  EXPECT_EQ(commands[7].data[0], 0x10);
  EXPECT_EQ(commands[12].data, framebuffer.data());
  EXPECT_EQ(commands.back().data, framebuffer.data());

  hardware.clear_log();
  panel.display(framebuffer, RefreshMode::kPartial);
  EXPECT_EQ(hardware.command_codes(), concat({ kWriteFrameCodes, kTurnOnCodes, kWriteFrameCodes }));

  hardware.clear_log();
  panel.display(framebuffer, RefreshMode::kFull);
  ASSERT_EQ(hardware.command_codes(), concat({ kInitCodes, kWriteFrameCodes, kTurnOnCodes }));
  EXPECT_EQ(hardware.commands()[7].data[0], 0x50);
}

TEST_F(Epd2in9V1Test, DisplayErrors) {
  Epd2in9V1 panel(hardware.make_io());
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), std::logic_error);
  panel.init();
  EXPECT_THROW(panel.display(Framebuffer(8, 8), RefreshMode::kFull), std::invalid_argument);
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFast), std::invalid_argument);
}

TEST_F(Epd2in9V1Test, SleepAndWake) {
  Epd2in9V1 panel(hardware.make_io(true));
  panel.init();
  hardware.clear_log();
  panel.sleep();
  ASSERT_EQ(hardware.command_codes(), Bytes{ 0x10 });
  EXPECT_EQ(hardware.commands()[0].data, Bytes{ 0x01 });
  EXPECT_EQ(hardware.power_levels, std::vector<bool>{ false });
  EXPECT_EQ(hardware.reset_levels, std::vector<bool>{ false });
  EXPECT_FALSE(hardware.data_command_level);
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), std::logic_error);

  panel.init();
  hardware.clear_log();
  panel.display(pattern(0), RefreshMode::kFull);
  EXPECT_EQ(hardware.command_codes(), concat({ kWriteFrameCodes, kTurnOnCodes }));
}

TEST_F(Epd2in9V1Test, BusyTimeoutDuringRefresh) {
  Epd2in9V1 panel(hardware.make_io());
  panel.init();
  hardware.busy = [](std::size_t) { return true; };
  EXPECT_THROW(panel.display(pattern(0), RefreshMode::kFull), TimeoutError);
  EXPECT_GE(hardware.clock->total_slept(), milliseconds{ 10000 });
}

}  // namespace
}  // namespace epaper
