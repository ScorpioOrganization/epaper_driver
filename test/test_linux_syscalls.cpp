#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "epaper/errors.hpp"
#include "epaper/linux/syscalls.hpp"
#include "fakes/fake_syscalls.hpp"

namespace epaper::os {
namespace {

using testing::FakeSyscalls;

std::filesystem::path make_temp_dir() {
  auto pattern = (std::filesystem::temp_directory_path() / "epaper_syscalls_XXXXXX").string();
  const char* created = ::mkdtemp(pattern.data());
  EXPECT_NE(created, nullptr);
  return std::filesystem::path(pattern);
}

TEST(LinuxSyscalls, OpenIoctlClose) {
  LinuxSyscalls syscalls;
  const int fd = syscalls.open("/dev/null", O_RDWR);
  ASSERT_GE(fd, 0);
  // Close-on-exec is always added.
  EXPECT_NE(::fcntl(fd, F_GETFD) & FD_CLOEXEC, 0);
  std::uint8_t mode = 0;
  errno = 0;
  EXPECT_EQ(syscalls.ioctl(fd, SPI_IOC_WR_MODE, &mode), -1);
  EXPECT_EQ(errno, ENOTTY);
  EXPECT_EQ(syscalls.close(fd), 0);
}

TEST(LinuxSyscalls, OpenMissingFile) {
  LinuxSyscalls syscalls;
  errno = 0;
  EXPECT_EQ(syscalls.open("/nonexistent/epaper/device", O_RDWR), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(LinuxSyscalls, ListGpiochips) {
  const auto directory = make_temp_dir();
  for (const auto* name : { "gpiochip1", "gpiochip0", "spidev0.0", "gpiochip_not_a_number_but_matches" }) {
    std::ofstream(directory / name) << "";
  }
  LinuxSyscalls syscalls(directory.string());
  const std::vector<std::string> expected = {
    (directory / "gpiochip0").string(),
    (directory / "gpiochip1").string(),
    (directory / "gpiochip_not_a_number_but_matches").string(),
  };
  EXPECT_EQ(syscalls.list_gpiochips(), expected);
  std::filesystem::remove_all(directory);
}

TEST(LinuxSyscalls, ListGpiochipsOfMissingDirectory) {
  LinuxSyscalls syscalls("/nonexistent/epaper/dev");
  EXPECT_TRUE(syscalls.list_gpiochips().empty());
}

TEST(FileDescriptor, DefaultIsInvalid) {
  FileDescriptor fd;
  EXPECT_FALSE(fd.valid());
  EXPECT_EQ(fd.get(), -1);
  fd.reset();
  EXPECT_FALSE(fd.valid());
}

TEST(FileDescriptor, ClosesOnDestruction) {
  auto syscalls = std::make_shared<FakeSyscalls>();
  {
    FileDescriptor fd(syscalls, 7);
    EXPECT_TRUE(fd.valid());
    EXPECT_EQ(fd.get(), 7);
  }
  EXPECT_EQ(syscalls->closed, std::vector<int>{ 7 });
}

TEST(FileDescriptor, MoveTransfersOwnership) {
  auto syscalls = std::make_shared<FakeSyscalls>();
  FileDescriptor first(syscalls, 7);
  FileDescriptor second(std::move(first));
  EXPECT_FALSE(first.valid());  // Moved-from state is specified.
  EXPECT_EQ(second.get(), 7);

  FileDescriptor third(syscalls, 8);
  third = std::move(second);
  EXPECT_EQ(syscalls->closed, std::vector<int>{ 8 });
  EXPECT_EQ(third.get(), 7);
  EXPECT_FALSE(second.valid());  // Moved-from state is specified.

  third.reset();
  EXPECT_EQ(syscalls->closed, (std::vector<int>{ 8, 7 }));
  EXPECT_FALSE(third.valid());
}

TEST(FileDescriptor, WithoutSyscallsNothingIsClosed) {
  FileDescriptor fd(nullptr, 9);
  EXPECT_TRUE(fd.valid());
  fd.reset();
  EXPECT_FALSE(fd.valid());
}

TEST(Helpers, OpenOrThrow) {
  auto syscalls = std::make_shared<FakeSyscalls>();
  syscalls->add_spidev("/dev/spidev0.0");
  auto fd = open_or_throw(syscalls, "/dev/spidev0.0", O_RDWR);
  EXPECT_TRUE(fd.valid());
  try {
    open_or_throw(syscalls, "/dev/missing", O_RDWR);
    FAIL() << "expected SystemError";
  } catch (const SystemError& error) {
    EXPECT_EQ(error.error_number(), ENOENT);
    EXPECT_NE(std::string(error.what()).find("cannot open /dev/missing"), std::string::npos);
  }
}

TEST(Helpers, IoctlOrThrow) {
  FakeSyscalls syscalls;
  std::uint8_t mode = 1;
  EXPECT_EQ(ioctl_or_throw(syscalls, 3, SPI_IOC_WR_MODE, &mode, "set mode on", "/dev/spidev0.0"), 0);
  EXPECT_EQ(syscalls.spi_mode, 1);
  syscalls.fail_ioctl[SPI_IOC_WR_MODE] = EINVAL;
  try {
    ioctl_or_throw(syscalls, 3, SPI_IOC_WR_MODE, &mode, "set mode on", "/dev/spidev0.0");
    FAIL() << "expected SystemError";
  } catch (const SystemError& error) {
    EXPECT_EQ(error.error_number(), EINVAL);
    EXPECT_NE(std::string(error.what()).find("set mode on /dev/spidev0.0"), std::string::npos);
  }
}

}  // namespace
}  // namespace epaper::os
