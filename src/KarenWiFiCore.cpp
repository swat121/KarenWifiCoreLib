#include "KarenWiFiCore.h"
#include <stdarg.h>

KarenWiFiCore::KarenWiFiCore() {}

void KarenWiFiCore::log(const char *fmt, ...) const
{
  if (!_debug)
    return;
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[KarenWiFiCore] ");
  Serial.println(buf);
}

static const char *wifiStatusToStr(wl_status_t st)
{
  switch (st)
  {
  case WL_IDLE_STATUS:
    return "IDLE";
  case WL_NO_SSID_AVAIL:
    return "NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "SCAN_COMPLETED";
  case WL_CONNECTED:
    return "CONNECTED";
  case WL_CONNECT_FAILED:
    return "CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "DISCONNECTED";
  default:
    return "UNKNOWN";
  }
}

void KarenWiFiCore::beginSta(const WifiStaCredentials *networks, size_t count, bool keepApIfRunning)
{
  WiFi.disconnect(false);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);

  if (keepApIfRunning)
  {
    WiFi.mode(WIFI_AP_STA);
    _mode = WifiMode::AP_STA;
  }
  else
  {
    WiFi.mode(WIFI_STA);
    _mode = WifiMode::STA_ONLY;
  }

  if (_hostname)
  {
    WiFi.setHostname(_hostname);
  }

  _netCount = min(count, (size_t)MAX_NETWORKS);
  for (size_t i = 0; i < _netCount; ++i)
  {
    _networks[i] = networks[i];
  }

  _currentNetworkIndex = -1;
  _lostAt = 0;
  _lastErrorTime = 0;
  _pendingModeReset = false;
  _state = WifiState::IDLE;

  log("beginSta: %d network(s) loaded", (int)_netCount);
}

void KarenWiFiCore::beginAP(const WifiApConfig &cfg)
{
  WiFi.mode(WIFI_AP);
  _mode = WifiMode::AP_ONLY;

  bool ok = WiFi.softAP(
      cfg.ssid,
      cfg.password[0] != '\0' ? cfg.password : nullptr,
      cfg.channel,
      cfg.hidden,
      cfg.maxConnections);

  _apRunning = ok;
  _state = ok ? WifiState::AP_RUNNING : WifiState::ERROR;

  if (ok)
  {
    log("beginAP: AP started, IP=%s", WiFi.softAPIP().toString().c_str());
  }
  else
  {
    log("beginAP: AP start failed");
  }
}

void KarenWiFiCore::beginApSta(const WifiStaCredentials *networks, size_t count, const WifiApConfig &apCfg)
{
  WiFi.mode(WIFI_AP_STA);
  _mode = WifiMode::AP_STA;

  bool ok = WiFi.softAP(
      apCfg.ssid,
      apCfg.password[0] != '\0' ? apCfg.password : nullptr,
      apCfg.channel,
      apCfg.hidden,
      apCfg.maxConnections);

  if (!ok)
  {
    log("beginApSta: AP start failed, aborting");
    _apRunning = false;
    _state = WifiState::ERROR;
    return;
  }

  _apRunning = true;
  log("beginApSta: AP started, IP=%s", WiFi.softAPIP().toString().c_str());

  beginSta(networks, count, true);
}

void KarenWiFiCore::stop()
{
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  _mode = WifiMode::OFF;
  _state = WifiState::IDLE;
  _apRunning = false;
  _netCount = 0;
  _currentNetworkIndex = -1;
  _lostAt = 0;
  _lastErrorTime = 0;
  _pendingModeReset = false;

  log("stop: WiFi stopped");
}

bool KarenWiFiCore::isStaConnected() const
{
  return (WiFi.getMode() & WIFI_MODE_STA) && WiFi.status() == WL_CONNECTED;
}

void KarenWiFiCore::scanNetworks()
{
  log("scanNetworks: Starting scan");

  int n = WiFi.scanNetworks();
  if (n <= 0)
  {
    log("scanNetworks: No networks found");
    WiFi.scanDelete();
    return;
  }

  log("scanNetworks: Found %d network(s)", n);
  for (int i = 0; i < n; ++i)
  {
    log("scanNetworks: %d: %s (%ddBm) %s",
        i + 1,
        WiFi.SSID(i).c_str(),
        WiFi.RSSI(i),
        WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured");
  }

  WiFi.scanDelete();
}

bool KarenWiFiCore::getStaConnectionInfo(WifiConnectionInfo &out) const
{
  if (!isStaConnected())
  {
    log("getStaConnectionInfo: STA not connected");
    return false;
  }

  out.ssid = WiFi.SSID();
  out.bssid = WiFi.BSSIDstr();
  out.rssi = WiFi.RSSI();
  out.channel = WiFi.channel();
  out.localIp = WiFi.localIP();
  out.gateway = WiFi.gatewayIP();
  out.subnet = WiFi.subnetMask();

  log("getStaConnectionInfo: SSID=%s IP=%s RSSI=%ddBm",
      out.ssid.c_str(), out.localIp.toString().c_str(), (int)out.rssi);
  return true;
}

void KarenWiFiCore::startStaConnectSequence()
{
  if (_netCount == 0)
  {
    log("startStaConnectSequence: no networks configured");
    _state = WifiState::ERROR;
    _lastErrorTime = millis();
    if (_onError)
      _onError();
    return;
  }

  _currentNetworkIndex = -1;
  tryNextNetwork();
}

void KarenWiFiCore::tryNextNetwork()
{
  _currentNetworkIndex++;

  if (_currentNetworkIndex >= (int)_netCount)
  {
    log("tryNextNetwork: all %d network(s) tried, entering ERROR", (int)_netCount);
    _state = WifiState::ERROR;
    _lastErrorTime = millis();
    if (_onError)
      _onError();
    return;
  }

  const auto &cred = _networks[_currentNetworkIndex];
  log("tryNextNetwork: %d/%d SSID=%s", _currentNetworkIndex + 1, (int)_netCount, cred.ssid);

  WiFi.disconnect(false);

  if (cred.password[0] != '\0')
  {
    WiFi.begin(cred.ssid, cred.password);
  }
  else
  {
    WiFi.begin(cred.ssid);
  }

  _connectStartMs = millis();
  _state = WifiState::CONNECTING;
}

void KarenWiFiCore::loop()
{
  bool staEnable = (_mode == WifiMode::STA_ONLY || _mode == WifiMode::AP_STA);
  if (!staEnable || _netCount == 0)
    return;

  if (_pendingModeReset)
  {
    if (millis() - _modeResetStartMs >= kModeResetDelayMs)
    {
      _pendingModeReset = false;
      tryNextNetwork();
    }
    return;
  }

  wl_status_t st = WiFi.status();

  switch (_state)
  {
  case WifiState::IDLE:
    startStaConnectSequence();
    break;

  case WifiState::CONNECTING:
  {
    if (st == WL_CONNECTED)
    {
      log("loop: connected, IP=%s", WiFi.localIP().toString().c_str());
      _state = WifiState::CONNECTED;
      _lostAt = 0;
      if (_onConnected)
        _onConnected();
      break;
    }

    // Ignore failure codes during the grace period: the WiFi stack may still
    // report the stale status from the previous attempt right after WiFi.begin().
    bool graceExpired = (millis() - _connectStartMs) > kConnectGracePeriodMs;
    if (graceExpired && (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL))
    {
      log("loop: failure (%s), trying next network", wifiStatusToStr(st));
      WiFi.disconnect(false);
      _pendingModeReset = true;
      _modeResetStartMs = millis();
      break;
    }

    if (millis() - _connectStartMs > _connectTimeoutMs)
    {
      log("loop: timeout after %lums, trying next network", _connectTimeoutMs);
      WiFi.disconnect(false);
      _pendingModeReset = true;
      _modeResetStartMs = millis();
    }
    break;
  }

  case WifiState::CONNECTED:
    if (st != WL_CONNECTED)
    {
      if (_lostAt == 0)
      {
        log("loop: connection lost, waiting %lums before reconnect", _reconnectDelayMs);
        _lostAt = millis();
        if (_onDisconnected)
          _onDisconnected();
      }
      else if (millis() - _lostAt > _reconnectDelayMs)
      {
        _lostAt = 0;
        _state = WifiState::IDLE;
      }
    }
    break;

  case WifiState::AP_RUNNING:
    break;

  case WifiState::ERROR:
    if (millis() - _lastErrorTime > _retryAfterError)
    {
      log("loop: retrying after error");
      _state = WifiState::IDLE;
    }
    break;

  default:
    log("loop: unexpected state %d", (int)_state);
    break;
  }
}

void KarenWiFiCore::useDebugMode()
{
  _debug = true;
}
