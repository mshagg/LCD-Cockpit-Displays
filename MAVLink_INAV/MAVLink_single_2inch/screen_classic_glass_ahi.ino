// ----------------------------------------------------
// Screen design: classic glass cockpit AHI
// ----------------------------------------------------
//
// Full-screen landscape artificial horizon for the
// standalone ESP32-S3 + 2.0 inch ST7789VW fork.
//
// Display orientation expected:
//   320 x 240 landscape
//
// This version:
//   - clean central AHI
//   - transparent-style speed tape on left edge
//   - transparent-style altitude tape on right edge
//   - larger KMH and M tape labels
//   - current speed box hard against left edge
//   - current altitude box hard against right edge
//   - roll arc moved upward
//   - fixed yellow roll pointer near top edge
//   - moving cyan roll marker
//   - bottom boxed telemetry:
//       SAT  hard left
//       RSSI centred
//       CELL hard right
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include "fast_trig.h"

// ----------------------------------------------------
// External MAVLink telemetry
// ----------------------------------------------------

extern float airspeed;
extern float altitude_msl;
extern bool mavlinkVfrHudValid;

extern uint8_t gpsSatellitesVisible;
extern bool mavlinkGpsValid;

extern uint8_t rssiPercent;
extern bool rssiValid;

extern float batteryCellVoltage;
extern bool batteryVoltageValid;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t CG_BLACK       = RGB565(0, 0, 0);
static const uint16_t CG_WHITE       = RGB565(245, 245, 245);
static const uint16_t CG_SOFT_WHITE  = RGB565(180, 180, 180);

static const uint16_t CG_SKY         = RGB565(35, 95, 165);
static const uint16_t CG_SKY_DARK    = RGB565(15, 45, 95);
static const uint16_t CG_GROUND      = RGB565(125, 72, 32);
static const uint16_t CG_GROUND_DARK = RGB565(82, 45, 22);

static const uint16_t CG_YELLOW      = RGB565(255, 210, 0);
static const uint16_t CG_CYAN        = RGB565(0, 210, 255);
static const uint16_t CG_GREEN       = RGB565(0, 230, 90);
static const uint16_t CG_RED         = RGB565(255, 60, 50);

static const uint16_t CG_TAPE_EDGE   = RGB565(210, 210, 210);
static const uint16_t CG_TAPE_SHADOW = RGB565(8, 12, 18);
static const uint16_t CG_TAPE_BOX    = RGB565(3, 9, 16);

// ----------------------------------------------------
// Constants
// ----------------------------------------------------

static const float CG_RAD_TO_DEG = 57.2957795f;
static const float CG_DEG_TO_RAD = 0.0174532925f;

static const float CG_PITCH_PIXELS_PER_DEG = 3.0f;

static const int CG_AIRCRAFT_WING_SPAN = 86;
static const int CG_AIRCRAFT_GAP       = 13;

static const int CG_TAPE_WIDTH = 43;

// Roll arc geometry.
static const int CG_ROLL_CENTER_Y = 112;
static const int CG_ROLL_POINTER_TOP_Y = 4;
static const int CG_ROLL_POINTER_BASE_Y = 17;

static const float CG_SPEED_SCALE = 3.6f;       // m/s to km/h
static const float CG_SPEED_PIXELS_PER_UNIT = 2.0f;
static const int CG_SPEED_MINOR_STEP = 10;
static const int CG_SPEED_MAJOR_STEP = 20;

static const float CG_ALT_PIXELS_PER_UNIT = 0.6f;
static const int CG_ALT_MINOR_STEP = 25;
static const int CG_ALT_MAJOR_STEP = 50;

static const int CG_BOTTOM_BOX_W = 72;
static const int CG_BOTTOM_BOX_H = 34;

// ----------------------------------------------------
// Helper functions
// ----------------------------------------------------

static void cgRotatedPoint(
  int cx,
  int cy,
  float angleRad,
  float localX,
  float localY,
  int *screenX,
  int *screenY
) {
  float s;
  float c;
  fastSinCosRad(angleRad, &s, &c);

  *screenX = (int)roundf((float)cx + localX * c - localY * s);
  *screenY = (int)roundf((float)cy + localX * s + localY * c);
}

static void cgDrawRotatedLine(
  Arduino_GFX *display,
  int cx,
  int cy,
  float angleRad,
  float x1,
  float y1,
  float x2,
  float y2,
  uint16_t colour
) {
  int sx1;
  int sy1;
  int sx2;
  int sy2;

  cgRotatedPoint(cx, cy, angleRad, x1, y1, &sx1, &sy1);
  cgRotatedPoint(cx, cy, angleRad, x2, y2, &sx2, &sy2);

  display->drawLine(sx1, sy1, sx2, sy2, colour);
}

static uint8_t cgBestTextSizeForBox(
  Arduino_GFX *display,
  const char *text,
  int maxWidth
) {
  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;

  display->setTextSize(2);
  display->getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);

  if ((int)textW <= maxWidth) {
    return 2;
  }

  return 1;
}

static void cgPrintCentered(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  const char *text,
  uint8_t textSize,
  uint16_t colour,
  uint16_t bg
) {
  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;

  display->setTextSize(textSize);
  display->getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);

  int drawX = x + ((w - (int)textW) / 2);

  if (drawX < x + 1) {
    drawX = x + 1;
  }

  display->setTextColor(colour, bg);
  display->setCursor(drawX, y);
  display->print(text);
}

static void cgPrintTapeText(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text
) {
  display->setTextSize(1);

  display->setTextColor(CG_TAPE_SHADOW);
  display->setCursor(x + 1, y + 1);
  display->print(text);

  display->setTextColor(CG_WHITE);
  display->setCursor(x, y);
  display->print(text);
}

static void cgPrintLargeTapeLabel(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text
) {
  display->setTextSize(2);

  display->setTextColor(CG_TAPE_SHADOW);
  display->setCursor(x + 1, y + 1);
  display->print(text);

  display->setTextColor(CG_CYAN);
  display->setCursor(x, y);
  display->print(text);
}

// ----------------------------------------------------
// Colour helpers
// ----------------------------------------------------

static uint16_t cgSatColour()
{
  if (!mavlinkGpsValid) {
    return CG_RED;
  }

  if (gpsSatellitesVisible >= 6) {
    return CG_GREEN;
  }

  if (gpsSatellitesVisible >= 4) {
    return CG_YELLOW;
  }

  return CG_RED;
}

static uint16_t cgRssiColour()
{
  if (!rssiValid) {
    return CG_RED;
  }

  if (rssiPercent >= 60) {
    return CG_GREEN;
  }

  if (rssiPercent >= 35) {
    return CG_YELLOW;
  }

  return CG_RED;
}

static uint16_t cgCellColour()
{
  if (!batteryVoltageValid) {
    return CG_RED;
  }

  if (batteryCellVoltage >= 3.60f) {
    return CG_GREEN;
  }

  if (batteryCellVoltage >= 3.40f) {
    return CG_YELLOW;
  }

  return CG_RED;
}

// ----------------------------------------------------
// Horizon
// ----------------------------------------------------

static void cgDrawHorizonBackground(
  Arduino_GFX *display,
  float rollRad,
  float pitchDeg
) {
  int w = display->width();
  int h = display->height();

  int cx = w / 2;
  int cy = h / 2;

  float horizonAngle = -rollRad;
  float pitchOffset = pitchDeg * CG_PITCH_PIXELS_PER_DEG;

  display->fillScreen(CG_SKY);
  display->fillRect(0, 0, w, 38, CG_SKY_DARK);

  const float big = 700.0f;

  int x1;
  int y1;
  int x2;
  int y2;
  int x3;
  int y3;
  int x4;
  int y4;

  cgRotatedPoint(cx, cy, horizonAngle, -big, pitchOffset,       &x1, &y1);
  cgRotatedPoint(cx, cy, horizonAngle,  big, pitchOffset,       &x2, &y2);
  cgRotatedPoint(cx, cy, horizonAngle,  big, pitchOffset + big, &x3, &y3);
  cgRotatedPoint(cx, cy, horizonAngle, -big, pitchOffset + big, &x4, &y4);

  display->fillTriangle(x1, y1, x2, y2, x3, y3, CG_GROUND);
  display->fillTriangle(x1, y1, x3, y3, x4, y4, CG_GROUND);

  int gx1;
  int gy1;
  int gx2;
  int gy2;
  int gx3;
  int gy3;
  int gx4;
  int gy4;

  cgRotatedPoint(cx, cy, horizonAngle, -big, pitchOffset + 75.0f, &gx1, &gy1);
  cgRotatedPoint(cx, cy, horizonAngle,  big, pitchOffset + 75.0f, &gx2, &gy2);
  cgRotatedPoint(cx, cy, horizonAngle,  big, pitchOffset + big,   &gx3, &gy3);
  cgRotatedPoint(cx, cy, horizonAngle, -big, pitchOffset + big,   &gx4, &gy4);

  display->fillTriangle(gx1, gy1, gx2, gy2, gx3, gy3, CG_GROUND_DARK);
  display->fillTriangle(gx1, gy1, gx3, gy3, gx4, gy4, CG_GROUND_DARK);

  cgDrawRotatedLine(
    display,
    cx,
    cy,
    horizonAngle,
    -big,
    pitchOffset,
    big,
    pitchOffset,
    CG_WHITE
  );

  cgDrawRotatedLine(
    display,
    cx,
    cy + 1,
    horizonAngle,
    -big,
    pitchOffset,
    big,
    pitchOffset,
    CG_SOFT_WHITE
  );
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

static void cgDrawPitchLadder(
  Arduino_GFX *display,
  float rollRad,
  float pitchDeg
) {
  int w = display->width();
  int h = display->height();

  int cx = w / 2;
  int cy = h / 2;

  float horizonAngle = -rollRad;

  for (int markDeg = -30; markDeg <= 30; markDeg += 5) {
    if (markDeg == 0) {
      continue;
    }

    float localY =
      (pitchDeg - (float)markDeg) *
      CG_PITCH_PIXELS_PER_DEG;

    if (localY < -180.0f || localY > 180.0f) {
      continue;
    }

    int lineHalfLength;

    if (markDeg % 10 == 0) {
      lineHalfLength = 36;
    } else {
      lineHalfLength = 20;
    }

    cgDrawRotatedLine(
      display,
      cx,
      cy,
      horizonAngle,
      -lineHalfLength,
      localY,
      lineHalfLength,
      localY,
      CG_WHITE
    );

    if (markDeg % 10 == 0) {
      char label[8];
      snprintf(label, sizeof(label), "%d", abs(markDeg));

      int lx1;
      int ly1;
      int lx2;
      int ly2;

      cgRotatedPoint(
        cx,
        cy,
        horizonAngle,
        -lineHalfLength - 18,
        localY - 4,
        &lx1,
        &ly1
      );

      cgRotatedPoint(
        cx,
        cy,
        horizonAngle,
        lineHalfLength + 8,
        localY - 4,
        &lx2,
        &ly2
      );

      display->setTextSize(1);
      display->setTextColor(CG_WHITE);

      if (lx1 > CG_TAPE_WIDTH + 2 && lx1 < w - CG_TAPE_WIDTH - 12 &&
          ly1 > 0 && ly1 < h - 8) {
        display->setCursor(lx1, ly1);
        display->print(label);
      }

      if (lx2 > CG_TAPE_WIDTH + 2 && lx2 < w - CG_TAPE_WIDTH - 12 &&
          ly2 > 0 && ly2 < h - 8) {
        display->setCursor(lx2, ly2);
        display->print(label);
      }
    }
  }
}

// ----------------------------------------------------
// Roll scale
// ----------------------------------------------------

static void cgDrawRollScale(
  Arduino_GFX *display,
  float rollRad
) {
  int w = display->width();

  int cx = w / 2;
  int cy = CG_ROLL_CENTER_Y;

  int rOuter = 108;
  int rInnerShort = 101;
  int rInnerLong = 96;

  for (int tick = -60; tick <= 60; tick += 10) {
    float tickSin;
    float tickCos;
    fastSinCosDeg((float)tick, &tickSin, &tickCos);

    int rInner = rInnerShort;

    if (tick % 30 == 0) {
      rInner = rInnerLong;
    }

    int x1 = cx + (int)roundf(tickSin * rInner);
    int y1 = cy - (int)roundf(tickCos * rInner);

    int x2 = cx + (int)roundf(tickSin * rOuter);
    int y2 = cy - (int)roundf(tickCos * rOuter);

    display->drawLine(x1, y1, x2, y2, CG_WHITE);
  }

  display->fillTriangle(
    cx,
    CG_ROLL_POINTER_TOP_Y,
    cx - 7,
    CG_ROLL_POINTER_BASE_Y,
    cx + 7,
    CG_ROLL_POINTER_BASE_Y,
    CG_YELLOW
  );

  float markerAngle = rollRad;

  int markerR = 94;

  float markerSin;
  float markerCos;
  float leftSin;
  float leftCos;
  float rightSin;
  float rightCos;
  fastSinCosRad(markerAngle, &markerSin, &markerCos);
  fastSinCosRad(markerAngle - 0.055f, &leftSin, &leftCos);
  fastSinCosRad(markerAngle + 0.055f, &rightSin, &rightCos);

  int mx = cx + (int)roundf(markerSin * markerR);
  int my = cy - (int)roundf(markerCos * markerR);

  int bx1 = cx + (int)roundf(leftSin * (markerR + 10));
  int by1 = cy - (int)roundf(leftCos * (markerR + 10));

  int bx2 = cx + (int)roundf(rightSin * (markerR + 10));
  int by2 = cy - (int)roundf(rightCos * (markerR + 10));

  display->fillTriangle(
    mx,
    my,
    bx1,
    by1,
    bx2,
    by2,
    CG_CYAN
  );
}

// ----------------------------------------------------
// Speed tape
// ----------------------------------------------------

static void cgDrawSpeedTape(
  Arduino_GFX *display,
  float speedKmh,
  bool valid
) {
  int h = display->height();
  int centerY = h / 2;

  int tapeW = CG_TAPE_WIDTH;

  display->drawLine(tapeW - 1, 0, tapeW - 1, h, CG_TAPE_EDGE);

  cgPrintLargeTapeLabel(display, 4, 5, "KMH");

  if (!valid) {
    display->fillRoundRect(0, centerY - 17, tapeW, 34, 3, CG_TAPE_BOX);
    display->drawRoundRect(0, centerY - 17, tapeW, 34, 3, CG_RED);
    cgPrintCentered(display, 0, centerY - 7, tapeW, "---", 2, CG_RED, CG_TAPE_BOX);
    return;
  }

  if (speedKmh < 0.0f) {
    speedKmh = 0.0f;
  }

  int minTick =
    ((int)floorf((speedKmh - 70.0f) / (float)CG_SPEED_MINOR_STEP)) *
    CG_SPEED_MINOR_STEP;

  int maxTick =
    ((int)ceilf((speedKmh + 70.0f) / (float)CG_SPEED_MINOR_STEP)) *
    CG_SPEED_MINOR_STEP;

  for (int tick = minTick; tick <= maxTick; tick += CG_SPEED_MINOR_STEP) {
    if (tick < 0) {
      continue;
    }

    int tickY =
      centerY -
      (int)roundf(((float)tick - speedKmh) * CG_SPEED_PIXELS_PER_UNIT);

    if (tickY < 31 || tickY > h - 10) {
      continue;
    }

    bool major = (tick % CG_SPEED_MAJOR_STEP) == 0;
    int tickLen = major ? 16 : 9;

    display->drawLine(
      tapeW - 4 - tickLen,
      tickY + 1,
      tapeW - 4,
      tickY + 1,
      CG_TAPE_SHADOW
    );

    display->drawLine(
      tapeW - 4 - tickLen,
      tickY,
      tapeW - 4,
      tickY,
      CG_WHITE
    );

    if (major) {
      char tickText[8];
      snprintf(tickText, sizeof(tickText), "%d", tick);

      cgPrintTapeText(display, 3, tickY - 4, tickText);
    }
  }

  char speedText[10];
  snprintf(speedText, sizeof(speedText), "%.0f", speedKmh);

  uint8_t textSize = cgBestTextSizeForBox(display, speedText, tapeW - 4);
  int textY = centerY - ((textSize == 2) ? 7 : 4);

  display->fillRoundRect(0, centerY - 17, tapeW, 34, 3, CG_TAPE_BOX);
  display->drawRoundRect(0, centerY - 17, tapeW, 34, 3, CG_CYAN);

  cgPrintCentered(
    display,
    0,
    textY,
    tapeW,
    speedText,
    textSize,
    CG_WHITE,
    CG_TAPE_BOX
  );

  display->fillTriangle(
    tapeW - 1,
    centerY,
    tapeW + 8,
    centerY - 7,
    tapeW + 8,
    centerY + 7,
    CG_CYAN
  );
}

// ----------------------------------------------------
// Altitude tape
// ----------------------------------------------------

static void cgDrawAltitudeTape(
  Arduino_GFX *display,
  float altitudeM,
  bool valid
) {
  int w = display->width();
  int h = display->height();
  int centerY = h / 2;

  int tapeW = CG_TAPE_WIDTH;
  int x = w - tapeW;

  display->drawLine(x, 0, x, h, CG_TAPE_EDGE);

  cgPrintLargeTapeLabel(display, x + 16, 5, "M");

  if (!valid) {
    display->fillRoundRect(x, centerY - 17, tapeW, 34, 3, CG_TAPE_BOX);
    display->drawRoundRect(x, centerY - 17, tapeW, 34, 3, CG_RED);
    cgPrintCentered(display, x, centerY - 7, tapeW, "---", 2, CG_RED, CG_TAPE_BOX);
    return;
  }

  int minTick =
    ((int)floorf((altitudeM - 160.0f) / (float)CG_ALT_MINOR_STEP)) *
    CG_ALT_MINOR_STEP;

  int maxTick =
    ((int)ceilf((altitudeM + 160.0f) / (float)CG_ALT_MINOR_STEP)) *
    CG_ALT_MINOR_STEP;

  for (int tick = minTick; tick <= maxTick; tick += CG_ALT_MINOR_STEP) {
    int tickY =
      centerY -
      (int)roundf(((float)tick - altitudeM) * CG_ALT_PIXELS_PER_UNIT);

    if (tickY < 31 || tickY > h - 10) {
      continue;
    }

    bool major = (tick % CG_ALT_MAJOR_STEP) == 0;
    int tickLen = major ? 16 : 9;

    display->drawLine(
      x + 4,
      tickY + 1,
      x + 4 + tickLen,
      tickY + 1,
      CG_TAPE_SHADOW
    );

    display->drawLine(
      x + 4,
      tickY,
      x + 4 + tickLen,
      tickY,
      CG_WHITE
    );

    if (major) {
      char tickText[10];
      snprintf(tickText, sizeof(tickText), "%d", tick);

      cgPrintTapeText(display, x + 18, tickY - 4, tickText);
    }
  }

  char altText[10];
  snprintf(altText, sizeof(altText), "%.0f", altitudeM);

  uint8_t textSize = cgBestTextSizeForBox(display, altText, tapeW - 4);
  int textY = centerY - ((textSize == 2) ? 7 : 4);

  display->fillRoundRect(x, centerY - 17, tapeW, 34, 3, CG_TAPE_BOX);
  display->drawRoundRect(x, centerY - 17, tapeW, 34, 3, CG_CYAN);

  cgPrintCentered(
    display,
    x,
    textY,
    tapeW,
    altText,
    textSize,
    CG_WHITE,
    CG_TAPE_BOX
  );

  display->fillTriangle(
    x,
    centerY,
    x - 8,
    centerY - 7,
    x - 8,
    centerY + 7,
    CG_CYAN
  );
}

// ----------------------------------------------------
// Fixed aircraft symbol
// ----------------------------------------------------

static void cgDrawAircraftSymbol(Arduino_GFX *display)
{
  int w = display->width();
  int h = display->height();

  int cx = w / 2;
  int cy = h / 2;

  int leftOuter = cx - (CG_AIRCRAFT_WING_SPAN / 2);
  int leftInner = cx - CG_AIRCRAFT_GAP;
  int rightInner = cx + CG_AIRCRAFT_GAP;
  int rightOuter = cx + (CG_AIRCRAFT_WING_SPAN / 2);

  display->drawLine(leftOuter, cy, leftInner, cy, CG_YELLOW);
  display->drawLine(rightInner, cy, rightOuter, cy, CG_YELLOW);

  display->drawLine(leftOuter, cy + 1, leftInner, cy + 1, CG_YELLOW);
  display->drawLine(rightInner, cy + 1, rightOuter, cy + 1, CG_YELLOW);

  display->drawLine(cx, cy - 10, cx, cy + 10, CG_YELLOW);
  display->drawCircle(cx, cy, 4, CG_YELLOW);
  display->fillCircle(cx, cy, 2, CG_BLACK);
}

// ----------------------------------------------------
// Bottom telemetry boxes
// ----------------------------------------------------

static void cgDrawBottomInfoBox(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  const char *label,
  const char *value,
  uint16_t valueColour
) {
  display->fillRoundRect(x, y, w, h, 3, CG_TAPE_BOX);
  display->drawRoundRect(x, y, w, h, 3, CG_CYAN);

  cgPrintCentered(
    display,
    x,
    y + 5,
    w,
    label,
    1,
    CG_CYAN,
    CG_TAPE_BOX
  );

  cgPrintCentered(
    display,
    x,
    y + 17,
    w,
    value,
    2,
    valueColour,
    CG_TAPE_BOX
  );
}

static void cgDrawBottomTelemetryStrip(Arduino_GFX *display)
{
  int w = display->width();
  int h = display->height();

  char satsText[8];
  char rssiText[8];
  char cellText[10];

  if (mavlinkGpsValid) {
    snprintf(
      satsText,
      sizeof(satsText),
      "%02u",
      (unsigned int)gpsSatellitesVisible
    );
  } else {
    snprintf(satsText, sizeof(satsText), "--");
  }

  if (rssiValid) {
    snprintf(
      rssiText,
      sizeof(rssiText),
      "%u%%",
      (unsigned int)rssiPercent
    );
  } else {
    snprintf(rssiText, sizeof(rssiText), "--");
  }

  if (batteryVoltageValid) {
    snprintf(
      cellText,
      sizeof(cellText),
      "%.2fV",
      batteryCellVoltage
    );
  } else {
    snprintf(cellText, sizeof(cellText), "--");
  }

  int y = h - CG_BOTTOM_BOX_H - 3;

  int satX = 0;
  int rssiX = (w - CG_BOTTOM_BOX_W) / 2;
  int cellX = w - CG_BOTTOM_BOX_W;

  cgDrawBottomInfoBox(
    display,
    satX,
    y,
    CG_BOTTOM_BOX_W,
    CG_BOTTOM_BOX_H,
    "SAT",
    satsText,
    cgSatColour()
  );

  cgDrawBottomInfoBox(
    display,
    rssiX,
    y,
    CG_BOTTOM_BOX_W,
    CG_BOTTOM_BOX_H,
    "RSSI",
    rssiText,
    cgRssiColour()
  );

  cgDrawBottomInfoBox(
    display,
    cellX,
    y,
    CG_BOTTOM_BOX_W,
    CG_BOTTOM_BOX_H,
    "CELL",
    cellText,
    cgCellColour()
  );
}

// ----------------------------------------------------
// Public screen entry point
// ----------------------------------------------------

void drawClassicGlassCockpitAhiScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad,
  float yawRad
) {
  (void)yawRad;

  float pitchDeg = pitchRad * CG_RAD_TO_DEG;

  float speedKmh = airspeed * CG_SPEED_SCALE;
  float altitudeM = altitude_msl;

  cgDrawHorizonBackground(display, rollRad, pitchDeg);
  cgDrawPitchLadder(display, rollRad, pitchDeg);
  cgDrawRollScale(display, rollRad);

  cgDrawSpeedTape(display, speedKmh, mavlinkVfrHudValid);
  cgDrawAltitudeTape(display, altitudeM, mavlinkVfrHudValid);

  cgDrawAircraftSymbol(display);
  cgDrawBottomTelemetryStrip(display);
}
