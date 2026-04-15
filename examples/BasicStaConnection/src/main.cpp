#include <Arduino.h>
#include "KarenWiFiCore.h"

KarenWiFiCore wifi;

// List of networks to try in order. The first reachable one will be used.
// On connection loss the library reconnects automatically.
WifiStaCredentials networks[] = {
    {"TP-Link_Guest_C6201", "YourPassword_1"},
    {"TP-Link_FF28", "34012833"},
};

void setup()
{
  Serial.begin(115200);
  delay(1000);

  wifi.useDebugMode();

  wifi.onConnected([]()
                   { Serial.println("[app] Connected!"); });

  wifi.onDisconnected([]()
                      { Serial.println("[app] Disconnected, will reconnect..."); });

  wifi.onError([]()
               { Serial.println("[app] All networks failed, will retry in 60s."); });

  Serial.println("Initial WiFi scan...");
  wifi.scanNetworks();

  wifi.beginSta(networks, sizeof(networks) / sizeof(networks[0]));
}

void loop()
{
  wifi.loop();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000)
  {
    lastPrint = millis();

    WifiState state = wifi.getState();
    Serial.print("WiFi state: ");
    Serial.println((int)state);

    if (wifi.isStaConnected())
    {
      WifiConnectionInfo info;
      if (wifi.getStaConnectionInfo(info))
      {
        Serial.println("== Connected to ==");
        Serial.println("SSID: " + info.ssid);
        Serial.println("BSSID: " + info.bssid);
        Serial.print("RSSI: ");
        Serial.println(info.rssi);
        Serial.print("Channel: ");
        Serial.println(info.channel);
        Serial.print("IP: ");
        Serial.println(info.localIp);
        Serial.print("GW: ");
        Serial.println(info.gateway);
        Serial.print("Mask: ");
        Serial.println(info.subnet);
        Serial.println("==================");
      }
    }
  }
}
