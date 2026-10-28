#pragma once

#include <WebServer.h>

#include "Gimbal.h"
#include "MotionHold.h"

// HTTP API: GET or POST /api/<command>?param=value, JSON responses. The endpoint list is served at `/`.
class WebApi {
 public:
  WebApi(Gimbal& gimbal, MotionHold& motion) : gimbal_(gimbal), motion_(motion), server_(80) {}

  void begin();
  // Call from loop().
  void handle() { server_.handleClient(); }

 private:
  void route(const char* path, void (WebApi::*handler)());

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

  // Sends `packet`, waits for the reply and answers the HTTP request with its outcome.
  void sendCommand(dji::Packet packet);
  // Fires `packet` without waiting for a reply.
  void sendUnacknowledged(dji::Packet packet);
  void hold(MotionHold::Mode mode, const dji::Axes& value);

  float floatArg(const char* name, float fallback);
  void sendJson(int status, const String& body);
  void sendOk() { sendJson(200, "{\"ok\":true}"); }
  void sendError(int status, const char* message);

  Gimbal& gimbal_;
  MotionHold& motion_;
  WebServer server_;
};
