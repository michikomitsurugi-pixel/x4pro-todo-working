#include "ui.h"
#include <Adafruit_GFX.h>
#include <EInkDisplay.h>
#include <XteinkDetect.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include "config.h"

// The SDK takes display pins from its X4 Pro board profile; these arguments are ignored.
static EInkDisplay display(-1, -1, -1, -1, -1, -1);

// We draw into an Adafruit GFX canvas (same 1-bit, MSB-first, 1 = white layout as the
// SDK framebuffer) and copy it across before each refresh.
static constexpr int NATIVE_W = 800, NATIVE_H = 480;
static GFXcanvas1* canvas = nullptr;
static int fastCount = 0;
static bool firstRender = true;

static constexpr uint16_t WHITE = 1, BLACK = 0;

// Portrait layout, 480 x 800
static const int MARGIN = 16;
static const int HEADER_H = 90;
static const int ROW_H = 64;
static const int FOOTER_Y = HEADER_H + ITEMS_PER_PAGE * ROW_H;  // 730
static const int BOX = 26;
static const int TEXT_X = MARGIN + BOX + 16;
// Footer tap targets
static const int REMOVE_X0 = 0, REMOVE_X1 = 230;
static const int PREV_X0 = 270, PREV_X1 = 340;
static const int NEXT_X0 = 410, NEXT_X1 = 480;

int uiPageCount(size_t n) { return n == 0 ? 1 : (int)((n + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE); }

void uiBegin() {
  freeink::applyXteinkDisplayController();  // picks the right driver for this unit's screen chip
  display.begin();
  canvas = new GFXcanvas1(NATIVE_W, NATIVE_H);
  canvas->setRotation(DISPLAY_ROTATION);
  canvas->setTextWrap(false);
}

void uiSleep() { display.deepSleep(); }

void uiTouchToScreen(float nx, float ny, int& x, int& y) {
  if (TOUCH_FLIP_X) nx = 1.0f - nx;
  if (TOUCH_FLIP_Y) ny = 1.0f - ny;
  int X = constrain((int)(nx * NATIVE_W), 0, NATIVE_W - 1);
  int Y = constrain((int)(ny * NATIVE_H), 0, NATIVE_H - 1);
  // Inverse of Adafruit GFX canvas rotation.
  switch (DISPLAY_ROTATION & 3) {
    case 0: x = X;                y = Y;                break;
    case 1: x = Y;                y = NATIVE_W - 1 - X; break;
    case 2: x = NATIVE_W - 1 - X; y = NATIVE_H - 1 - Y; break;
    default: x = NATIVE_H - 1 - Y; y = X;               break;
  }
}

Hit uiHitTest(const TaskStore& store, int page, int x, int y) {
  Hit h;
  const int n = (int)store.count();
  if (y >= HEADER_H && y < FOOTER_Y) {
    int i = page * ITEMS_PER_PAGE + (y - HEADER_H) / ROW_H;
    if (i < n) { h.kind = HitKind::Row; h.index = i; }
  } else if (y >= FOOTER_Y) {
    if (x >= REMOVE_X0 && x < REMOVE_X1 && store.doneCount() > 0) h.kind = HitKind::RemoveDone;
    else if (x >= PREV_X0 && x < PREV_X1 && page > 0) h.kind = HitKind::PrevPage;
    else if (x >= NEXT_X0 && x < NEXT_X1 && page + 1 < uiPageCount(n)) h.kind = HitKind::NextPage;
  }
  return h;
}

static int textWidth(const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

// Fit as many whole words from s as possible into maxW. Returns the rest of the text.
static const char* fitLine(const char* s, int maxW, char* out, size_t outSz) {
  char tmp[MAX_TASK_LEN + 1];
  out[0] = 0;
  const char* rest = s;
  const char* p = s;
  while (*p) {
    const char* we = p;
    while (*we && *we != ' ') we++;
    size_t len = (size_t)(we - s);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, s, len);
    tmp[len] = 0;
    if (textWidth(tmp) > maxW) break;
    strlcpy(out, tmp, outSz);
    p = we;
    while (*p == ' ') p++;
    rest = p;
  }
  if (out[0] == 0 && *s) {  // one word wider than the line: cut it by characters
    size_t len = 1;
    while (s[len] && len + 1 < sizeof(tmp)) {
      memcpy(tmp, s, len + 1);
      tmp[len + 1] = 0;
      if (textWidth(tmp) > maxW) break;
      len++;
    }
    memcpy(out, s, len);
    out[len] = 0;
    return s + len;
  }
  return rest;
}

static void ellipsize(char* line, size_t lineSz, int maxW) {
  char tmp[MAX_TASK_LEN + 4];
  size_t len = strlen(line);
  while (len > 0) {
    snprintf(tmp, sizeof(tmp), "%.*s...", (int)len, line);
    if (textWidth(tmp) <= maxW) break;
    len--;
  }
  snprintf(tmp, sizeof(tmp), "%.*s...", (int)len, line);
  strlcpy(line, tmp, lineSz);
}

static void drawRow(const TodoItem& it, int y) {
  const int w = canvas->width();

  // Checkbox
  int bx = MARGIN, by = y + (ROW_H - BOX) / 2;
  canvas->drawRect(bx, by, BOX, BOX, BLACK);
  canvas->drawRect(bx + 1, by + 1, BOX - 2, BOX - 2, BLACK);
  if (it.done) {
    for (int t = 0; t < 3; t++) {
      canvas->drawLine(bx + 5, by + 13 + t, bx + 11, by + 19 + t, BLACK);
      canvas->drawLine(bx + 11, by + 19 + t, bx + 21, by + 7 + t, BLACK);
    }
  }

  // Text, up to two lines
  canvas->setFont(&FreeSans12pt7b);
  canvas->setTextColor(BLACK);
  const int maxW = w - TEXT_X - MARGIN;
  char l1[MAX_TASK_LEN + 4], l2[MAX_TASK_LEN + 4];
  const char* rest = fitLine(it.text, maxW, l1, sizeof(l1));
  l2[0] = 0;
  if (*rest) {
    const char* rest2 = fitLine(rest, maxW, l2, sizeof(l2));
    if (*rest2) ellipsize(l2, sizeof(l2), maxW);
  }

  const int lines = l2[0] ? 2 : 1;
  int baselines[2] = {y + ROW_H / 2 + 8, 0};
  if (lines == 2) { baselines[0] = y + 27; baselines[1] = y + 53; }
  const char* texts[2] = {l1, l2};
  for (int i = 0; i < lines; i++) {
    canvas->setCursor(TEXT_X, baselines[i]);
    canvas->print(texts[i]);
    if (it.done) canvas->fillRect(TEXT_X, baselines[i] - 8, textWidth(texts[i]), 2, BLACK);
  }

  canvas->drawFastHLine(MARGIN, y + ROW_H - 1, w - 2 * MARGIN, BLACK);
}

static void printRight(const char* s, int rightX, int baseline) {
  canvas->setCursor(rightX - textWidth(s), baseline);
  canvas->print(s);
}

static void printCentered(const char* s, int x0, int x1, int baseline) {
  canvas->setCursor(x0 + (x1 - x0 - textWidth(s)) / 2, baseline);
  canvas->print(s);
}

void uiRender(const TaskStore& store, int page, const UiStatus& st, bool forceFull) {
  const int w = canvas->width();
  const size_t n = store.count();
  const int pages = uiPageCount(n);
  if (page >= pages) page = pages - 1;
  char buf[64];

  canvas->fillScreen(WHITE);
  canvas->setTextColor(BLACK);

  // Header: title, done count, status line
  canvas->setFont(&FreeSansBold18pt7b);
  canvas->setCursor(MARGIN, 44);
  canvas->print("To do");
  canvas->setFont(&FreeSans12pt7b);
  snprintf(buf, sizeof(buf), "%u of %u done", (unsigned)store.doneCount(), (unsigned)n);
  printRight(buf, w - MARGIN, 42);

  canvas->setFont(&FreeSans9pt7b);
  const char* link = st.sleeping ? "Asleep - press power to wake"
                   : st.phoneConnected ? "Phone connected" : "Ready to connect";
  canvas->setCursor(MARGIN, 74);
  canvas->print(link);
  if (st.batteryPct >= 0) {
    snprintf(buf, sizeof(buf), "Battery %d%%", st.batteryPct);
    printRight(buf, w - MARGIN, 74);
  }
  canvas->fillRect(0, HEADER_H - 3, w, 3, BLACK);

  // Rows
  if (n == 0) {
    canvas->setFont(&FreeSans12pt7b);
    canvas->setCursor(MARGIN, HEADER_H + 60);
    canvas->print("Nothing to do.");
    canvas->setFont(&FreeSans9pt7b);
    canvas->setCursor(MARGIN, HEADER_H + 95);
    canvas->print("Add tasks from the companion app on your phone.");
  } else {
    const size_t first = (size_t)page * ITEMS_PER_PAGE;
    for (size_t i = first; i < n && i < first + ITEMS_PER_PAGE; i++)
      drawRow(store.at(i), HEADER_H + (int)(i - first) * ROW_H);
  }

  // Footer: "Remove done" button and page controls
  canvas->fillRect(0, FOOTER_Y, w, 3, BLACK);
  const int fb = FOOTER_Y + 44;  // footer baseline
  canvas->setFont(&FreeSans12pt7b);
  if (store.doneCount() > 0) {
    if (st.removeArmed) {
      canvas->fillRect(REMOVE_X0 + 8, FOOTER_Y + 12, REMOVE_X1 - REMOVE_X0 - 16, 46, BLACK);
      canvas->setTextColor(WHITE);
      canvas->setFont(&FreeSans9pt7b);
      printCentered("Tap again to remove", REMOVE_X0 + 8, REMOVE_X1 - 8, fb - 3);
      canvas->setTextColor(BLACK);
      canvas->setFont(&FreeSans12pt7b);
    } else {
      canvas->drawRect(REMOVE_X0 + 8, FOOTER_Y + 12, REMOVE_X1 - REMOVE_X0 - 16, 46, BLACK);
      printCentered("Remove done", REMOVE_X0 + 8, REMOVE_X1 - 8, fb);
    }
  }
  if (pages > 1) {
    if (page > 0) printCentered("<", PREV_X0, PREV_X1, fb);
    snprintf(buf, sizeof(buf), "%d/%d", page + 1, pages);
    printCentered(buf, PREV_X1, NEXT_X0, fb);
    if (page + 1 < pages) printCentered(">", NEXT_X0, NEXT_X1, fb);
  }

  // Copy to the display framebuffer and refresh.
  const size_t bytes = min((size_t)display.getBufferSize(), (size_t)NATIVE_W * NATIVE_H / 8);
  memcpy(display.getFrameBuffer(), canvas->getBuffer(), bytes);
  const bool full = forceFull || firstRender || fastCount >= FULL_REFRESH_EVERY;
  display.displayBuffer(full ? EInkDisplay::FULL_REFRESH : EInkDisplay::FAST_REFRESH);
  fastCount = full ? 0 : fastCount + 1;
  firstRender = false;
}
