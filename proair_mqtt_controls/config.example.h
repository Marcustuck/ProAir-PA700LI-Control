#pragma once
// Copy to config.local.h and edit that ignored file only. Never publish real values.
static const char* WIFI_SSID="";
static const char* WIFI_PASSWORD="";
static const char* MQTT_SERVER="";
static const int MQTT_PORT=1883;
static const char* MQTT_USER="";
static const char* MQTT_PASS="";
static const char* MQTT_CLIENT="";
// Your authorized installation's protocol credentials; no shared/default credentials are supplied.
// Printable ASCII, 1..24 characters each. Both must be configured before any TX.
static const char* GM3_LOGIN="";
static const char* GM3_PIN="";
#ifndef ENABLE_CONTROL
#define ENABLE_CONTROL 0
#endif
#ifndef DE_RE_WIRING_CONFIRMED
#define DE_RE_WIRING_CONFIRMED 0
#endif
