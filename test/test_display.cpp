#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "epaper/display.hpp"
#include "epaper/errors.hpp"
#include "fakes/fake_hal.hpp"

namespace epaper {
namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;
using testing::FakeClock;

/// Panel that records calls in a log shared with the test.
class RecordingPanel final : public Panel {
public:
  struct Log {
    std::vector<std::string> calls;
    std::vector<RefreshMode> modes;
    std::vector<Framebuffer> frames;
    bool supports_partial = true;
    bool supports_fast = true;
    bool resumes_after_sleep = false;
    bool fail_display = false;
    bool fail_sleep = false;
  };

  explicit RecordingPanel(std::shared_ptr<Log> log)
  : _log(std::move(log)) { }

  std::string name() const override {
    return "recording";
  }

  std::uint16_t width() const noexcept override {
    return 16;
  }

  std::uint16_t height() const noexcept override {
    return 8;
  }

  bool supports(RefreshMode mode) const noexcept override {
    return mode == RefreshMode::kFull || (mode == RefreshMode::kPartial && _log->supports_partial) ||
           (mode == RefreshMode::kFast && _log->supports_fast);
  }

  void init() override {
    _log->calls.push_back("init");
  }

  void display(const Framebuffer& framebuffer, RefreshMode mode) override {
    if (_log->fail_display) {
      throw Error("panel unplugged");
    }
    _log->calls.push_back("display");
    _log->modes.push_back(mode);
    _log->frames.push_back(framebuffer);
  }

  void sleep() override {
    if (_log->fail_sleep) {
      throw Error("panel unplugged");
    }
    _log->calls.push_back("sleep");
  }

  bool resumes_after_sleep() const noexcept override {
    return _log->resumes_after_sleep;
  }

private:
  std::shared_ptr<Log> _log;
};

class DisplayTest : public ::testing::Test {
protected:
  std::unique_ptr<Display> make(DisplayOptions options = { }) {
    return std::make_unique<Display>(std::make_shared<RecordingPanel>(log), options, clock);
  }

  std::shared_ptr<RecordingPanel::Log> log = std::make_shared<RecordingPanel::Log>();
  std::shared_ptr<FakeClock> clock = std::make_shared<FakeClock>();
};

TEST_F(DisplayTest, RejectsMissingDependencies) {
  EXPECT_THROW(Display(nullptr), std::invalid_argument);
  EXPECT_THROW(Display(std::make_shared<RecordingPanel>(log), { }, nullptr), std::invalid_argument);
}

TEST_F(DisplayTest, Accessors) {
  DisplayOptions options;
  options.rotation = Rotation::k90;
  options.full_refresh_every = 7;
  auto display = make(options);
  EXPECT_EQ(display->canvas().width(), 8);
  EXPECT_EQ(display->canvas().height(), 16);
  EXPECT_EQ(display->framebuffer().width(), 16);
  EXPECT_EQ(display->panel().name(), "recording");
  EXPECT_EQ(display->options().full_refresh_every, 7u);
  EXPECT_FALSE(display->awake());
  // Drawing goes into the display's own framebuffer.
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  EXPECT_EQ(display->framebuffer().get(15, 0), Color::kBlack);
}

TEST_F(DisplayTest, DefaultClockIsSystemClock) {
  Display display(std::make_shared<RecordingPanel>(log));
  EXPECT_TRUE(display.refresh());
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFull });
}

TEST(SystemClock, SleepsAndAdvances) {
  SystemClock clock;
  const auto before = clock.now();
  clock.sleep_for(milliseconds{ 5 });
  EXPECT_GE(clock.now() - before, milliseconds{ 5 });
}

TEST_F(DisplayTest, FirstRefreshInitializesAndIsFull) {
  auto display = make();
  EXPECT_TRUE(display->refresh());
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display" }));
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFull });
  EXPECT_TRUE(display->awake());
}

TEST_F(DisplayTest, BeginForcesFullRefresh) {
  auto display = make();
  display->begin();
  display->canvas().draw_pixel(1, 1, Color::kBlack);
  display->refresh();
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display" }));
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFull });
}

TEST_F(DisplayTest, SkipsUnchangedFrames) {
  auto display = make();
  EXPECT_TRUE(display->refresh());
  EXPECT_FALSE(display->refresh());
  EXPECT_TRUE(display->refresh(RefreshMode::kPartial, true));
  EXPECT_EQ(log->modes, (std::vector<RefreshMode>{ RefreshMode::kFull, RefreshMode::kPartial }));
}

TEST_F(DisplayTest, PartialAfterFullAndPeriodicFullRefresh) {
  DisplayOptions options;
  options.full_refresh_every = 2;
  options.full_refresh_period = seconds{ 0 };
  auto display = make(options);
  std::vector<RefreshMode> expected;
  for (int i = 0; i < 7; ++i) {
    display->canvas().draw_pixel(i, 0, Color::kBlack);
    ASSERT_TRUE(display->refresh());
  }
  EXPECT_EQ(
    log->modes, (std::vector<RefreshMode>{
        RefreshMode::kFull, RefreshMode::kPartial, RefreshMode::kPartial, RefreshMode::kFull,
        RefreshMode::kPartial, RefreshMode::kPartial, RefreshMode::kFull }));
  EXPECT_EQ(display->partial_refreshes_since_full(), 0u);
  EXPECT_EQ(log->frames.back(), display->framebuffer());
}

TEST_F(DisplayTest, CountLimitCanBeDisabled) {
  DisplayOptions options;
  options.full_refresh_every = 0;
  options.full_refresh_period = seconds{ 0 };
  auto display = make(options);
  for (int i = 0; i < 100; ++i) {
    display->canvas().draw_pixel(i % 16, i / 16, Color::kBlack);
    display->refresh();
  }
  EXPECT_EQ(display->partial_refreshes_since_full(), 99u);
}

TEST_F(DisplayTest, FullRefreshAfterPeriod) {
  DisplayOptions options;
  options.full_refresh_every = 0;
  options.full_refresh_period = seconds{ 60 };
  auto display = make(options);
  display->refresh();
  clock->advance(milliseconds{ 59000 });
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  display->refresh();
  clock->advance(milliseconds{ 1000 });
  display->canvas().draw_pixel(1, 0, Color::kBlack);
  display->refresh();
  EXPECT_EQ(log->modes, (std::vector<RefreshMode>{ RefreshMode::kFull, RefreshMode::kPartial, RefreshMode::kFull }));
}

TEST_F(DisplayTest, UnsupportedModesFallBackToFull) {
  log->supports_partial = false;
  log->supports_fast = false;
  auto display = make();
  display->refresh();
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  display->refresh(RefreshMode::kPartial);
  display->canvas().draw_pixel(1, 0, Color::kBlack);
  display->refresh(RefreshMode::kFast);
  EXPECT_EQ(log->modes, (std::vector<RefreshMode>{ RefreshMode::kFull, RefreshMode::kFull, RefreshMode::kFull }));
}

TEST_F(DisplayTest, FastIsNotPromoted) {
  auto display = make();
  display->refresh(RefreshMode::kFast);
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFast });
  // Fast is a full waveform: partial refreshes can follow.
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  display->refresh();
  EXPECT_EQ(log->modes.back(), RefreshMode::kPartial);
}

TEST_F(DisplayTest, Clear) {
  auto display = make();
  display->clear(Color::kBlack);
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFull });
  EXPECT_EQ(log->frames.back(), Framebuffer(16, 8, Color::kBlack));
  // Even an unchanged canvas is pushed again.
  display->clear(Color::kBlack);
  EXPECT_EQ(log->modes.size(), 2u);
  display->clear();
  EXPECT_EQ(log->frames.back(), Framebuffer(16, 8));
}

TEST_F(DisplayTest, SleepAndAutomaticWake) {
  auto display = make();
  display->sleep();
  EXPECT_TRUE(log->calls.empty());

  display->refresh();
  display->sleep();
  EXPECT_FALSE(display->awake());
  // Nothing changed: the sleeping panel still shows the right image.
  EXPECT_FALSE(display->refresh());

  display->canvas().draw_pixel(3, 3, Color::kBlack);
  EXPECT_TRUE(display->refresh());
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display", "sleep", "init", "display" }));
  EXPECT_EQ(log->modes.back(), RefreshMode::kFull);
}

TEST_F(DisplayTest, ResumingPanelStaysPartialAfterWake) {
  log->resumes_after_sleep = true;
  auto display = make();
  display->refresh();
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  display->refresh();
  display->sleep();

  display->canvas().draw_pixel(1, 0, Color::kBlack);
  EXPECT_TRUE(display->refresh());
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display", "display", "sleep", "init", "display" }));
  EXPECT_EQ(log->modes, (std::vector<RefreshMode>{ RefreshMode::kFull, RefreshMode::kPartial, RefreshMode::kPartial }));
  // Sleeping does not reset the ghosting budget.
  EXPECT_EQ(display->partial_refreshes_since_full(), 2u);
}

TEST_F(DisplayTest, ResumingPanelStillNeedsItsFirstFullRefresh) {
  log->resumes_after_sleep = true;
  auto display = make();
  display->begin();
  display->sleep();
  display->refresh();
  EXPECT_EQ(log->modes, std::vector<RefreshMode>{ RefreshMode::kFull });

  // An explicit begin() always asks for a full refresh.
  display->sleep();
  display->begin();
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  display->refresh();
  EXPECT_EQ(log->modes.back(), RefreshMode::kFull);
}

TEST_F(DisplayTest, FailedSleepForcesFullRefresh) {
  log->resumes_after_sleep = true;
  auto display = make();
  display->refresh();
  log->fail_sleep = true;
  EXPECT_THROW(display->sleep(), Error);
  EXPECT_FALSE(display->awake());

  display->canvas().draw_pixel(0, 0, Color::kBlack);
  EXPECT_TRUE(display->refresh());
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display", "init", "display" }));
  EXPECT_EQ(log->modes.back(), RefreshMode::kFull);
}

TEST_F(DisplayTest, FailedRefreshReinitializesNextTime) {
  auto display = make();
  display->refresh();
  log->fail_display = true;
  display->canvas().draw_pixel(0, 0, Color::kBlack);
  EXPECT_THROW(display->refresh(), Error);
  EXPECT_FALSE(display->awake());

  log->fail_display = false;
  // Same canvas as the failed attempt, still pushed.
  EXPECT_TRUE(display->refresh());
  EXPECT_EQ(log->calls, (std::vector<std::string>{ "init", "display", "init", "display" }));
  EXPECT_EQ(log->modes.back(), RefreshMode::kFull);
}

}  // namespace
}  // namespace epaper
