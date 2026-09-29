#pragma once
#include <Arduino.h>
#include "config.h"

struct TodoItem {
  uint32_t id;
  bool done;
  char text[MAX_TASK_LEN + 1];
};

// Fixed-size task list persisted to LittleFS as JSON.
class TaskStore {
 public:
  bool begin();
  bool save();

  size_t count() const { return n_; }
  size_t doneCount() const;
  const TodoItem& at(size_t i) const { return items_[i]; }
  int indexOf(uint32_t id) const;

  bool add(const char* text);
  bool toggleAt(size_t index);
  bool toggle(uint32_t id);
  bool remove(uint32_t id);
  bool edit(uint32_t id, const char* text);
  bool move(uint32_t id, int newIndex);
  size_t clearDone();

  void toJson(String& out) const;

 private:
  TodoItem items_[MAX_TASKS];
  size_t n_ = 0;
  uint32_t nextId_ = 1;
};
