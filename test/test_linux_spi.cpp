#include <linux/spi/spidev.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "epaper/errors.hpp"
#include "epaper/linux/spi.hpp"
#include "fakes/fake_syscalls.hpp"

namespace epaper::os {
namespace {

using testing::FakeSyscalls;

class LinuxSpiTest : public ::testing::Test {
protected:
  void SetUp() override {
    syscalls->add_spidev("/dev/spidev0.0");
  }

  std::shared_ptr<FakeSyscalls> syscalls = std::make_shared<FakeSyscalls>();
};

TEST_F(LinuxSpiTest, ConfiguresDevice) {
  SpiConfig config;
  config.speed_hz = 2000000;
  config.mode = 3;
  LinuxSpiDevice spi(syscalls, config);
  EXPECT_EQ(syscalls->spi_mode, 3);
  EXPECT_EQ(syscalls->spi_bits_per_word, 8);
  EXPECT_EQ(syscalls->spi_speed_hz, 2000000u);
  EXPECT_EQ(spi.config().path, "/dev/spidev0.0");
  ASSERT_EQ(syscalls->open_files.size(), 1u);
  EXPECT_EQ(syscalls->open_files.begin()->second, "/dev/spidev0.0");
}

TEST_F(LinuxSpiTest, ClosesDeviceOnDestruction) {
  {
    LinuxSpiDevice spi(syscalls, SpiConfig{ });
  }
  EXPECT_TRUE(syscalls->open_files.empty());
  EXPECT_EQ(syscalls->closed.size(), 1u);
}

TEST_F(LinuxSpiTest, SplitsLongWrites) {
  SpiConfig config;
  config.max_transfer_size = 4096;
  LinuxSpiDevice spi(syscalls, config);
  std::vector<std::uint8_t> frame(4736);
  for (std::size_t i = 0; i < frame.size(); ++i) {
    frame[i] = static_cast<std::uint8_t>(i);
  }
  spi.write(frame.data(), frame.size());
  ASSERT_EQ(syscalls->spi_transfers.size(), 2u);
  EXPECT_EQ(syscalls->spi_transfers[0].bytes.size(), 4096u);
  EXPECT_EQ(syscalls->spi_transfers[1].bytes.size(), 640u);
  std::vector<std::uint8_t> joined = syscalls->spi_transfers[0].bytes;
  joined.insert(joined.end(), syscalls->spi_transfers[1].bytes.begin(), syscalls->spi_transfers[1].bytes.end());
  EXPECT_EQ(joined, frame);
  EXPECT_EQ(syscalls->spi_transfers[0].speed_hz, config.speed_hz);
  EXPECT_EQ(syscalls->spi_transfers[0].bits_per_word, 8);
}

TEST_F(LinuxSpiTest, SingleByteAndEmptyWrites) {
  LinuxSpiDevice spi(syscalls, SpiConfig{ });
  const std::uint8_t byte = 0x12;
  spi.write(&byte, 1);
  spi.write(&byte, 0);
  ASSERT_EQ(syscalls->spi_transfers.size(), 1u);
  EXPECT_EQ(syscalls->spi_transfers[0].bytes, std::vector<std::uint8_t>{ 0x12 });
}

TEST_F(LinuxSpiTest, RejectsInvalidConfiguration) {
  EXPECT_THROW(LinuxSpiDevice(nullptr, SpiConfig{ }), std::invalid_argument);
  SpiConfig config;
  config.speed_hz = 0;
  EXPECT_THROW(LinuxSpiDevice(syscalls, config), std::invalid_argument);
  config = SpiConfig{ };
  config.max_transfer_size = 0;
  EXPECT_THROW(LinuxSpiDevice(syscalls, config), std::invalid_argument);
  config = SpiConfig{ };
  config.mode = 4;
  EXPECT_THROW(LinuxSpiDevice(syscalls, config), std::invalid_argument);
  EXPECT_TRUE(syscalls->open_files.empty());
}

TEST_F(LinuxSpiTest, OpenFailure) {
  SpiConfig config;
  config.path = "/dev/spidev9.9";
  EXPECT_THROW(LinuxSpiDevice(syscalls, config), SystemError);
}

TEST_F(LinuxSpiTest, SetupFailuresCloseTheDevice) {
  for (const auto request : { SPI_IOC_WR_MODE, SPI_IOC_WR_BITS_PER_WORD, SPI_IOC_WR_MAX_SPEED_HZ }) {
    syscalls->fail_ioctl.clear();
    syscalls->fail_ioctl[request] = EINVAL;
    EXPECT_THROW(LinuxSpiDevice(syscalls, SpiConfig{ }), SystemError);
    EXPECT_TRUE(syscalls->open_files.empty());
  }
}

TEST_F(LinuxSpiTest, TransferFailure) {
  LinuxSpiDevice spi(syscalls, SpiConfig{ });
  syscalls->fail_ioctl[SPI_IOC_MESSAGE(1)] = EIO;
  const std::uint8_t byte = 0;
  try {
    spi.write(&byte, 1);
    FAIL() << "expected SystemError";
  } catch (const SystemError& error) {
    EXPECT_EQ(error.error_number(), EIO);
  }
}

}  // namespace
}  // namespace epaper::os
