/*
  ======================================================================
  sdcard.cpp  -  THE microSD CARD (the code)
  ======================================================================
  Mounting the card and choosing photo file names. The writing of the
  photo itself happens in photo.cpp.
  ======================================================================
*/

#include "sdcard.h"     // our own header: SD pins and the promises this file must keep
#include "FS.h"         // "file system" basics: the File type and folder functions
#include "SD_MMC.h"     // the SD card, using the chip's dedicated SD interface (SD_MMC)

// ----------------------------------------------------------------------
// The variables promised with "extern" in sdcard.h are created here.
// ----------------------------------------------------------------------
bool sdReady = false;                 // starts false: no card until startSD() succeeds
int lastPhotoNumber = 0;              // starts at 0: no photo saved yet this session

// ----------------------------------------------------------------------
// Functions promised in sdcard.h
// ----------------------------------------------------------------------
bool startSD() {
  SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0);    // tell the SD library which pins the slot uses
  // Mount the card. "/sdcard" is where it appears inside the board's file
  // system; true = 1-bit mode (this slot only has one data line wired).
  sdReady = SD_MMC.begin("/sdcard", true);
  // Make the photos folder if it's missing (only if the card is ready).
  if (sdReady && !SD_MMC.exists("/photos")) SD_MMC.mkdir("/photos");
  Serial.println(sdReady ? "SD card ready" : "No SD card: preview only");  // report the result
  return sdReady;                       // hand the result back to the caller too
}                                       // end of startSD

// Tries IMG_0001, IMG_0002 ... until it finds a name not already on the card.
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
