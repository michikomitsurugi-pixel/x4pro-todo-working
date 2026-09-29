#pragma once
#include <Arduino.h>

// BLE GATT service (UUIDs must match app/index.html):
//   command characteristic (write): one JSON command per write, e.g. {"op":"add","text":"Call Sam"}
//   state characteristic (notify):  full task list as JSON, split into chunks, ends with '\n'
void bleBegin();
bool bleConnected();
// Copies the next received command into out. Returns false if none are waiting.
bool bleNextCommand(char* out, size_t outSz);
void bleSendState(const String& json);
