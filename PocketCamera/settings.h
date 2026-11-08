/*
  ======================================================================
  settings.h  -  THE WHEEL LIST AND SAVED SETTINGS (what other files can use)
  ======================================================================
  The wheel scrolls through all presets, then one extra "GRID" item.
  Example with 7 presets: items 0-6 are presets, item 7 is GRID.
  On GRID, the shutter turns the rule-of-thirds lines on or off.

  The chosen preset and the grid setting are saved in the chip's flash
  memory (with the Preferences library), so they survive power-off.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types and functions

// ----------------------------------------------------------------------
// Shared variables (created in settings.cpp; "extern" explained in display.h)
// ----------------------------------------------------------------------
extern const int GRID_ITEM;            // position of GRID in the list (7 with 7 presets)
extern const int NUM_ITEMS;            // total items the wheel can land on (8 with 7 presets)

extern int currentPreset;              // which preset the camera is using (index into presets[])
extern int currentItem;                // where the wheel is in the list (a preset, or GRID_ITEM)
extern bool gridOn;                    // true = rule-of-thirds lines are drawn
extern bool settingsUnsaved;           // true = something changed that isn't saved to flash yet
extern unsigned long settingsChangedAt;  // the time (millis()) of that last change

// ----------------------------------------------------------------------
// Functions (the code is in settings.cpp)
// ----------------------------------------------------------------------
void loadSettings();          // reads the saved preset and grid setting (or defaults on the first run)
void saveSettings();          // writes the preset and grid setting to flash
void markChanged();           // notes that something changed; loop() saves it SAVE_DELAY_MS later
void showGridLabel();         // shows the grid state in white, e.g. "GRID: ON"
void changeItem(int clicks);  // moves through the list; clicks: positive = forward, negative = backward
