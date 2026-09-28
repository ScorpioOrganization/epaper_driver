#pragma once

// Umbrella header: everything a typical application needs.

#include "epaper/canvas.hpp"
#include "epaper/display.hpp"
#include "epaper/errors.hpp"
#include "epaper/font.hpp"
#include "epaper/framebuffer.hpp"
#include "epaper/hal.hpp"
#include "epaper/linux/gpio.hpp"
#include "epaper/linux/spi.hpp"
#include "epaper/linux/syscalls.hpp"
#include "epaper/panel.hpp"
#include "epaper/panels/epd_2in9_v1.hpp"
#include "epaper/panels/epd_2in9_v2.hpp"
#include "epaper/panels/pbm_file_panel.hpp"
#include "epaper/types.hpp"
