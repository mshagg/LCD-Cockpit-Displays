// ----------------------------------------------------
// Full-screen glass-style artificial horizon / PFD
// ----------------------------------------------------
//
// Full-screen display:
//   - heading tape at top
//   - airspeed tape on the left
//   - altitude tape on the right
//   - attitude display continues behind the tapes
//   - sky colour behind heading tape
//   - moving roll pointer below heading tape
//   - ground speed box bottom-left
//
// No bezel.
// ----------------------------------------------------

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Flight data from mavlink_telemetry.ino
extern float airspeed;      // m/s
extern float groundspeed;   // m/s
extern float altitude_msl;  // metres
extern int16_t heading_deg;

// ----------------------------------------------------
// Local tuning
// ----------------------------------------------------
//
// Roll pointer direction comes from:
//   CONFIG_AHI_ROLL_POINTER_SIGN
//
// Do not define a local roll-pointer sign here.

// Roll arc position.
// This leaves only a small gap below the heading tape.
const int PFD_ROLL_ARC_CY = 110;
const int PFD_ROLL_ARC_OUTER_R = 70;
const int PFD_ROLL_ARC_INNER_R = 56;

// ----------------------------------------------------
// Local helpers
// ----------------------------------------------------

static inline float pfdDisplayAirspeed()
{
  return airspeed * CONFIG_GLASS_SPEED_SCALE;
}

static inline float pfdDisplayGroundSpeed()
{
  return groundspeed * CONFIG_GLASS_SPEED_SCALE;
}

static inline float pfdDisplayAltitude()
{
  return altitude_msl * CONFIG_GLASS_ALT_SCALE;
}

static inline const char* pfdSpeedUnitLabel()
{
  return CONFIG_GLASS_SPEED_LABEL;
}

static inline const char* pfdAltUnitLabel()
{
  return CONFIG_GLASS_ALT_LABEL;
}

static int pfdNormalizeHeading(int heading)
{
  while (heading < 0) {
    heading += 360;
  }

  while (heading >= 360) {
    heading -= 360;
  }

  return heading;
}

static void pfdTransformPoint(
  float xr,
  float yr,
  float cx,
  float cy,
  float rollRad,
  int &x,
  int &y
) {
  float c = cosf(rollRad);
  float s = sinf(rollRad);

  x = (int)lroundf(cx + xr * c - yr * s);
  y = (int)lroundf(cy + xr * s + yr * c);
}

// ----------------------------------------------------
// Sky / ground fill
// ----------------------------------------------------

static void drawPfdSkyGround(
  Arduino_GFX *display,
  int xMin,
  int xMax,
  int yMin,
  int yMax,
  float cx,
  float horizonCy,
  float rollRad,
  uint16_t skyColor,
  uint16_t groundColor
) {
  float s = sinf(rollRad);
  float c = cosf(rollRad);

  for (int y = yMin; y <= yMax; y++) {
    float dy = (float)y - horizonCy;

    if (fabsf(s) < 0.001f) {
      uint16_t colour = (y < horizonCy) ? skyColor : groundColor;
      display->drawFastHLine(xMin, y, xMax - xMin + 1, colour);
      continue;
    }

    float yrLeft  = -((float)xMin - cx) * s + dy * c;
    float yrRight = -((float)xMax - cx) * s + dy * c;

    if (yrLeft < 0.0f && yrRight < 0.0f) {
      display->drawFastHLine(xMin, y, xMax - xMin + 1, skyColor);
    }
    else if (yrLeft >= 0.0f && yrRight >= 0.0f) {
      display->drawFastHLine(xMin, y, xMax - xMin + 1, groundColor);
    }
    else {
      int xCross = (int)lroundf(cx + (dy * c) / s);

      if (xCross < xMin) {
        xCross = xMin;
      }

      if (xCross > xMax) {
        xCross = xMax;
      }

      if (yrLeft < 0.0f) {
        display->drawFastHLine(xMin, y, xCross - xMin + 1, skyColor);

        if (xCross + 1 <= xMax) {
          display->drawFastHLine(xCross + 1, y, xMax - xCross, groundColor);
        }
      } else {
        display->drawFastHLine(xMin, y, xCross - xMin + 1, groundColor);

        if (xCross + 1 <= xMax) {
          display->drawFastHLine(xCross + 1, y, xMax - xCross, skyColor);
        }
      }
    }
  }
}

// ----------------------------------------------------
// Heading tape
// ----------------------------------------------------

static void drawPfdHeadingTape(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  int centreX,
  uint16_t bgColor,
  uint16_t lineColor,
  uint16_t textColor,
  uint16_t pointerColor
) {
  int currentHeading = pfdNormalizeHeading((int)heading_deg);

  float halfRange = CONFIG_GLASS_HEADING_HALF_RANGE_DEG;
  int majorStep = CONFIG_GLASS_HEADING_MAJOR_STEP_DEG;
  int minorStep = CONFIG_GLASS_HEADING_MINOR_STEP_DEG;

  float pxPerDeg = ((float)w / 2.0f - 8.0f) / halfRange;

  display->fillRect(x, y, w, h, bgColor);
  display->drawRect(x, y, w, h, lineColor);

  int start =
    ((int)floorf(((float)currentHeading - halfRange) / (float)minorStep)) *
    minorStep;

  int end =
    ((int)ceilf(((float)currentHeading + halfRange) / (float)minorStep)) *
    minorStep;

  display->setTextSize(1);
  display->setTextColor(textColor);

  for (int mark = start; mark <= end; mark += minorStep) {
    float delta = (float)mark - (float)currentHeading;
    int xx = (int)lroundf((float)centreX + delta * pxPerDeg);

    if (xx < x + 2 || xx > x + w - 3) {
      continue;
    }

    int shownHeading = pfdNormalizeHeading(mark);
    bool major = (shownHeading % majorStep) == 0;

    int tickTop = major ? y + h - 13 : y + h - 7;
    int tickBottom = y + h - 2;

    display->drawLine(xx, tickTop, xx, tickBottom, lineColor);

    if (major) {
      char label[4];

      if (shownHeading == 0) {
        snprintf(label, sizeof(label), "N");
      }
      else if (shownHeading == 90) {
        snprintf(label, sizeof(label), "E");
      }
      else if (shownHeading == 180) {
        snprintf(label, sizeof(label), "S");
      }
      else if (shownHeading == 270) {
        snprintf(label, sizeof(label), "W");
      }
      else {
        snprintf(label, sizeof(label), "%02d", shownHeading / 10);
      }

      int labelW = strlen(label) * 6;
      int labelX = xx - labelW / 2;

      if (labelX >= x + 2 && labelX + labelW <= x + w - 2) {
        display->setCursor(labelX, y + 3);
        display->print(label);
      }
    }
  }

  // Current heading value box.
  char currentText[5];
  snprintf(currentText, sizeof(currentText), "%03d", currentHeading);

  const int readoutW = 56;
  const int readoutH = 22;
  const int readoutX = centreX - readoutW / 2;
  const int readoutY = y + 3;

  display->fillRect(readoutX, readoutY, readoutW, readoutH, GFX_BLACK);
  display->drawRect(readoutX, readoutY, readoutW, readoutH, lineColor);

  display->setTextSize(2);
  display->setTextColor(pointerColor);
  display->setCursor(readoutX + 10, readoutY + 4);
  display->print(currentText);

  // Small centre index marker at the bottom of the heading tape.
  display->drawFastVLine(centreX, y + h - 5, 5, pointerColor);
}

// ----------------------------------------------------
// Attitude display elements
// ----------------------------------------------------

static void drawPfdHorizonLine(
  Arduino_GFX *display,
  float cx,
  float horizonCy,
  float rollRad,
  uint16_t colour
) {
  int x1, y1, x2, y2;

  pfdTransformPoint(-220, 0, cx, horizonCy, rollRad, x1, y1);
  pfdTransformPoint( 220, 0, cx, horizonCy, rollRad, x2, y2);

  display->drawLine(x1, y1, x2, y2, colour);
}

static void drawPfdPitchLadder(
  Arduino_GFX *display,
  int screenW,
  int screenH,
  float cx,
  float horizonCy,
  float rollRad,
  uint16_t colour
) {
  display->setTextSize(1);
  display->setTextColor(colour);

  const float pxPerDeg = CONFIG_GLASS_AHI_PITCH_PIXELS_PER_DEG;

  // Major pitch lines every 10 degrees.
  for (int mark = -30; mark <= 30; mark += 10) {
    if (mark == 0) {
      continue;
    }

    float yr = -mark * pxPerDeg;

    float outer = 42.0f;
    float inner = 15.0f;

    int x1, y1, x2, y2;
    int x3, y3, x4, y4;

    pfdTransformPoint(-outer, yr, cx, horizonCy, rollRad, x1, y1);
    pfdTransformPoint(-inner, yr, cx, horizonCy, rollRad, x2, y2);
    pfdTransformPoint( inner, yr, cx, horizonCy, rollRad, x3, y3);
    pfdTransformPoint( outer, yr, cx, horizonCy, rollRad, x4, y4);

    display->drawLine(x1, y1, x2, y2, colour);
    display->drawLine(x3, y3, x4, y4, colour);

    int label = abs(mark);

    int lx, ly, rx, ry;
    pfdTransformPoint(-55, yr, cx, horizonCy, rollRad, lx, ly);
    pfdTransformPoint( 48, yr, cx, horizonCy, rollRad, rx, ry);

    if (ly > 8 && ly < screenH - 8) {
      display->setCursor(lx - 3, ly - 3);
      display->print(label);
    }

    if (ry > 8 && ry < screenH - 8) {
      display->setCursor(rx - 2, ry - 3);
      display->print(label);
    }
  }

  // Minor pitch lines every 5 degrees.
  for (int mark = -25; mark <= 25; mark += 5) {
    if (mark == 0 || (mark % 10) == 0) {
      continue;
    }

    float yr = -mark * pxPerDeg;

    float outer = 22.0f;
    float inner = 8.0f;

    int x1, y1, x2, y2;
    int x3, y3, x4, y4;

    pfdTransformPoint(-outer, yr, cx, horizonCy, rollRad, x1, y1);
    pfdTransformPoint(-inner, yr, cx, horizonCy, rollRad, x2, y2);
    pfdTransformPoint( inner, yr, cx, horizonCy, rollRad, x3, y3);
    pfdTransformPoint( outer, yr, cx, horizonCy, rollRad, x4, y4);

    display->drawLine(x1, y1, x2, y2, colour);
    display->drawLine(x3, y3, x4, y4, colour);
  }
}

// ----------------------------------------------------
// Roll arc and moving pointer
// ----------------------------------------------------

static void drawPfdRollScale(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t colour
) {
  const int rOuter = PFD_ROLL_ARC_OUTER_R;
  const int rInnerLong = PFD_ROLL_ARC_INNER_R;
  const int rInnerShort = PFD_ROLL_ARC_INNER_R + 6;

  int lastX = 0;
  int lastY = 0;
  bool haveLast = false;

  for (int deg = -60; deg <= 60; deg += 3) {
    float a = ((float)deg - 90.0f) * DEG_TO_RAD;

    int x = (int)lroundf(cx + cosf(a) * rOuter);
    int y = (int)lroundf(cy + sinf(a) * rOuter);

    if (haveLast) {
      display->drawLine(lastX, lastY, x, y, colour);
    }

    lastX = x;
    lastY = y;
    haveLast = true;
  }

  for (int deg = -60; deg <= 60; deg += 10) {
    float a = ((float)deg - 90.0f) * DEG_TO_RAD;

    int rInner = ((deg % 30) == 0) ? rInnerLong : rInnerShort;

    int x1 = (int)lroundf(cx + cosf(a) * rInner);
    int y1 = (int)lroundf(cy + sinf(a) * rInner);
    int x2 = (int)lroundf(cx + cosf(a) * rOuter);
    int y2 = (int)lroundf(cy + sinf(a) * rOuter);

    display->drawLine(x1, y1, x2, y2, colour);
  }

  // Fixed zero-bank reference marker.
  display->drawTriangle(
    cx,
    cy - rOuter - 2,
    cx - 6,
    cy - rOuter + 9,
    cx + 6,
    cy - rOuter + 9,
    colour
  );
}

static void drawPfdMovingRollPointer(
  Arduino_GFX *display,
  int cx,
  int cy,
  float rollRad,
  uint16_t fillColour,
  uint16_t outlineColour
) {
  float bankDeg =
    rollRad *
    RAD_TO_DEG *
    CONFIG_AHI_ROLL_POINTER_SIGN;

  if (bankDeg < -60.0f) {
    bankDeg = -60.0f;
  }

  if (bankDeg > 60.0f) {
    bankDeg = 60.0f;
  }

  float a = (bankDeg - 90.0f) * DEG_TO_RAD;

  float radialX = cosf(a);
  float radialY = sinf(a);

  float tangentX = -sinf(a);
  float tangentY = cosf(a);

  // Same size as the fixed roll marker.
  // Bottom/tip corner aligns to the roll arc.
  const float tipR  = PFD_ROLL_ARC_OUTER_R;
  const float baseR = PFD_ROLL_ARC_OUTER_R + 11;
  const float halfW = 6.0f;

  int tipX = (int)lroundf(cx + radialX * tipR);
  int tipY = (int)lroundf(cy + radialY * tipR);

  int b1X = (int)lroundf(cx + radialX * baseR + tangentX * halfW);
  int b1Y = (int)lroundf(cy + radialY * baseR + tangentY * halfW);

  int b2X = (int)lroundf(cx + radialX * baseR - tangentX * halfW);
  int b2Y = (int)lroundf(cy + radialY * baseR - tangentY * halfW);

  display->fillTriangle(tipX, tipY, b1X, b1Y, b2X, b2Y, fillColour);
  display->drawTriangle(tipX, tipY, b1X, b1Y, b2X, b2Y, outlineColour);
}

// ----------------------------------------------------
// Aircraft symbol
// ----------------------------------------------------

static void drawPfdAircraftSymbol(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t wingColor,
  uint16_t outlineColor
) {
  // Black backing for contrast.
  display->drawLine(cx - 54, cy - 1, cx - 18, cy - 1, outlineColor);
  display->drawLine(cx - 54, cy,     cx - 18, cy,     outlineColor);
  display->drawLine(cx - 54, cy + 1, cx - 18, cy + 1, outlineColor);

  display->drawLine(cx + 18, cy - 1, cx + 54, cy - 1, outlineColor);
  display->drawLine(cx + 18, cy,     cx + 54, cy,     outlineColor);
  display->drawLine(cx + 18, cy + 1, cx + 54, cy + 1, outlineColor);

  // Yellow aircraft cue.
  display->drawLine(cx - 52, cy, cx - 18, cy, wingColor);
  display->drawLine(cx - 52, cy + 1, cx - 18, cy + 1, wingColor);

  display->drawLine(cx + 18, cy, cx + 52, cy, wingColor);
  display->drawLine(cx + 18, cy + 1, cx + 52, cy + 1, wingColor);

  display->fillRect(cx - 16, cy - 3, 8, 6, wingColor);
  display->fillRect(cx + 8,  cy - 3, 8, 6, wingColor);

  display->drawLine(cx, cy, cx, cy + 11, wingColor);
  display->drawLine(cx - 1, cy, cx - 1, cy + 11, wingColor);
}

// ----------------------------------------------------
// Speed tape
// ----------------------------------------------------

static void drawPfdSpeedTape(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  int centreY,
  uint16_t lineColor,
  uint16_t textColor,
  uint16_t boxColor
) {
  float current = pfdDisplayAirspeed();

  float halfRange = CONFIG_GLASS_SPEED_HALF_RANGE;
  float majorStep = CONFIG_GLASS_SPEED_MAJOR_STEP;
  float minorStep = CONFIG_GLASS_SPEED_MINOR_STEP;

  float pxPerUnit = 100.0f / halfRange;

  // No panel fill: attitude background continues behind tape.
  display->drawFastVLine(x + w - 1, y, h, lineColor);

  float start = floorf((current - halfRange) / minorStep) * minorStep;
  float end   = ceilf((current + halfRange) / minorStep) * minorStep;

  display->setTextSize(1);
  display->setTextColor(textColor);

  for (float v = start; v <= end + 0.01f; v += minorStep) {
    int yy = (int)lroundf(centreY - (v - current) * pxPerUnit);

    if (yy < y + 8 || yy > y + h - 8) {
      continue;
    }

    bool major =
      (fmodf(fabsf(v), majorStep) < 0.01f) ||
      (fmodf(fabsf(v), majorStep) > (majorStep - 0.01f));

    int tickLen = major ? 11 : 6;

    display->drawLine(x + w - 1 - tickLen, yy, x + w - 1, yy, lineColor);

    if (major) {
      int iv = (int)lroundf(v);
      display->setCursor(x + 3, yy - 3);
      display->print(iv);
    }
  }

  // Current speed box.
  int boxH = 24;
  int boxY = centreY - boxH / 2;

  display->fillRect(x, boxY, w + 8, boxH, boxColor);
  display->drawRect(x, boxY, w + 8, boxH, lineColor);

  display->setTextSize(2);
  display->setTextColor(textColor);
  display->setCursor(x + 4, boxY + 5);
  display->print((int)lroundf(current));

  display->setTextSize(1);
  display->setTextColor(textColor);
  display->setCursor(x + 2, y + h - 12);
  display->print(pfdSpeedUnitLabel());
}

// ----------------------------------------------------
// Altitude tape
// ----------------------------------------------------

static void drawPfdAltTape(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  int centreY,
  uint16_t lineColor,
  uint16_t textColor,
  uint16_t boxColor
) {
  float current = pfdDisplayAltitude();

  float halfRange = CONFIG_GLASS_ALT_HALF_RANGE;
  float majorStep = CONFIG_GLASS_ALT_MAJOR_STEP;
  float minorStep = CONFIG_GLASS_ALT_MINOR_STEP;

  float pxPerUnit = 100.0f / halfRange;

  // No panel fill: attitude background continues behind tape.
  display->drawFastVLine(x, y, h, lineColor);

  float start = floorf((current - halfRange) / minorStep) * minorStep;
  float end   = ceilf((current + halfRange) / minorStep) * minorStep;

  display->setTextSize(1);
  display->setTextColor(textColor);

  for (float v = start; v <= end + 0.01f; v += minorStep) {
    int yy = (int)lroundf(centreY - (v - current) * pxPerUnit);

    if (yy < y + 8 || yy > y + h - 8) {
      continue;
    }

    bool major =
      (fmodf(fabsf(v), majorStep) < 0.01f) ||
      (fmodf(fabsf(v), majorStep) > (majorStep - 0.01f));

    int tickLen = major ? 11 : 6;

    display->drawLine(x, yy, x + tickLen, yy, lineColor);

    if (major) {
      int iv = (int)lroundf(v);
      display->setCursor(x + 14, yy - 3);
      display->print(iv);
    }
  }

  // Current altitude box.
  int boxH = 24;
  int boxW = w + 9;
  int boxY = centreY - boxH / 2;
  int boxX = x - 9;

  display->fillRect(boxX, boxY, boxW, boxH, boxColor);
  display->drawRect(boxX, boxY, boxW, boxH, lineColor);

  display->setTextSize(2);
  display->setTextColor(textColor);
  display->setCursor(boxX + 4, boxY + 5);
  display->print((int)lroundf(current));

  display->setTextSize(1);
  display->setTextColor(textColor);
  display->setCursor(x + w - 8, y + h - 12);
  display->print(pfdAltUnitLabel());
}

// ----------------------------------------------------
// Ground speed box
// ----------------------------------------------------

static void drawPfdGroundSpeedBox(
  Arduino_GFX *display,
  int x,
  int y,
  uint16_t boxColor,
  uint16_t outlineColor,
  uint16_t textColor
) {
  const int boxW = 72;
  const int boxH = 28;

  int gs = (int)lroundf(pfdDisplayGroundSpeed());

  char gsText[8];
  snprintf(gsText, sizeof(gsText), "%d", gs);

  display->fillRect(x, y, boxW, boxH, boxColor);
  display->drawRect(x, y, boxW, boxH, outlineColor);

  display->setTextSize(1);
  display->setTextColor(textColor);
  display->setCursor(x + 7, y + 11);
  display->print("GS");

  int valueTextW = strlen(gsText) * 12;
  int valueX = x + boxW - valueTextW - 4;

  display->setTextSize(2);
  display->setTextColor(textColor);
  display->setCursor(valueX, y + 7);
  display->print(gsText);
}

// ----------------------------------------------------
// Main screen draw function
// ----------------------------------------------------

void drawArtificialHorizonScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad
) {
  const int W = 240;
  const int H = 280;

  const int tapeW = 36;

  const int centreX = W / 2;
  const int centreY = H / 2;

  const int attX0 = tapeW;
  const int attX1 = W - tapeW - 1;

  // Colours.
  const uint16_t skyColor      = rgb565(95, 135, 235);
  const uint16_t groundColor   = rgb565(145, 92, 28);

  const uint16_t horizonColor  = GFX_WHITE;
  const uint16_t ladderColor   = GFX_WHITE;
  const uint16_t tapeLineColor = GFX_WHITE;
  const uint16_t tapeTextColor = GFX_WHITE;

  const uint16_t boxBgColor    = GFX_BLACK;
  const uint16_t wingColor     = rgb565(235, 210, 55);

  const uint16_t purpleText    = rgb565(190, 70, 255);

  float pitchDeg = pitchRad * RAD_TO_DEG;

  float horizonCy =
    (float)centreY +
    pitchDeg * CONFIG_GLASS_AHI_PITCH_PIXELS_PER_DEG;

  // AHI background spans the full screen, including under tapes.
  drawPfdSkyGround(
    display,
    0,
    W - 1,
    0,
    H - 1,
    (float)centreX,
    horizonCy,
    rollRad,
    skyColor,
    groundColor
  );

  // Vertical tape boundaries, but no black side panels.
  display->drawFastVLine(attX0, 0, H, GFX_WHITE);
  display->drawFastVLine(attX1, 0, H, GFX_WHITE);

  // Roll scale.
  drawPfdRollScale(
    display,
    centreX,
    PFD_ROLL_ARC_CY,
    GFX_WHITE
  );

  // Moving roll pointer.
  drawPfdMovingRollPointer(
    display,
    centreX,
    PFD_ROLL_ARC_CY,
    rollRad,
    GFX_WHITE,
    GFX_BLACK
  );

  // Horizon and pitch ladder.
  drawPfdHorizonLine(
    display,
    (float)centreX,
    horizonCy,
    rollRad,
    horizonColor
  );

  drawPfdPitchLadder(
    display,
    W,
    H,
    (float)centreX,
    horizonCy,
    rollRad,
    ladderColor
  );

  // Fixed aircraft symbol.
  drawPfdAircraftSymbol(
    display,
    centreX,
    centreY,
    wingColor,
    GFX_BLACK
  );

  // Left airspeed tape. No black panel fill.
  drawPfdSpeedTape(
    display,
    0,
    0,
    tapeW,
    H,
    centreY,
    tapeLineColor,
    tapeTextColor,
    boxBgColor
  );

  // Right altitude tape. No black panel fill.
  drawPfdAltTape(
    display,
    W - tapeW,
    0,
    tapeW,
    H,
    centreY,
    tapeLineColor,
    tapeTextColor,
    boxBgColor
  );

  // Top heading tape with sky-colour backing.
  drawPfdHeadingTape(
    display,
    attX0,
    0,
    attX1 - attX0 + 1,
    CONFIG_GLASS_HEADING_TAPE_HEIGHT,
    centreX,
    skyColor,
    GFX_WHITE,
    GFX_WHITE,
    purpleText
  );

  // Bottom-left ground speed box.
  // Drawn last so nothing overwrites it.
  drawPfdGroundSpeedBox(
    display,
    0,
    H - 28,
    GFX_BLACK,
    GFX_WHITE,
    purpleText
  );
}