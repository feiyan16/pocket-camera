/*
  ======================================================================
  camera.h  -  THE OV3660 CAMERA (what other files can use)
  ======================================================================
  How the camera is used:
    The preview uses raw pixels (format "RGB565", 2 bytes per pixel)
    instead of compressed JPEG. Raw pixels need no decoding, so the
    preview is sharp (no JPEG blockiness) and fast. Raw pixels at full
    photo size would not fit in memory, so for every photo the camera is
    briefly restarted in JPEG mode at 1600 x 1200 (see photo.cpp).

  Presets:
    A preset is a name plus three camera settings (brightness, colour
    strength, white balance). The list itself is in camera.cpp.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types (int8_t, uint8_t ...)
// Espressif's camera driver: esp_camera_init(), esp_camera_fb_get(), the
// camera_config_t, pixformat_t, framesize_t and camera_grab_mode_t types, etc.
#include "esp_camera.h"

// ----------------------------------------------------------------------
// Pins
// ----------------------------------------------------------------------
// Camera pins, from nulllab's documentation for this board. The camera has
// an 8-bit parallel data bus (Y2..Y9) plus timing and control lines.
// They are wired on the board itself, so they can't change.
#define PWDN_GPIO_NUM   -1   // power-down pin: -1 = not connected on this board
#define RESET_GPIO_NUM  -1   // reset pin: -1 = not connected on this board
#define XCLK_GPIO_NUM   15   // clock the ESP32 sends TO the camera (20 MHz, set in camera.cpp)
#define SIOD_GPIO_NUM    4   // camera control data line (SCCB, almost the same as I2C)
#define SIOC_GPIO_NUM    5   // camera control clock line (SCCB)
#define Y9_GPIO_NUM     16   // pixel data bit 7 (the most significant bit)
#define Y8_GPIO_NUM     17   // pixel data bit 6
#define Y7_GPIO_NUM     18   // pixel data bit 5
#define Y6_GPIO_NUM     12   // pixel data bit 4
#define Y5_GPIO_NUM     10   // pixel data bit 3
#define Y4_GPIO_NUM      8   // pixel data bit 2
#define Y3_GPIO_NUM      9   // pixel data bit 1
#define Y2_GPIO_NUM     11   // pixel data bit 0 (the least significant bit)
#define VSYNC_GPIO_NUM   6   // "vertical sync": pulses at the start of every frame
#define HREF_GPIO_NUM    7   // "horizontal reference": HIGH while a row of pixels is being sent
#define PCLK_GPIO_NUM   13   // pixel clock: ticks once for every byte the camera sends

// ----------------------------------------------------------------------
// Presets
// ----------------------------------------------------------------------
//   brightness: -2 ... +2      saturation: -2 ... +2
//   wbMode (white balance): 0 auto, 1 sunny, 2 cloudy, 3 office, 4 home
//
// White balance makes white things look white under different light.
// "Auto" measures the colours in the picture itself; the others assume a
// kind of light. Picking one that doesn't match the light shifts the colours
// warmer or cooler, which is how these work as "tints".

// A struct bundles several values into one new type.
// Here one Preset = a name plus three camera settings.
struct Preset {
  const char *name;   // label text; use only A-Z, 0-9, space and # . : - (the font has nothing else)
  int8_t brightness;  // int8_t = small signed whole number (-128..127); here -2..+2
  int8_t saturation;  // colour strength: -2 washed out ... +2 vivid
  uint8_t wbMode;     // uint8_t = small unsigned whole number (0..255); here 0..4
};                    // a struct definition ends with a semicolon

// The list of presets and how many there are (both created in camera.cpp).
// "extern" explained in display.h. "presets[]" without a number means "an
// array whose length is decided in camera.cpp"; other files use NUM_PRESETS.
extern Preset presets[];        // the list itself
extern const int NUM_PRESETS;   // how many presets (7 with the list as it is)

// ----------------------------------------------------------------------
// Functions (the code is in camera.cpp)
// ----------------------------------------------------------------------
// Sends the current preset (presets[currentPreset]) and the flip settings
// to the camera sensor.
void applyPreset();

// Starts the camera with a given pixel format and size.
//   format:  PIXFORMAT_RGB565 (raw pixels) or PIXFORMAT_JPEG (compressed)
//   size:    e.g. FRAMESIZE_CIF (400 x 296) or FRAMESIZE_UXGA (1600 x 1200)
//   fbCount: how many frame buffers (memory for whole frames) to use
//   grab:    CAMERA_GRAB_LATEST = always hand over the newest frame;
//            CAMERA_GRAB_WHEN_EMPTY = fill a buffer only once it's been returned
// Returns true if the camera started.
bool startCamera(pixformat_t format, framesize_t size, int fbCount, camera_grab_mode_t grab);

// Starts the live preview: raw pixels, PREVIEW_SIZE, two frame buffers.
bool startPreview();

// Starts photo mode: full-size JPEG (PHOTO_SIZE), one frame buffer.
bool startPhotoMode();
