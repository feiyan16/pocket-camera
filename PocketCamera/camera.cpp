/*
  ======================================================================
  camera.cpp  -  THE OV3660 CAMERA (the code)
  ======================================================================
  The preset list, sending settings to the sensor, and starting the
  camera in preview mode or photo mode.
  ======================================================================
*/

#include "camera.h"     // our own header: pins, Preset, the promises this file must keep
#include "config.h"     // PREVIEW_SIZE, PHOTO_SIZE, PHOTO_QUALITY, FLIP_V, MIRROR_H
#include "settings.h"   // currentPreset: which preset is chosen

// ----------------------------------------------------------------------
// The preset list  (placeholders: rename, add or remove freely)
// ----------------------------------------------------------------------
// Each {...} fills one Preset's fields in order:
// {name, brightness, saturation, wbMode}. The [] lets the compiler count them.
Preset presets[] = {
  {"AUTO",    0, 0, 0},   // automatic everything
  {"SUNNY",   0, 0, 1},   // white balance for daylight
  {"CLOUDY",  0, 0, 2},   // white balance for overcast skies (a bit warmer)
  {"OFFICE",  0, 0, 3},   // white balance for fluorescent lights
  {"INDOOR",  0, 0, 4},   // the camera calls this "home": light bulbs (cooler correction)
  {"BRIGHT",  1, 0, 0},   // automatic white balance, brightness raised by 1
  {"DARK",   -1, 0, 0},   // automatic white balance, brightness lowered by 1
};                        // end of the list
// How many presets there are: total bytes of the list / bytes of one entry.
// Example: 7 entries x 8 bytes = 56 bytes; 56 / 8 = 7. Updates automatically.
// This has to be worked out HERE, where the full list is visible; other files
// only see "extern Preset presets[]" and can't count it themselves.
const int NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);

// ----------------------------------------------------------------------
// Functions promised in camera.h
// ----------------------------------------------------------------------
// The sensor applies these settings before making each picture, so the
// preview and saved photos both get them.
void applyPreset() {
  sensor_t *s = esp_camera_sensor_get();   // pointer to the sensor's settings (from the camera driver)
  if (s == NULL) return;                   // camera not started: nothing to set (and calling through NULL would crash)
  // "const Preset &p" makes p another name for the list entry (no copy); const = we won't change it.
  const Preset &p = presets[currentPreset];
  // Each setting is a function stored inside the sensor struct. The sensor
  // itself is passed in as the first argument, so the function knows which
  // sensor to change.
  s->set_brightness(s, p.brightness);      // -2 ... +2
  s->set_saturation(s, p.saturation);      // -2 ... +2
  s->set_whitebal(s, 1);                   // 1 = automatic white balance on
  s->set_awb_gain(s, 1);                   // 1 = on; the white-balance presets need this
  s->set_wb_mode(s, p.wbMode);             // 0 auto, 1 sunny, 2 cloudy, 3 office, 4 home
  // "condition ? A : B" means "if condition then A else B". Example: FLIP_V true -> 1.
  s->set_vflip(s, FLIP_V ? 1 : 0);         // the camera's vertical flip
  s->set_hmirror(s, MIRROR_H ? 1 : 0);     // the camera's horizontal mirror
}                                          // end of applyPreset

// Fills in every camera setting, then starts the camera (details of each input in camera.h).
bool startCamera(pixformat_t format, framesize_t size, int fbCount, camera_grab_mode_t grab) {
  camera_config_t config = {};          // a struct of every camera setting; = {} starts them all at zero
  // The camera's clock (XCLK) is produced by the chip's LEDC signal generator: use its channel 0 and timer 0.
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;   // LEDC timer 0 sets the clock's frequency
  config.pin_d0 = Y2_GPIO_NUM;          // data bit 0
  config.pin_d1 = Y3_GPIO_NUM;          // data bit 1
  config.pin_d2 = Y4_GPIO_NUM;          // data bit 2
  config.pin_d3 = Y5_GPIO_NUM;          // data bit 3
  config.pin_d4 = Y6_GPIO_NUM;          // data bit 4
  config.pin_d5 = Y7_GPIO_NUM;          // data bit 5
  config.pin_d6 = Y8_GPIO_NUM;          // data bit 6
  config.pin_d7 = Y9_GPIO_NUM;          // data bit 7
  config.pin_xclk  = XCLK_GPIO_NUM;     // clock to the camera
  config.pin_pclk  = PCLK_GPIO_NUM;     // pixel clock from the camera
  config.pin_vsync = VSYNC_GPIO_NUM;    // frame-start signal
  config.pin_href  = HREF_GPIO_NUM;     // row-valid signal
  config.pin_sccb_sda = SIOD_GPIO_NUM;  // control data line
  config.pin_sccb_scl = SIOC_GPIO_NUM;  // control clock line
  config.pin_pwdn  = PWDN_GPIO_NUM;     // not connected (-1)
  config.pin_reset = RESET_GPIO_NUM;    // not connected (-1)
  config.xclk_freq_hz = 20000000;       // 20 MHz camera clock, the usual value for these sensors
  config.pixel_format = format;         // raw pixels or JPEG (from the caller)
  config.frame_size   = size;           // picture size (from the caller)
  config.jpeg_quality = PHOTO_QUALITY;  // only used in JPEG mode
  config.fb_count     = fbCount;        // number of frame buffers (from the caller)
  config.fb_location  = CAMERA_FB_IN_PSRAM;  // keep frames in the 8 MB PSRAM (the regular memory is too small)
  config.grab_mode    = grab;           // which frame to hand over (from the caller)

  // Start the camera. "&config" passes the address of config, so the driver
  // reads our settings directly instead of a copy. It returns an error code.
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {                  // ESP_OK means success; anything else is a failure
    // Print the code in hexadecimal (%x) and as readable text.
    // Example: "Camera init failed: 0x106 (ESP_ERR_NOT_SUPPORTED)"
    Serial.printf("Camera init failed: 0x%x (%s)\n", err, esp_err_to_name(err));
    return false;                       // tell the caller it failed
  }                                     // end of the error check
  applyPreset();                        // a fresh start loses settings, so send the preset and flips again
  return true;                          // success
}                                       // end of startCamera

// With two buffers the camera fills one while we draw the other, and
// GRAB_LATEST always gives us the newest finished frame.
bool startPreview() {
  return startCamera(PIXFORMAT_RGB565, PREVIEW_SIZE, 2, CAMERA_GRAB_LATEST);  // raw pixels, 400 x 296, 2 buffers, newest frame
}                                       // end of startPreview

// One buffer is enough for single photos, and saves memory.
bool startPhotoMode() {
  return startCamera(PIXFORMAT_JPEG, PHOTO_SIZE, 1, CAMERA_GRAB_WHEN_EMPTY);  // JPEG, 1600 x 1200, 1 buffer
}                                       // end of startPhotoMode
