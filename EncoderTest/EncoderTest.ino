/*
  ======================================================================
  ENCODER TEST
  ======================================================================
  Checks the 3-pin mouse-wheel encoder on GPIO 47 (A) and GPIO 46 (B).
  Open the Serial Monitor at 115200 baud and turn the wheel slowly.
  Every time either leg changes, it prints the two readings.

  How to read the output:
    - Nothing changes (stays A=1 B=1):  the encoder isn't connected properly,
                                        or the common leg isn't on GND
    - Only A or only B changes:         one signal leg isn't reaching its pin
    - Both change, 4 lines per click:   working; ENC_STEPS_PER_CLICK = 4
    - Both change, 2 lines per click:   working; set ENC_STEPS_PER_CLICK = 2
    - Stays at A=0 B=0 however you turn: something holds both pins at GND
                                        (e.g. a solder bridge to the common leg)
  Many mouse encoders rest with both contacts closed, so A=0 B=0 at rest
  is normal, as long as the values change when you turn.

  Quick pin check without the encoder: briefly touch a jumper wire from
  GPIO 47 (or 46) to GND. It should show A=0 (or B=0), then 1 when released.

  Wiring
    common leg -> GND, signal legs -> GPIO 47 and GPIO 46
    (the two wide bent legs at the edges are mounting tabs: not wired)
  ======================================================================
*/

#define ENC_A 47   // encoder signal leg A
#define ENC_B 46   // encoder signal leg B

void setup() {                                   // runs once at power-on
  Serial.begin(115200);                          // USB serial at 115200 baud
  delay(1500);                                   // a moment to open the Serial Monitor
  // INPUT_PULLUP: a resistor inside the chip holds the pin HIGH (1) until the
  // encoder connects it to GND, which makes it read LOW (0).
  pinMode(ENC_A, INPUT_PULLUP);                  // leg A
  pinMode(ENC_B, INPUT_PULLUP);                  // leg B
  Serial.println("Turn the wheel slowly...");    // instructions
}                                                // end of setup

void loop() {                                    // runs forever, as fast as it can
  // "static" keeps the value between passes through loop(); -1 means "nothing yet",
  // so the very first reading is always printed.
  static int last = -1;
  int a = digitalRead(ENC_A);                    // 1 = open, 0 = connected to GND
  int b = digitalRead(ENC_B);                    // same for leg B
  // Combine both readings into one number: A is shifted left one bit.
  // Example: a = 1, b = 0 -> (1 << 1) | 0 = 2 (binary 10).
  int state = (a << 1) | b;
  if (state != last) {                           // print only when something changed
    Serial.printf("A=%d B=%d\n", a, b);          // e.g. "A=1 B=0"
    last = state;                                // remember it for the next pass
  }                                              // end of the change check
}                                                // end of loop
