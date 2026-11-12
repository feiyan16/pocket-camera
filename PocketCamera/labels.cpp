/*
  ======================================================================
  labels.cpp  -  ON-SCREEN TEXT (the code)
  ======================================================================
  Turns text into a pixel mask (letters + outline), converts it from
  text coordinates to camera coordinates, and switches labels on and off.

  Four labels exist: the bottom label, and a three-line preset menu on
  the right (prev / main / next). Each has its own camMask, so they can
  all be on screen at once.
  ======================================================================
*/

#include "labels.h"     // our own header: Label, the masks, MASK_W / MASK_H
#include "config.h"     // LABEL_MAP, LABEL_BOTTOM_GAP, PANEL_W / PANEL_H
#include "display.h"    // TXT_W / TXT_H: the screen size in the text rotation
#include <string.h>     // strlen(), strchr(), memset() (C text and memory functions)
#include <ctype.h>      // toupper() (C character functions)

// ----------------------------------------------------------------------
// The variables promised with "extern" in labels.h are created here.
// ----------------------------------------------------------------------
// "= {}" starts every field at 0 / false, including the camMask array.
// (Listing only some fields, like {false, 0, 0 ...}, also works but makes
// the compiler warn that camMask has no starting value.)
Label label = {};       // the bottom label
Label mainLabel = {};   // preset menu: selected item
Label prevLabel = {};   // preset menu: item above
Label nextLabel = {};   // preset menu: item below
// The addresses of all four, in one list. "&" = "address of".
Label *allLabels[NUM_LABELS] = {&label, &mainLabel, &prevLabel, &nextLabel};

// The shared scratch mask (starts all 0). Only one is needed: each label's
// text is drawn here and copied into that label's camMask straight away.
uint8_t textMask[MASK_H][MASK_W];

// ----------------------------------------------------------------------
// Helpers used only in this file (so they aren't listed in labels.h)
// ----------------------------------------------------------------------
// Finds which row of FONT_BITS holds a character.
// Example: 'A' is at position 15 in FONT_CHARS, so it returns 15.
int glyphIndex(char c) {               // c = the character to look up
  c = toupper(c);                      // 'a' becomes 'A' (the font only has capitals)
  const char *p = strchr(FONT_CHARS, c);  // strchr finds c in the string; gives its address, or NULL if missing
  if (p == NULL || c == '\0') return 0;   // not in the font (or the end-of-text marker): use row 0, a space
  return p - FONT_CHARS;               // address of c minus address of the start = its position
}                                      // end of glyphIndex

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

// Draws text into the scratch textMask, then copies it into ONE label at
// (tx0, ty0) in text coordinates, and switches that label on.
// "Label *l" = the address of the label to fill, e.g. &mainLabel.
// outline = true: dark outline around the letters; false: letters only.
// Example: placeLabel(&mainLabel, "CLOUDY", 0xFFFF, 191, 56, true).
void placeLabel(Label *l, const char *text, uint16_t color, int tx0, int ty0, bool outline) {
  int w = buildTextMask(text);                 // draw the words into textMask; w = their width
  // Keep the label fully on screen. constrain(value, low, high) clamps a number.
  // Example: tx0 = -5 becomes 0; a label that would stick out of the bottom is pushed up.
  // Without this, part of the label would land outside the picture and be cut off.
  tx0 = constrain(tx0, 0, TXT_W - w);             // left edge: never off the left or right side
  ty0 = constrain(ty0, 0, TXT_H - MASK_H);       // same for the top edge. Example: 230 becomes 222 (240 - 18) so it isn't cut off

  // Find the label's rectangle in camera coordinates by converting two opposite corners.
  int ax, ay, bx, by;                             // the two converted corners
  textPixelToCamera(tx0, ty0, ax, ay);            // top-left corner of the text
  textPixelToCamera(tx0 + w - 1, ty0 + MASK_H - 1, bx, by);  // bottom-right corner of the text
  // After rotating, either corner could be the smaller one, so take min and max.
  // "l->cx0" = the cx0 field of the label that l points to.
  l->cx0 = min(ax, bx);  l->cx1 = max(ax, bx) + 1;     // +1 because x1 is "just past" the edge
  l->cy0 = min(ay, by);  l->cy1 = max(ay, by) + 1;     // same for y
  int cw = l->cx1 - l->cx0;                       // width of the rectangle in camera coordinates

  // Copy every mask pixel to its spot in THIS label's camera-coordinate mask.
  for (int y = 0; y < MASK_H; y++) {              // every row of the text mask...
    for (int x = 0; x < w; x++) {                 // ...and every column
      int cx, cy;                                 // where this pixel lands on the camera picture
      textPixelToCamera(tx0 + x, ty0 + y, cx, cy);  // convert its screen position
      // camMask is one long row-after-row list, so position (column, row)
      // is at row * width + column. Example: row 2, column 5, width 18 -> 2*18+5 = 41.
      uint8_t v = textMask[y][x];                 // 0 = see-through, 1 = outline, 2 = letter
      if (!outline && v == 1) v = 0;              // no outline wanted: outline pixels become see-through
      l->camMask[(cy - l->cy0) * cw + (cx - l->cx0)] = v;  // store it in this label's own mask
    }                                             // end of columns
  }                                               // end of rows
  l->color = color;                               // remember the letter colour
  l->shownAt = millis();                          // millis() = milliseconds since the board started
  l->active = true;                               // switch it on: showFrame() starts painting it
}                                                 // end of placeLabel

// ----------------------------------------------------------------------
// Functions promised in labels.h
// ----------------------------------------------------------------------
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

// Same width formula as buildTextMask(), without drawing anything, so callers
// can position a label before it's built.
int labelWidth(const char *text) {
  return min((int)strlen(text), MAX_LABEL_CHARS) * FONT_ADV + 2;  // letters x 13 + 2 for the outline
}                                                     // end of labelWidth

// The bottom label: centred across, LABEL_BOTTOM_GAP pixels up from the bottom.
void showLabel(const char *text, uint16_t color) {
  int tx0 = (TXT_W - labelWidth(text)) / 2;       // left edge, centred. Example: (280 - 67) / 2 = 106
  int ty0 = TXT_H - LABEL_BOTTOM_GAP - MASK_H;    // top edge. Example: 240 - 12 - 18 = 210
  placeLabel(&label, text, color, tx0, ty0, true);  // build it, convert it, switch it on (with outline)
}                                                 // end of showLabel

// The three-line menu, right-aligned: every line's RIGHT edge is
// MENU_RIGHT_GAP pixels from the screen edge, whatever its length.
void showPresetMenu(const char *main_text, const char *prev_text, const char *next_text) {
  int step = MASK_H + MENU_LINE_GAP;              // distance between lines. Example: 18 + 8 = 26 px
  int main_ty0 = TXT_H - (MENU_FROM_BOTTOM + MASK_H);  // selected item's top edge. Example: 240 - (166 + 18) = 56
  // Each line uses ITS OWN width, so all three line up on the right.
  // Example: "CLOUDY" is 80 px wide -> left edge 280 - (10 + 80) = 190.
  placeLabel(&mainLabel, main_text, 0xFFFF, TXT_W - (MENU_RIGHT_GAP + labelWidth(main_text)), main_ty0, true);          // white with outline, middle
  placeLabel(&prevLabel, prev_text, 0xC618, TXT_W - (MENU_RIGHT_GAP + labelWidth(prev_text)), main_ty0 - step, false);  // light grey, no outline, above
  placeLabel(&nextLabel, next_text, 0xC618, TXT_W - (MENU_RIGHT_GAP + labelWidth(next_text)), main_ty0 + step, false);  // light grey, no outline, below
}                                                 // end of showPresetMenu

// Switches the label off. Nothing needs erasing: the next frame simply doesn't include it.
void hideLabel() {
  label.active = false;   // switch the label off; showFrame() stops painting it
}                                                 // end of hideLabel

// Switches the three menu labels off.
void hidePresetMenu() {
  mainLabel.active = false;   // the real labels (not copies), so they really disappear
  prevLabel.active = false;   // the line above disappears too
  nextLabel.active = false;   // and the line below
}                                                 // end of hidePresetMenu
