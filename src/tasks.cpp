#include "tasks.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* PATH = "/tasks.json";
static const char* TMP_PATH = "/tasks.tmp";

// The built-in fonts are ASCII only. Each non-ASCII UTF-8 character becomes '?',
// control characters become spaces, and runs of spaces collapse.
static void sanitize(const char* in, char* out, size_t outSz) {
  size_t j = 0;
  bool lastSpace = true;
  for (size_t i = 0; in[i] && j + 1 < outSz; i++) {
    unsigned char c = (unsigned char)in[i];
    if (c >= 0x80) {
      if ((c & 0xC0) == 0x80) continue;  // UTF-8 continuation byte
      c = '?';
    }
    if (c < 0x20 || c == 0x7F) c = ' ';
    if (c == ' ') {
      if (lastSpace) continue;
      lastSpace = true;
    } else {
      lastSpace = false;
    }
    out[j++] = (char)c;
  }
  while (j > 0 && out[j - 1] == ' ') j--;
  out[j] = 0;
}

bool TaskStore::begin() {
  if (!LittleFS.begin(true)) return false;  // formats on first boot
  File f = LittleFS.open(PATH, "r");
  if (!f) return true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return true;

  n_ = 0;
  nextId_ = doc["next"] | 1;
  for (JsonObjectConst t : doc["tasks"].as<JsonArrayConst>()) {
    if (n_ >= MAX_TASKS) break;
    TodoItem& it = items_[n_];
    it.id = t["id"] | 0;
    it.done = t["done"] | false;
    sanitize(t["text"] | "", it.text, sizeof(it.text));
    if (it.id == 0 || it.text[0] == 0) continue;
    if (it.id >= nextId_) nextId_ = it.id + 1;
    n_++;
  }
  return true;
}

bool TaskStore::save() {
  JsonDocument doc;
  doc["next"] = nextId_;
  JsonArray arr = doc["tasks"].to<JsonArray>();
  for (size_t i = 0; i < n_; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = items_[i].id;
    o["done"] = items_[i].done;
    o["text"] = items_[i].text;
  }
  // Write to a temp file then rename, so a power loss can't corrupt the list.
  File f = LittleFS.open(TMP_PATH, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (!ok) return false;
  LittleFS.remove(PATH);
  return LittleFS.rename(TMP_PATH, PATH);
}

size_t TaskStore::doneCount() const {
  size_t c = 0;
  for (size_t i = 0; i < n_; i++) c += items_[i].done ? 1 : 0;
  return c;
}

int TaskStore::indexOf(uint32_t id) const {
  for (size_t i = 0; i < n_; i++)
    if (items_[i].id == id) return (int)i;
  return -1;
}

bool TaskStore::add(const char* text) {
  if (n_ >= MAX_TASKS) return false;
  TodoItem& it = items_[n_];
  sanitize(text, it.text, sizeof(it.text));
  if (it.text[0] == 0) return false;
  it.id = nextId_++;
  it.done = false;
  n_++;
  return true;
}

bool TaskStore::toggleAt(size_t index) {
  if (index >= n_) return false;
  items_[index].done = !items_[index].done;
  return true;
}

bool TaskStore::toggle(uint32_t id) {
  int i = indexOf(id);
  return i >= 0 && toggleAt((size_t)i);
}

bool TaskStore::remove(uint32_t id) {
  int i = indexOf(id);
  if (i < 0) return false;
  for (size_t k = (size_t)i; k + 1 < n_; k++) items_[k] = items_[k + 1];
  n_--;
  return true;
}

bool TaskStore::edit(uint32_t id, const char* text) {
  int i = indexOf(id);
  if (i < 0) return false;
  char buf[MAX_TASK_LEN + 1];
  sanitize(text, buf, sizeof(buf));
  if (buf[0] == 0) return false;
  strlcpy(items_[i].text, buf, sizeof(items_[i].text));
  return true;
}

bool TaskStore::move(uint32_t id, int to) {
  int from = indexOf(id);
  if (from < 0 || n_ == 0) return false;
  if (to < 0) to = 0;
  if (to >= (int)n_) to = (int)n_ - 1;
  if (to == from) return false;
  TodoItem tmp = items_[from];
  if (to < from)
    for (int k = from; k > to; k--) items_[k] = items_[k - 1];
  else
    for (int k = from; k < to; k++) items_[k] = items_[k + 1];
  items_[to] = tmp;
  return true;
}

size_t TaskStore::clearDone() {
  size_t j = 0, removed = 0;
  for (size_t i = 0; i < n_; i++) {
    if (items_[i].done) { removed++; continue; }
    if (j != i) items_[j] = items_[i];
    j++;
  }
  n_ = j;
  return removed;
}

void TaskStore::toJson(String& out) const {
  JsonDocument doc;
  JsonArray arr = doc["tasks"].to<JsonArray>();
  for (size_t i = 0; i < n_; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = items_[i].id;
    o["done"] = items_[i].done;
    o["text"] = items_[i].text;
  }
  doc["max"] = MAX_TASKS;
  out.clear();
  serializeJson(doc, out);
}
