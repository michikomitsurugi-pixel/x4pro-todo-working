#pragma once
#include "tasks.h"

struct UiStatus {
  bool phoneConnected = false;
  bool sleeping = false;
  bool removeArmed = false;  // "Remove done" tapped once, waiting for confirmation
  int batteryPct = -1;       // -1 = unknown
};

enum class HitKind { None, Row, PrevPage, NextPage, RemoveDone };
struct Hit {
  HitKind kind = HitKind::None;
  int index = -1;  // task index for HitKind::Row
};

constexpr int ITEMS_PER_PAGE = 10;
int uiPageCount(size_t taskCount);

void uiBegin();
void uiRender(const TaskStore& store, int page, const UiStatus& st, bool forceFull);
void uiSleep();

// Convert a touch (normalized 0..1 in the panel's native frame) to screen coordinates.
void uiTouchToScreen(float nx, float ny, int& x, int& y);
Hit uiHitTest(const TaskStore& store, int page, int x, int y);
