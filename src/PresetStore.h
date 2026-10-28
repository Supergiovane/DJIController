#pragma once

#include <Arduino.h>
#include <DjiCommands.h>

struct Preset {
  bool used = false;
  char name[24] = {};
  dji::Axes angles;
};

// Named gimbal positions kept in flash, so every client sees the same list.
class PresetStore {
 public:
  static constexpr int kMaxPresets = 12;

  void begin();

  const Preset& get(int slot) const { return presets_[slot]; }
  static bool validSlot(int slot) { return slot >= 0 && slot < kMaxPresets; }

  // Saves into `slot`, or into the first free slot when `slot` is -1. Returns the slot used, or -1 if none is free.
  int save(int slot, const String& name, const dji::Axes& angles);
  bool remove(int slot);
  void clear();

 private:
  void persist();

  Preset presets_[kMaxPresets];
};
