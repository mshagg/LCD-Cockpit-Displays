// ----------------------------------------------------
// Screen design: split AHI / right-side gauges
// ----------------------------------------------------
//
// Standalone ESP32-S3 + 2.0 inch ST7789VW fork.
//
// Display orientation expected:
//   320 x 240 landscape
//
// Layout:
//   - left half: compact AHI with transparent speed and altitude tapes
//   - right half: four boxed circular gauges:
//       V     = cell voltage
//       A     = current draw
//       %     = remaining battery capacity
//       RSSI  = receiver signal
//   - thin black border around each area
//   - thin black separator between the two areas
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

extern float batteryCellVoltage;
extern bool batteryVoltageValid;

extern float batteryCurrentA;
extern bool batteryCurrentValid;

extern int8_t batteryRemainingPercent;
extern bool mavlinkBatteryValid;
extern bool mavlinkBatteryStatusValid;

extern uint8_t rssiPercent;
extern bool rssiValid;

// ----------------------------------------------------
// Colours - precomputed RGB565 values
// ----------------------------------------------------

static const uint16_t SA_BLACK        = 0x0000;
static const uint16_t SA_WHITE        = 0xF7BE;
static const uint16_t SA_SOFT_WHITE   = 0xB5B6;

static const uint16_t SA_SKY          = 0x22F4;
static const uint16_t SA_SKY_DARK     = 0x096B;
static const uint16_t SA_GROUND       = 0x7A44;
static const uint16_t SA_GROUND_DARK  = 0x5162;

static const uint16_t SA_YELLOW       = 0xFE80;
static const uint16_t SA_CYAN         = 0x069F;
static const uint16_t SA_GREEN        = 0x072B;
static const uint16_t SA_RED          = 0xF9E6;
static const uint16_t SA_AMBER        = 0xFD20;

static const uint16_t SA_PANEL_BG     = 0x0042;
static const uint16_t SA_PANEL_EDGE   = 0x5AEB;
static const uint16_t SA_PANEL_HI     = 0x8C71;
static const uint16_t SA_PANEL_DARK   = 0x2104;

static const uint16_t SA_TAPE_EDGE    = 0xD69A;
static const uint16_t SA_TAPE_SHADOW  = 0x0862;

static const uint16_t SA_GAUGE_FACE   = 0x0021;
static const uint16_t SA_GAUGE_EDGE   = 0x8C71;
static const uint16_t SA_GAUGE_DARK   = 0x2104;
static const uint16_t SA_GAUGE_HUB    = 0x2966;

// ----------------------------------------------------
// Constants
// ----------------------------------------------------

static const float SA_RAD_TO_DEG = 57.2957795f;
static const float SA_DEG_TO_RAD = 0.0174532925f;

static const float SA_PITCH_PIXELS_PER_DEG = 3.0f;

static const int SA_LEFT_X = 0;
static const int SA_LEFT_Y = 0;
static const int SA_LEFT_W = 160;
static const int SA_LEFT_H = 240;

static const int SA_RIGHT_X = 160;
static const int SA_RIGHT_Y = 0;
static const int SA_RIGHT_W = 160;
static const int SA_RIGHT_H = 240;

static const int SA_TAPE_W = 29;

static const int SA_SPEED_TAPE_X = SA_LEFT_X;
static const int SA_ALT_TAPE_X = SA_LEFT_X + SA_LEFT_W - SA_TAPE_W;

static const int SA_AHI_CX = 80;
static const int SA_AHI_CY = 120;

static const int SA_AIRCRAFT_WING_SPAN = 52;
static const int SA_AIRCRAFT_GAP = 10;

static const float SA_SPEED_SCALE = 3.6f;
static const float SA_SPEED_PIXELS_PER_UNIT = 1.6f;
static const int SA_SPEED_MINOR_STEP = 10;
static const int SA_SPEED_MAJOR_STEP = 20;

static const float SA_ALT_PIXELS_PER_UNIT = 0.55f;
static const int SA_ALT_MINOR_STEP = 25;
static const int SA_ALT_MAJOR_STEP = 50;

static const float SA_GAUGE_START_DEG = 135.0f;
static const float SA_GAUGE_END_DEG   = 405.0f;

static const uint8_t SA_GAUGE_CELL   = 0;
static const uint8_t SA_GAUGE_AMPS   = 1;
static const uint8_t SA_GAUGE_REMAIN = 2;
static const uint8_t SA_GAUGE_RSSI   = 3;

// ----------------------------------------------------
// General helpers
// ----------------------------------------------------

static float saClampFloat(
  float value,
  float low,
  float high
) {
  if (value < low) {
    return low;
  }

  if (value > high) {
    return high;
  }

  return value;
}

static void saRotatedPoint(
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

static void saDrawRotatedLine(
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

  saRotatedPoint(cx, cy, angleRad, x1, y1, &sx1, &sy1);
  saRotatedPoint(cx, cy, angleRad, x2, y2, &sx2, &sy2);

  display->drawLine(sx1, sy1, sx2, sy2, colour);
}

static void saFillRotatedBand(
  Arduino_GFX *display,
  int cx,
  int cy,
  float angleRad,
  float yTop,
  float yBottom,
  uint16_t colour
) {
  const float big = 520.0f;

  int x1;
  int y1;
  int x2;
  int y2;
  int x3;
  int y3;
  int x4;
  int y4;

  saRotatedPoint(cx, cy, angleRad, -big, yTop, &x1, &y1);
  saRotatedPoint(cx, cy, angleRad,  big, yTop, &x2, &y2);
  saRotatedPoint(cx, cy, angleRad,  big, yBottom, &x3, &y3);
  saRotatedPoint(cx, cy, angleRad, -big, yBottom, &x4, &y4);

  display->fillTriangle(x1, y1, x2, y2, x3, y3, colour);
  display->fillTriangle(x1, y1, x3, y3, x4, y4, colour);
}

static void saPrintCenteredTransparent(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  const char *text,
  uint8_t textSize,
  uint16_t colour
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

  display->setTextColor(SA_TAPE_SHADOW);
  display->setCursor(drawX + 1, y + 1);
  display->print(text);

  display->setTextColor(colour);
  display->setCursor(drawX, y);
  display->print(text);
}

static void saPrintTapeText(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text
) {
  display->setTextSize(1);

  display->setTextColor(SA_TAPE_SHADOW);
  display->setCursor(x + 1, y + 1);
  display->print(text);

  display->setTextColor(SA_WHITE);
  display->setCursor(x, y);
  display->print(text);
}

static void saPrintSmallTransparent(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
) {
  display->setTextSize(1);

  display->setTextColor(SA_TAPE_SHADOW);
  display->setCursor(x + 1, y + 1);
  display->print(text);

  display->setTextColor(colour);
  display->setCursor(x, y);
  display->print(text);
}

static uint8_t saBestTextSizeForBox(
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

// ----------------------------------------------------
// Left AHI background
// ----------------------------------------------------

static void saDrawAhiBackground(
  Arduino_GFX *display,
  float rollRad,
  float pitchDeg
) {
  float horizonAngle = -rollRad;
  float pitchOffset = pitchDeg * SA_PITCH_PIXELS_PER_DEG;

  display->fillRect(SA_LEFT_X, SA_LEFT_Y, SA_LEFT_W, SA_LEFT_H, SA_SKY);

  saFillRotatedBand(
    display,
    SA_AHI_CX,
    SA_AHI_CY,
    horizonAngle,
    pitchOffset - 520.0f,
    pitchOffset - 75.0f,
    SA_SKY_DARK
  );

  saFillRotatedBand(
    display,
    SA_AHI_CX,
    SA_AHI_CY,
    horizonAngle,
    pitchOffset,
    pitchOffset + 520.0f,
    SA_GROUND
  );

  saFillRotatedBand(
    display,
    SA_AHI_CX,
    SA_AHI_CY,
    horizonAngle,
    pitchOffset + 75.0f,
    pitchOffset + 520.0f,
    SA_GROUND_DARK
  );

  saDrawRotatedLine(
    display,
    SA_AHI_CX,
    SA_AHI_CY,
    horizonAngle,
    -520.0f,
    pitchOffset,
    520.0f,
    pitchOffset,
    SA_WHITE
  );

  saDrawRotatedLine(
    display,
    SA_AHI_CX,
    SA_AHI_CY + 1,
    horizonAngle,
    -520.0f,
    pitchOffset,
    520.0f,
    pitchOffset,
    SA_SOFT_WHITE
  );
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

static void saDrawPitchLadder(
  Arduino_GFX *display,
  float rollRad,
  float pitchDeg
) {
  float horizonAngle = -rollRad;

  for (int markDeg = -30; markDeg <= 30; markDeg += 5) {
    if (markDeg == 0) {
      continue;
    }

    float localY =
      (pitchDeg - (float)markDeg) *
      SA_PITCH_PIXELS_PER_DEG;

    if (localY < -150.0f || localY > 150.0f) {
      continue;
    }

    int lineHalfLength;

    if (markDeg % 10 == 0) {
      lineHalfLength = 22;
    } else {
      lineHalfLength = 13;
    }

    saDrawRotatedLine(
      display,
      SA_AHI_CX,
      SA_AHI_CY,
      horizonAngle,
      -(float)lineHalfLength,
      localY,
      (float)lineHalfLength,
      localY,
      SA_WHITE
    );

    if (markDeg % 10 == 0) {
      char label[8];

      snprintf(
        label,
        sizeof(label),
        "%d",
        abs(markDeg)
      );

      int lx1;
      int ly1;
      int lx2;
      int ly2;

      saRotatedPoint(
        SA_AHI_CX,
        SA_AHI_CY,
        horizonAngle,
        -(float)lineHalfLength - 13.0f,
        localY - 4.0f,
        &lx1,
        &ly1
      );

      saRotatedPoint(
        SA_AHI_CX,
        SA_AHI_CY,
        horizonAngle,
        (float)lineHalfLength + 7.0f,
        localY - 4.0f,
        &lx2,
        &ly2
      );

      display->setTextSize(1);
      display->setTextColor(SA_WHITE);

      if (lx1 > SA_LEFT_X + SA_TAPE_W + 2 &&
          lx1 < SA_LEFT_X + SA_LEFT_W - SA_TAPE_W - 12 &&
          ly1 > SA_LEFT_Y + 4 &&
          ly1 < SA_LEFT_Y + SA_LEFT_H - 10) {
        display->setCursor(lx1, ly1);
        display->print(label);
      }

      if (lx2 > SA_LEFT_X + SA_TAPE_W + 2 &&
          lx2 < SA_LEFT_X + SA_LEFT_W - SA_TAPE_W - 12 &&
          ly2 > SA_LEFT_Y + 4 &&
          ly2 < SA_LEFT_Y + SA_LEFT_H - 10) {
        display->setCursor(lx2, ly2);
        display->print(label);
      }
    }
  }
}

// ----------------------------------------------------
// Roll reference
// ----------------------------------------------------

static void saDrawRollScale(
  Arduino_GFX *display,
  float rollRad
) {
  int cx = SA_AHI_CX;
  int cy = 116;

  int rOuter = 50;
  int rInnerShort = 46;
  int rInnerLong = 42;

  for (int tick = -60; tick <= 60; tick += 15) {
    float tickSin;
    float tickCos;
    fastSinCosDeg((float)tick, &tickSin, &tickCos);

    int rInner = rInnerShort;

    if (tick == -60 ||
        tick == -30 ||
        tick == 0 ||
        tick == 30 ||
        tick == 60) {
      rInner = rInnerLong;
    }

    int x1 = cx + (int)roundf(tickSin * (float)rInner);
    int y1 = cy - (int)roundf(tickCos * (float)rInner);
    int x2 = cx + (int)roundf(tickSin * (float)rOuter);
    int y2 = cy - (int)roundf(tickCos * (float)rOuter);

    display->drawLine(x1, y1, x2, y2, SA_WHITE);
  }

  display->fillTriangle(
    cx,
    58,
    cx - 5,
    68,
    cx + 5,
    68,
    SA_YELLOW
  );

  float markerAngle = rollRad;
  int markerR = 40;

  float markerSin;
  float markerCos;
  float leftSin;
  float leftCos;
  float rightSin;
  float rightCos;
  fastSinCosRad(markerAngle, &markerSin, &markerCos);
  fastSinCosRad(markerAngle - 0.075f, &leftSin, &leftCos);
  fastSinCosRad(markerAngle + 0.075f, &rightSin, &rightCos);

  int mx = cx + (int)roundf(markerSin * (float)markerR);
  int my = cy - (int)roundf(markerCos * (float)markerR);

  int bx1 =
    cx + (int)roundf(leftSin * (float)(markerR + 8));

  int by1 =
    cy - (int)roundf(leftCos * (float)(markerR + 8));

  int bx2 =
    cx + (int)roundf(rightSin * (float)(markerR + 8));

  int by2 =
    cy - (int)roundf(rightCos * (float)(markerR + 8));

  display->fillTriangle(mx, my, bx1, by1, bx2, by2, SA_CYAN);
}

// ----------------------------------------------------
// Fixed aircraft symbol
// ----------------------------------------------------

static void saDrawAircraftSymbol(Arduino_GFX *display)
{
  int cx = SA_AHI_CX;
  int cy = SA_AHI_CY;

  int leftOuter = cx - (SA_AIRCRAFT_WING_SPAN / 2);
  int leftInner = cx - SA_AIRCRAFT_GAP;
  int rightInner = cx + SA_AIRCRAFT_GAP;
  int rightOuter = cx + (SA_AIRCRAFT_WING_SPAN / 2);

  display->drawLine(leftOuter, cy, leftInner, cy, SA_YELLOW);
  display->drawLine(rightInner, cy, rightOuter, cy, SA_YELLOW);

  display->drawLine(leftOuter, cy + 1, leftInner, cy + 1, SA_YELLOW);
  display->drawLine(rightInner, cy + 1, rightOuter, cy + 1, SA_YELLOW);

  display->drawLine(cx, cy - 8, cx, cy + 8, SA_YELLOW);
  display->drawCircle(cx, cy, 4, SA_YELLOW);
  display->fillCircle(cx, cy, 2, SA_BLACK);
}

// ----------------------------------------------------
// Speed tape
// ----------------------------------------------------

static void saDrawSpeedTape(
  Arduino_GFX *display,
  float speedKmh,
  bool valid
) {
  int centerY = SA_LEFT_H / 2;
  int x = SA_SPEED_TAPE_X;
  int tapeW = SA_TAPE_W;

  display->drawLine(
    x + tapeW - 1,
    1,
    x + tapeW - 1,
    SA_LEFT_H - 2,
    SA_TAPE_EDGE
  );

  saPrintSmallTransparent(display, x + 4, 6, "KMH", SA_CYAN);

  if (!valid) {
    display->drawRoundRect(x, centerY - 15, tapeW, 30, 3, SA_RED);
    saPrintCenteredTransparent(display, x, centerY - 6, tapeW, "--", 2, SA_RED);
    return;
  }

  if (speedKmh < 0.0f) {
    speedKmh = 0.0f;
  }

  int minTick =
    ((int)floorf((speedKmh - 70.0f) / (float)SA_SPEED_MINOR_STEP)) *
    SA_SPEED_MINOR_STEP;

  int maxTick =
    ((int)ceilf((speedKmh + 70.0f) / (float)SA_SPEED_MINOR_STEP)) *
    SA_SPEED_MINOR_STEP;

  for (int tick = minTick; tick <= maxTick; tick += SA_SPEED_MINOR_STEP) {
    if (tick < 0) {
      continue;
    }

    int tickY =
      centerY -
      (int)roundf(((float)tick - speedKmh) * SA_SPEED_PIXELS_PER_UNIT);

    if (tickY < 19 || tickY > SA_LEFT_H - 11) {
      continue;
    }

    bool major = (tick % SA_SPEED_MAJOR_STEP) == 0;
    int tickLen = major ? 10 : 6;

    display->drawLine(
      x + tapeW - 4 - tickLen,
      tickY + 1,
      x + tapeW - 4,
      tickY + 1,
      SA_TAPE_SHADOW
    );

    display->drawLine(
      x + tapeW - 4 - tickLen,
      tickY,
      x + tapeW - 4,
      tickY,
      SA_WHITE
    );

    if (major) {
      char tickText[8];
      snprintf(tickText, sizeof(tickText), "%d", tick);
      saPrintTapeText(display, x + 2, tickY - 4, tickText);
    }
  }

  char speedText[10];
  snprintf(speedText, sizeof(speedText), "%.0f", speedKmh);

  uint8_t textSize =
    saBestTextSizeForBox(display, speedText, tapeW - 3);

  int textY =
    centerY - ((textSize == 2) ? 7 : 4);

  display->drawRoundRect(x, centerY - 15, tapeW, 30, 3, SA_CYAN);

  saPrintCenteredTransparent(
    display,
    x,
    textY,
    tapeW,
    speedText,
    textSize,
    SA_WHITE
  );

  display->fillTriangle(
    x + tapeW - 1,
    centerY,
    x + tapeW + 7,
    centerY - 6,
    x + tapeW + 7,
    centerY + 6,
    SA_CYAN
  );
}

// ----------------------------------------------------
// Altitude tape
// ----------------------------------------------------

static void saDrawAltitudeTape(
  Arduino_GFX *display,
  float altitudeM,
  bool valid
) {
  int centerY = SA_LEFT_H / 2;
  int tapeW = SA_TAPE_W;
  int x = SA_ALT_TAPE_X;

  display->drawLine(
    x,
    1,
    x,
    SA_LEFT_H - 2,
    SA_TAPE_EDGE
  );

  saPrintSmallTransparent(display, x + 10, 6, "M", SA_CYAN);

  if (!valid) {
    display->drawRoundRect(x, centerY - 15, tapeW, 30, 3, SA_RED);
    saPrintCenteredTransparent(display, x, centerY - 6, tapeW, "--", 2, SA_RED);
    return;
  }

  int minTick =
    ((int)floorf((altitudeM - 160.0f) / (float)SA_ALT_MINOR_STEP)) *
    SA_ALT_MINOR_STEP;

  int maxTick =
    ((int)ceilf((altitudeM + 160.0f) / (float)SA_ALT_MINOR_STEP)) *
    SA_ALT_MINOR_STEP;

  for (int tick = minTick; tick <= maxTick; tick += SA_ALT_MINOR_STEP) {
    int tickY =
      centerY -
      (int)roundf(((float)tick - altitudeM) * SA_ALT_PIXELS_PER_UNIT);

    if (tickY < 19 || tickY > SA_LEFT_H - 11) {
      continue;
    }

    bool major = (tick % SA_ALT_MAJOR_STEP) == 0;
    int tickLen = major ? 10 : 6;

    display->drawLine(
      x + 3,
      tickY + 1,
      x + 3 + tickLen,
      tickY + 1,
      SA_TAPE_SHADOW
    );

    display->drawLine(
      x + 3,
      tickY,
      x + 3 + tickLen,
      tickY,
      SA_WHITE
    );

    if (major) {
      char tickText[8];
      snprintf(tickText, sizeof(tickText), "%d", tick);
      saPrintTapeText(display, x + 13, tickY - 4, tickText);
    }
  }

  char altText[10];
  snprintf(altText, sizeof(altText), "%.0f", altitudeM);

  uint8_t textSize =
    saBestTextSizeForBox(display, altText, tapeW - 3);

  int textY =
    centerY - ((textSize == 2) ? 7 : 4);

  display->drawRoundRect(x, centerY - 15, tapeW, 30, 3, SA_CYAN);

  saPrintCenteredTransparent(
    display,
    x,
    textY,
    tapeW,
    altText,
    textSize,
    SA_WHITE
  );

  display->fillTriangle(
    x,
    centerY,
    x - 7,
    centerY - 6,
    x - 7,
    centerY + 6,
    SA_CYAN
  );
}

// ----------------------------------------------------
// Right-side circular gauge helpers
// ----------------------------------------------------

static void saGaugePolarPoint(
  int cx,
  int cy,
  float angleDeg,
  float radius,
  int *x,
  int *y
) {
  float s;
  float c;
  fastSinCosDeg(angleDeg, &s, &c);

  *x = cx + (int)roundf(c * radius);
  *y = cy + (int)roundf(s * radius);
}

static float saGaugeValueToAngle(
  float value,
  float minValue,
  float maxValue
) {
  if (maxValue <= minValue) {
    return SA_GAUGE_START_DEG;
  }

  float clampedValue =
    saClampFloat(value, minValue, maxValue);

  float fraction =
    (clampedValue - minValue) /
    (maxValue - minValue);

  return SA_GAUGE_START_DEG +
         fraction * (SA_GAUGE_END_DEG - SA_GAUGE_START_DEG);
}

static uint16_t saGaugeValueColour(
  uint8_t gaugeType,
  float value,
  bool valid
) {
  if (!valid) {
    return SA_RED;
  }

  if (gaugeType == SA_GAUGE_CELL) {
    if (value >= 3.60f) {
      return SA_GREEN;
    }

    if (value >= 3.40f) {
      return SA_AMBER;
    }

    return SA_RED;
  }

  if (gaugeType == SA_GAUGE_AMPS) {
    if (value <= 50.0f) {
      return SA_GREEN;
    }

    if (value <= 75.0f) {
      return SA_AMBER;
    }

    return SA_RED;
  }

  if (gaugeType == SA_GAUGE_REMAIN ||
      gaugeType == SA_GAUGE_RSSI) {
    if (value >= 60.0f) {
      return SA_GREEN;
    }

    if (value >= 35.0f) {
      return SA_AMBER;
    }

    return SA_RED;
  }

  return SA_GREEN;
}

static void saDrawPanelScrew(
  Arduino_GFX *display,
  int x,
  int y
) {
  display->fillCircle(x, y, 3, SA_PANEL_DARK);
  display->drawCircle(x, y, 3, SA_PANEL_HI);
  display->fillCircle(x, y, 1, SA_GAUGE_EDGE);

  display->drawLine(x - 2, y, x + 2, y, SA_BLACK);
}

static void saDrawGaugeBox(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h
) {
  display->fillRect(x, y, w, h, SA_PANEL_BG);

  display->drawRect(x, y, w, h, SA_PANEL_EDGE);
  display->drawRect(x + 1, y + 1, w - 2, h - 2, SA_PANEL_DARK);

  display->drawLine(x + 2, y + 2, x + w - 3, y + 2, SA_PANEL_HI);
  display->drawLine(x + 2, y + 2, x + 2, y + h - 3, SA_PANEL_HI);

  display->drawLine(x + 2, y + h - 3, x + w - 3, y + h - 3, SA_BLACK);
  display->drawLine(x + w - 3, y + 2, x + w - 3, y + h - 3, SA_BLACK);

  saDrawPanelScrew(display, x + 7, y + 7);
  saDrawPanelScrew(display, x + w - 8, y + 7);
  saDrawPanelScrew(display, x + 7, y + h - 8);
  saDrawPanelScrew(display, x + w - 8, y + h - 8);
}

static void saDrawGaugeArc(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float startFraction,
  float endFraction,
  uint16_t colour
) {
  float startDeg =
    SA_GAUGE_START_DEG +
    startFraction * (SA_GAUGE_END_DEG - SA_GAUGE_START_DEG);

  float endDeg =
    SA_GAUGE_START_DEG +
    endFraction * (SA_GAUGE_END_DEG - SA_GAUGE_START_DEG);

  for (float angleDeg = startDeg;
       angleDeg <= endDeg;
       angleDeg += 2.0f) {
    int x1;
    int y1;
    int x2;
    int y2;

    saGaugePolarPoint(cx, cy, angleDeg, (float)r - 5.0f, &x1, &y1);
    saGaugePolarPoint(cx, cy, angleDeg, (float)r - 4.0f, &x2, &y2);

    display->drawLine(x1, y1, x2, y2, colour);
  }
}

static void saDrawGaugeTicks(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r
) {
  for (int i = 0; i <= 5; i++) {
    float fraction = (float)i / 5.0f;

    float angleDeg =
      SA_GAUGE_START_DEG +
      fraction * (SA_GAUGE_END_DEG - SA_GAUGE_START_DEG);

    int x1;
    int y1;
    int x2;
    int y2;

    saGaugePolarPoint(cx, cy, angleDeg, (float)r - 9.0f, &x1, &y1);
    saGaugePolarPoint(cx, cy, angleDeg, (float)r - 3.0f, &x2, &y2);

    display->drawLine(x1, y1, x2, y2, SA_WHITE);
  }
}

static void saDrawGaugeNeedle(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float value,
  bool valid
) {
  if (!valid) {
    display->drawLine(cx - 8, cy - 8, cx + 8, cy + 8, SA_RED);
    display->drawLine(cx + 8, cy - 8, cx - 8, cy + 8, SA_RED);
    return;
  }

  float angleDeg =
    saGaugeValueToAngle(value, minValue, maxValue);

  float s;
  float c;
  fastSinCosDeg(angleDeg, &s, &c);

  int tipX =
    cx + (int)roundf(c * ((float)r - 10.0f));

  int tipY =
    cy + (int)roundf(s * ((float)r - 10.0f));

  int tailX =
    cx - (int)roundf(c * 5.0f);

  int tailY =
    cy - (int)roundf(s * 5.0f);

  int baseX1 =
    tailX - (int)roundf(s * 2.0f);

  int baseY1 =
    tailY + (int)roundf(c * 2.0f);

  int baseX2 =
    tailX + (int)roundf(s * 2.0f);

  int baseY2 =
    tailY - (int)roundf(c * 2.0f);

  display->fillTriangle(tipX, tipY, baseX1, baseY1, baseX2, baseY2, SA_WHITE);
  display->drawLine(cx, cy, tipX, tipY, SA_SOFT_WHITE);

  display->fillCircle(cx, cy, 4, SA_GAUGE_HUB);
  display->drawCircle(cx, cy, 4, SA_GAUGE_EDGE);
}

static void saFormatGaugeValue(
  char *buffer,
  size_t bufferSize,
  float value,
  bool valid,
  uint8_t decimals
) {
  if (!valid) {
    snprintf(buffer, bufferSize, "--");
    return;
  }

  if (decimals == 0) {
    snprintf(buffer, bufferSize, "%.0f", value);
  } else if (decimals == 1) {
    snprintf(buffer, bufferSize, "%.1f", value);
  } else {
    snprintf(buffer, bufferSize, "%.2f", value);
  }
}

static void saDrawCircularGauge(
  Arduino_GFX *display,
  int panelX,
  int panelY,
  int panelW,
  int panelH,
  const char *title,
  float value,
  bool valid,
  float minValue,
  float maxValue,
  uint8_t decimals,
  uint8_t gaugeType
) {
  saDrawGaugeBox(display, panelX, panelY, panelW, panelH);

  int cx = panelX + (panelW / 2);
  int cy = panelY + 36;
  int r = 24;

  display->fillCircle(cx, cy, r + 3, SA_GAUGE_DARK);
  display->drawCircle(cx, cy, r + 3, SA_GAUGE_EDGE);
  display->drawCircle(cx, cy, r + 1, SA_PANEL_HI);
  display->fillCircle(cx, cy, r - 2, SA_GAUGE_FACE);

  if (gaugeType == SA_GAUGE_CELL) {
    saDrawGaugeArc(display, cx, cy, r, 0.00f, 0.33f, SA_RED);
    saDrawGaugeArc(display, cx, cy, r, 0.33f, 0.50f, SA_AMBER);
    saDrawGaugeArc(display, cx, cy, r, 0.50f, 1.00f, SA_GREEN);
  } else if (gaugeType == SA_GAUGE_AMPS) {
    saDrawGaugeArc(display, cx, cy, r, 0.00f, 0.50f, SA_GREEN);
    saDrawGaugeArc(display, cx, cy, r, 0.50f, 0.75f, SA_AMBER);
    saDrawGaugeArc(display, cx, cy, r, 0.75f, 1.00f, SA_RED);
  } else {
    saDrawGaugeArc(display, cx, cy, r, 0.00f, 0.20f, SA_RED);
    saDrawGaugeArc(display, cx, cy, r, 0.20f, 0.35f, SA_AMBER);
    saDrawGaugeArc(display, cx, cy, r, 0.35f, 1.00f, SA_GREEN);
  }

  saDrawGaugeTicks(display, cx, cy, r);

  saDrawGaugeNeedle(
    display,
    cx,
    cy,
    r,
    minValue,
    maxValue,
    value,
    valid
  );

  char valueText[14];

  saFormatGaugeValue(
    valueText,
    sizeof(valueText),
    value,
    valid,
    decimals
  );

  uint16_t valueColour =
    saGaugeValueColour(gaugeType, value, valid);

  saPrintCenteredTransparent(
    display,
    panelX + 4,
    panelY + 68,
    panelW - 8,
    title,
    2,
    SA_CYAN
  );

  saPrintCenteredTransparent(
    display,
    panelX + 4,
    panelY + 88,
    panelW - 8,
    valueText,
    2,
    valueColour
  );
}

static void saDrawRightGaugesPanel(Arduino_GFX *display)
{
  display->fillRect(
    SA_RIGHT_X,
    SA_RIGHT_Y,
    SA_RIGHT_W,
    SA_RIGHT_H,
    SA_PANEL_BG
  );

  float cellVoltage = batteryCellVoltage;
  bool cellValid = batteryVoltageValid;

  float currentA = batteryCurrentA;
  bool currentValid = batteryCurrentValid;

  if (currentA < 0.0f) {
    currentA = 0.0f;
  }

  float remainingPercent = (float)batteryRemainingPercent;
  bool remainingValid =
    batteryRemainingPercent >= 0 &&
    (mavlinkBatteryValid || mavlinkBatteryStatusValid);

  float rssiValue = (float)rssiPercent;
  bool rssiGaugeValid = rssiValid;

  saDrawCircularGauge(
    display,
    164,
    4,
    72,
    112,
    "V",
    cellVoltage,
    cellValid,
    3.0f,
    4.2f,
    2,
    SA_GAUGE_CELL
  );

  saDrawCircularGauge(
    display,
    244,
    4,
    72,
    112,
    "A",
    currentA,
    currentValid,
    0.0f,
    100.0f,
    1,
    SA_GAUGE_AMPS
  );

  saDrawCircularGauge(
    display,
    164,
    124,
    72,
    112,
    "BAT%",
    remainingPercent,
    remainingValid,
    0.0f,
    100.0f,
    0,
    SA_GAUGE_REMAIN
  );

  saDrawCircularGauge(
    display,
    244,
    124,
    72,
    112,
    "RSSI",
    rssiValue,
    rssiGaugeValid,
    0.0f,
    100.0f,
    0,
    SA_GAUGE_RSSI
  );
}

// ----------------------------------------------------
// Borders
// ----------------------------------------------------

static void saDrawBorders(Arduino_GFX *display)
{
  display->drawRect(SA_LEFT_X, SA_LEFT_Y, SA_LEFT_W, SA_LEFT_H, SA_BLACK);
  display->drawRect(SA_RIGHT_X, SA_RIGHT_Y, SA_RIGHT_W, SA_RIGHT_H, SA_BLACK);

  display->drawLine(
    SA_RIGHT_X,
    0,
    SA_RIGHT_X,
    display->height() - 1,
    SA_BLACK
  );
}

// ----------------------------------------------------
// Public screen entry point
// ----------------------------------------------------

void drawSplitAhiScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad,
  float yawRad
) {
  (void)yawRad;

  float pitchDeg = pitchRad * SA_RAD_TO_DEG;

  float speedKmh = airspeed * SA_SPEED_SCALE;
  float altitudeM = altitude_msl;

  saDrawAhiBackground(display, rollRad, pitchDeg);
  saDrawPitchLadder(display, rollRad, pitchDeg);
  saDrawRollScale(display, rollRad);
  saDrawAircraftSymbol(display);

  saDrawSpeedTape(display, speedKmh, mavlinkVfrHudValid);
  saDrawAltitudeTape(display, altitudeM, mavlinkVfrHudValid);

  saDrawRightGaugesPanel(display);
  saDrawBorders(display);
}
