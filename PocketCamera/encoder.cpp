/*
  ======================================================================
  encoder.cpp  -  THE SCROLL WHEEL (the code)
  ======================================================================
  A small background task on core 0 watches both legs every millisecond
  and keeps a running count of steps. loop() asks for whole clicks.
  ======================================================================
*/

#include "encoder.h"    // our own header: ENC_A / ENC_B and the promises this file must keep
#include "config.h"     // ENC_STEPS_PER_CLICK and ENC_REVERSE

// ----------------------------------------------------------------------
// Data used only in this file (so it isn't in encoder.h)
// ----------------------------------------------------------------------
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

// ----------------------------------------------------------------------
// Functions promised in encoder.h
// ----------------------------------------------------------------------
void startEncoder() {
  // INPUT_PULLUP turns on a resistor inside the chip that holds the pin HIGH,
  // so the encoder only needs to connect the pin to GND. No extra parts.
  pinMode(ENC_A, INPUT_PULLUP);        // encoder leg A
  pinMode(ENC_B, INPUT_PULLUP);        // encoder leg B
  // Start the encoder task:
  //   encoderTask = the function to run, "encoder" = its name (for debugging),
  //   2048 = bytes of memory for its stack, NULL = no parameter,
  //   1 = priority (low), NULL = we don't need a handle to it,
  //   0 = run it on core 0 (loop() runs on core 1).
  xTaskCreatePinnedToCore(encoderTask, "encoder", 2048, NULL, 1, NULL, 0);
}                                      // end of startEncoder

// Turns raw steps into whole clicks since the last call. Partial clicks carry over.
// Example: 6 new steps at 4 per click -> 1 click now, 2 steps saved for later.
int readEncoderClicks() {
  static int32_t used = 0;             // "static" keeps its value between calls: steps already turned into clicks
  int32_t total = encSteps;            // read the shared total once (it could change while we work)
  int32_t clicks = (total - used) / ENC_STEPS_PER_CLICK;  // whole clicks (integer division drops the remainder)
  used += clicks * ENC_STEPS_PER_CLICK;  // mark those steps as used
  return ENC_REVERSE ? -clicks : clicks;  // flip the sign if the direction is reversed
}                                      // end of readEncoderClicks
