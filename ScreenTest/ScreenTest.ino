/*
  ======================================================================
  SCREEN TEST
  ======================================================================
  Checks that the 1.69" 240 x 280 ST7789 display is wired correctly to the
  nulllab ESP32-S3 CAM, before running the full camera sketch.

  What you should see:
    1. The screen flashes red, then green, then blue.
    2. A white frame around all four edges, a grey bar with "TOP BAR",
       "Screen OK!" in the middle and "bottom" near the bottom.
    3. The backlight switches off for half a second every 2 seconds
       (this proves the BLK wire works).
  The Serial Monitor (115200 baud) should print "Display init OK".

  If something's wrong:
    - Completely dark, no light:      check BLK (GPIO 43), VCC and GND
    - Light but no picture:           check SCL, SDA, DC, CS, and RES (to RST or 3V3)
    - Colours inverted:               change "true /* IPS * /" below to false
    - Frame missing on one edge:      the offsets (0, 20) need adjusting
    - Noise or garbage:               lower the speed in gfx->begin(...)
  The corners of this screen are rounded, so the frame looks slightly cut
  off right at the corners; that's normal.

  Wiring
    GND -> GND        VCC -> 3V3
    SCL -> GPIO 14    SDA -> GPIO 1
    RES -> RST pin    DC  -> GPIO 41
    CS  -> GPIO 42    BLK -> GPIO 43

  Board settings: the same as PocketCamera (ESP32S3 Dev Module, OPI PSRAM,
  Flash Size 8MB, USB CDC On Boot Enabled).
  ======================================================================
*/

#include <Arduino_GFX_Library.h>  // the display library ("GFX Library for Arduino" by Moon On Our Nation)

// #define NAME value: before compiling, every NAME in the code is replaced by value.
#define TFT_SCK   14   // display SCL: the SPI clock line
#define TFT_MOSI   1   // display SDA: the SPI data line ("Master Out, Slave In")
#define TFT_DC    41   // Data/Command: LOW = next byte is a command, HIGH = pixel data
#define TFT_CS    42   // Chip Select: LOW = "display, listen to me"
#define TFT_BLK   43   // backlight: HIGH = on, LOW = off

// The bus says HOW to talk to the display: these pins, on the chip's HSPI unit.
// MISO (data back from the display) isn't used: GFX_NOT_DEFINED.
Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED, HSPI);  // DC, CS, clock, data, MISO, SPI unit

// The display object says WHAT the display is: an ST7789 controller.
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, GFX_NOT_DEFINED, 0 /* rotation */, true /* IPS */,  // the bus, no reset pin, portrait rotation, IPS panel
  // Size, then offsets: the controller's memory is 240 x 320, the panel shows
  // 280 rows of it centred, so 20 rows at each end are unused.
  240, 280, 0, 20, 0, 20);                                 // width, height, offsets for rotations 0/1 and 2/3

void setup() {                                   // runs once at power-on
  Serial.begin(115200);                          // USB serial at 115200 baud
  delay(1000);                                   // a moment to open the Serial Monitor

  pinMode(TFT_BLK, OUTPUT);                      // the backlight pin is an output
  digitalWrite(TFT_BLK, HIGH);                   // backlight on

  // Start the display. Note: begin() can't detect wiring mistakes, because the
  // display never sends anything back; "OK" only means the ESP32 side started.
  if (!gfx->begin()) {                           // "!" = not: if it did NOT start...
    Serial.println("Display init FAILED: check wiring");  // ...say so...
    while (true) delay(1000);                    // ...and stop here forever
  }                                              // end of the check
  Serial.println("Display init OK");             // the ESP32 side started

  // 1. Colour test. Colours are RGB565 numbers: 5 bits red, 6 green, 5 blue.
  gfx->fillScreen(0xF800); delay(700);           // red (0xF800 = all red bits on), wait 0.7 s
  gfx->fillScreen(0x07E0); delay(700);           // green (0x07E0 = all green bits on)
  gfx->fillScreen(0x001F); delay(700);           // blue (0x001F = all blue bits on)

  // 2. Edge test: a 2-pixel white frame that should touch all four edges.
  gfx->fillScreen(0x0000);                       // black background
  gfx->drawRect(0, 0, 240, 280, 0xFFFF);         // outer rectangle: x, y, width, height, white
  gfx->drawRect(1, 1, 238, 278, 0xFFFF);         // a second one just inside, so the frame is 2 px thick

  // 3. A bar at the top (where the old status bar was) and some text.
  gfx->fillRect(2, 2, 236, 38, 0x39E7);          // grey bar: x, y, width, height, colour
  gfx->setTextSize(2);                           // built-in font at 2x size (12 x 16 px per character)
  gfx->setTextColor(0xFFE0);                     // yellow (red + green)
  gfx->setCursor(16, 13);                        // where the text starts: 16 px in, 13 px down
  gfx->print("TOP BAR");                         // write it

  gfx->setTextColor(0xFFFF);                     // white
  gfx->setCursor(40, 150);                       // roughly the middle
  gfx->print("Screen OK!");                      // write it
  gfx->setCursor(40, 250);                       // near the bottom
  gfx->print("bottom");                          // write it
}                                                // end of setup

void loop() {                                    // runs forever after setup()
  // Blink the backlight to test the BLK wire.
  delay(2000);                                   // wait 2 s
  digitalWrite(TFT_BLK, LOW);                    // backlight off
  delay(500);                                    // for 0.5 s
  digitalWrite(TFT_BLK, HIGH);                   // back on
}                                                // end of loop (then it starts again)
