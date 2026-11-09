/*
  ======================================================================
  POCKET CAMERA
  ======================================================================
  Hardware:
    - nulllab ESP32-S3 CAM dev board (ESP32-S3 chip, 8 MB flash,
      8 MB PSRAM, OV3660 camera, microSD slot, battery charger)
    - 1.69" 240 x 280 ST7789 colour display (SPI)
    - 12 x 12 mm tactile button (the shutter)
    - 3-pin mouse-wheel rotary encoder (scrolls through presets)

  What it does:
    - Shows a live camera preview that fills the whole screen.
    - Turning the wheel scrolls through picture "presets" (AUTO, SUNNY,
      CLOUDY ...) and one GRID item. A label with the name pops up for
      1.5 seconds.
    - Pressing the shutter saves a 1600 x 1200 JPEG photo to the SD card
      as /photos/IMG_0001.jpg, IMG_0002.jpg, and so on.
    - On the GRID item, the shutter turns rule-of-thirds guide lines
      on or off instead of taking a photo.
    - The chosen preset and grid setting are remembered after power-off.

  ----------------------------------------------------------------------
  Files in this sketch (each opens as a tab in the Arduino IDE)
    PocketCamera.ino   this file: setup() and loop() only
    config.h           every adjustable setting (rotations, crop, photo
                       quality, label colours and timing, encoder feel)
    display.h / .cpp   the screen: pins, start-up, rotations, messages,
                       drawing each camera frame
    font.h             the FreeMono Bold letter table (data only)
    labels.h / .cpp    on-screen text: letters + outline, positioning
    camera.h / .cpp    the camera: pins, preset list, preview/photo modes
    button.h / .cpp    the shutter button: pin, debouncing
    encoder.h / .cpp   the scroll wheel: pins, background task, clicks
    sdcard.h / .cpp    the SD card: pins, mounting, photo file names
    settings.h / .cpp  the wheel list (presets + GRID), saving to flash
    photo.h / .cpp     taking a photo (uses camera, SD, screen, labels)

  Each ".h" file lists what a part offers; the matching ".cpp" file holds
  the code. Pin numbers are at the top of each part's ".h" file.

  ----------------------------------------------------------------------
  Arduino IDE settings (Tools menu)
    Board:            ESP32S3 Dev Module
    USB CDC On Boot:  Enabled
    USB Mode:         Hardware CDC and JTAG
    Upload Mode:      UART0 / Hardware CDC
    Flash Size:       8MB (64Mb)
    Partition Scheme: 8M with spiffs (3MB APP/1.5MB SPIFFS)
    PSRAM:            OPI PSRAM

  Library (Sketch -> Include Library -> Manage Libraries)
    "GFX Library for Arduino" by Moon On Our Nation
  The camera driver, SD_MMC and Preferences libraries come with the
  ESP32 board package, so nothing else needs installing.

  ----------------------------------------------------------------------
  Wiring
    Display   GND -> GND        VCC -> 3V3
              SCL -> GPIO 14    SDA -> GPIO 1
              RES -> RST pin    DC  -> GPIO 41
              CS  -> GPIO 42    BLK -> GPIO 43
    Shutter   two diagonally opposite legs -> GPIO 44 and GND
    Encoder   common leg -> GND, signal legs -> GPIO 47 and GPIO 46
              (the two wide bent legs are mounting tabs: not wired)
  ======================================================================
*/

// #include copies another file's declarations into this one, so we can use
// what it offers. Quotes "..." mean "look in this sketch's folder first".
// Each line below brings in one part of the camera.
#include "config.h"     // adjustable settings (LABEL_MS, SAVE_DELAY_MS, PRINT_FPS ...)
#include "display.h"    // startDisplay(), showMessage(), showFrame()
#include "labels.h"     // showLabel(), hideLabel(), and the label itself
#include "camera.h"     // startPreview(), the preset list
#include "button.h"     // startButton(), buttonPressed()
#include "encoder.h"    // startEncoder(), readEncoderClicks()
#include "sdcard.h"     // startSD(), sdReady
#include "settings.h"   // loadSettings(), saveSettings(), changeItem(), gridOn ...
#include "photo.h"      // takePhoto()

// ======================================================================
// SETUP: runs once at power-on or reset
// ======================================================================
void setup() {
  Serial.begin(115200);                // start the USB serial connection at 115200 baud (match the Serial Monitor)
  delay(2000);                         // wait 2 s so there's time to open the Serial Monitor

  startButton();                       // shutter pin (button.cpp)

  // Start the screen (display.cpp). It returns false only if the memory for
  // one screen of pixels couldn't be reserved (e.g. PSRAM not enabled in Tools).
  if (!startDisplay()) {
    showMessage("MEMORY ERROR", 0);    //   say so
    while (true) delay(1000);          //   and stop here forever
  }                                    // end of the display check

  loadSettings();                      // read the saved preset and grid BEFORE starting the camera, so it starts with them
  if (!startPreview()) {               // start the live preview; if it fails:
    showMessage("CAMERA ERROR", 0);    //   say so (the Serial Monitor shows the error code)
    while (true) delay(1000);          //   and stop here forever
  }                                    // end of the camera check

  startSD();                           // mount the SD card; sets sdReady (sdcard.cpp)
  startEncoder();                      // encoder pins + background task on core 0 (encoder.cpp)

  // Starting label: the current preset in yellow, or a red warning if there's no card.
  if (sdReady) showLabel(presets[currentPreset].name, 0xFFE0);
  else         showLabel("NO SD CARD", 0xF800);  // 0xF800 = red
}                                      // end of setup

// ======================================================================
// LOOP: runs over and over, forever, after setup()
// ======================================================================
void loop() {
  // Encoder: move through the list if the wheel turned
  int clicks = readEncoderClicks();    // whole clicks since last time (0 if none)
  if (clicks != 0) changeItem(clicks); // "!=" means "not equal": only act if it moved

  // Save settings once nothing has changed for SAVE_DELAY_MS.
  // millis() - settingsChangedAt = milliseconds since the last change.
  if (settingsUnsaved && millis() - settingsChangedAt >= SAVE_DELAY_MS) {
    saveSettings();                    // write to flash
    settingsUnsaved = false;           // nothing pending any more
  }                                    // end of saving

  // Shutter: on the GRID item it toggles the grid; anywhere else it takes a photo.
  if (buttonPressed()) {
    if (currentItem == GRID_ITEM) {    // on GRID:
      gridOn = !gridOn;                //   flip it ("!" turns true into false and false into true)
      showGridLabel();                 //   show the new state
      markChanged();                   //   and remember to save it
    } else {                           // on a preset:
      takePhoto();                     //   take a photo
    }                                  // end of the if/else
  }                                    // end of the shutter

  // Hide the label once LABEL_MS has passed since it appeared.
  if (label.active && millis() - label.shownAt >= LABEL_MS) {
    hideLabel();                       // switch it off
  }                                    // end of label timing
  // Same for the preset menu (all three lines appear together, so checking mainLabel is enough).
  if (mainLabel.active && millis() - mainLabel.shownAt >= LABEL_MS) {
    hidePresetMenu();                  // switch all three off
  }                                    // end of menu timing

  // Live preview: get the newest frame, draw it, give the buffer back.
  camera_fb_t *fb = esp_camera_fb_get();   // NULL if no frame was ready in time
  if (fb) {                            // got one:
    showFrame(fb);                     //   draw it (with grid and label)
    esp_camera_fb_return(fb);          //   return the buffer so the camera can fill it again
  }                                    // end of the preview

  // Frames per second, printed once a second (if PRINT_FPS is true).
  if (PRINT_FPS) {
    static int frames = 0;             // frames counted this second ("static": kept between loops)
    static unsigned long lastReport = 0;  // when we last printed
    frames++;                          // one more frame ("++" adds 1)
    if (millis() - lastReport >= 1000) {  // a second has passed:
      Serial.printf("%d fps\n", frames);  //   print it, e.g. "18 fps"
      frames = 0;                      //   start counting again
      lastReport = millis();           //   from now
    }                                  // end of the one-second check
  }                                    // end of PRINT_FPS
}                                      // end of loop
