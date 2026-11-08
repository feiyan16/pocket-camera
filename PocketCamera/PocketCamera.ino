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

  How the preview works:
    The camera sends raw pixels (format "RGB565", 2 bytes per pixel)
    instead of compressed JPEG. Raw pixels need no decoding, so the
    preview is sharp (no JPEG blockiness) and fast. Raw pixels at full
    photo size would not fit in memory, so for every photo the camera is
    briefly restarted in JPEG mode at 1600 x 1200, one photo is taken,
    and the camera is restarted in preview mode again.

  How labels work:
    Labels are FreeMono Bold letters with a 1-pixel dark outline and no
    box. They are painted into each camera frame in memory before the
    frame is sent to the screen, so they never flicker.

  Why there are two "rotations":
    The camera module is mounted a quarter turn from the screen. The
    camera picture only looks upright in display rotation 2 (CAM_ROT),
    but in that rotation text would run top-to-bottom. So text is laid
    out in rotation 1 or 3 (TEXT_ROT), where it reads left-to-right as
    you hold the camera, and is then converted to the camera's
    coordinates pixel by pixel.

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

// #include copies another file's declarations into this sketch, so we can use
// what it defines. Angle brackets <...> mean "look in the installed libraries".
#include <Arduino_GFX_Library.h>  // display library: drawing pixels, rotation, the ST7789 driver
// Preferences stores small values (like the chosen preset) in the chip's flash
// memory, so they survive power-off. It comes with the ESP32 board package.
#include <Preferences.h>
// Quotes "..." also search the board package. esp_camera.h is Espressif's camera
// driver: esp_camera_init(), esp_camera_fb_get(), the camera_config_t type, etc.
#include "esp_camera.h"
#include "FS.h"                   // "file system" basics: the File type used to write photos
#include "SD_MMC.h"               // the SD card, using the chip's dedicated SD interface (SD_MMC)

// ======================================================================
// 1. PINS
// ======================================================================
// #define NAME value is a find-and-replace done before compiling: everywhere the
// code says TFT_SCK, the compiler sees 14. Changing a wire means changing one
// line here instead of hunting through the code.
// The numbers are the chip's GPIO numbers, as printed on the board's pinout
// ("GPIO14" etc.).
#define TFT_SCK     14   // display clock line, labelled SCL on the display (it's SPI, not I2C)
#define TFT_MOSI     1   // display data line, labelled SDA on the display ("Master Out, Slave In")
#define TFT_DC      41   // display Data/Command pin: LOW = the next byte is a command, HIGH = pixel data
#define TFT_CS      42   // display Chip Select: LOW = "display, listen to me"
#define TFT_BLK     43   // display backlight: HIGH = light on, LOW = off
#define BUTTON_PIN  44   // shutter button (other leg to GND, so pressed = LOW)
#define ENC_A       47   // encoder signal leg A
#define ENC_B       46   // encoder signal leg B (moved here from GPIO 48, which didn't respond)

// These three are wired to the microSD slot on the board itself; they can't change.
#define SD_CLK      39   // SD clock
#define SD_CMD      38   // SD command line
#define SD_D0       40   // SD data line 0 (1-bit mode uses only one data line)

// Camera pins, from nulllab's documentation for this board. The camera has
// an 8-bit parallel data bus (Y2..Y9) plus timing and control lines.
#define PWDN_GPIO_NUM   -1   // power-down pin: -1 = not connected on this board
#define RESET_GPIO_NUM  -1   // reset pin: -1 = not connected on this board
#define XCLK_GPIO_NUM   15   // clock the ESP32 sends TO the camera (20 MHz, set below)
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

// ======================================================================
// 2. ADJUSTABLE CONSTANTS
// ======================================================================
// "const" means the value can't change while the program runs. The compiler
// can then optimise it, and it stops you from changing it by accident.

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
// coordinates (see textPixelToCamera()). Normally the same as TEXT_ROT.
// If labels appear backwards, upside down or on the wrong edge while the
// "SAVING..." message reads correctly, set this to the other value (1 <-> 3).
const uint8_t LABEL_MAP = TEXT_ROT;

// Display clock: how fast pixels are sent to the screen, in Hz.
// 80000000 = 80 MHz. Faster = smoother preview, but long or loose wires can
// garble the signal at high speed; if the picture shows noise, use 40000000.
const uint32_t DISPLAY_HZ = 80000000;

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

// Photo settings
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

// Colours are 16-bit "RGB565" numbers: 5 bits red, 6 bits green, 5 bits blue.
// Examples: 0x0000 = black, 0xFFFF = white, 0xF800 = red, 0x07E0 = green, 0xFFE0 = yellow.
const uint16_t GRID_COLOR    = 0xFFFF;   // grid lines: white
const uint16_t OUTLINE_COLOR = 0x0000;   // outline around label letters: black
const int LABEL_BOTTOM_GAP   = 12;       // pixels between the label and the bottom edge (as held)
const unsigned long LABEL_MS = 1500;     // how long a label stays up, in milliseconds (1500 = 1.5 s)
const bool PRINT_FPS = true;             // true: print frames per second to the Serial Monitor

// Encoder settings
// One click of the wheel produces several small signal changes ("steps").
// Example: with 4, four steps make one click. If one click jumps two items,
// set 8; if two clicks are needed to move one item, set 2.
const int  ENC_STEPS_PER_CLICK = 4;
const bool ENC_REVERSE = false;            // true reverses the scroll direction (same as swapping the A/B wires)
// Settings are saved to flash this long after the LAST change, so scrolling
// through several presets causes one save instead of many (flash wears out
// after many writes).
const unsigned long SAVE_DELAY_MS = 3000;  // 3000 ms = 3 seconds

// ======================================================================
// 3. PRESETS  (placeholders: rename, add or remove freely)
// ======================================================================
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

// The list of presets. Each {...} fills one Preset's fields in order:
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
const int NUM_PRESETS = sizeof(presets) / sizeof(presets[0]);

// The wheel scrolls through all presets, then one extra "GRID" item.
// Example with 7 presets: items 0-6 are presets, item 7 is GRID.
const int GRID_ITEM = NUM_PRESETS;      // position of GRID in the list (7)
const int NUM_ITEMS = NUM_PRESETS + 1;  // total items the wheel can land on (8)

// ======================================================================
// 4. FONT: FreeMono Bold, 13 x 16 pixels per character
// ======================================================================
// Converted from the FreeMono Bold font file into a table of pixels, so the
// code knows exactly which pixels every letter uses.
// Each character is 16 rows of 16 bits; a 1 bit is a letter pixel.
// Bit 15 (0x8000) is the leftmost pixel, bit 14 (0x4000) the next, and so on.
// Example: the row 0x7FE0 = 0111 1111 1110 0000 in binary, which draws
//          ".##########....." - a solid bar 10 pixels wide, 1 pixel in.
// Lower-case letters are shown as upper case; any other character as a space.
const int FONT_ADV = 13;   // pixels per character, across ("advance"): each letter is 13 pixels wide
const int FONT_H = 16;     // pixels per character, down: 16 rows (the last few are blank except for 'Q')
// Every character the font has, in the same order as the rows of FONT_BITS below.
// Example: ' ' is row 0, '#' is row 1, 'A' is row 15.
const char FONT_CHARS[] = " #.:-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
// FONT_BITS[character][row]: 41 characters, 16 rows each. Each picture below
// shows the character the next line of numbers draws ('#' = pixel on).
const uint16_t FONT_BITS[41][16] = {
  // space:
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},  // space, rows 0-15
  // '#':
  //   ....##..##...
  //   ....##..##...
  //   ....##..##...
  //   ....##.##....
  //   ..#########..
  //   ..#########..
  //   ...###.##....
  //   ...##..##....
  //   ..#########..
  //   ..#########..
  //   ...##..##....
  //   ...##..##....
  //   ...##..##....
  //   ...##..##....
  {0x0CC0, 0x0CC0, 0x0CC0, 0x0D80, 0x3FE0, 0x3FE0, 0x1D80, 0x1980, 0x3FE0, 0x3FE0, 0x1980, 0x1980, 0x1980, 0x1980, 0x0000, 0x0000},  // '#', rows 0-15
  // '.':
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .....###.....
  //   .....###.....
  //   .............
  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0700, 0x0700, 0x0000, 0x0000, 0x0000},  // '.', rows 0-15
  // ':':
  //   .............
  //   .............
  //   .............
  //   .............
  //   .....###.....
  //   .....###.....
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .....###.....
  //   .....###.....
  //   .............
  {0x0000, 0x0000, 0x0000, 0x0000, 0x0700, 0x0700, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0700, 0x0700, 0x0000, 0x0000, 0x0000},  // ':', rows 0-15
  // '-':
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .###########.
  //   .###########.
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  //   .............
  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x7FF0, 0x7FF0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},  // '-', rows 0-15
  // '0':
  //   ....#####....
  //   ...#######...
  //   ...##...##...
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ...##...##...
  //   ...#######...
  //   ....#####....
  //   .............
  {0x0F80, 0x1FC0, 0x18C0, 0x3060, 0x3060, 0x3060, 0x3060, 0x3060, 0x3060, 0x3060, 0x18C0, 0x1FC0, 0x0F80, 0x0000, 0x0000, 0x0000},  // '0', rows 0-15
  // '1':
  //   ....###......
  //   ...####......
  //   ..#####......
  //   ..#..##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   ..########...
  //   ..########...
  //   .............
  {0x0E00, 0x1E00, 0x3E00, 0x2600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x3FC0, 0x3FC0, 0x0000, 0x0000, 0x0000},  // '1', rows 0-15
  // '2':
  //   ....####.....
  //   ...######....
  //   ..##...###...
  //   ..##....##...
  //   ........##...
  //   .......###...
  //   ......###....
  //   .....###.....
  //   ....###......
  //   ...###.......
  //   ..###...##...
  //   .#########...
  //   .#########...
  //   .............
  {0x0F00, 0x1F80, 0x31C0, 0x30C0, 0x00C0, 0x01C0, 0x0380, 0x0700, 0x0E00, 0x1C00, 0x38C0, 0x7FC0, 0x7FC0, 0x0000, 0x0000, 0x0000},  // '2', rows 0-15
  // '3':
  //   ....#####....
  //   ...#######...
  //   ...##....##..
  //   .........##..
  //   .........##..
  //   ......####...
  //   ......####...
  //   ........###..
  //   .........##..
  //   .........##..
  //   ..##....###..
  //   ..########...
  //   ...######....
  //   .............
  {0x0F80, 0x1FC0, 0x1860, 0x0060, 0x0060, 0x03C0, 0x03C0, 0x00E0, 0x0060, 0x0060, 0x30E0, 0x3FC0, 0x1F80, 0x0000, 0x0000, 0x0000},  // '3', rows 0-15
  // '4':
  //   ......###....
  //   ......###....
  //   .....####....
  //   .....####....
  //   ....##.##....
  //   ....#..##....
  //   ...##..##....
  //   ..##...##....
  //   ..########...
  //   ..########...
  //   .......##....
  //   .....#####...
  //   .....#####...
  //   .............
  {0x0380, 0x0380, 0x0780, 0x0780, 0x0D80, 0x0980, 0x1980, 0x3180, 0x3FC0, 0x3FC0, 0x0180, 0x07C0, 0x07C0, 0x0000, 0x0000, 0x0000},  // '4', rows 0-15
  // '5':
  //   ...#######...
  //   ...#######...
  //   ...##........
  //   ...##........
  //   ...######....
  //   ...#######...
  //   ...##...###..
  //   .........##..
  //   .........##..
  //   .........##..
  //   ..##....###..
  //   ..########...
  //   ...######....
  //   .............
  {0x1FC0, 0x1FC0, 0x1800, 0x1800, 0x1F80, 0x1FC0, 0x18E0, 0x0060, 0x0060, 0x0060, 0x30E0, 0x3FC0, 0x1F80, 0x0000, 0x0000, 0x0000},  // '5', rows 0-15
  // '6':
  //   ......#####..
  //   .....######..
  //   ....###......
  //   ...###.......
  //   ..###........
  //   ..##.####....
  //   ..########...
  //   ..####..###..
  //   ..###....##..
  //   ..###....##..
  //   ...##...###..
  //   ...#######...
  //   .....####....
  //   .............
  {0x03E0, 0x07E0, 0x0E00, 0x1C00, 0x3800, 0x3780, 0x3FC0, 0x3CE0, 0x3860, 0x3860, 0x18E0, 0x1FC0, 0x0780, 0x0000, 0x0000, 0x0000},  // '6', rows 0-15
  // '7':
  //   ..#########..
  //   ..#########..
  //   ..##....##...
  //   ........##...
  //   ........##...
  //   .......##....
  //   .......##....
  //   .......##....
  //   ......##.....
  //   ......##.....
  //   ......##.....
  //   .....##......
  //   .....##......
  //   .............
  {0x3FE0, 0x3FE0, 0x30C0, 0x00C0, 0x00C0, 0x0180, 0x0180, 0x0180, 0x0300, 0x0300, 0x0300, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000},  // '7', rows 0-15
  // '8':
  //   ....#####....
  //   ...#######...
  //   ..###...###..
  //   ..##.....##..
  //   ..###...###..
  //   ...#######...
  //   ...#######...
  //   ..###...###..
  //   ..##.....##..
  //   ..##.....##..
  //   ..###...###..
  //   ...#######...
  //   ....#####....
  //   .............
  {0x0F80, 0x1FC0, 0x38E0, 0x3060, 0x38E0, 0x1FC0, 0x1FC0, 0x38E0, 0x3060, 0x3060, 0x38E0, 0x1FC0, 0x0F80, 0x0000, 0x0000, 0x0000},  // '8', rows 0-15
  // '9':
  //   ....####.....
  //   ...#######...
  //   ..###...##...
  //   ..##....###..
  //   ..##....###..
  //   ..###..####..
  //   ...########..
  //   ....####.##..
  //   ........###..
  //   .......###...
  //   ......###....
  //   ..######.....
  //   ..#####......
  //   .............
  {0x0F00, 0x1FC0, 0x38C0, 0x30E0, 0x30E0, 0x39E0, 0x1FE0, 0x0F60, 0x00E0, 0x01C0, 0x0380, 0x3F00, 0x3E00, 0x0000, 0x0000, 0x0000},  // '9', rows 0-15
  // 'A':
  //   ..#####......
  //   ..######.....
  //   ....####.....
  //   ....####.....
  //   ...##..##....
  //   ...##..##....
  //   ...##..##....
  //   ..########...
  //   ..########...
  //   .###....###..
  //   .##......##..
  //   #####..#####.
  //   #####..#####.
  //   .............
  {0x3E00, 0x3F00, 0x0F00, 0x0F00, 0x1980, 0x1980, 0x1980, 0x3FC0, 0x3FC0, 0x70E0, 0x6060, 0xF9F0, 0xF9F0, 0x0000, 0x0000, 0x0000},  // 'A', rows 0-15
  // 'B':
  //   .########....
  //   .#########...
  //   ..##....###..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##....###..
  //   ..########...
  //   ..#########..
  //   ..##.....###.
  //   ..##......##.
  //   ..##......##.
  //   .##########..
  //   .#########...
  //   .............
  {0x7F80, 0x7FC0, 0x30E0, 0x3060, 0x3060, 0x30E0, 0x3FC0, 0x3FE0, 0x3070, 0x3030, 0x3030, 0x7FE0, 0x7FC0, 0x0000, 0x0000, 0x0000},  // 'B', rows 0-15
  // 'C':
  //   ....########.
  //   ...#########.
  //   ..###....###.
  //   .###......##.
  //   .##.......##.
  //   .##..........
  //   .##..........
  //   .##..........
  //   .##..........
  //   .###.........
  //   ..###.....##.
  //   ...#########.
  //   .....#####...
  //   .............
  {0x0FF0, 0x1FF0, 0x3870, 0x7030, 0x6030, 0x6000, 0x6000, 0x6000, 0x6000, 0x7000, 0x3830, 0x1FF0, 0x07C0, 0x0000, 0x0000, 0x0000},  // 'C', rows 0-15
  // 'D':
  //   .#######.....
  //   .########....
  //   ..##....##...
  //   ..##....###..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##....##...
  //   .#########...
  //   .#######.....
  //   .............
  {0x7F00, 0x7F80, 0x30C0, 0x30E0, 0x3060, 0x3060, 0x3060, 0x3060, 0x3060, 0x3060, 0x30C0, 0x7FC0, 0x7F00, 0x0000, 0x0000, 0x0000},  // 'D', rows 0-15
  // 'E':
  //   .##########..
  //   .##########..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##..##.....
  //   ..######.....
  //   ..######.....
  //   ..##..##.##..
  //   ..##..##.##..
  //   ..##.....##..
  //   .##########..
  //   .##########..
  //   .............
  {0x7FE0, 0x7FE0, 0x3060, 0x3060, 0x3060, 0x3300, 0x3F00, 0x3F00, 0x3360, 0x3360, 0x3060, 0x7FE0, 0x7FE0, 0x0000, 0x0000, 0x0000},  // 'E', rows 0-15
  // 'F':
  //   .##########..
  //   .##########..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##..##.##..
  //   ..##..##.....
  //   ..######.....
  //   ..######.....
  //   ..##..##.....
  //   ..##.........
  //   ..##.........
  //   .######......
  //   .######......
  //   .............
  {0x7FE0, 0x7FE0, 0x3060, 0x3060, 0x3360, 0x3300, 0x3F00, 0x3F00, 0x3300, 0x3000, 0x3000, 0x7E00, 0x7E00, 0x0000, 0x0000, 0x0000},  // 'F', rows 0-15
  // 'G':
  //   ....#######..
  //   ...########..
  //   ..###....##..
  //   .###.....##..
  //   .##..........
  //   .##..........
  //   .##..........
  //   .##...######.
  //   .##...######.
  //   .##......##..
  //   ..##.....##..
  //   ..#########..
  //   ....#####....
  //   .............
  {0x0FE0, 0x1FE0, 0x3860, 0x7060, 0x6000, 0x6000, 0x6000, 0x63F0, 0x63F0, 0x6060, 0x3060, 0x3FE0, 0x0F80, 0x0000, 0x0000, 0x0000},  // 'G', rows 0-15
  // 'H':
  //   .#####..#####
  //   .#####..#####
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..#########..
  //   ..#########..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##.....##..
  //   .#####..#####
  //   .#####..#####
  //   .............
  {0x7CF8, 0x7CF8, 0x3060, 0x3060, 0x3060, 0x3FE0, 0x3FE0, 0x3060, 0x3060, 0x3060, 0x3060, 0x7CF8, 0x7CF8, 0x0000, 0x0000, 0x0000},  // 'H', rows 0-15
  // 'I':
  //   ..########...
  //   ..########...
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   ..########...
  //   ..########...
  //   .............
  {0x3FC0, 0x3FC0, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x3FC0, 0x3FC0, 0x0000, 0x0000, 0x0000},  // 'I', rows 0-15
  // 'J':
  //   .....#######.
  //   .....#######.
  //   ........##...
  //   ........##...
  //   ........##...
  //   ........##...
  //   ........##...
  //   .##.....##...
  //   .##.....##...
  //   .##.....##...
  //   .##....###...
  //   .########....
  //   ...#####.....
  //   .............
  {0x07F0, 0x07F0, 0x00C0, 0x00C0, 0x00C0, 0x00C0, 0x00C0, 0x60C0, 0x60C0, 0x60C0, 0x61C0, 0x7F80, 0x1F00, 0x0000, 0x0000, 0x0000},  // 'J', rows 0-15
  // 'K':
  //   .#####.#####.
  //   .#####.#####.
  //   ..##...###...
  //   ..##..###....
  //   ..##.###.....
  //   ..#####......
  //   ..######.....
  //   ..###.###....
  //   ..##...##....
  //   ..##...###...
  //   ..##....##...
  //   .#####..#####
  //   .#####...####
  //   .............
  {0x7DF0, 0x7DF0, 0x31C0, 0x3380, 0x3700, 0x3E00, 0x3F00, 0x3B80, 0x3180, 0x31C0, 0x30C0, 0x7CF8, 0x7C78, 0x0000, 0x0000, 0x0000},  // 'K', rows 0-15
  // 'L':
  //   .######......
  //   .######......
  //   ...##........
  //   ...##........
  //   ...##........
  //   ...##........
  //   ...##........
  //   ...##........
  //   ...##.....##.
  //   ...##.....##.
  //   ...##.....##.
  //   .###########.
  //   .###########.
  //   .............
  {0x7E00, 0x7E00, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1830, 0x1830, 0x1830, 0x7FF0, 0x7FF0, 0x0000, 0x0000, 0x0000},  // 'L', rows 0-15
  // 'M':
  //   ####.....####
  //   ####.....####
  //   .####...####.
  //   .####...####.
  //   .####...####.
  //   .#####.#####.
  //   .##.##.##.##.
  //   .##.#####.##.
  //   .##..###..##.
  //   .##..###..##.
  //   .##.......##.
  //   #####...#####
  //   #####...#####
  //   .............
  {0xF078, 0xF078, 0x78F0, 0x78F0, 0x78F0, 0x7DF0, 0x6DB0, 0x6FB0, 0x6730, 0x6730, 0x6030, 0xF8F8, 0xF8F8, 0x0000, 0x0000, 0x0000},  // 'M', rows 0-15
  // 'N':
  //   .###...#####.
  //   .####..#####.
  //   ..###....##..
  //   ..####...##..
  //   ..#####..##..
  //   ..##.##..##..
  //   ..##.###.##..
  //   ..##..##.##..
  //   ..##..#####..
  //   ..##...####..
  //   ..##....###..
  //   .#####..###..
  //   .#####...##..
  //   .............
  {0x71F0, 0x79F0, 0x3860, 0x3C60, 0x3E60, 0x3660, 0x3760, 0x3360, 0x33E0, 0x31E0, 0x30E0, 0x7CE0, 0x7C60, 0x0000, 0x0000, 0x0000},  // 'N', rows 0-15
  // 'O':
  //   ....####.....
  //   ..########...
  //   .###....###..
  //   .##......##..
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   .##......##..
  //   .###....###..
  //   ..########...
  //   ....####.....
  //   .............
  {0x0F00, 0x3FC0, 0x70E0, 0x6060, 0xC030, 0xC030, 0xC030, 0xC030, 0xC030, 0x6060, 0x70E0, 0x3FC0, 0x0F00, 0x0000, 0x0000, 0x0000},  // 'O', rows 0-15
  // 'P':
  //   .########....
  //   .#########...
  //   ..##....###..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##....###..
  //   ..########...
  //   ..#######....
  //   ..##.........
  //   ..##.........
  //   ..##.........
  //   .######......
  //   .######......
  //   .............
  {0x7F80, 0x7FC0, 0x30E0, 0x3060, 0x3060, 0x30E0, 0x3FC0, 0x3F80, 0x3000, 0x3000, 0x3000, 0x7E00, 0x7E00, 0x0000, 0x0000, 0x0000},  // 'P', rows 0-15
  // 'Q':
  //   ....####.....
  //   ..########...
  //   .###....###..
  //   .##......##..
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   ##........##.
  //   .##......##..
  //   .###....###..
  //   ..########...
  //   ...#####.....
  //   ...##.....##.
  //   ..##########.
  //   ..#########..
  {0x0F00, 0x3FC0, 0x70E0, 0x6060, 0xC030, 0xC030, 0xC030, 0xC030, 0xC030, 0x6060, 0x70E0, 0x3FC0, 0x1F00, 0x1830, 0x3FF0, 0x3FE0},  // 'Q', rows 0-15
  // 'R':
  //   .########....
  //   .#########...
  //   ..##....###..
  //   ..##.....##..
  //   ..##.....##..
  //   ..##....###..
  //   ..########...
  //   ..#######....
  //   ..##...###...
  //   ..##....###..
  //   ..##.....##..
  //   .#####...####
  //   .#####....###
  //   .............
  {0x7F80, 0x7FC0, 0x30E0, 0x3060, 0x3060, 0x30E0, 0x3FC0, 0x3F80, 0x31C0, 0x30E0, 0x3060, 0x7C78, 0x7C38, 0x0000, 0x0000, 0x0000},  // 'R', rows 0-15
  // 'S':
  //   ....#######..
  //   ...########..
  //   ..###....##..
  //   ..##.....##..
  //   ..##.........
  //   ..###........
  //   ...######....
  //   ....######...
  //   ........###..
  //   ..##.....##..
  //   ..##....###..
  //   ..########...
  //   ..#######....
  //   .............
  {0x0FE0, 0x1FE0, 0x3860, 0x3060, 0x3000, 0x3800, 0x1F80, 0x0FC0, 0x00E0, 0x3060, 0x30E0, 0x3FC0, 0x3F80, 0x0000, 0x0000, 0x0000},  // 'S', rows 0-15
  // 'T':
  //   .###########.
  //   .###########.
  //   .##..##...##.
  //   .##..##...##.
  //   .##..##...##.
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   ...######....
  //   ...######....
  //   .............
  {0x7FF0, 0x7FF0, 0x6630, 0x6630, 0x6630, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x1F80, 0x1F80, 0x0000, 0x0000, 0x0000},  // 'T', rows 0-15
  // 'U':
  //   #####..#####.
  //   #####..#####.
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   .##......##..
  //   ..##....##...
  //   ..########...
  //   ....####.....
  //   .............
  {0xF9F0, 0xF9F0, 0x6060, 0x6060, 0x6060, 0x6060, 0x6060, 0x6060, 0x6060, 0x6060, 0x30C0, 0x3FC0, 0x0F00, 0x0000, 0x0000, 0x0000},  // 'U', rows 0-15
  // 'V':
  //   #####...#####
  //   #####...#####
  //   .##.......##.
  //   ..##.....##..
  //   ..##.....##..
  //   ..###...###..
  //   ...##...##...
  //   ...###..##...
  //   ...###.###...
  //   ....##.##....
  //   ....#####....
  //   .....###.....
  //   .....###.....
  //   .............
  {0xF8F8, 0xF8F8, 0x6030, 0x3060, 0x3060, 0x38E0, 0x18C0, 0x1CC0, 0x1DC0, 0x0D80, 0x0F80, 0x0700, 0x0700, 0x0000, 0x0000, 0x0000},  // 'V', rows 0-15
  // 'W':
  //   #####..#####.
  //   #####..#####.
  //   ###.......##.
  //   .##.......##.
  //   .##..##..###.
  //   .##..###.##..
  //   .##.####.##..
  //   .##.#######..
  //   .##########..
  //   ..###..####..
  //   ..###..####..
  //   ..###..####..
  //   ..##....###..
  //   .............
  {0xF9F0, 0xF9F0, 0xE030, 0x6030, 0x6670, 0x6760, 0x6F60, 0x6FE0, 0x7FE0, 0x39E0, 0x39E0, 0x39E0, 0x30E0, 0x0000, 0x0000, 0x0000},  // 'W', rows 0-15
  // 'X':
  //   ####...####..
  //   ####...####..
  //   .###...###...
  //   ..###..##....
  //   ...#####.....
  //   ...#####.....
  //   ....###......
  //   ...#####.....
  //   ..#######....
  //   ..###..##....
  //   .###....##...
  //   #####..#####.
  //   #####..#####.
  //   .............
  {0xF1E0, 0xF1E0, 0x71C0, 0x3980, 0x1F00, 0x1F00, 0x0E00, 0x1F00, 0x3F80, 0x3980, 0x70C0, 0xF9F0, 0xF9F0, 0x0000, 0x0000, 0x0000},  // 'X', rows 0-15
  // 'Y':
  //   ####...####..
  //   ####...####..
  //   .###...###...
  //   ..###..##....
  //   ...##.###....
  //   ....####.....
  //   ....####.....
  //   .....##......
  //   .....##......
  //   .....##......
  //   .....##......
  //   ...######....
  //   ...######....
  //   .............
  {0xF1E0, 0xF1E0, 0x71C0, 0x3980, 0x1B80, 0x0F00, 0x0F00, 0x0600, 0x0600, 0x0600, 0x0600, 0x1F80, 0x1F80, 0x0000, 0x0000, 0x0000},  // 'Y', rows 0-15
  // 'Z':
  //   ..########...
  //   ..########...
  //   ..##....##...
  //   ..##...##....
  //   ..##..##.....
  //   ......##.....
  //   .....##......
  //   ....##.......
  //   ....##...##..
  //   ...##....##..
  //   ..##.....##..
  //   ..#########..
  //   ..#########..
  //   .............
  {0x3FC0, 0x3FC0, 0x30C0, 0x3180, 0x3300, 0x0300, 0x0600, 0x0C00, 0x0C60, 0x1860, 0x3060, 0x3FE0, 0x3FE0, 0x0000, 0x0000, 0x0000},  // 'Z', rows 0-15
};  // end of the font table
// Longest text a label can have. 280 px across / 13 px per letter = 21, so 20 fits safely.
const int MAX_LABEL_CHARS = 20;
// Size of the mask (pixel grid) a label is drawn into. +2 leaves a 1-pixel
// border on each side for the outline. Example: 20 x 13 + 2 = 262 pixels wide.
const int MASK_W = MAX_LABEL_CHARS * FONT_ADV + 2;
const int MASK_H = FONT_H + 2;                        // 16 + 2 = 18 pixels tall

// ======================================================================
// 5. PROGRAM STATE AND TYPES
// ======================================================================
// Variables declared out here (outside any function) are "global": every
// function can read and change them, and they keep their values between loops.
// Custom types (structs) must be defined here, before the first function:
// the Arduino IDE automatically writes declarations for all functions just
// above the first function, and they need to know these types already.

// Screen size in each rotation. These are starting guesses; setup() replaces
// them with the sizes the display library actually reports.
int CAM_W = PANEL_W,  CAM_H = PANEL_H;   // camera rotation: 240 wide, 280 tall
int TXT_W = PANEL_H,  TXT_H = PANEL_W;   // text rotation: 280 wide, 240 tall

// Positions of the rule-of-thirds lines, in camera coordinates (set in setup()).
// Example: CAM_W = 240 -> vertical lines at x = 80 and x = 160.
int GRID_X1, GRID_X2, GRID_Y1, GRID_Y2;

// Memory for one whole screen of pixels: 240 x 280 = 67,200 pixels x 2 bytes
// = 134,400 bytes. Each frame is assembled here, then sent in one transfer.
// "uint16_t *" is a pointer: it holds the memory address where the pixels
// start. NULL means "points nowhere yet"; setup() allocates the memory.
uint16_t *screenBuf = NULL;

// Label masks: small pixel grids where 0 = see-through, 1 = outline, 2 = letter.
uint8_t textMask[MASK_H][MASK_W];   // the label as you read it (text coordinates), 2-D: [row][column]
uint8_t camMask[MASK_W * MASK_H];   // the same label turned to camera coordinates, stored row after row

bool sdReady = false;                 // true once the SD card is mounted and usable
int lastPhotoNumber = 0;              // number of the last saved photo (0 = none yet)
int currentPreset = 0;                // which preset the camera is using (index into presets[])
int currentItem = 0;                  // where the wheel is in the list (a preset, or GRID_ITEM)
bool gridOn = false;                  // true = rule-of-thirds lines are drawn
bool settingsUnsaved = false;         // true = something changed that isn't saved to flash yet
unsigned long settingsChangedAt = 0;  // the time (millis()) of that last change

// The label currently on screen, as a rectangle in camera coordinates.
struct Label {
  bool active;              // true = a label is showing
  int cx0, cy0, cx1, cy1;   // corners; x1 and y1 are "just past" the edge, so width = cx1 - cx0
  uint16_t color;           // colour of the letters (RGB565)
  unsigned long shownAt;    // when it appeared (millis()), to know when to hide it
};                          // end of the struct
// One global Label, starting switched off with every field 0.
Label label = {false, 0, 0, 0, 0, 0, 0};

// A debounced input. Button contacts "bounce" (flicker on/off for a few
// milliseconds) when pressed; debouncing waits until the reading is steady.
struct Debounced {
  uint8_t pin;          // which GPIO
  bool stable;          // the last accepted (steady) reading
  bool candidate;       // a new reading we are timing
  unsigned long since;  // when that candidate first appeared
};                      // end of the struct

// ======================================================================
// 6. DISPLAY, TEXT AND LABELS
// ======================================================================
// "new" creates an object and gives back a pointer to it; "->" is used to
// call functions through a pointer (e.g. gfx->fillScreen()).

// The bus describes HOW to talk to the display: which pins, which SPI unit.
// HSPI is one of the chip's SPI hardware units. MISO (data back from the
// display) isn't used, so it's GFX_NOT_DEFINED.
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

// Rotation changes only affect what is drawn NEXT; pixels already on the
// screen stay where they are. So we can draw the picture in one rotation and
// text in another.
void textRotation()   { gfx->setRotation(TEXT_ROT); }  // switch to the text rotation
void cameraRotation() { gfx->setRotation(CAM_ROT); }   // switch back to the camera rotation

// Finds which row of FONT_BITS holds a character.
// Example: 'A' is at position 15 in FONT_CHARS, so it returns 15.
int glyphIndex(char c) {               // c = the character to look up
  c = toupper(c);                      // 'a' becomes 'A' (the font only has capitals)
  const char *p = strchr(FONT_CHARS, c);  // strchr finds c in the string; gives its address, or NULL if missing
  if (p == NULL || c == '\0') return 0;   // not in the font (or the end-of-text marker): use row 0, a space
  return p - FONT_CHARS;               // address of c minus address of the start = its position
}                                      // end of glyphIndex

// Draws text into textMask, with a 1-pixel outline around every letter.
// Returns the width in pixels (the height is always MASK_H).
// Example: "SUNNY" -> 5 letters x 13 px + 2 = 67 pixels wide.
int buildTextMask(const char *text) {        // text = the words to draw
  int n = min((int)strlen(text), MAX_LABEL_CHARS);  // number of letters, capped at 20; strlen counts characters
  int w = n * FONT_ADV + 2;                  // mask width: letters + 1 pixel each side for the outline
  memset(textMask, 0, sizeof(textMask));     // memset fills memory with a value: here, everything see-through (0)

  // 1. Letters. They're placed 1 pixel in from the edge, leaving room for the outline.
  for (int i = 0; i < n; i++) {                       // for each letter...
    const uint16_t *g = FONT_BITS[glyphIndex(text[i])];  // ...look up its 16 rows of bits
    for (int row = 0; row < FONT_H; row++) {          // for each of its 16 rows...
      for (int col = 0; col < FONT_ADV; col++) {      // ...and each of its 13 columns:
        // 0x8000 >> col moves a single 1 bit to the right, one place per column.
        // Example: col 0 -> 0x8000, col 1 -> 0x4000, col 2 -> 0x2000.
        // "&" keeps only the bits set in both, so the result is non-zero if this pixel is part of the letter.
        if (g[row] & (0x8000 >> col)) textMask[row + 1][i * FONT_ADV + col + 1] = 2;  // mark it as a letter pixel
      }                                               // end of columns
    }                                                 // end of rows
  }                                                   // end of letters
  // 2. Outline: every empty pixel that touches a letter pixel, including
  //    diagonally, becomes outline. Example: a letter pixel at (5,5) makes
  //    (4,4) to (6,6) outline, unless they are letter pixels themselves.
  for (int y = 0; y < MASK_H; y++) {                  // every row of the mask...
    for (int x = 0; x < w; x++) {                     // ...and every column
      if (textMask[y][x] != 0) continue;              // already a letter: skip ("continue" jumps to the next x)
      bool touches = false;                           // assume no letter next to it, until we find one
      for (int dy = -1; dy <= 1 && !touches; dy++) {  // look at the row above, this row and the row below
        for (int dx = -1; dx <= 1; dx++) {            // and the column left, this column and the column right
          int yy = y + dy, xx = x + dx;               // the neighbour's position
          if (yy >= 0 && yy < MASK_H && xx >= 0 && xx < w && textMask[yy][xx] == 2) {  // inside the mask, and a letter?
            touches = true;                           // found one
            break;                                    // "break" leaves the inner loop early
          }                                           // end of the check
        }                                             // end of dx
      }                                               // end of dy
      if (touches) textMask[y][x] = 1;                // mark it as outline
    }                                                 // end of x
  }                                                   // end of y
  return w;                                           // hand back the width
}                                                     // end of buildTextMask

// Converts one pixel position from text coordinates to camera coordinates.
// Both rotations show the same physical pixels, just numbered differently,
// like describing a seat by (row, seat) from the front or from the side.
// "int &cx" is a reference: the function writes its answer straight into the
// caller's variable. (These formulas assume CAM_ROT = 2.)
// Example (LABEL_MAP 1): text pixel (tx=10, ty=200) -> camera pixel (cx=200, cy=269).
void textPixelToCamera(int tx, int ty, int &cx, int &cy) {
  if (LABEL_MAP == 1) {      // text rotation 1:
    cx = ty;                 // the text's "down" is the camera's "right"
    cy = PANEL_H - 1 - tx;   // the text's "right" is the camera's "up" (counting from the bottom)
  } else {                   // text rotation 3:
    cx = PANEL_W - 1 - ty;   // the text's "down" is the camera's "left"
    cy = tx;                 // the text's "right" is the camera's "down"
  }                          // end of the if/else
}                            // end of textPixelToCamera

// Full-screen white message on black, e.g. "SAVING...".
// holdMs = how long to wait afterwards, in milliseconds (0 = return straight away).
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

// Shows a label at the bottom centre (as you hold the camera) for LABEL_MS.
// The label is converted to camera coordinates here, once; showFrame() then
// paints it into every frame while it's active.
// Example: showLabel("SUNNY", 0xFFE0) shows "SUNNY" in yellow.
void showLabel(const char *text, uint16_t color) {
  int w = buildTextMask(text);                    // draw the words into textMask; w = their width
  int tx0 = (TXT_W - w) / 2;                      // left edge, centred across the screen (text coordinates)
  int ty0 = TXT_H - LABEL_BOTTOM_GAP - MASK_H;    // top edge, LABEL_BOTTOM_GAP pixels up from the bottom

  // Find the label's rectangle in camera coordinates by converting two opposite corners.
  int ax, ay, bx, by;                             // the two converted corners
  textPixelToCamera(tx0, ty0, ax, ay);            // top-left corner of the text
  textPixelToCamera(tx0 + w - 1, ty0 + MASK_H - 1, bx, by);  // bottom-right corner of the text
  // After rotating, either corner could be the smaller one, so take min and max.
  label.cx0 = min(ax, bx);  label.cx1 = max(ax, bx) + 1;     // +1 because x1 is "just past" the edge
  label.cy0 = min(ay, by);  label.cy1 = max(ay, by) + 1;     // same for y
  int cw = label.cx1 - label.cx0;                 // width of the rectangle in camera coordinates

  // Copy every mask pixel to its spot in the camera-coordinate mask.
  for (int y = 0; y < MASK_H; y++) {              // every row of the text mask...
    for (int x = 0; x < w; x++) {                 // ...and every column
      int cx, cy;                                 // where this pixel lands on the camera picture
      textPixelToCamera(tx0 + x, ty0 + y, cx, cy);  // convert its screen position
      // camMask is one long row-after-row list, so position (column, row)
      // is at row * width + column. Example: row 2, column 5, width 18 -> 2*18+5 = 41.
      camMask[(cy - label.cy0) * cw + (cx - label.cx0)] = textMask[y][x];
    }                                             // end of columns
  }                                               // end of rows
  label.color = color;                            // remember the letter colour
  label.shownAt = millis();                       // millis() = milliseconds since the board started
  label.active = true;                            // switch it on: showFrame() starts painting it
}                                                 // end of showLabel

// Hides the label. Nothing needs erasing: the next frame simply doesn't include it.
void hideLabel() {
  label.active = false;   // switch the label off; showFrame() stops painting it
}                                                 // end of hideLabel

// ======================================================================
// 7. CAMERA
// ======================================================================
// Sends the current preset and the flip settings to the camera sensor.
// The sensor applies them before making each picture, so the preview and
// saved photos both get them.
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

// Starts the camera with a given pixel format and size.
//   format:  PIXFORMAT_RGB565 (raw pixels) or PIXFORMAT_JPEG (compressed)
//   size:    e.g. FRAMESIZE_CIF (400 x 296) or FRAMESIZE_UXGA (1600 x 1200)
//   fbCount: how many frame buffers (memory for whole frames) to use
//   grab:    CAMERA_GRAB_LATEST = always hand over the newest frame;
//            CAMERA_GRAB_WHEN_EMPTY = fill a buffer only once it's been returned
// Returns true if the camera started.
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

// Starts the live preview: raw pixels, 400 x 296, two frame buffers.
// With two buffers the camera fills one while we draw the other, and
// GRAB_LATEST always gives us the newest finished frame.
bool startPreview() {
  return startCamera(PIXFORMAT_RGB565, PREVIEW_SIZE, 2, CAMERA_GRAB_LATEST);  // raw pixels, 400 x 296, 2 buffers, newest frame
}                                       // end of startPreview

// Starts photo mode: full-size JPEG, one frame buffer.
// One buffer is enough for single photos, and saves memory.
bool startPhotoMode() {
  return startCamera(PIXFORMAT_JPEG, PHOTO_SIZE, 1, CAMERA_GRAB_WHEN_EMPTY);  // JPEG, 1600 x 1200, 1 buffer
}                                       // end of startPhotoMode

// ======================================================================
// 8. DRAWING CAMERA FRAMES
// ======================================================================
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

// ======================================================================
// 9. INPUTS: SHUTTER BUTTON AND ENCODER
// ======================================================================
// The shutter's debounce state: pin 44, starting "not pressed" (HIGH, because
// the internal pull-up holds the pin HIGH until the button connects it to GND).
Debounced buttonIn = {BUTTON_PIN, HIGH, HIGH, 0};

// Returns true ONCE each time the input settles into a new state (pressed or
// released). "Debounced &d" passes the real struct, so changes made here stick.
// Example: a press that bounces HIGH-LOW-HIGH-LOW over 5 ms counts once,
// 30 ms after the last bounce.
bool settledChange(Debounced &d, unsigned long settleMs) {
  bool now = digitalRead(d.pin);        // read the pin: HIGH (1) or LOW (0)
  if (now != d.candidate) {             // the reading changed:
    d.candidate = now;                  //   remember the new reading...
    d.since = millis();                 //   ...and restart the timer
  }                                     // end of change check
  // If the candidate differs from the accepted state and has held for settleMs, accept it.
  if (d.candidate != d.stable && millis() - d.since >= settleMs) {
    d.stable = d.candidate;             // accept the new state
    return true;                        // report the change (once)
  }                                     // end of settle check
  return false;                         // no new settled change
}                                       // end of settledChange

// True once per press. "&&" only checks the second part if the first is
// true, and the first part has to run every time to keep the timer updated.
bool buttonPressed() {
  return settledChange(buttonIn, 30) && buttonIn.stable == LOW;  // a settled change that is a press (LOW)
}                                       // end of buttonPressed

// The encoder's two legs each connect to GND or not, slightly out of step.
// Reading them as two bits (A then B) gives four states. Turning one way they
// go 00 -> 01 -> 11 -> 10 -> 00; the other way, 00 -> 10 -> 11 -> 01 -> 00.
// The table below turns "previous state, new state" into +1, -1 or 0.
// Index = previous * 4 + new. Example: previous 00, new 01 -> index 1 -> -1.
// Jumps that skip a state (contact bounce) give 0 and are ignored.
const int8_t ENC_TABLE[16] = {
   0, -1,  1,  0,   // previous 00 -> new 00, 01, 10, 11
   1,  0,  0, -1,   // previous 01 -> new 00, 01, 10, 11
  -1,  0,  0,  1,   // previous 10 -> new 00, 01, 10, 11
   0,  1, -1,  0    // previous 11 -> new 00, 01, 10, 11
};                  // end of the table
// Running total of steps. "volatile" tells the compiler another task changes
// it, so it must always read the real memory. Only encoderTask writes to it.
volatile int32_t encSteps = 0;

// A small program that runs on its own on core 0 (loop() runs on core 1),
// checking the encoder every millisecond. loop() is busy drawing frames and
// only comes round every few tens of milliseconds, too slow to catch every step.
void encoderTask(void *param) {        // param is unused (FreeRTOS tasks must accept one)
  // Read both legs as two bits. << 1 shifts A left one place.
  // Example: A = 1, B = 0 -> (1 << 1) | 0 = 2 (binary 10).
  uint8_t prev = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);
  while (true) {                       // forever:
    uint8_t now = (digitalRead(ENC_A) << 1) | digitalRead(ENC_B);  // read the current state
    if (now != prev) {                 // it changed:
      // (prev << 2) | now = prev * 4 + now, the table index.
      encSteps = encSteps + ENC_TABLE[(prev << 2) | now];  // add +1, -1 or 0
      prev = now;                      // remember the state for next time
    }                                  // end of change check
    vTaskDelay(1);                     // sleep 1 ms (one FreeRTOS tick), leaving the core free for other work
  }                                    // end of forever
}                                      // end of encoderTask

// Turns raw steps into whole clicks since the last call. Partial clicks carry over.
// Example: 6 new steps at 4 per click -> 1 click now, 2 steps saved for later.
int readEncoderClicks() {
  static int32_t used = 0;             // "static" keeps its value between calls: steps already turned into clicks
  int32_t total = encSteps;            // read the shared total once (it could change while we work)
  int32_t clicks = (total - used) / ENC_STEPS_PER_CLICK;  // whole clicks (integer division drops the remainder)
  used += clicks * ENC_STEPS_PER_CLICK;  // mark those steps as used
  return ENC_REVERSE ? -clicks : clicks;  // flip the sign if the direction is reversed
}                                      // end of readEncoderClicks

// ======================================================================
// 10. THE WHEEL LIST AND SAVED SETTINGS
// ======================================================================
Preferences prefs;                     // the flash-storage object

// Reads the saved preset and grid setting (or defaults on the first run).
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

// ======================================================================
// 11. SAVING PHOTOS
// ======================================================================
// Finds the next unused file name, e.g. /photos/IMG_0008.jpg, and also hands
// back its number through "int &number" (a reference: written into the
// caller's variable). Returns "" if all 9999 names are used.
String nextFileName(const char *folder, const char *prefix, const char *ext, int &number) {
  for (int i = lastPhotoNumber + 1; i < 10000; i++) {  // start after the last known photo, so it stays fast
    char path[40];                     // room for the name (40 characters is plenty)
    // snprintf builds text from a pattern: %s inserts text, %04d inserts a
    // number padded to 4 digits. Example: ("/photos", "IMG_", 8, "jpg") -> "/photos/IMG_0008.jpg".
    // sizeof(path) stops it writing past the end of the 40 characters.
    snprintf(path, sizeof(path), "%s/%s%04d.%s", folder, prefix, i, ext);
    if (!SD_MMC.exists(path)) {        // "!" means "not": this name isn't taken yet
      number = i;                      // hand back the number
      return String(path);             // and the name
    }                                  // end of the check
  }                                    // end of the search
  return "";                           // nothing free
}                                      // end of nextFileName

// Takes and saves one full-size photo, then returns to the preview.
void takePhoto() {
  if (!sdReady) {                      // no card:
    showLabel("NO SD CARD", 0xF800);   //   say so in red
    return;                            //   and stop here
  }                                    // end of the card check
  showMessage("SAVING...", 0);         // full-screen message while we work

  // Switch the camera to full-size JPEG. The camera can't change format
  // while running, so it's stopped (deinit) and started again.
  esp_camera_deinit();                 // stop the preview camera and free its memory
  bool saved = false;                  // becomes true only if the file is really written
  if (startPhotoMode()) {              // start in photo mode; skip everything if that failed
    // Let exposure and white balance settle on the new mode: grab and discard a few frames.
    for (int i = 0; i < PHOTO_WARMUP_FRAMES; i++) {
      camera_fb_t *fb = esp_camera_fb_get();  // take a frame...
      if (fb) esp_camera_fb_return(fb);       // ...and give it straight back (if we got one)
    }                                  // end of warm-up
    camera_fb_t *fb = esp_camera_fb_get();    // the real photo
    if (fb) {                          // got one:
      int number = 0;                  // will receive the photo's number
      String path = nextFileName("/photos", "IMG_", "jpg", number);  // e.g. "/photos/IMG_0008.jpg"
      File f = SD_MMC.open(path, FILE_WRITE);  // create the file for writing
      if (f) {                         // created OK:
        f.write(fb->buf, fb->len);     //   write all the JPEG bytes
        f.close();                     //   close it (this finishes the write)
        lastPhotoNumber = number;      //   remember the number for the label and the next name
        saved = true;                  //   success
        // %u prints an unsigned number. Example: "Saved /photos/IMG_0008.jpg (284512 bytes)"
        Serial.printf("Saved %s (%u bytes)\n", path.c_str(), fb->len);
      } else {                         // couldn't create the file:
        Serial.println("Could not create photo file");  // e.g. card full, or write-protected
      }                                // end of the file check
      esp_camera_fb_return(fb);        // always give the frame buffer back to the camera
    } else {                           // no frame at all:
      Serial.println("Photo capture failed");  // the camera didn't deliver a frame in time
    }                                  // end of the frame check
    esp_camera_deinit();               // stop photo mode
  }                                    // end of photo mode

  // Back to the live preview. If even that fails, there's nothing useful to do, so stop.
  if (!startPreview()) {
    showMessage("CAMERA ERROR", 0);    // say so
    while (true) delay(1000);          // stay here forever (wait 1 s, repeat)
  }                                    // end of the preview check

  if (saved) {                         // tell the user how it went:
    char text[16];                     // room for the label text
    snprintf(text, sizeof(text), "SAVED #%04d", lastPhotoNumber);  // e.g. "SAVED #0008"
    showLabel(text, 0x07E0);           // in green
  } else {                             // not saved:
    showLabel("SAVE FAILED", 0xF800);  // in red
  }                                    // end of the result
}                                      // end of takePhoto

// ======================================================================
// 12. SETUP: runs once at power-on or reset
// ======================================================================
void setup() {
  Serial.begin(115200);                // start the USB serial connection at 115200 baud (match the Serial Monitor)
  delay(2000);                         // wait 2 s so there's time to open the Serial Monitor

  // INPUT_PULLUP turns on a resistor inside the chip that holds the pin HIGH,
  // so a button or encoder only needs to connect the pin to GND. No extra parts.
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // shutter
  pinMode(ENC_A, INPUT_PULLUP);        // encoder leg A
  pinMode(ENC_B, INPUT_PULLUP);        // encoder leg B
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
  if (screenBuf == NULL) {             // couldn't get the memory (e.g. PSRAM not enabled in Tools):
    showMessage("MEMORY ERROR", 0);    //   say so
    while (true) delay(1000);          //   and stop here forever
  }                                    // end of the memory check

  loadSettings();                      // read the saved preset and grid BEFORE starting the camera, so it starts with them
  if (!startPreview()) {               // start the live preview; if it fails:
    showMessage("CAMERA ERROR", 0);    //   say so (the Serial Monitor shows the error code)
    while (true) delay(1000);          //   and stop here forever
  }                                    // end of the camera check

  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);    // tell the SD library which pins the slot uses
  // Mount the card. "/sdcard" is where it appears inside the board's file
  // system; true = 1-bit mode (this slot only has one data line wired).
  sdReady = SD_MMC.begin("/sdcard", true);
  // Make the photos folder if it's missing (only if the card is ready).
  if (sdReady && !SD_MMC.exists("/photos")) SD_MMC.mkdir("/photos");
  Serial.println(sdReady ? "SD card ready" : "No SD card: preview only");  // report the result

  // Start the encoder task:
  //   encoderTask = the function to run, "encoder" = its name (for debugging),
  //   2048 = bytes of memory for its stack, NULL = no parameter,
  //   1 = priority (low), NULL = we don't need a handle to it,
  //   0 = run it on core 0 (loop() runs on core 1).
  xTaskCreatePinnedToCore(encoderTask, "encoder", 2048, NULL, 1, NULL, 0);

  // Starting label: the current preset in yellow, or a red warning if there's no card.
  if (sdReady) showLabel(presets[currentPreset].name, 0xFFE0);
  else         showLabel("NO SD CARD", 0xF800);  // 0xF800 = red
}                                      // end of setup

// ======================================================================
// 13. LOOP: runs over and over, forever, after setup()
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
