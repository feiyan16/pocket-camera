/*
  ======================================================================
  labels.h  -  ON-SCREEN TEXT (what other files can use)
  ======================================================================
  Labels are FreeMono Bold letters with a 1-pixel dark outline and no
  box, e.g. "SUNNY" or "SAVED #0008". They are painted into each camera
  frame in memory (by showFrame() in display.cpp) before the frame is
  sent to the screen, so they never flicker.

  Why labels need converting:
    The camera module is mounted a quarter turn from the screen. The
    camera picture only looks upright in display rotation 2 (CAM_ROT),
    but in that rotation text would run top-to-bottom. So text is laid
    out in rotation 1 or 3 (TEXT_ROT), where it reads left-to-right as
    you hold the camera, and is then converted to the camera's
    coordinates pixel by pixel.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types (uint8_t, uint16_t ...)
#include "font.h"       // FONT_ADV and FONT_H, needed to size the masks below

// ----------------------------------------------------------------------
// Sizes
// ----------------------------------------------------------------------
// Longest text a label can have. 280 px across / 13 px per letter = 21, so 20 fits safely.
const int MAX_LABEL_CHARS = 20;
// Size of the mask (pixel grid) a label is drawn into. +2 leaves a 1-pixel
// border on each side for the outline. Example: 20 x 13 + 2 = 262 pixels wide.
const int MASK_W = MAX_LABEL_CHARS * FONT_ADV + 2;
const int MASK_H = FONT_H + 2;                        // 16 + 2 = 18 pixels tall

// ----------------------------------------------------------------------
// Types
// ----------------------------------------------------------------------
// A struct bundles several values into one new type.
// The label currently on screen, as a rectangle in camera coordinates.
struct Label {
  bool active;              // true = a label is showing
  int cx0, cy0, cx1, cy1;   // corners; x1 and y1 are "just past" the edge, so width = cx1 - cx0
  uint16_t color;           // colour of the letters (RGB565)
  unsigned long shownAt;    // when it appeared (millis()), to know when to hide it
  uint8_t mask;
};                          // a struct definition ends with a semicolon

// ----------------------------------------------------------------------
// Shared variables (created in labels.cpp; "extern" explained in display.h)
// ----------------------------------------------------------------------
extern Label label;   // the one label: showFrame() paints it, loop() hides it after LABEL_MS
extern Label mainLabel;
extern Label prevLabel;
extern Label nextLabel;

// Label masks: small pixel grids where 0 = see-through, 1 = outline, 2 = letter.
extern uint8_t textMask[MASK_H][MASK_W];   // the label as you read it (text coordinates), 2-D: [row][column]
extern uint8_t camMask[MASK_W * MASK_H];   // the same label turned to camera coordinates, stored row after row

// ----------------------------------------------------------------------
// Functions (the code is in labels.cpp)
// ----------------------------------------------------------------------
// Draws text into textMask, with a 1-pixel outline around every letter.
// Returns the width in pixels (the height is always MASK_H).
// Example: "SUNNY" -> 5 letters x 13 px + 2 = 67 pixels wide.
int buildTextMask(const char *text);

// Shows a label at the bottom centre (as you hold the camera) for LABEL_MS.
// Example: showLabel("SUNNY", 0xFFE0) shows "SUNNY" in yellow.
void showLabel(const char *text, uint16_t color);

void showPresetMenu();

void hidePresetMenu();

// Hides the label. Nothing needs erasing: the next frame simply doesn't include it.
void hideLabel();
