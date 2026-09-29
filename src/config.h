#pragma once
#include <Arduino.h>

// Pins live in the FreeInk SDK's X4 Pro board profile, not here.

// ---- Behaviour ----
constexpr uint32_t IDLE_SLEEP_MS      = 3UL * 60UL * 1000UL; // sleep after 3 min idle (not while phone connected)
constexpr uint32_t REMOVE_CONFIRM_MS  = 5000;  // "Remove done" must be tapped twice within this time
constexpr int      FULL_REFRESH_EVERY = 15;    // fast refreshes before a full one (clears ghosting)
constexpr size_t   MAX_TASKS          = 40;
constexpr size_t   MAX_TASK_LEN       = 120;
constexpr char     DEVICE_NAME[]      = "X4-Todo";

// ---- Light ----
// Holding the Home key steps through these brightness levels (percent), then off.
constexpr uint8_t LIGHT_LEVELS[] = {10, 35, 70};
constexpr uint8_t LIGHT_WARMTH = 40;  // 0 = cool white, 100 = warm

// ---- Orientation (adjust during bring-up) ----
// Portrait. If the screen is upside down, change 3 to 1.
constexpr uint8_t DISPLAY_ROTATION = 3;
// If taps land in the wrong place (e.g. mirrored), flip one or both of these.
constexpr bool TOUCH_FLIP_X = false;
constexpr bool TOUCH_FLIP_Y = false;
