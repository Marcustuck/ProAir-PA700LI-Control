#pragma once
#include <stdint.h>
// Never commit config.local.h. A fresh checkout uses only blank example values.
#if __has_include("config.local.h")
#include "config.local.h"
#else
#include "config.example.h"
#endif
static constexpr uint32_t BAUDRATE=115200;
static constexpr int RX_PIN=18, TX_PIN=17, DE_PIN=4;
#ifndef ENABLE_CONTROL
#define ENABLE_CONTROL 0
#endif
#ifndef DE_RE_WIRING_CONFIRMED
#define DE_RE_WIRING_CONFIRMED 0
#endif
#if ENABLE_CONTROL && !DE_RE_WIRING_CONFIRMED
#error "Before enabling controls confirm GPIO4 DE and /RE wiring, LOW=receive, HIGH=drive; then set DE_RE_WIRING_CONFIRMED=1."
#endif
