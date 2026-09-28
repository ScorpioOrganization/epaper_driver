#include <linux/gpio.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "epaper/errors.hpp"
#include "epaper/linux/gpio.hpp"
#include "fakes/fake_syscalls.hpp"

namespace epaper::os {
namespace {

using testing::FakeSyscalls;

TEST(LineSpec, ParseName) {
  const auto spec = LineSpec::parse("PR.04");
  EXPECT_EQ(spec.name, "PR.04");
  EXPECT_TRUE(spec.chip.empty());
  EXPECT_LT(spec.offset, 0);
  EXPECT_EQ(spec.to_string(), "PR.04");
}

TEST(LineSpec, ParseChipAndOffset) {
  auto spec = LineSpec::parse("gpiochip0:112");
  EXPECT_EQ(spec.chip, "gpiochip0");
  EXPECT_EQ(spec.offset, 112);
  EXPECT_TRUE(spec.name.empty());
  EXPECT_EQ(spec.to_string(), "gpiochip0:112");

  spec = LineSpec::parse("/dev/gpiochip4:17");
  EXPECT_EQ(spec.chip, "/dev/gpiochip4");
  EXPECT_EQ(spec.offset, 17);
}

TEST(LineSpec, NameOnSpecificChipToString) {
  LineSpec spec;
  spec.chip = "gpiochip1";
  spec.name = "GPIO17";
  EXPECT_EQ(spec.to_string(), "GPIO17@gpiochip1");
}

TEST(LineSpec, ParseErrors) {
  for (const auto* text : { "", ":12", "gpiochip0:", "gpiochip0:x1", "gpiochip0:-1", "gpiochip0:1234567" }) {
    EXPECT_THROW(LineSpec::parse(text), std::invalid_argument) << text;
  }
}

TEST(GpiochipPath, NamesAndPaths) {
  EXPECT_EQ(gpiochip_path("gpiochip0"), "/dev/gpiochip0");
  EXPECT_EQ(gpiochip_path("/tmp/gpiochip0"), "/tmp/gpiochip0");
  EXPECT_THROW(gpiochip_path(""), std::invalid_argument);
}

class LinuxGpioTest : public ::testing::Test {
protected:
  void SetUp() override {
    syscalls->add_chip("/dev/gpiochip0", { "PA.00", "PR.04", "PH.00" });
    syscalls->add_chip("/dev/gpiochip1", { "PAA.00", "PP.04", "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345" });
  }

  std::shared_ptr<FakeSyscalls> syscalls = std::make_shared<FakeSyscalls>();
};

TEST_F(LinuxGpioTest, OutputByChipAndOffset) {
  LinuxOutputPin pin(syscalls, LineSpec::parse("gpiochip1:2"), "epaper-test", true);
  EXPECT_EQ(pin.line().chip_path(), "/dev/gpiochip1");
  EXPECT_EQ(pin.line().offset(), 2u);
  auto* line = syscalls->find_line("/dev/gpiochip1", 2);
  ASSERT_NE(line, nullptr);
  EXPECT_EQ(line->flags, GPIO_V2_LINE_FLAG_OUTPUT);
  EXPECT_EQ(line->consumer, "epaper-test");
  EXPECT_TRUE(line->value);

  pin.write(false);
  pin.write(true);
  EXPECT_EQ(line->writes, (std::vector<bool>{ false, true }));
  // The chip file descriptor is only needed while requesting the line.
  EXPECT_EQ(syscalls->open_files.size(), 0u);
}

TEST_F(LinuxGpioTest, OutputDefaultsLow) {
  LinuxOutputPin pin(syscalls, LineSpec::parse("gpiochip0:0"), "epaper-test");
  EXPECT_FALSE(syscalls->find_line("/dev/gpiochip0", 0)->value);
}

TEST_F(LinuxGpioTest, InputFoundByNameOnAnyChip) {
  LinuxInputPin pin(syscalls, LineSpec::parse("PP.04"), "epaper-test");
  EXPECT_EQ(pin.line().chip_path(), "/dev/gpiochip1");
  EXPECT_EQ(pin.line().offset(), 1u);
  auto* line = syscalls->find_line("/dev/gpiochip1", 1);
  ASSERT_NE(line, nullptr);
  EXPECT_EQ(line->flags, GPIO_V2_LINE_FLAG_INPUT);
  EXPECT_FALSE(pin.read());
  line->value = true;
  EXPECT_TRUE(pin.read());
}

TEST_F(LinuxGpioTest, NameLookupRestrictedToChip) {
  LineSpec spec;
  spec.chip = "gpiochip0";
  spec.name = "PH.00";
  LinuxInputPin pin(syscalls, spec, "epaper-test");
  EXPECT_EQ(pin.line().chip_path(), "/dev/gpiochip0");
  EXPECT_EQ(pin.line().offset(), 2u);

  spec.name = "PP.04";
  EXPECT_THROW(LinuxInputPin(syscalls, spec, "epaper-test"), Error);
}

TEST_F(LinuxGpioTest, NameFillingTheWholeField) {
  LinuxInputPin pin(syscalls, LineSpec::parse("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"), "t");
  EXPECT_EQ(pin.line().offset(), 2u);
}

TEST_F(LinuxGpioTest, ConsumerIsTruncated) {
  const std::string consumer(100, 'c');
  LinuxOutputPin pin(syscalls, LineSpec::parse("gpiochip0:1"), consumer);
  EXPECT_EQ(syscalls->find_line("/dev/gpiochip0", 1)->consumer, std::string(GPIO_MAX_NAME_SIZE - 1, 'c'));
}

TEST_F(LinuxGpioTest, ReleasesLineOnDestruction) {
  {
    LinuxOutputPin pin(syscalls, LineSpec::parse("gpiochip0:1"), "t");
    EXPECT_EQ(syscalls->lines.size(), 1u);
  }
  EXPECT_TRUE(syscalls->lines.empty());
}

TEST_F(LinuxGpioTest, UnknownLineName) {
  try {
    LinuxInputPin pin(syscalls, LineSpec::parse("GPIO17"), "t");
    FAIL() << "expected Error";
  } catch (const Error& error) {
    EXPECT_NE(std::string(error.what()).find("'GPIO17' not found"), std::string::npos);
  }
  EXPECT_TRUE(syscalls->open_files.empty());
}

TEST_F(LinuxGpioTest, NoChipsAtAll) {
  auto empty = std::make_shared<FakeSyscalls>();
  EXPECT_THROW(LinuxInputPin(empty, LineSpec::parse("PR.04"), "t"), Error);
}

TEST_F(LinuxGpioTest, InvalidArguments) {
  EXPECT_THROW(LinuxOutputPin(nullptr, LineSpec::parse("gpiochip0:1"), "t"), std::invalid_argument);
  LineSpec spec;
  spec.offset = 1;
  EXPECT_THROW(LinuxOutputPin(syscalls, spec, "t"), std::invalid_argument);
}

TEST_F(LinuxGpioTest, OpenFailures) {
  EXPECT_THROW(LinuxOutputPin(syscalls, LineSpec::parse("gpiochip7:1"), "t"), SystemError);
  syscalls->fail_open.insert("/dev/gpiochip0");
  EXPECT_THROW(LinuxOutputPin(syscalls, LineSpec::parse("PR.04"), "t"), SystemError);
}

TEST_F(LinuxGpioTest, IoctlFailures) {
  for (const auto request : { GPIO_GET_CHIPINFO_IOCTL, GPIO_V2_GET_LINEINFO_IOCTL, GPIO_V2_GET_LINE_IOCTL }) {
    syscalls->fail_ioctl.clear();
    syscalls->fail_ioctl[request] = EBUSY;
    EXPECT_THROW(LinuxOutputPin(syscalls, LineSpec::parse("PR.04"), "t"), SystemError);
    EXPECT_TRUE(syscalls->open_files.empty());
  }
}

TEST_F(LinuxGpioTest, ValueIoFailures) {
  LinuxOutputPin output(syscalls, LineSpec::parse("gpiochip0:0"), "t");
  LinuxInputPin input(syscalls, LineSpec::parse("gpiochip0:2"), "t");
  syscalls->fail_ioctl[GPIO_V2_LINE_SET_VALUES_IOCTL] = EIO;
  syscalls->fail_ioctl[GPIO_V2_LINE_GET_VALUES_IOCTL] = EIO;
  EXPECT_THROW(output.write(true), SystemError);
  EXPECT_THROW(input.read(), SystemError);
}

}  // namespace
}  // namespace epaper::os
