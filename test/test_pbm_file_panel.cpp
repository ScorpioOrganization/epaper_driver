#include <stdlib.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "epaper/errors.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/panels/pbm_file_panel.hpp"

namespace epaper {
namespace {

class PbmFilePanelTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto pattern = (std::filesystem::temp_directory_path() / "epaper_pbm_XXXXXX").string();
    ASSERT_NE(::mkdtemp(pattern.data()), nullptr);
    directory = pattern;
  }

  void TearDown() override {
    std::filesystem::remove_all(directory);
  }

  static std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
  }

  std::filesystem::path directory;
};

TEST_F(PbmFilePanelTest, Identity) {
  PbmFilePanel panel((directory / "screen.pbm").string(), 296, 128);
  EXPECT_EQ(panel.width(), 296);
  EXPECT_EQ(panel.height(), 128);
  EXPECT_EQ(panel.path(), (directory / "screen.pbm").string());
  EXPECT_NE(panel.name().find("screen.pbm"), std::string::npos);
  EXPECT_TRUE(panel.supports(RefreshMode::kFull));
  EXPECT_TRUE(panel.supports(RefreshMode::kFast));
  EXPECT_TRUE(panel.supports(RefreshMode::kPartial));
  // A file keeps its image forever: waking up needs no full refresh.
  EXPECT_TRUE(panel.resumes_after_sleep());
  EXPECT_FALSE(panel.initialized());
  EXPECT_FALSE(panel.last_mode().has_value());
  EXPECT_EQ(PbmFilePanel("x.pbm").width(), 128);
  EXPECT_EQ(PbmFilePanel("x.pbm").height(), 296);
}

TEST_F(PbmFilePanelTest, RejectsInvalidArguments) {
  EXPECT_THROW(PbmFilePanel(""), std::invalid_argument);
  EXPECT_THROW(PbmFilePanel("x.pbm", 0, 10), std::invalid_argument);
  EXPECT_THROW(PbmFilePanel("x.pbm", 10, 0), std::invalid_argument);
}

TEST_F(PbmFilePanelTest, WritesFrames) {
  const auto path = directory / "screen.pbm";
  PbmFilePanel panel(path.string(), 16, 4);
  panel.init();
  EXPECT_TRUE(panel.initialized());
  EXPECT_EQ(panel.init_count(), 1u);

  Framebuffer framebuffer(16, 4);
  framebuffer.set(3, 2, Color::kBlack);
  panel.display(framebuffer, RefreshMode::kPartial);
  EXPECT_EQ(Framebuffer::from_pbm(read(path)), framebuffer);
  EXPECT_EQ(panel.display_count(), 1u);
  EXPECT_EQ(panel.last_mode(), RefreshMode::kPartial);
  EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));

  framebuffer.fill(Color::kBlack);
  panel.display(framebuffer, RefreshMode::kFull);
  EXPECT_EQ(Framebuffer::from_pbm(read(path)), framebuffer);
  EXPECT_EQ(panel.display_count(), 2u);
  EXPECT_EQ(panel.last_mode(), RefreshMode::kFull);

  panel.sleep();
  EXPECT_FALSE(panel.initialized());
  EXPECT_EQ(panel.sleep_count(), 1u);
  EXPECT_THROW(panel.display(framebuffer, RefreshMode::kFull), std::logic_error);
}

TEST_F(PbmFilePanelTest, RejectsWrongSize) {
  PbmFilePanel panel((directory / "screen.pbm").string(), 16, 4);
  panel.init();
  EXPECT_THROW(panel.display(Framebuffer(16, 5), RefreshMode::kFull), std::invalid_argument);
  EXPECT_THROW(panel.display(Framebuffer(8, 4), RefreshMode::kFull), std::invalid_argument);
}

TEST_F(PbmFilePanelTest, WriteFailure) {
  PbmFilePanel panel((directory / "missing" / "screen.pbm").string(), 8, 8);
  panel.init();
  EXPECT_THROW(panel.display(Framebuffer(8, 8), RefreshMode::kFull), Error);
  EXPECT_EQ(panel.display_count(), 0u);
}

TEST_F(PbmFilePanelTest, RenameFailure) {
  // The target is a non-empty directory, so the temporary file cannot replace it.
  const auto target = directory / "busy";
  std::filesystem::create_directories(target / "child");
  PbmFilePanel panel(target.string(), 8, 8);
  panel.init();
  EXPECT_THROW(panel.display(Framebuffer(8, 8), RefreshMode::kFull), SystemError);
  EXPECT_FALSE(std::filesystem::exists(target.string() + ".tmp"));
  EXPECT_EQ(panel.display_count(), 0u);
}

}  // namespace
}  // namespace epaper
