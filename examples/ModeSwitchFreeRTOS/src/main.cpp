//
// ModeSwitchFreeRTOS — KarenWiFiCore example
//
// Demonstrates two FreeRTOS tasks:
//   - WiFiTask  : runs wifi.loop() and applies mode-change commands
//   - ButtonTask: debounces BOOT button and enqueues commands via a FreeRTOS Queue
//
// Each short button press toggles between:
//   STA  ↔  AP
//
// Because WiFiTask and ButtonTask are independent, a button press always
// interrupts the current connection attempt within ~10 ms — even while the
// library is actively trying every network in the list.
//
// Board: WeAct Studio ESP32-C3 (BOOT button = GPIO 9)
//

#include <Arduino.h>
#include "KarenWiFiCore.h"

// ─── Hardware ────────────────────────────────────────────────────────────────

static constexpr uint8_t BUTTON_PIN = 9;    // BOOT button (active LOW)
static constexpr uint32_t DEBOUNCE_MS = 50; // button debounce window
static constexpr uint32_t MIN_HOLD_MS = 50; // minimum press to count as a click

// ─── WiFi config ─────────────────────────────────────────────────────────────

static WifiStaCredentials networks[] = {
    {"YourSSID_1", "YourPassword_1"},
    {"YourSSID_2", "YourPassword_2"},
};
static constexpr size_t NETWORK_COUNT = sizeof(networks) / sizeof(networks[0]);

static WifiApConfig apConfig("KarenAP", "12345678", /*channel=*/6);

// ─── Commands (sent from ButtonTask → WiFiTask via Queue) ────────────────────

enum class WifiCmd : uint8_t
{
  START_STA,
  START_AP,
  _COUNT
};

static const char *const CMD_NAMES[] = {"STA", "AP"};

// ─── Shared objects ──────────────────────────────────────────────────────────

static KarenWiFiCore wifi;
static QueueHandle_t cmdQueue;

// ─── WiFiTask ────────────────────────────────────────────────────────────────
//
// Runs wifi.loop() every 10 ms and checks the queue before each iteration.
// All WiFi API calls are confined to this single task — no mutex needed.

static void wifiTask(void *)
{
  for (;;)
  {
    WifiCmd cmd;
    if (xQueueReceive(cmdQueue, &cmd, 0) == pdTRUE)
    {
      switch (cmd)
      {
      case WifiCmd::START_STA:
        Serial.println("[wifi] → STA mode");
        wifi.stop();
        wifi.beginSta(networks, NETWORK_COUNT);
        break;

      case WifiCmd::START_AP:
        Serial.println("[wifi] → AP mode");
        wifi.stop();
        wifi.beginAP(apConfig);
        break;

      default:
        break;
      }
    }

    wifi.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ─── ButtonTask ──────────────────────────────────────────────────────────────
//
// Samples GPIO every DEBOUNCE_MS ms.
// On each release after a valid press, advances the mode index and sends
// the corresponding command to WiFiTask via the queue.

static void buttonTask(void *)
{
  uint8_t modeIndex = 0; // current position in the cycle (starts at STA)
  bool lastLevel = HIGH;
  uint32_t pressedAt = 0;

  for (;;)
  {
    bool level = digitalRead(BUTTON_PIN);

    if (lastLevel == HIGH && level == LOW)
    {
      // falling edge — button down
      pressedAt = xTaskGetTickCount() * portTICK_PERIOD_MS;
    }
    else if (lastLevel == LOW && level == HIGH)
    {
      // rising edge — button released
      uint32_t heldMs = xTaskGetTickCount() * portTICK_PERIOD_MS - pressedAt;
      if (heldMs >= MIN_HOLD_MS)
      {
        modeIndex = (modeIndex + 1) % static_cast<uint8_t>(WifiCmd::_COUNT);
        WifiCmd cmd = static_cast<WifiCmd>(modeIndex);
        Serial.printf("[button] pressed → %s\n", CMD_NAMES[modeIndex]);
        xQueueSend(cmdQueue, &cmd, 0); // non-blocking; queue depth = 4
      }
    }

    lastLevel = level;
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
  }
}

// ─── Arduino entry points ────────────────────────────────────────────────────

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  wifi.useDebugMode();
  wifi.onConnected([]()
                   { Serial.println("[app] Connected!"); });
  wifi.onDisconnected([]()
                      { Serial.println("[app] Connection lost, reconnecting..."); });
  wifi.onError([]()
               { Serial.println("[app] All networks failed."); });

  // Start in STA mode right away
  wifi.beginSta(networks, NETWORK_COUNT);

  // Queue holds up to 4 pending commands (one WifiCmd byte each)
  cmdQueue = xQueueCreate(4, sizeof(WifiCmd));

  // WiFiTask: higher priority so it always processes commands quickly
  xTaskCreate(wifiTask, "WiFiTask", 4096, nullptr, 2, nullptr);
  // ButtonTask: lower priority, runs purely on its DEBOUNCE_MS tick
  xTaskCreate(buttonTask, "ButtonTask", 2048, nullptr, 1, nullptr);

  Serial.println("[app] Ready. Press BOOT button to toggle WiFi mode.");
  Serial.println("[app] Toggle: STA ↔ AP");
}

void loop()
{
  // Arduino loop() is itself a FreeRTOS task (priority 1).
  // We don't use it here — just yield the CPU.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
