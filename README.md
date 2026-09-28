# epaper_driver

Pure C++17 driver for small SPI e-paper panels on Linux single board computers (NVIDIA Jetson, Raspberry
Pi). No third party dependencies: just the standard library and the Linux kernel
headers (spidev + GPIO character device v2, kernel 5.10 or newer).

Supported panels:

| Panel | Class | Controller | Refresh modes |
| --- | --- | --- | --- |
| Waveshare 2.9" e-Paper Module V2 / V3 / Rev2.x (296x128, 12956) | `epaper::Epd2in9V2` | SSD1680 | full, fast, partial |
| Waveshare 2.9" e-Paper Module V1 (296x128, 12956) | `epaper::Epd2in9V1` | IL3820 | full, partial |
| Virtual panel writing a PBM image file | `epaper::PbmFilePanel` | - | all |

V2 boards have a "V2" / "Rev2.x" sticker on the back, V1 boards have none. Rev2.1 and later boards have a 9 pin
GH1.25 connector with an extra PWR pin (see Wiring) and a level shifter, so they also work with 5 V logic.

## Usage

```cpp
#include "epaper/epaper.hpp"

auto syscalls = std::make_shared<epaper::os::LinuxSyscalls>();
epaper::PanelIo io;
io.spi = std::make_shared<epaper::os::LinuxSpiDevice>(syscalls, epaper::os::SpiConfig{ });  // /dev/spidev0.0
io.reset = std::make_shared<epaper::os::LinuxOutputPin>(syscalls, epaper::os::LineSpec::parse("GPIO17"), "demo", true);
io.data_command = std::make_shared<epaper::os::LinuxOutputPin>(syscalls, epaper::os::LineSpec::parse("GPIO25"), "demo");
io.busy = std::make_shared<epaper::os::LinuxInputPin>(syscalls, epaper::os::LineSpec::parse("GPIO24"), "demo");
io.clock = std::make_shared<epaper::SystemClock>();

epaper::DisplayOptions options;
options.rotation = epaper::Rotation::k90;  // 296x128 landscape canvas

epaper::Display display(std::make_shared<epaper::Epd2in9V2>(std::move(io)), options);
display.begin();
display.clear();                                   // full refresh to white

auto& canvas = display.canvas();
canvas.draw_text(4, 4, "Hello", epaper::kFont5x7, 3);
canvas.draw_seven_segment(4, 40, "25.2", { 22, 40, 5, 4 }, epaper::Color::kBlack);
canvas.draw_rect(200, 40, 80, 40, epaper::Color::kBlack);
display.refresh();                                 // partial refresh, no flashing

display.sleep();                                   // deep sleep, the image stays visible
```

`Display::refresh()` does nothing when the canvas did not change, uses a full refresh for the first frame
after `begin()` and then every `full_refresh_every` partial refreshes or `full_refresh_period` (ghosting
cleanup), and wakes a sleeping panel automatically. After `sleep()` the next refresh stays partial on panels that
can resume (`Panel::resumes_after_sleep()`: V2 and the PBM file), V1 needs a full one.

## Refresh details

The panel drivers follow Waveshare's reference code, plus a few changes that make better use of the hardware:

- V2, back to back partial refreshes: the reference repeats the whole partial setup on every frame (reset pulse,
  waveform upload, RAM ping-pong option, border, a separate analog power up with its own BUSY wait). That setup
  stays valid until the next `init()` or full refresh, so it runs only for the first partial refresh after one;
  the following ones only send the frame and a single `0x22 0xCF` update (clock + analog on, display mode 2,
  analog + clock off).
- V2, partial setup: loads the partial waveform together with its voltages (gate, source, VCOM), as Waveshare's
  Python driver does, instead of only the LUT like their C driver. Nothing depends on what the reset pulse left
  in those registers.
- V2, BUSY: no fixed 50 ms wait after BUSY drops (Waveshare's C driver has it, their Python driver does not).
- V2, waking up: the driver keeps a copy of the frame on the panel. After `sleep()` + `init()` (even when PWR was
  cut and the controller lost its RAM) it writes that frame back into both RAM banks, so the first refresh after
  waking up is still partial, without flashing. A partial refresh before any frame was shown is done as a full
  one.
- V1: the reference update (`0x22 0xC4`) leaves the charge pump running until `sleep()`, which fades and wears a
  panel that stays awake between frames. The driver switches it off after every refresh (`0x22 0xC3`, as GxEPD2
  does).
- Boards with a PWR line: RST and DC are driven low before PWR is cut (as Waveshare's `module_exit()` does), so
  they do not feed the unpowered controller.

Not used on purpose: 4 level grayscale (V2 supports it, but only with a full refresh), the controller's own
temperature compensated waveforms from OTP (Waveshare's waveforms are the tested ones, and the panel is rated for
0 to 50 C anyway), writing only the changed rows (the refresh time is set by the waveform, not by the ~10 ms
SPI transfer of a full frame at 4 MHz).

Main types:

- `Framebuffer` - 1 bit per pixel image in panel RAM layout, `to_pbm()` / `from_pbm()` for previews.
- `Canvas` - clipped drawing with rotation: pixels, lines, rectangles, inverted areas, bitmaps, text in
  the built in 5x7 font with integer scaling, seven segment digits.
- `Panel` - one panel driver; `PanelIo` bundles SPI, RST, DC, BUSY, optional PWR and a clock.
- `Display` - canvas + panel + refresh policy.
- `os::LinuxSpiDevice`, `os::LinuxOutputPin`, `os::LinuxInputPin` - hardware access. GPIO lines are given
  by name (as printed by `gpioinfo`) or as `<chip>:<offset>`. Long SPI writes are split to respect the
  spidev `bufsiz` limit (4096 bytes by default, a full frame is 4736 bytes).
- `os::Syscalls` - every system call goes through this interface, so the Linux backends and panel drivers
  can be tested against a fake kernel.

Interfaces have protected non-virtual destructors (C++ Core Guidelines C.35): implementations are owned
through `std::shared_ptr`.

## Wiring

The module uses the Waveshare Raspberry Pi pinout, the same physical header pins work on a Jetson AGX
Orin. Power the module from 3.3 V.

| Module pin | Header pin | Raspberry Pi 5 line | Jetson AGX Orin line |
| --- | --- | --- | --- |
| VCC | 1 (3.3 V) | - | - |
| GND | 6 | - | - |
| DIN (MOSI) | 19 | SPI0 MOSI | SPI1_MOSI |
| CLK | 23 | SPI0 SCLK | SPI1_CLK |
| CS | 24 | SPI0 CE0 | SPI1_CS0 |
| DC | 22 | `GPIO25` | `PP.04` |
| RST | 11 | `GPIO17` | `PR.04` |
| BUSY | 18 | `GPIO24` | `PH.00` |
| PWR (Rev2.1+ boards only) | 12 | `GPIO18` | `PH.07` |

PWR switches the module's supply on Rev2.1+ boards: connect it either to a GPIO (`PanelIo::power`, then
`sleep()` cuts the power completely and the V2 driver still resumes with a partial refresh) or to 3.3 V
(`PanelIo::power` left null).

Host setup:

- Raspberry Pi 5: `dtparam=spi=on` in `/boot/firmware/config.txt`, the panel is `/dev/spidev0.0`.
- Jetson AGX Orin: enable SPI1 on the 40 pin header with `sudo /opt/nvidia/jetson-io/jetson-io.py`,
  load the driver with `sudo modprobe spidev`, the panel is `/dev/spidev0.0`. Check the GPIO line names
  with `gpioinfo` and make sure the BUSY pin is muxed as a GPIO input.
- The process needs read/write access to `/dev/spidev*` and `/dev/gpiochip*` (root, a privileged
  container or matching udev rules).

## Building

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
cmake --install build --prefix /usr/local   # optional, installs epaper_driverConfig.cmake
```

Tests and install rules are enabled by default only when this is the top level project
(`EPAPER_DRIVER_BUILD_TESTS`, `EPAPER_DRIVER_INSTALL`). Tests use the system GoogleTest when available
and download a pinned release otherwise.

Using it from another CMake project:

```cmake
add_subdirectory(epaper_driver)          # or find_package(epaper_driver) after installing
target_link_libraries(my_app PRIVATE epaper_driver::epaper_driver)
```

## Testing without hardware

- Unit tests run the panel drivers against a recording fake HAL and check the exact command / data byte
  streams against Waveshare's reference driver; the Linux backends run against a fake kernel.
- `PbmFilePanel` stands in for the real panel: every refresh writes a PBM image (view it with any image
  viewer, or convert with `pnmtopng`).

## Credits

Initialization sequences and waveform tables of the Waveshare panels come from Waveshare's e-Paper
reference drivers (`EPD_2in9_V2.c`, `EPD_2in9.c`), MIT licensed, Copyright (c) Waveshare team.
