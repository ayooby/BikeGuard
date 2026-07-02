#pragma once

#if defined(BOARD_A9G)
#include "a9g/ignition.h"
#elif defined(BOARD_ESP)
#include "esp/ignition.h"
#else
#error "Please define BOARD_A9G or BOARD_ESP in build flags"
#endif
