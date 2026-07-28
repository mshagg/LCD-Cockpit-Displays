// ----------------------------------------------------
// Boot splash screen
// ----------------------------------------------------
//
// Vector-drawn INAV-style splash based on the supplied
// reference image.
//
// Display orientation expected:
//   320 x 240 landscape
//
// This splash is shown once at boot and is not selectable.
// Public entry point:
//   drawBootSplashScreen(Arduino_GFX *display)
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>

// ----------------------------------------------------
// Colours - precomputed RGB565 values
// ----------------------------------------------------

static const uint16_t BS_BLACK        = 0x0000;
static const uint16_t BS_WHITE        = 0xFFFF;
static const uint16_t BS_OFF_WHITE    = 0xF7DE;

static const uint16_t BS_BLUE         = 0x2D9D;
static const uint16_t BS_BLUE_LIGHT   = 0x5DFF;
static const uint16_t BS_BLUE_DARK    = 0x1578;

static const uint16_t BS_GREY         = 0x5AEB;
static const uint16_t BS_GREY_DARK    = 0x3186;
static const uint16_t BS_GREY_LIGHT   = 0x9CF3;

// ----------------------------------------------------
// Geometry
// ----------------------------------------------------

static const int BS_SCREEN_W = 320;
static const int BS_SCREEN_H = 240;

static const int BS_CX = 160;
static const int BS_CY = 120;

static const int BS_CENTER_OUTER_R = 78;
static const int BS_CENTER_RING_R  = 74;
static const int BS_CENTER_WHITE_R = 69;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static void bsDrawThickLine(
  Arduino_GFX *display,
  int x1,
  int y1,
  int x2,
  int y2,
  uint16_t colour
) {
  display->drawLine(x1, y1, x2, y2, colour);
  display->drawLine(x1 + 1, y1, x2 + 1, y2, colour);
  display->drawLine(x1 - 1, y1, x2 - 1, y2, colour);
  display->drawLine(x1, y1 + 1, x2, y2 + 1, colour);
  display->drawLine(x1, y1 - 1, x2, y2 - 1, colour);
}

static void bsDrawThickCircle(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  uint16_t colour
) {
  display->drawCircle(cx, cy, r, colour);
  display->drawCircle(cx, cy, r - 1, colour);
  display->drawCircle(cx, cy, r + 1, colour);
  display->drawCircle(cx, cy, r - 2, colour);
  display->drawCircle(cx, cy, r + 2, colour);
}

static void bsDrawHorizontalBand(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  bool leftSide
) {
  int radius = h / 2;

  // Outer dark outline.
  display->fillRoundRect(
    x,
    y,
    w,
    h,
    radius,
    BS_GREY_DARK
  );

  // Main blue fill.
  display->fillRoundRect(
    x + 3,
    y + 3,
    w - 6,
    h - 6,
    radius - 3,
    BS_BLUE
  );

  // Highlight on the left half of each strip.
  if (leftSide) {
    display->fillRoundRect(
      x + 6,
      y + 5,
      w / 2,
      h - 10,
      radius - 5,
      BS_BLUE_LIGHT
    );
  } else {
    display->fillRoundRect(
      x + 6,
      y + 5,
      w / 2,
      h - 10,
      radius - 5,
      BS_BLUE_LIGHT
    );
  }

  // Repaint most of the strip with normal blue so the
  // highlight stays subtle rather than becoming a block.
  display->fillRect(
    x + (w / 3),
    y + 5,
    (w * 2) / 3 - 8,
    h - 10,
    BS_BLUE
  );

  // Angled inner cut near the centre circle.
  // This mimics the diagonal notches in the reference.
  if (leftSide) {
    display->fillTriangle(
      x + w - 26,
      y + h - 2,
      x + w,
      y + h - 2,
      x + w,
      y + 2,
      BS_BLACK
    );

    display->drawLine(
      x + w - 27,
      y + h - 2,
      x + w - 1,
      y + 2,
      BS_GREY_DARK
    );
  } else {
    display->fillTriangle(
      x,
      y + 2,
      x + 26,
      y + 2,
      x,
      y + h - 2,
      BS_BLACK
    );

    display->drawLine(
      x + 1,
      y + h - 2,
      x + 27,
      y + 2,
      BS_GREY_DARK
    );
  }
}

static void bsDrawSideBands(Arduino_GFX *display)
{
  // Left side bands.
  bsDrawHorizontalBand(display, 8,   23, 132, 42, true);
  bsDrawHorizontalBand(display, 30,  95, 108, 42, true);
  bsDrawHorizontalBand(display, 56, 166,  86, 42, true);

  // Right side bands.
  bsDrawHorizontalBand(display, 180, 23, 132, 42, false);
  bsDrawHorizontalBand(display, 182, 95, 108, 42, false);
  bsDrawHorizontalBand(display, 178, 166, 86, 42, false);
}

static void bsDrawRotor(
  Arduino_GFX *display,
  int cx,
  int cy
) {
  bsDrawThickCircle(display, cx, cy, 22, BS_GREY);

  display->fillCircle(cx, cy, 7, BS_GREY);
  display->drawCircle(cx, cy, 8, BS_GREY_DARK);
}

static void bsDrawDroneMark(Arduino_GFX *display)
{
  // Arms.
  bsDrawThickLine(display, 141, 91, 157, 107, BS_GREY);
  bsDrawThickLine(display, 179, 91, 163, 107, BS_GREY);
  bsDrawThickLine(display, 141, 149, 157, 133, BS_GREY);
  bsDrawThickLine(display, 179, 149, 163, 133, BS_GREY);

  // Rotors.
  bsDrawRotor(display, 139, 83);
  bsDrawRotor(display, 181, 83);
  bsDrawRotor(display, 139, 157);
  bsDrawRotor(display, 181, 157);

  // Clean small white gaps where the pin overlaps the arms.
  display->fillCircle(BS_CX, BS_CY, 25, BS_OFF_WHITE);

  // Blue map pin.
  display->fillCircle(BS_CX, BS_CY - 7, 22, BS_BLUE);
  display->fillTriangle(
    BS_CX - 18,
    BS_CY + 5,
    BS_CX + 18,
    BS_CY + 5,
    BS_CX,
    BS_CY + 36,
    BS_BLUE
  );

  display->drawCircle(BS_CX, BS_CY - 7, 22, BS_BLUE_DARK);
  display->drawLine(BS_CX - 18, BS_CY + 5, BS_CX, BS_CY + 36, BS_BLUE_DARK);
  display->drawLine(BS_CX + 18, BS_CY + 5, BS_CX, BS_CY + 36, BS_BLUE_LIGHT);

  // Pin hole.
  display->fillCircle(BS_CX, BS_CY - 7, 12, BS_OFF_WHITE);
}

static void bsDrawCentreCircle(Arduino_GFX *display)
{
  // Outer black / grey ring.
  display->fillCircle(BS_CX, BS_CY, BS_CENTER_OUTER_R, BS_BLACK);
  display->drawCircle(BS_CX, BS_CY, BS_CENTER_OUTER_R, BS_GREY_DARK);
  display->drawCircle(BS_CX, BS_CY, BS_CENTER_OUTER_R - 1, BS_GREY_DARK);

  // Main white face.
  display->fillCircle(BS_CX, BS_CY, BS_CENTER_RING_R, BS_GREY);
  display->fillCircle(BS_CX, BS_CY, BS_CENTER_WHITE_R, BS_OFF_WHITE);

  // Subtle rim.
  display->drawCircle(BS_CX, BS_CY, BS_CENTER_WHITE_R, BS_GREY_LIGHT);
}

// ----------------------------------------------------
// Public splash entry point
// ----------------------------------------------------

void drawBootSplashScreen(Arduino_GFX *display)
{
  display->fillScreen(BS_BLACK);

  bsDrawSideBands(display);
  bsDrawCentreCircle(display);
  bsDrawDroneMark(display);
}