#pragma once

#if defined(BOARD_A9G)
#include "a9g/sleep.h"
#elif defined(BOARD_ESP)
#include "esp/sleep.h"
#else
#error "Please define BOARD_A9G or BOARD_ESP in build flags"
#endif
