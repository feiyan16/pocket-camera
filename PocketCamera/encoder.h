/*
  ======================================================================
  encoder.h  -  THE SCROLL WHEEL (what other files can use)
  ======================================================================
  A 3-pin mouse-wheel rotary encoder. The middle (common) leg goes to
  GND, the two signal legs to GPIO 47 and GPIO 46. The two wide bent legs
  at the edges are mounting tabs and are not wired.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types and functions

// ----------------------------------------------------------------------
// Pins
// ----------------------------------------------------------------------
#define ENC_A       47   // encoder signal leg A
#define ENC_B       46   // encoder signal leg B (moved here from GPIO 48, which didn't respond)

// ----------------------------------------------------------------------
// Functions (the code is in encoder.cpp)
// ----------------------------------------------------------------------
// Sets up both pins and starts a background task that watches the wheel.
// Call once in setup().
void startEncoder();

// Whole clicks turned since the last call: positive one way, negative the
// other, 0 if the wheel hasn't moved. Example: if (readEncoderClicks() == 1) ...
int readEncoderClicks();
