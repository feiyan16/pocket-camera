/*
  ======================================================================
  photo.cpp  -  TAKING A PHOTO (the code)
  ======================================================================
  The preview runs in raw-pixel mode, which can't produce a full-size
  photo (it wouldn't fit in memory). So for every photo:
    1. stop the preview camera
    2. restart it in JPEG mode at 1600 x 1200
    3. throw away a few frames while exposure settles
    4. save one frame to the SD card
    5. restart the preview
  ======================================================================
*/

#include "photo.h"      // our own header
#include "config.h"     // PHOTO_WARMUP_FRAMES
#include "camera.h"     // startPhotoMode(), startPreview() and the esp_camera_... functions
#include "sdcard.h"     // sdReady, lastPhotoNumber, nextFileName()
#include "display.h"    // showMessage()
#include "labels.h"     // showLabel()
#include "FS.h"         // the File type used to write the photo
#include "SD_MMC.h"     // SD_MMC.open(), to create the photo file

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
