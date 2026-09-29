#include "ble_link.h"
#include <NimBLEDevice.h>
#include "config.h"

#define SERVICE_UUID "7d2a0001-5b1e-4c8e-9a51-2f3a8c9e0b01"
#define CMD_UUID     "7d2a0002-5b1e-4c8e-9a51-2f3a8c9e0b01"
#define STATE_UUID   "7d2a0003-5b1e-4c8e-9a51-2f3a8c9e0b01"

static constexpr size_t CMD_MAX = 384;
struct CmdMsg { char s[CMD_MAX]; };

static QueueHandle_t cmdQueue;
static NimBLECharacteristic* stateChr = nullptr;
static volatile bool connected = false;
static volatile uint16_t mtu = 23;

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { connected = true; }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    connected = false;
    mtu = 23;
  }
  void onMTUChange(uint16_t m, NimBLEConnInfo&) override { mtu = m; }
};

class CmdCB : public NimBLECharacteristicCallbacks {
  // Runs on the BLE task: just copy the bytes into a queue for the main loop.
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    CmdMsg m;
    size_t len = v.length() < CMD_MAX - 1 ? v.length() : CMD_MAX - 1;
    memcpy(m.s, v.data(), len);
    m.s[len] = 0;
    xQueueSend(cmdQueue, &m, 0);
  }
};

static ServerCB serverCb;
static CmdCB cmdCb;

void bleBegin() {
  cmdQueue = xQueueCreate(8, sizeof(CmdMsg));
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(247);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCb);
  server->advertiseOnDisconnect(true);

  NimBLEService* svc = server->createService(SERVICE_UUID);
  NimBLECharacteristic* cmd = svc->createCharacteristic(CMD_UUID, NIMBLE_PROPERTY::WRITE);
  cmd->setCallbacks(&cmdCb);
  stateChr = svc->createCharacteristic(STATE_UUID, NIMBLE_PROPERTY::NOTIFY);

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(DEVICE_NAME);
  adv->addServiceUUID(SERVICE_UUID);
  adv->enableScanResponse(true);
  adv->start();
}

bool bleConnected() { return connected; }

bool bleNextCommand(char* out, size_t outSz) {
  CmdMsg m;
  if (xQueueReceive(cmdQueue, &m, 0) != pdTRUE) return false;
  strlcpy(out, m.s, outSz);
  return true;
}

void bleSendState(const String& json) {
  if (!connected || !stateChr) return;
  String payload = json + "\n";  // compact JSON never contains a raw newline
  size_t chunk = mtu > 3 ? mtu - 3 : 20;
  if (chunk > 180) chunk = 180;
  const uint8_t* p = (const uint8_t*)payload.c_str();
  size_t left = payload.length();
  while (left > 0 && connected) {
    size_t n = left < chunk ? left : chunk;
    stateChr->notify(p, n);
    p += n;
    left -= n;
    delay(8);  // give the stack room to flush
  }
}
