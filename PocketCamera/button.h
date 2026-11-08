/*
  ======================================================================
  button.h  -  THE SHUTTER BUTTON (what other files can use)
  ======================================================================
  The 12 x 12 mm tactile button. Two diagonally opposite legs are wired:
  one to GPIO 44, the other to GND. Pressing it connects the pin to GND,
  so the pin reads LOW while pressed.
  ======================================================================
*/

#pragma once            // read this file only once per .cpp file (see config.h)

#include <Arduino.h>    // basic Arduino types and functions

// ----------------------------------------------------------------------
// Pins
// ----------------------------------------------------------------------
#define BUTTON_PIN  44   // shutter button (other leg to GND, so pressed = LOW)

// ----------------------------------------------------------------------
// Functions (the code is in button.cpp)
// ----------------------------------------------------------------------
// Sets up the button pin. Call once in setup().
void startButton();

// True once per press (already debounced). Call it every loop.
// Example: if (buttonPressed()) takePhoto();
bool buttonPressed();
