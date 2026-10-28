#pragma once

#include <WebServer.h>

#include "Gimbal.h"
#include "MotionHold.h"
#include "PresetStore.h"

// Web UI at `/` and HTTP API: GET or POST /api/<command>?param=value, JSON responses. The endpoint list is at /api.
class WebApi {
 public:
  WebApi(Gimbal& gimbal, MotionHold& motion, PresetStore& presets)
      : gimbal_(gimbal), motion_(motion), presets_(presets), server_(80) {}

  void begin();
  // Call from loop().
  void handle() { server_.handleClient(); }

 private:
  void route(const char* path, void (WebApi::*handler)());

  void handleUi();
  void handleIndex();
  void handleStatus();
  void handleAngles();
  void handleVersion();
  void handlePosition();
  void handleSpeed();
  void handleJoystick();
  void handleStop();
  void handleRecenter();
  void handleFollow();
  void handleRecord();
  void handleShutter();
  void handleActiveTrack();
  void handleWifi();
  void handleWifiStatus();
  void handleWifiScan();
  void handleWifiJoin();
  void handleWifiForget();
  void handlePresets();
  void handlePresetSave();
  void handlePresetRecall();
  void handlePresetDelete();
  void handleConfig();
  void importConfig();

  // Sends `packet`, waits for the reply and answers the HTTP request with its outcome.
  void sendCommand(dji::Packet packet);
  // Fires `packet` without waiting for a reply.
  void sendUnacknowledged(dji::Packet packet);
  void hold(MotionHold::Mode mode, const dji::Axes& value);
  bool readAttitude(dji::Axes& out);

  float floatArg(const char* name, float fallback);
  int slotArg();
  void sendJson(int status, const String& body);
  void sendOk() { sendJson(200, "{\"ok\":true}"); }
  void sendError(int status, const char* message);

  Gimbal& gimbal_;
  MotionHold& motion_;
  PresetStore& presets_;
  WebServer server_;
};
