#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

static constexpr unsigned long kDefaultRetryAfterErrorMs = 60000UL;
static constexpr unsigned long kDefaultReconnectDelayMs  = 5000UL;
static constexpr unsigned long kDefaultConnectTimeoutMs  = 10000UL;
static constexpr unsigned long kModeResetDelayMs         = 200UL;
// How long to wait after WiFi.begin() before treating WL_NO_SSID_AVAIL /
// WL_CONNECT_FAILED as a real failure. Prevents reacting to stale status
// left over from the previous connection attempt.
static constexpr unsigned long kConnectGracePeriodMs     = 1500UL;

struct WifiStaCredentials {
  char ssid[33];
  char password[65];
};

enum class WifiMode {
  OFF,
  STA_ONLY,
  AP_ONLY,
  AP_STA
};

enum class WifiState {
  IDLE,
  CONNECTING,
  CONNECTED,
  AP_RUNNING,
  ERROR
};

struct WifiApConfig {
  char ssid[33];
  char password[65];
  uint8_t channel        = 1;
  bool    hidden         = false;
  uint8_t maxConnections = 4;
};

struct WifiConnectionInfo {
  String    ssid;
  String    bssid;
  int32_t   rssi    = 0;
  uint8_t   channel = 0;
  IPAddress localIp;
  IPAddress gateway;
  IPAddress subnet;
};

class KarenWiFiCore {
public:
  KarenWiFiCore();

  void beginSta(const WifiStaCredentials* networks, size_t count, bool keepApIfRunning = false);
  void beginAP(const WifiApConfig& cfg);
  void beginApSta(const WifiStaCredentials* networks, size_t count, const WifiApConfig& cfg);

  void stop();
  void loop();

  void scanNetworks();
  void useDebugMode();

  void setConnectTimeoutMs(unsigned long ms) { _connectTimeoutMs = ms; }
  void setReconnectDelayMs(unsigned long ms)  { _reconnectDelayMs = ms; }
  void setRetryAfterErrorMs(unsigned long ms) { _retryAfterError = ms; }
  void setHostname(const char* hostname)      { _hostname = hostname; }

  void onConnected(std::function<void()> cb)    { _onConnected = cb; }
  void onDisconnected(std::function<void()> cb) { _onDisconnected = cb; }
  void onError(std::function<void()> cb)        { _onError = cb; }

  WifiMode  getMode()  const { return _mode; }
  WifiState getState() const { return _state; }

  bool isStaConnected() const;
  bool getStaConnectionInfo(WifiConnectionInfo& info) const;

private:
  WifiMode  _mode  = WifiMode::OFF;
  WifiState _state = WifiState::IDLE;

  bool _debug = false;

  static const size_t MAX_NETWORKS = 10;
  WifiStaCredentials _networks[MAX_NETWORKS];

  size_t _netCount            = 0;
  int    _currentNetworkIndex = -1;

  unsigned long _connectStartMs   = 0;
  unsigned long _retryAfterError  = kDefaultRetryAfterErrorMs;
  unsigned long _reconnectDelayMs = kDefaultReconnectDelayMs;
  unsigned long _connectTimeoutMs = kDefaultConnectTimeoutMs;
  unsigned long _lostAt           = 0;
  unsigned long _lastErrorTime    = 0;
  unsigned long _modeResetStartMs = 0;

  bool _apRunning       = false;
  bool _pendingModeReset = false;

  const char* _hostname = nullptr;

  std::function<void()> _onConnected;
  std::function<void()> _onDisconnected;
  std::function<void()> _onError;

  void startStaConnectSequence();
  void tryNextNetwork();
  void log(const char* fmt, ...) const;
};
