#include "PresetStore.h"

#include <Preferences.h>

namespace {
constexpr const char* kPrefsNamespace = "presets";
constexpr const char* kKey = "list";
}  // namespace

void PresetStore::begin() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  // A blob of another size was written by a different firmware layout: start empty rather than misread it.
  if (prefs.isKey(kKey) && prefs.getBytesLength(kKey) == sizeof(presets_)) {
    prefs.getBytes(kKey, presets_, sizeof(presets_));
  }
  prefs.end();
}

int PresetStore::save(int slot, const String& name, const dji::Axes& angles) {
  if (slot == -1) {
    for (int i = 0; i < kMaxPresets && slot == -1; ++i) {
      if (!presets_[i].used) slot = i;
    }
  }
  if (!validSlot(slot)) return -1;

  Preset& preset = presets_[slot];
  preset.used = true;
  strlcpy(preset.name, name.c_str(), sizeof(preset.name));
  preset.angles = angles;
  persist();
  return slot;
}

bool PresetStore::remove(int slot) {
  if (!validSlot(slot) || !presets_[slot].used) return false;
  presets_[slot] = Preset();
  persist();
  return true;
}

void PresetStore::clear() {
  for (Preset& preset : presets_) preset = Preset();
  persist();
}

void PresetStore::persist() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putBytes(kKey, presets_, sizeof(presets_));
  prefs.end();
}
