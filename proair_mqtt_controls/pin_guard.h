#pragma once
// Official installed Nano core macros (not guessed aliases).
#if defined(ARDUINO_NANO_ESP32) && defined(BOARD_HAS_PIN_REMAP) && !defined(BOARD_USES_HW_GPIO_NUMBERS)
#error "ProAir hardware requires Tools > Pin Numbering > By GPIO number (legacy). Keep RX18/TX17; do not rewire."
#endif
