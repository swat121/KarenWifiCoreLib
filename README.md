# KarenWiFiCore

Non-blocking WiFi manager for ESP32 (Arduino framework).  
Supports STA, AP and AP+STA modes with automatic multi-network fallback and reconnect.

## Features

- **Multi-network fallback** — provide a list of networks; the library tries each one in order
- **Non-blocking** — no `delay()`, designed to run inside `loop()`
- **Auto-reconnect** — on connection loss the library waits a configurable delay, then reconnects
- **AP / STA / AP+STA** — full mode support
- **Event callbacks** — `onConnected`, `onDisconnected`, `onError`
- **Configurable timeouts** — connect timeout, reconnect delay, error retry interval
- **Custom hostname** — `setHostname()`
- **Debug logging** — enable with `useDebugMode()`

## Installation

### Via `lib_deps` (recommended)

Add to your `platformio.ini`:

```ini
lib_deps = https://github.com/deniskuschovyi/KarenWiFiCore.git
```

### Manual

Clone or download the repository and place it in the `lib/` folder of your PlatformIO project.

## Quick Start

```cpp
#include <Arduino.h>
#include "KarenWiFiCore.h"

KarenWiFiCore wifi;

WifiStaCredentials networks[] = {
  { "HomeNetwork",  "password1" },
  { "OfficeWiFi",   "password2" },
};

void setup() {
  Serial.begin(115200);

  wifi.useDebugMode();

  wifi.onConnected([]()    { Serial.println("Connected!"); });
  wifi.onDisconnected([]() { Serial.println("Lost connection, reconnecting..."); });
  wifi.onError([]()        { Serial.println("All networks failed."); });

  wifi.beginSta(networks, sizeof(networks) / sizeof(networks[0]));
}

void loop() {
  wifi.loop(); // must be called every iteration
}
```

## API Reference

### Initialization

| Method | Description |
|--------|-------------|
| `beginSta(networks, count)` | Start in STA mode with a list of credentials |
| `beginAP(cfg)` | Start in AP mode |
| `beginApSta(networks, count, apCfg)` | Start in AP+STA mode |
| `stop()` | Turn off WiFi |

### Configuration (call before `beginSta`)

| Method | Default | Description |
|--------|---------|-------------|
| `setConnectTimeoutMs(ms)` | 10 000 | Timeout per network before trying the next one |
| `setReconnectDelayMs(ms)` | 5 000 | Delay before reconnecting after connection loss |
| `setRetryAfterErrorMs(ms)` | 60 000 | Delay before retrying after all networks fail |
| `setHostname(name)` | — | Custom mDNS hostname |

### Status

| Method | Returns |
|--------|---------|
| `getState()` | `WifiState` enum |
| `getMode()` | `WifiMode` enum |
| `isStaConnected()` | `bool` |
| `getStaConnectionInfo(info)` | `bool` — fills `WifiConnectionInfo` |

### WifiState values

| Value | Meaning |
|-------|---------|
| `IDLE` | Not started or waiting to retry |
| `CONNECTING` | Trying to connect |
| `CONNECTED` | STA connected |
| `AP_RUNNING` | AP-only mode active |
| `ERROR` | All networks failed |

## Examples

- [`examples/BasicStaConnection`](examples/BasicStaConnection) — STA mode with multi-network fallback and status printing

## License

MIT © 2026 Denis Kuschovyi
