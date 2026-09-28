#include <gtest/gtest.h>

#include <cerrno>
#include <stdexcept>
#include <string>

#include "epaper/errors.hpp"
#include "epaper/types.hpp"

namespace epaper {
namespace {

TEST(Types, RotationFromDegrees) {
  EXPECT_EQ(rotation_from_degrees(0), Rotation::k0);
  EXPECT_EQ(rotation_from_degrees(90), Rotation::k90);
  EXPECT_EQ(rotation_from_degrees(180), Rotation::k180);
  EXPECT_EQ(rotation_from_degrees(270), Rotation::k270);
  EXPECT_THROW(rotation_from_degrees(45), std::invalid_argument);
  EXPECT_THROW(rotation_from_degrees(-90), std::invalid_argument);
  EXPECT_THROW(rotation_from_degrees(360), std::invalid_argument);
}

TEST(Types, RefreshModeNames) {
  EXPECT_STREQ(to_string(RefreshMode::kFull), "full");
  EXPECT_STREQ(to_string(RefreshMode::kFast), "fast");
  EXPECT_STREQ(to_string(RefreshMode::kPartial), "partial");
  EXPECT_STREQ(to_string(static_cast<RefreshMode>(42)), "unknown");
}

TEST(Types, InvertColor) {
  EXPECT_EQ(invert(Color::kBlack), Color::kWhite);
  EXPECT_EQ(invert(Color::kWhite), Color::kBlack);
  static_assert(invert(Color::kBlack) == Color::kWhite, "invert is constexpr");
}

TEST(Errors, SystemErrorKeepsErrno) {
  const SystemError error("cannot open /dev/spidev0.0", ENOENT);
  EXPECT_EQ(error.error_number(), ENOENT);
  EXPECT_NE(std::string(error.what()).find("cannot open /dev/spidev0.0: "), std::string::npos);
  EXPECT_NE(std::string(error.what()).find("No such file"), std::string::npos);
}

TEST(Errors, Hierarchy) {
  EXPECT_THROW(throw TimeoutError("busy"), Error);
  EXPECT_THROW(throw SystemError("x", EIO), Error);
  EXPECT_THROW(throw Error("x"), std::runtime_error);
}

}  // namespace
}  // namespace epaper
