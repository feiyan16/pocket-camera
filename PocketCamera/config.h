/*
  ======================================================================
  config.h  -  ADJUSTABLE SETTINGS
  ======================================================================
  Every number you might want to tweak lives here, in one place: screen
  rotations, preview crop, photo size and quality, label colours and
  timing, encoder feel, how often settings are saved.

  Pin numbers are NOT here: each part's pins are in that part's own
  header (display.h, camera.h, button.h, encoder.h, sdcard.h).

  Why "const" values in a header are fine:
    This file is #included by several .cpp files, so each of them gets its
    own copy of these lines. In C++, a "const" value declared outside any
    function is private to each file that includes it, so the copies never
    clash. (A plain, non-const variable here WOULD clash: the linker would
    complain "multiple definition".)
  ======================================================================
*/

// #pragma once = "only read this file once per .cpp file", even if it's
// #included several times (directly and through other headers). Without it,
// the same definitions could appear twice and fail to compile.
#pragma once

// Arduino.h gives us the basic types and functions: uint8_t, uint16_t,
// millis(), digitalRead(), Serial... The .ino file gets it automatically, but
// .h and .cpp files must include it themselves.
#include <Arduino.h>
// esp_camera.h defines framesize_t and the FRAMESIZE_... names used below.
// Quotes "..." also search the ESP32 board package, where the camera driver lives.
#include "esp_camera.h"

// "const" means the value can't change while the program runs. The compiler
// can then optimise it, and it stops you from changing it by accident.

// ----------------------------------------------------------------------
// Display
// ----------------------------------------------------------------------
// The panel's own size, in its natural portrait orientation. The display
// library needs this before it can rotate anything.
const int PANEL_W = 240;  // pixel columns
const int PANEL_H = 280;  // pixel rows

// Display rotations: 0, 1, 2 or 3, a quarter turn each.
// Example: rotation 0 is portrait (240 wide, 280 tall); rotation 1 is turned
// a quarter turn (280 wide, 240 tall).
const uint8_t CAM_ROT  = 2;  // rotation for the camera picture: the one where it looks upright (keep at 2)
const uint8_t TEXT_ROT = 1;  // rotation for text: 1 or 3, whichever makes "SAVING..." read left-to-right

// How label positions are converted from text coordinates to camera
// coordinates (see textPixelToCamera() in labels.cpp). Normally the same as
// TEXT_ROT. If labels appear backwards, upside down or on the wrong edge
// while the "SAVING..." message reads correctly, set this to the other
// value (1 <-> 3).
const uint8_t LABEL_MAP = TEXT_ROT;

// Display clock: how fast pixels are sent to the screen, in Hz.
// 80000000 = 80 MHz. Faster = smoother preview, but long or loose wires can
// garble the signal at high speed; if the picture shows noise, use 40000000.
const uint32_t DISPLAY_HZ = 80000000;

// ----------------------------------------------------------------------
// Preview
// ----------------------------------------------------------------------
// Preview size. The camera only offers fixed sizes; CIF is 400 x 296, the
// smallest that is big enough to fill the 240 x 280 screen (as it's mounted).
// framesize_t is a list (enum) of sizes defined by the camera driver.
const framesize_t PREVIEW_SIZE = FRAMESIZE_CIF;
// The frame is larger than the screen, so part of it is cropped (cut off).
// Example: CROP_X = 0 keeps frame columns 0..239 and cuts columns 240..399.
//          CROP_X = 80 keeps columns 80..319, cutting 80 from each side.
const int CROP_X = 0;     // columns cut from the start of each row
const int CROP_Y = 8;     // rows cut from the top (and, implicitly, the bottom): (296 - 280) / 2 = 8
// RGB565 pixels are 2 bytes. The camera sends the high byte first, the ESP32
// stores numbers low byte first, so the bytes need swapping.
// Example: pure red is 0xF800; without the swap it would arrive as 0x00F8 (a blue-green).
// If the preview colours look wild, set this to false.
const bool SWAP_BYTES = true;

// ----------------------------------------------------------------------
// Photos
// ----------------------------------------------------------------------
const framesize_t PHOTO_SIZE = FRAMESIZE_UXGA;   // UXGA = 1600 x 1200 pixels
// JPEG quality: 0-63, LOWER = better quality and bigger files.
// Example: 10 gives a very good photo of roughly 200-400 KB (it depends on the scene).
const int PHOTO_QUALITY = 10;
// After the camera restarts in photo mode, its automatic exposure and white
// balance need a few frames to adjust. These frames are thrown away.
// If photos come out too dark or with a colour cast, raise this (e.g. to 10).
const int PHOTO_WARMUP_FRAMES = 6;

// Picture flips. They're applied inside the camera, so photos are flipped too.
// The camera module is turned a quarter turn, so the camera's "vertical flip"
// flips the picture LEFT-RIGHT as you hold the camera, and its "horizontal
// mirror" flips it TOP-BOTTOM. Setting both to true turns it 180 degrees.
const bool FLIP_V   = true;   // true: un-mirrors the picture left-right (as held)
const bool MIRROR_H = false;  // true would flip it top-bottom (as held)

// ----------------------------------------------------------------------
// Grid, labels and the Serial Monitor
// ----------------------------------------------------------------------
// Colours are 16-bit "RGB565" numbers: 5 bits red, 6 bits green, 5 bits blue.
// Examples: 0x0000 = black, 0xFFFF = white, 0xF800 = red, 0x07E0 = green, 0xFFE0 = yellow.
const uint16_t GRID_COLOR    = 0xFFFF;   // grid lines: white
const uint16_t OUTLINE_COLOR = 0x0000;   // outline around label letters: black
const int LABEL_BOTTOM_GAP   = 12;       // pixels between the label and the bottom edge (as held)
const unsigned long LABEL_MS = 1500;     // how long a label stays up, in milliseconds (1500 = 1.5 s)
const bool PRINT_FPS = true;             // true: print frames per second to the Serial Monitor

// ----------------------------------------------------------------------
// Encoder and saved settings
// ----------------------------------------------------------------------
// One click of the wheel produces several small signal changes ("steps").
// Example: with 4, four steps make one click. If one click jumps two items,
// set 8; if two clicks are needed to move one item, set 2.
const int  ENC_STEPS_PER_CLICK = 4;
const bool ENC_REVERSE = false;            // true reverses the scroll direction (same as swapping the A/B wires)
// Settings are saved to flash this long after the LAST change, so scrolling
// through several presets causes one save instead of many (flash wears out
// after many writes).
const unsigned long SAVE_DELAY_MS = 3000;  // 3000 ms = 3 seconds

// ----------------------------------------------------------------------
// Preset menu (right side, as you hold the camera)
// ----------------------------------------------------------------------
const int MENU_RIGHT_GAP   = 10;   // px from the right edge to the end of each line (estimate: measure to the cover edge)
const int MENU_FROM_BOTTOM = 140;  // px from the bottom edge to the bottom of the selected item (estimate: measure the star's point)
const int MENU_LINE_GAP    = 8;    // px of space between menu lines
