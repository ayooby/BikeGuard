#pragma once

#if defined(BOARD_A9G)
#include "a9g/motion.h"
#elif defined(BOARD_ESP)
#include "esp/motion.h"
#else
#error "Please define BOARD_A9G or BOARD_ESP in build flags"
#endif
