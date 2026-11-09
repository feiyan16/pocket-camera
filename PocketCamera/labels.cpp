/*
  ======================================================================
  labels.cpp  -  ON-SCREEN TEXT (the code)
  ======================================================================
  Turns text into a pixel mask (letters + outline), converts it from
  text coordinates to camera coordinates, and switches labels on and off.
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
// One global Label, starting switched off with every field 0.
Label label = {false, 0, 0, 0, 0, 0, 0};
// Filter settings labels
Label mainLabel = {false, 0, 0, 0, 0, 0, 0};
Label prevLabel = {false, 0, 0, 0, 0, 0, 0};
Label nextLabel = {false, 0, 0, 0, 0, 0, 0};

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
}    

// The label is converted to camera coordinates here, once; showFrame() then
// paints it into every frame while it's active.
void _showLabel(Label *label, uint16_t color, int w, int tx0, int ty0) {
  // Find the label's rectangle in camera coordinates by converting two opposite corners.
  int ax, ay, bx, by;                             // the two converted corners
  textPixelToCamera(tx0, ty0, ax, ay);            // top-left corner of the text
  textPixelToCamera(tx0 + w - 1, ty0 + MASK_H - 1, bx, by);  // bottom-right corner of the text
  // After rotating, either corner could be the smaller one, so take min and max.
  label->cx0 = min(ax, bx);  label->cx1 = max(ax, bx) + 1;     // +1 because x1 is "just past" the edge
  label->cy0 = min(ay, by);  label->cy1 = max(ay, by) + 1;     // same for y
  int cw = label->cx1 - label->cx0;                 // width of the rectangle in camera coordinates

  // Copy every mask pixel to its spot in the camera-coordinate mask.
  for (int y = 0; y < MASK_H; y++) {              // every row of the text mask...
    for (int x = 0; x < w; x++) {                 // ...and every column
      int cx, cy;                                 // where this pixel lands on the camera picture
      textPixelToCamera(tx0 + x, ty0 + y, cx, cy);  // convert its screen position
      // camMask is one long row-after-row list, so position (column, row)
      // is at row * width + column. Example: row 2, column 5, width 18 -> 2*18+5 = 41.
      label->mask[(cy - label->cy0) * cw + (cx - label->cx0)] = textMask[y][x];
    }                                             // end of columns
  }                                               // end of rows
  label->color = color;                            // remember the letter colour
  label->shownAt = millis();                       // millis() = milliseconds since the board started
  label->active = true;                            // switch it on: showFrame() starts painting it
}                                                 // end of showLabel

void showLabel(const char *text, uint16_t color) {
  int w = buildTextMask(text);                    // draw the words into textMask; w = their width
  int tx0 = (TXT_W - w) / 2;                      // left edge, centred across the screen (text coordinates)
  int ty0 = TXT_H - LABEL_BOTTOM_GAP - MASK_H;    // top edge, LABEL_BOTTOM_GAP pixels up from the bottom
  _showLabel(&label, color, w, tx0, ty0);
}

void showPresetMenu(const char *main_text, const char *prev_text, const char *next_text) {  
  int main_w = buildTextMask(main_text);         
  int main_tx0 = TXT_W - (10 + main_w); // 10 is an estimate, need to measure width from cover edge and star pointy bit
  int main_ty0 = TXT_H - (140 + MASK_H); // 140 is an estimate, need to measure where the star pointy bit actually points to
  _showLabel(&mainLabel, 0xFFE0, main_w, main_tx0, main_ty0);

  int prev_w = buildTextMask(prev_text);
  int prev_tx0 = TXT_W - (10 + prev_w)
  int prev_ty0 = main_ty0 - (8 + MASK_H);
  _showLabel(&prevLabel, 0xC618, prev_w, prev_tx0, prev_ty0);

  int next_w = buildTextMask(next_text);
  int next_tx0 = TXT_W - (10 + next_w)
  int next_ty0 = main_ty0 + (8 + MASK_H);  
  _showLabel(&nextLabel, 0xC618, next_w, next_tx0, next_ty0);
}

// Switches the label off. Nothing needs erasing: the next frame simply doesn't include it.
void hideLabel() {
  label.active = false;   // switch the label off; showFrame() stops painting it
}                                                 // end of hideLabel

void hidePresetMenu() {
  Label *labels[] = {&mainLabel, &prevLabel, &nextLabel}
  for (int i = 0; i < 3; i++) {
    labels[i]->active = false;
  }
}
