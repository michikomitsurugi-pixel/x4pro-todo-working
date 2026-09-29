#pragma once
// Minimal logging shim for FreeInk SDK libraries that expect the firmware to
// provide Logging.h. Messages go to the USB serial monitor.
#include <Arduino.h>

#define LOG_ERR(tag, fmt, ...) Serial.printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_WRN(tag, fmt, ...) Serial.printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_INF(tag, fmt, ...) Serial.printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define LOG_DBG(tag, fmt, ...) do {} while (0)
