// X4 Todo for the Xteink X4 Pro: a minimal to-do list firmware.
//
// Touch:    tap a task to mark it done / not done
//           "Remove done" (tap twice) clears finished tasks; < and > change page
// Buttons:  Left = previous page   Right = next page
//           Home key = back to page 1 and fully refresh the screen
//           Home key (hold) = light: low, medium, high, off
//           Power = sleep (the list stays on screen; press power again to wake)
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <BoardConfig.h>
#include <InputManager.h>
#include <BatteryMonitor.h>
#include <PowerManager.h>
#include <FrontlightManager.h>
#include <Preferences.h>
#include "config.h"
#include "tasks.h"
#include "ui.h"
#include "ble_link.h"

static TaskStore store;
static InputManager input;
static BatteryMonitor battery;
static FrontlightManager light;
static Preferences prefs;
static uint8_t lightStep = 0;  // 0 = off, 1..N = LIGHT_LEVELS[step - 1]
static constexpr uint8_t LIGHT_STEPS = sizeof(LIGHT_LEVELS) / sizeof(LIGHT_LEVELS[0]);

static int page = 0;
static bool dirty = true;
static bool forceFull = true;
static bool lastConnected = false;
static uint32_t lastActivity = 0;
static uint32_t removeArmedAt = 0;  // 0 = not armed

static void applyLight() {
  if (lightStep == 0) light.off();
  else light.setBrightness(LIGHT_LEVELS[lightStep - 1]);
}

static void cycleLight() {
  lightStep = (lightStep + 1) % (LIGHT_STEPS + 1);
  applyLight();
  prefs.putUChar("light", lightStep);  // remembered across sleep and restarts
}

static UiStatus status(bool sleeping = false) {
  UiStatus s;
  s.phoneConnected = bleConnected();
  s.sleeping = sleeping;
  s.removeArmed = removeArmedAt != 0 && !sleeping;
  uint16_t pct;
  if (battery.readPercentageChecked(pct)) s.batteryPct = pct;
  return s;
}

static void clampPage() {
  int pages = uiPageCount(store.count());
  if (page >= pages) page = pages - 1;
  if (page < 0) page = 0;
}

static void sendState() {
  String json;
  store.toJson(json);
  bleSendState(json);
}

static void commit() {
  store.save();
  clampPage();
  dirty = true;
  sendState();
}

static void goToSleep() {
  removeArmedAt = 0;
  uiRender(store, page, status(true), false);
  uiSleep();
  light.off();
#ifdef FREEINK_FRONTLIGHT_LS
  light.park();
#endif
  freeink::PowerManager::powerDownRailsForSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();  // waits for release; does not return
}

static void handleTap(float nx, float ny) {
  int x, y;
  uiTouchToScreen(nx, ny, x, y);
  Hit h = uiHitTest(store, page, x, y);

  // Any tap other than the confirming one cancels a pending "Remove done".
  if (h.kind != HitKind::RemoveDone && removeArmedAt) { removeArmedAt = 0; dirty = true; }

  switch (h.kind) {
    case HitKind::Row:
      if (store.toggleAt((size_t)h.index)) commit();
      break;
    case HitKind::PrevPage:
      page--; dirty = true;
      break;
    case HitKind::NextPage:
      page++; dirty = true;
      break;
    case HitKind::RemoveDone:
      if (removeArmedAt) {
        removeArmedAt = 0;
        store.clearDone();
        forceFull = true;
        commit();
      } else {
        removeArmedAt = millis();
        if (removeArmedAt == 0) removeArmedAt = 1;
        dirty = true;
      }
      break;
    default:
      break;
  }
}

static void handleCommand(const char* cmd) {
  JsonDocument d;
  if (deserializeJson(d, cmd)) return;
  const char* op = d["op"] | "";
  uint32_t id = d["id"] | 0;
  bool changed = false;

  if (!strcmp(op, "add"))         changed = store.add(d["text"] | "");
  else if (!strcmp(op, "toggle")) changed = store.toggle(id);
  else if (!strcmp(op, "del"))    changed = store.remove(id);
  else if (!strcmp(op, "edit"))   changed = store.edit(id, d["text"] | "");
  else if (!strcmp(op, "move"))   changed = store.move(id, d["to"] | 0);
  else if (!strcmp(op, "clear"))  changed = store.clearDone() > 0;
  // "get" (or anything unknown) just replies with the current state.

  if (changed) commit();
  else sendState();
}

void setup() {
  Serial.begin(115200);
  BoardConfig::holdPowerRails();
  // The stock X4 Pro bootloader rolls back to the previous firmware unless the new
  // one confirms it started properly. This is that confirmation.
  esp_ota_mark_app_valid_cancel_rollback();

  input.begin();
  freeink::PowerManager::waitForPowerButtonRelease();  // ignore the press that woke us

  store.begin();
  uiBegin();

  prefs.begin("x4todo", false);
  lightStep = prefs.getUChar("light", 0);
  if (lightStep > LIGHT_STEPS) lightStep = 0;
#ifdef FREEINK_FRONTLIGHT_LS
  light.releaseOnWake();
#endif
  light.begin();
  light.setColorTemperature(LIGHT_WARMTH);
  applyLight();
  bleBegin();
  lastActivity = millis();
}

void loop() {
  input.update();

  if (input.wasAnyPressed() || input.wasTouchActivity() || input.wasHomeKeyPressed()) lastActivity = millis();

  if (input.wasPressed(InputManager::BTN_POWER)) goToSleep();
  if (input.wasPressed(InputManager::BTN_UP) && page > 0) { page--; dirty = true; }  // Left key
  if (input.wasPressed(InputManager::BTN_DOWN) && page + 1 < uiPageCount(store.count())) { page++; dirty = true; }  // Right key
  if (input.wasHomeKeyTapped()) { page = 0; forceFull = true; dirty = true; }
  if (input.wasHomeKeyLongPressed()) cycleLight();

  float nx, ny;
  if (input.wasTouchTap(nx, ny)) handleTap(nx, ny);

  if (removeArmedAt && millis() - removeArmedAt > REMOVE_CONFIRM_MS) { removeArmedAt = 0; dirty = true; }

  char cmd[400];
  while (bleNextCommand(cmd, sizeof(cmd))) {
    lastActivity = millis();
    handleCommand(cmd);
  }

  bool conn = bleConnected();
  if (conn != lastConnected) {
    lastConnected = conn;
    lastActivity = millis();
    dirty = true;
  }

  if (dirty) {
    clampPage();
    uiRender(store, page, status(), forceFull);
    dirty = false;
    forceFull = false;
  }

  if (!conn && millis() - lastActivity > IDLE_SLEEP_MS) goToSleep();
  delay(10);
}
