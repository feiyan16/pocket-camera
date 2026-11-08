/*
  ======================================================================
  display.cpp  -  THE 1.69" ST7789 SCREEN (the code)
  ======================================================================
  Starts the display, switches between the two rotations, shows
  full-screen messages, and draws every camera frame (with the grid and
  the label painted in).

  A ".cpp" file holds the real code for what its ".h" file promises.
  The Arduino IDE compiles every .cpp file in the sketch folder and
  joins them together with the .ino file at the end ("linking").
  ======================================================================
*/

#include "display.h"    // our own header first: the pins and the promises this file must keep
#include "config.h"     // adjustable settings: rotations, crop, colours, DISPLAY_HZ ...
#include "labels.h"     // the label (label, camMask) and buildTextMask() / textMask for messages
#include "settings.h"   // gridOn: whether to draw the grid lines

// ----------------------------------------------------------------------
// The variables promised with "extern" in display.h are created here.
// ----------------------------------------------------------------------
// Screen size in each rotation. These are starting guesses; startDisplay()
// replaces them with the sizes the display library actually reports.
int CAM_W = PANEL_W,  CAM_H = PANEL_H;   // camera rotation: 240 wide, 280 tall
int TXT_W = PANEL_H,  TXT_H = PANEL_W;   // text rotation: 280 wide, 240 tall

// Grid line positions, set in startDisplay().
int GRID_X1, GRID_X2, GRID_Y1, GRID_Y2;

// Memory for one whole screen of pixels: 240 x 280 = 67,200 pixels x 2 bytes
// = 134,400 bytes. Each frame is assembled here, then sent in one transfer.
// "uint16_t *" is a pointer: it holds the memory address where the pixels
// start. NULL means "points nowhere yet"; startDisplay() allocates the memory.
// Only this file uses it, so it isn't in display.h.
uint16_t *screenBuf = NULL;

// "new" creates an object and gives back a pointer to it; "->" is used to
// call functions through a pointer (e.g. gfx->fillScreen()).

// The bus describes HOW to talk to the display: which pins, which SPI unit.
// HSPI is one of the chip's SPI hardware units. MISO (data back from the
// display) isn't used, so it's GFX_NOT_DEFINED. Only this file needs the bus.
Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED /* MISO */, HSPI);  // DC, CS, clock, data, MISO, SPI unit

// The display object describes WHAT the display is: an ST7789 controller.
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, GFX_NOT_DEFINED /* RES is wired to the board's RST pin */,  // the bus; no reset pin (it resets with the board)
  CAM_ROT, true /* IPS */,  // starting rotation; true = IPS panel (affects colour inversion)
  PANEL_W, PANEL_H,         // the panel's own size: 240 x 280
  // Offsets: the controller has memory for 240 x 320 pixels but the panel
  // shows only 280 rows, centred, so 20 rows at each end are unused.
  // Example: in rotation 0, screen row 0 is controller row 20.
  0, 20,   // column / row offsets used for rotations 0 and 1 (the library picks per rotation)
  0, 20);  // column / row offsets used for rotations 2 and 3

// ----------------------------------------------------------------------
// Rotations
// ----------------------------------------------------------------------
void textRotation()   { gfx->setRotation(TEXT_ROT); }  // switch to the text rotation
void cameraRotation() { gfx->setRotation(CAM_ROT); }   // switch back to the camera rotation

// ----------------------------------------------------------------------
// Start-up
// ----------------------------------------------------------------------
bool startDisplay() {
  pinMode(TFT_BLK, OUTPUT);            // backlight pin: we drive it
  digitalWrite(TFT_BLK, HIGH);         // backlight on

  // Start the display at DISPLAY_HZ. begin() returns false if it couldn't start
  // (note: it can't detect wiring problems, since the display never answers back).
  if (!gfx->begin(DISPLAY_HZ)) {
    Serial.println("Display init failed");  // print a warning; the program carries on anyway
  }                                    // end of the display check
  // Ask the library for the real screen size in each rotation.
  textRotation();                      // switch to the text rotation...
  TXT_W = gfx->width();                // ...its width (e.g. 280)
  TXT_H = gfx->height();               // ...its height (e.g. 240)
  cameraRotation();                    // switch to the camera rotation...
  CAM_W = gfx->width();                // ...its width (e.g. 240)
  CAM_H = gfx->height();               // ...its height (e.g. 280)
  // %d prints a whole number. Example: "Camera area 240 x 280, text area 280 x 240"
  Serial.printf("Camera area %d x %d, text area %d x %d\n", CAM_W, CAM_H, TXT_W, TXT_H);
  gfx->fillScreen(0x0000);             // start with a black screen

  // Grid lines at one third and two thirds across and down.
  // Example: CAM_W = 240 -> 80 and 160; CAM_H = 280 -> 93 and 186.
  GRID_X1 = CAM_W / 3;  GRID_X2 = CAM_W * 2 / 3;  // vertical lines
  GRID_Y1 = CAM_H / 3;  GRID_Y2 = CAM_H * 2 / 3;  // horizontal lines (integer division: 280 / 3 = 93)

  // Reserve memory for one screen of pixels in the 8 MB PSRAM.
  // ps_malloc(bytes) returns the address of the memory, or NULL if there isn't enough.
  // Example: 240 * 280 * 2 bytes = 134,400 bytes.
  screenBuf = (uint16_t *)ps_malloc(CAM_W * CAM_H * sizeof(uint16_t));
  // Report success: true if we got the memory, false if not (e.g. PSRAM not enabled in Tools).
  // setup() shows "MEMORY ERROR" and stops if this is false.
  return screenBuf != NULL;
}                                      // end of startDisplay

// ----------------------------------------------------------------------
// Full-screen messages
// ----------------------------------------------------------------------
void showMessage(const char *msg, int holdMs) {
  label.active = false;              // any label disappears with the rest of the screen
  int w = buildTextMask(msg);        // draw the words into textMask; w = their width
  int x0 = (TXT_W - w) / 2;          // left edge that centres them. Example: (280 - 132) / 2 = 74
  int y0 = (TXT_H - MASK_H) / 2;     // top edge that centres them vertically
  textRotation();                    // switch to the text rotation, so the message reads left-to-right
  gfx->fillScreen(0x0000);           // paint the whole screen black
  for (int y = 0; y < MASK_H; y++) {               // every row of the mask...
    for (int x = 0; x < w; x++) {                  // ...every column...
      if (textMask[y][x] == 2) gfx->drawPixel(x0 + x, y0 + y, 0xFFFF);  // ...letter pixels drawn white (no outline needed on black)
    }                                              // end of columns
  }                                                // end of rows
  cameraRotation();                  // back to the camera rotation for the preview
  delay(holdMs);                     // wait, if asked to
}                                    // end of showMessage

// ----------------------------------------------------------------------
// Drawing camera frames
// ----------------------------------------------------------------------
// Builds one screen image in screenBuf from a camera frame, row by row:
//   1. copy the visible part of the row (cropping), fixing the byte order
//   2. add grid lines, if on
//   3. add the label's outline and letter pixels, if a label is showing
// then sends the whole screen to the display in a single transfer.
// camera_fb_t is the driver's struct for one frame: buf (the pixels),
// len (bytes), width, height and format.
void showFrame(camera_fb_t *fb) {
  // Safety checks, so a wrong frame can never read outside its memory:
  if (fb->format != PIXFORMAT_RGB565) return;   // not raw pixels: skip it
  // The frame must be at least as big as the screen plus the crop.
  // Example: needs width >= 240 + 0 and height >= 280 + 8 = 288; CIF is 400 x 296, fine.
  if ((int)fb->width < CAM_W + CROP_X || (int)fb->height < CAM_H + CROP_Y) return;

  // Treat the frame's bytes as 16-bit pixels. (uint16_t *) "casts" the pointer to that type.
  const uint16_t *src = (const uint16_t *)fb->buf;
  int frameW = (int)fb->width;         // pixels per frame row (400)
  int labelW = label.cx1 - label.cx0;  // width of the label rectangle (camera coordinates)
  uint16_t *dst = screenBuf;           // where the next screen row goes; moves forward one row at a time

  for (int y = 0; y < CAM_H; y++) {    // for each of the 280 screen rows...
    // 1. The picture. Find where this screen row starts inside the frame.
    // Example: screen row 0 -> frame row 0 + 8 = 8, starting at column 0 + CROP_X.
    // A frame row is frameW pixels long, so frame row r starts at r * frameW.
    const uint16_t *row = src + (y + CROP_Y) * frameW + CROP_X;
    for (int x = 0; x < CAM_W; x++) {  // for each of the 240 pixels in the row...
      uint16_t p = row[x];             // the pixel from the camera
      // Swap its two bytes: >> 8 moves the high byte down, << 8 moves the low byte up,
      // | combines them. Example: 0x00F8 -> 0xF800 (red).
      dst[x] = SWAP_BYTES ? (uint16_t)((p >> 8) | (p << 8)) : p;
    }                                  // end of the pixel copy
    // 2. Grid lines (only while the grid is on)
    if (gridOn) {
      if (y == GRID_Y1 || y == GRID_Y2) {               // this row IS one of the horizontal lines:
        for (int x = 0; x < CAM_W; x++) dst[x] = GRID_COLOR;  // paint the whole row
      } else {                                          // any other row:
        dst[GRID_X1] = GRID_COLOR;                      // one pixel of the first vertical line
        dst[GRID_X2] = GRID_COLOR;                      // one pixel of the second vertical line
      }                                                 // end of grid
    }                                                   // end of "if gridOn"
    // 3. The label, if one is showing and this row passes through it
    if (label.active && y >= label.cy0 && y < label.cy1) {
      const uint8_t *m = camMask + (y - label.cy0) * labelW;  // the label-mask row matching this screen row
      for (int x = 0; x < labelW; x++) {                     // across the label's width:
        if (m[x] == 2)      dst[label.cx0 + x] = label.color;     // letter pixel: the label colour
        else if (m[x] == 1) dst[label.cx0 + x] = OUTLINE_COLOR;   // outline pixel: dark
      }                                                      // (0 = see-through: the picture stays)
    }                                                        // end of label
    dst += CAM_W;                      // move to where the next row starts (240 pixels further on)
  }                                    // end of rows
  // Send all 240 x 280 pixels, starting at the top-left corner (0, 0).
  gfx->draw16bitRGBBitmap(0, 0, screenBuf, CAM_W, CAM_H);
}                                      // end of showFrame
