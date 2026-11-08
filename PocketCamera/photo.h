/*
  ======================================================================
  photo.h  -  TAKING A PHOTO (what other files can use)
  ======================================================================
  Taking a photo needs the camera, the SD card, the screen and labels
  together, so it gets its own file instead of living in any one of them.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

// Takes and saves one full-size photo (1600 x 1200 JPEG), then returns to
// the live preview. Shows "SAVED #0008" in green, or a red error label.
// Takes roughly 1-2 seconds (an estimate; it depends on the card and scene).
void takePhoto();
