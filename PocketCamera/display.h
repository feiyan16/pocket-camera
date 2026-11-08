/*
  ======================================================================
  display.h  -  THE 1.69" ST7789 SCREEN (what other files can use)
  ======================================================================
  A ".h" (header) file is a list of what a part OFFERS: its pins, the
  variables other files may read, and the names of its functions. The
  actual code is in the matching ".cpp" file (display.cpp).

  Any file that wants to use the screen writes  #include "display.h"
  and can then call, for example,  showFrame(fb);

  About "extern":
    "extern int CAM_W;" means "a variable called CAM_W exists, but it is
    created in another file". It lets several files share ONE variable.
    The real one (with its starting value) is in display.cpp. Without
    extern, every file including this header would create its own copy,
    and the linker would stop with "multiple definition of CAM_W".
  ======================================================================
*/

#pragma once                      // read this file only once per .cpp file (see config.h)

#include <Arduino.h>              // basic Arduino types and functions (uint16_t, Serial, ...)
#include <Arduino_GFX_Library.h>  // display library: drawing pixels, rotation, the ST7789 driver
#include "esp_camera.h"           // camera driver: needed here for the camera_fb_t type used by showFrame()

// ----------------------------------------------------------------------
// Pins
// ----------------------------------------------------------------------
// #define NAME value is a find-and-replace done before compiling: everywhere the
// code says TFT_SCK, the compiler sees 14. Changing a wire means changing one
// line here instead of hunting through the code.
// The numbers are the chip's GPIO numbers, as printed on the board's pinout ("GPIO14" etc.).
#define TFT_SCK     14   // display clock line, labelled SCL on the display (it's SPI, not I2C)
#define TFT_MOSI     1   // display data line, labelled SDA on the display ("Master Out, Slave In")
#define TFT_DC      41   // display Data/Command pin: LOW = the next byte is a command, HIGH = pixel data
#define TFT_CS      42   // display Chip Select: LOW = "display, listen to me"
#define TFT_BLK     43   // display backlight: HIGH = light on, LOW = off
// The display's RES pin goes to the board's RST pin, so it resets with the board (no GPIO needed).

// ----------------------------------------------------------------------
// Shared variables (created in display.cpp)
// ----------------------------------------------------------------------
// The display object. Other files use it to draw, e.g. gfx->fillScreen(0x0000).
// "Arduino_GFX *" is a pointer: it holds the address of the display object.
extern Arduino_GFX *gfx;

// Screen size in each rotation (filled in by startDisplay()).
extern int CAM_W, CAM_H;   // camera rotation: 240 wide, 280 tall
extern int TXT_W, TXT_H;   // text rotation: 280 wide, 240 tall

// Positions of the rule-of-thirds lines, in camera coordinates (set in startDisplay()).
// Example: CAM_W = 240 -> vertical lines at x = 80 and x = 160.
extern int GRID_X1, GRID_X2, GRID_Y1, GRID_Y2;

// ----------------------------------------------------------------------
// Functions (the code is in display.cpp)
// ----------------------------------------------------------------------
// These lines are "declarations" (also called prototypes): just the name,
// what goes in and what comes out, ending in a semicolon instead of { code }.
// They tell the compiler the function exists, so other files can call it.

// Turns on the backlight, starts the display, measures both rotations,
// works out the grid positions and reserves the screen buffer.
// Returns false if the screen buffer couldn't be reserved (out of memory).
bool startDisplay();

// Rotation changes only affect what is drawn NEXT; pixels already on the
// screen stay where they are. So we can draw the picture in one rotation and
// text in another.
void textRotation();     // switch to the text rotation (TEXT_ROT)
void cameraRotation();   // switch back to the camera rotation (CAM_ROT)

// Full-screen white message on black, e.g. showMessage("SAVING...", 0).
// holdMs = how long to wait afterwards, in milliseconds (0 = return straight away).
void showMessage(const char *msg, int holdMs);

// Draws one camera frame to the screen, with the grid and the label added.
// camera_fb_t is the camera driver's struct for one frame.
void showFrame(camera_fb_t *fb);
