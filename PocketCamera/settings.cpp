/*
  ======================================================================
  settings.cpp  -  THE WHEEL LIST AND SAVED SETTINGS (the code)
  ======================================================================
*/

#include "settings.h"   // our own header: the promises this file must keep
#include <Preferences.h>  // stores small values in the chip's flash; comes with the ESP32 board package
#include "camera.h"     // presets[], NUM_PRESETS and applyPreset()
#include "labels.h"     // showLabel(), to show the preset name or grid state

// ----------------------------------------------------------------------
// The variables promised with "extern" in settings.h are created here.
// ----------------------------------------------------------------------
const int GRID_ITEM = NUM_PRESETS;      // GRID comes right after the last preset (7)
const int NUM_ITEMS = NUM_PRESETS + 1;  // every preset plus GRID (8)

int currentPreset = 0;                // starts on preset 0 (AUTO) until loadSettings() runs
int currentItem = 0;                  // the wheel starts on item 0
bool gridOn = false;                  // grid off by default
bool settingsUnsaved = false;         // nothing to save yet
unsigned long settingsChangedAt = 0;  // no change yet

Preferences prefs;                    // the flash-storage object (only this file uses it)

// ----------------------------------------------------------------------
// Functions promised in settings.h
// ----------------------------------------------------------------------
void loadSettings() {
  prefs.begin("camera", true);         // open the storage area named "camera"; true = read-only
  currentPreset = prefs.getUChar("preset", 0);  // saved preset, or 0 (AUTO) if nothing is saved yet
  gridOn = prefs.getBool("grid", false);        // saved grid setting, or off
  prefs.end();                         // close the storage area
  // Guard against a saved number that no longer exists (e.g. after deleting presets).
  if (currentPreset >= NUM_PRESETS) currentPreset = 0;
  currentItem = currentPreset;         // the wheel starts on the saved preset
}                                      // end of loadSettings

// Writes the preset and grid setting to flash.
void saveSettings() {
  prefs.begin("camera", false);        // open the storage area; false = read-write
  prefs.putUChar("preset", currentPreset);  // store the preset number under the key "preset"
  prefs.putBool("grid", gridOn);       // store the grid setting under the key "grid"
  prefs.end();                         // close (this also finishes the write)
}                                      // end of saveSettings

// Notes that something changed; loop() saves it SAVE_DELAY_MS later.
void markChanged() {
  settingsUnsaved = true;              // there is something to save
  settingsChangedAt = millis();        // and this is when it changed (restarts the 3-second wait)
}                                      // end of markChanged

// Shows the grid state in white, e.g. "GRID: ON".
void showGridLabel() {
  showLabel(gridOn ? "GRID: ON" : "GRID: OFF", 0xFFFF);  // pick the text with "? :", 0xFFFF = white
}                                      // end of showGridLabel

// Moves through the list (presets, then GRID), wrapping around at both ends.
void changeItem(int clicks) {          // clicks: positive = forward, negative = backward
  // In C++, % keeps the sign: -1 % 8 = -1. Adding NUM_ITEMS and taking % again
  // makes it positive. Example: from item 0 back one click -> (-1 % 8 + 8) % 8 = 7 (GRID).
  currentItem = ((currentItem + clicks) % NUM_ITEMS + NUM_ITEMS) % NUM_ITEMS;
  if (currentItem == GRID_ITEM) {      // landed on GRID:
    showGridLabel();                   //   show its state; the camera keeps its current preset
  } else {                             // landed on a preset:
    currentPreset = currentItem;       //   use it
    applyPreset();                     //   send it to the camera
    showLabel(presets[currentPreset].name, 0xFFE0);  // and show its name in yellow
  }                                    // end of the if/else
  markChanged();                       // remember to save
}                                      // end of changeItem
