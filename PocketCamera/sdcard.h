/*
  ======================================================================
  sdcard.h  -  THE microSD CARD (what other files can use)
  ======================================================================
  The microSD slot on the nulllab board, used through the chip's
  dedicated SD interface (SD_MMC) in 1-bit mode. Photos are saved as
  /photos/IMG_0001.jpg, IMG_0002.jpg, and so on.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types, and the String type used by nextFileName()

// ----------------------------------------------------------------------
// Pins
// ----------------------------------------------------------------------
// These three are wired to the microSD slot on the board itself; they can't change.
#define SD_CLK      39   // SD clock
#define SD_CMD      38   // SD command line
#define SD_D0       40   // SD data line 0 (1-bit mode uses only one data line)

// ----------------------------------------------------------------------
// Shared variables (created in sdcard.cpp; "extern" explained in display.h)
// ----------------------------------------------------------------------
extern bool sdReady;          // true once the SD card is mounted and usable
extern int lastPhotoNumber;   // number of the last saved photo (0 = none yet)

// ----------------------------------------------------------------------
// Functions (the code is in sdcard.cpp)
// ----------------------------------------------------------------------
// Mounts the card and creates the /photos folder if needed. Sets sdReady
// and also returns it (true = card ready). Call once in setup().
bool startSD();

// Finds the next unused file name, e.g. /photos/IMG_0008.jpg, and also hands
// back its number through "int &number" (a reference: written into the
// caller's variable). Returns "" if all 9999 names are used.
// Example: nextFileName("/photos", "IMG_", "jpg", n) -> "/photos/IMG_0008.jpg", n = 8
String nextFileName(const char *folder, const char *prefix, const char *ext, int &number);
