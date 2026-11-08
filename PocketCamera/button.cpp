/*
  ======================================================================
  button.cpp  -  THE SHUTTER BUTTON (the code)
  ======================================================================
  Reads the button with "debouncing". Button contacts "bounce" (flicker
  on/off for a few milliseconds) when pressed; debouncing waits until
  the reading is steady, so one press counts as exactly one press.
  ======================================================================
*/

#include "button.h"     // our own header: BUTTON_PIN and the promises this file must keep

// ----------------------------------------------------------------------
// Types and variables used only in this file (so they aren't in button.h)
// ----------------------------------------------------------------------
// A debounced input.
struct Debounced {
  uint8_t pin;          // which GPIO
  bool stable;          // the last accepted (steady) reading
  bool candidate;       // a new reading we are timing
  unsigned long since;  // when that candidate first appeared
};                      // end of the struct

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

// ----------------------------------------------------------------------
// Functions promised in button.h
// ----------------------------------------------------------------------
void startButton() {
  // INPUT_PULLUP turns on a resistor inside the chip that holds the pin HIGH,
  // so the button only needs to connect the pin to GND. No extra parts.
  pinMode(BUTTON_PIN, INPUT_PULLUP);   // shutter
}                                       // end of startButton

// "&&" only checks the second part if the first is true, and the first part
// has to run every time to keep the timer updated.
bool buttonPressed() {
  return settledChange(buttonIn, 30) && buttonIn.stable == LOW;  // a settled change that is a press (LOW)
}                                       // end of buttonPressed
