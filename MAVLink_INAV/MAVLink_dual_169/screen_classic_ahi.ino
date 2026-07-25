// ----------------------------------------------------
// Screen: classic AHI + heading indicator
// ----------------------------------------------------
//
// Top half:
//   - compact classic artificial horizon
//   - four decorative screws around gauge
//
// Lower half:
//   - clean round heading indicator
//   - N/E/S/W only
//   - smaller fixed aircraft symbol
//   - four decorative screws around gauge
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From MAVLink telemetry.
extern int16_t heading_deg;
extern bool mavlinkVfrHudValid;
extern float yaw;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t CLASSIC_BLACK      = RGB565(0, 0, 0);
static const uint16_t CLASSIC_WHITE      = RGB565(255, 255, 255);
static const uint16_t CLASSIC_GREY       = RGB565(120, 120, 120);
static const uint16_t CLASSIC_DIM_GREY   = RGB565(60, 60, 60);
static const uint16_t CLASSIC_DARK_GREY  = RGB565(28, 28, 28);
static const uint16_t CLASSIC_SKY        = RGB565(42, 94, 150);
static const uint16_t CLASSIC_GROUND     = RGB565(115, 72, 38);
static const uint16_t CLASSIC_YELLOW     = RGB565(255, 210, 60);
static const uint16_t CLASSIC_CARD       = RGB565(52, 45, 43);
static const uint16_t CLASSIC_CARD_DARK  = RGB565(34, 30, 30);
static const uint16_t CLASSIC_SCREW      = RGB565(92, 92, 92);
static const uint16_t CLASSIC_SCREW_HI   = RGB565(165, 165, 165);
static const uint16_t CLASSIC_SCREW_LO   = RGB565(35, 35, 35);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int CLASSIC_W = 240;
static const int CLASSIC_H = 280;

// Top compact AHI.
static const int CLASSIC_AHI_CX = 120;
static const int CLASSIC_AHI_CY = 69;
static const int CLASSIC_AHI_R  = 62;

// Lower heading indicator.
static const int CLASSIC_HDG_CX = 120;
static const int CLASSIC_HDG_CY = 211;
static const int CLASSIC_HDG_R  = 67;

// Screw positions.
static const int CLASSIC_TOP_SCREW_L = 35;
static const int CLASSIC_TOP_SCREW_R = 205;
static const int CLASSIC_TOP_SCREW_T = 16;
static const int CLASSIC_TOP_SCREW_B = 124;

static const int CLASSIC_BOT_SCREW_L = 35;
static const int CLASSIC_BOT_SCREW_R = 205;
static const int CLASSIC_BOT_SCREW_T = 153;
static const int CLASSIC_BOT_SCREW_B = 266;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static int classicTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void classicDrawCircleOutline(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r,
  uint16_t colour
) {
  display->drawCircle(cx, cy, r, colour);
  display->drawCircle(cx, cy, r - 1, colour);
}

static void classicDrawRotatedLine(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float x1,
  float y1,
  float x2,
  float y2,
  uint16_t colour
) {
  float s = sinf(rollRad);
  float c = cosf(rollRad);

  int sx1 = cx + (int)lroundf((x1 * c) - (y1 * s));
  int sy1 = cy + (int)lroundf((x1 * s) + (y1 * c));

  int sx2 = cx + (int)lroundf((x2 * c) - (y2 * s));
  int sy2 = cy + (int)lroundf((x2 * s) + (y2 * c));

  display->drawLine(sx1, sy1, sx2, sy2, colour);
}

static void classicDrawRotatedText(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float cardAngleDeg,
  float radius,
  const char *text,
  int textSize,
  uint16_t colour
) {
  float a = cardAngleDeg * DEG_TO_RAD;

  int x = cx + (int)lroundf(sinf(a) * radius);
  int y = cy - (int)lroundf(cosf(a) * radius);

  int textW = classicTextWidth(text, textSize);
  int textH = 8 * textSize;

  display->setTextSize(textSize);
  display->setTextColor(colour);
  display->setCursor(x - (textW / 2), y - (textH / 2));
  display->print(text);
}

static int classicCurrentHeadingDeg()
{
  int hdg;

  if (mavlinkVfrHudValid) {
    hdg = heading_deg;
  } else {
    hdg = (int)lroundf(yaw * RAD_TO_DEG);
  }

  while (hdg < 0) {
    hdg += 360;
  }

  while (hdg >= 360) {
    hdg -= 360;
  }

  return hdg;
}

// ----------------------------------------------------
// Decorative screws
// ----------------------------------------------------

static void classicDrawScrew(
  Arduino_Canvas *display,
  int x,
  int y,
  bool slotForward
) {
  display->fillCircle(x + 1, y + 1, 6, CLASSIC_BLACK);
  display->fillCircle(x, y, 6, CLASSIC_SCREW_LO);
  display->fillCircle(x, y, 5, CLASSIC_SCREW);
  display->drawCircle(x, y, 6, CLASSIC_SCREW_HI);
  display->drawCircle(x, y, 3, CLASSIC_DIM_GREY);

  if (slotForward) {
    display->drawLine(x - 4, y - 2, x + 4, y + 2, CLASSIC_BLACK);
    display->drawLine(x - 3, y - 2, x + 4, y + 1, CLASSIC_DARK_GREY);
    display->drawLine(x - 4, y - 1, x + 3, y + 2, CLASSIC_SCREW_HI);
  } else {
    display->drawLine(x - 4, y + 2, x + 4, y - 2, CLASSIC_BLACK);
    display->drawLine(x - 3, y + 2, x + 4, y - 1, CLASSIC_DARK_GREY);
    display->drawLine(x - 4, y + 1, x + 3, y - 2, CLASSIC_SCREW_HI);
  }
}

static void classicDrawDecorativeScrews(
  Arduino_Canvas *display
) {
  // Top AHI screws.
  classicDrawScrew(display, CLASSIC_TOP_SCREW_L, CLASSIC_TOP_SCREW_T, true);
  classicDrawScrew(display, CLASSIC_TOP_SCREW_R, CLASSIC_TOP_SCREW_T, false);
  classicDrawScrew(display, CLASSIC_TOP_SCREW_L, CLASSIC_TOP_SCREW_B, false);
  classicDrawScrew(display, CLASSIC_TOP_SCREW_R, CLASSIC_TOP_SCREW_B, true);

  // Lower heading indicator screws.
  classicDrawScrew(display, CLASSIC_BOT_SCREW_L, CLASSIC_BOT_SCREW_T, false);
  classicDrawScrew(display, CLASSIC_BOT_SCREW_R, CLASSIC_BOT_SCREW_T, true);
  classicDrawScrew(display, CLASSIC_BOT_SCREW_L, CLASSIC_BOT_SCREW_B, true);
  classicDrawScrew(display, CLASSIC_BOT_SCREW_R, CLASSIC_BOT_SCREW_B, false);
}

// ----------------------------------------------------
// Top AHI
// ----------------------------------------------------

static void classicDrawAhiBackground(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
) {
  float s = sinf(rollRad);
  float c = cosf(rollRad);

  float pitchDeg = pitchRad * RAD_TO_DEG;
  float pitchOffset = pitchDeg * 2.4f;

  for (int y = -CLASSIC_AHI_R; y <= CLASSIC_AHI_R; y++) {
    for (int x = -CLASSIC_AHI_R; x <= CLASSIC_AHI_R; x++) {
      if ((x * x) + (y * y) > (CLASSIC_AHI_R * CLASSIC_AHI_R)) {
        continue;
      }

      float levelY = (x * s) + (y * c);

      uint16_t colour =
        (levelY >= pitchOffset) ? CLASSIC_GROUND : CLASSIC_SKY;

      display->drawPixel(
        CLASSIC_AHI_CX + x,
        CLASSIC_AHI_CY + y,
        colour
      );
    }
  }

  classicDrawRotatedLine(
    display,
    CLASSIC_AHI_CX,
    CLASSIC_AHI_CY,
    rollRad,
    -CLASSIC_AHI_R,
    pitchOffset,
    CLASSIC_AHI_R,
    pitchOffset,
    CLASSIC_WHITE
  );

  classicDrawRotatedLine(
    display,
    CLASSIC_AHI_CX,
    CLASSIC_AHI_CY,
    rollRad,
    -CLASSIC_AHI_R,
    pitchOffset + 1,
    CLASSIC_AHI_R,
    pitchOffset + 1,
    CLASSIC_WHITE
  );
}

static void classicDrawPitchLadder(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
) {
  float pitchDeg = pitchRad * RAD_TO_DEG;
  float pitchOffset = pitchDeg * 2.4f;

  for (int mark = -30; mark <= 30; mark += 10) {
    if (mark == 0) {
      continue;
    }

    float y = pitchOffset - ((float)mark * 2.4f);

    if (fabsf(y) > CLASSIC_AHI_R - 10) {
      continue;
    }

    int halfLen = (abs(mark) % 20 == 0) ? 24 : 16;

    classicDrawRotatedLine(
      display,
      CLASSIC_AHI_CX,
      CLASSIC_AHI_CY,
      rollRad,
      -halfLen,
      y,
      halfLen,
      y,
      CLASSIC_WHITE
    );

    classicDrawRotatedLine(
      display,
      CLASSIC_AHI_CX,
      CLASSIC_AHI_CY,
      rollRad,
      -halfLen,
      y + 1,
      halfLen,
      y + 1,
      CLASSIC_WHITE
    );
  }
}

static void classicDrawBankScale(
  Arduino_Canvas *display,
  float rollRad
) {
  int cx = CLASSIC_AHI_CX;
  int cy = CLASSIC_AHI_CY;
  int rOuter = CLASSIC_AHI_R + 6;

  for (int deg = -60; deg <= 60; deg += 10) {
    int tickLen = 5;

    if (deg == 0 || abs(deg) == 30 || abs(deg) == 60) {
      tickLen = 9;
    }

    float a = (float)deg * DEG_TO_RAD;

    int x1 = cx + (int)lroundf(sinf(a) * (rOuter - tickLen));
    int y1 = cy - (int)lroundf(cosf(a) * (rOuter - tickLen));

    int x2 = cx + (int)lroundf(sinf(a) * rOuter);
    int y2 = cy - (int)lroundf(cosf(a) * rOuter);

    display->drawLine(x1, y1, x2, y2, CLASSIC_WHITE);
  }

  display->fillTriangle(
    cx,
    cy - CLASSIC_AHI_R - 2,
    cx - 6,
    cy - CLASSIC_AHI_R - 14,
    cx + 6,
    cy - CLASSIC_AHI_R - 14,
    CLASSIC_WHITE
  );

  float pointerAngle =
    rollRad * CONFIG_AHI_ROLL_POINTER_SIGN;

  int tipX =
    cx + (int)lroundf(sinf(pointerAngle) * (CLASSIC_AHI_R + 4));
  int tipY =
    cy - (int)lroundf(cosf(pointerAngle) * (CLASSIC_AHI_R + 4));

  int baseX =
    cx + (int)lroundf(sinf(pointerAngle) * (CLASSIC_AHI_R - 8));
  int baseY =
    cy - (int)lroundf(cosf(pointerAngle) * (CLASSIC_AHI_R - 8));

  display->drawLine(baseX, baseY, tipX, tipY, CLASSIC_YELLOW);
  display->drawLine(baseX - 1, baseY, tipX - 1, tipY, CLASSIC_YELLOW);
  display->drawLine(baseX + 1, baseY, tipX + 1, tipY, CLASSIC_YELLOW);
}

static void classicDrawAircraftSymbol(
  Arduino_Canvas *display
) {
  int cx = CLASSIC_AHI_CX;
  int cy = CLASSIC_AHI_CY;

  display->drawFastHLine(cx - 35, cy, 25, CLASSIC_YELLOW);
  display->drawFastHLine(cx + 10, cy, 25, CLASSIC_YELLOW);
  display->drawFastHLine(cx - 35, cy + 1, 25, CLASSIC_YELLOW);
  display->drawFastHLine(cx + 10, cy + 1, 25, CLASSIC_YELLOW);

  display->drawLine(cx, cy - 6, cx, cy + 9, CLASSIC_YELLOW);
  display->drawLine(cx - 6, cy + 9, cx + 6, cy + 9, CLASSIC_YELLOW);

  display->fillCircle(cx, cy, 2, CLASSIC_YELLOW);
}

static void classicDrawTopAhi(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
) {
  classicDrawAhiBackground(display, rollRad, pitchRad);
  classicDrawPitchLadder(display, rollRad, pitchRad);

  classicDrawCircleOutline(
    display,
    CLASSIC_AHI_CX,
    CLASSIC_AHI_CY,
    CLASSIC_AHI_R,
    CLASSIC_WHITE
  );

  classicDrawBankScale(display, rollRad);
  classicDrawAircraftSymbol(display);
}

// ----------------------------------------------------
// Lower heading indicator
// ----------------------------------------------------

static void classicDrawHeadingCard(
  Arduino_Canvas *display
) {
  int cx = CLASSIC_HDG_CX;
  int cy = CLASSIC_HDG_CY;
  int r = CLASSIC_HDG_R;

  int heading = classicCurrentHeadingDeg();

  display->fillCircle(cx, cy, r, CLASSIC_CARD);
  display->fillCircle(cx, cy, r - 9, CLASSIC_CARD_DARK);

  display->drawCircle(cx, cy, r, CLASSIC_WHITE);
  display->drawCircle(cx, cy, r - 2, CLASSIC_BLACK);
  display->drawCircle(cx, cy, r - 5, CLASSIC_WHITE);

  for (int bearing = 0; bearing < 360; bearing += 10) {
    float cardAngle = (float)(bearing - heading);
    float a = cardAngle * DEG_TO_RAD;

    int tickLen = 6;

    if ((bearing % 30) == 0) {
      tickLen = 13;
    }

    int x1 = cx + (int)lroundf(sinf(a) * (r - 12));
    int y1 = cy - (int)lroundf(cosf(a) * (r - 12));

    int x2 = cx + (int)lroundf(sinf(a) * (r - 12 - tickLen));
    int y2 = cy - (int)lroundf(cosf(a) * (r - 12 - tickLen));

    display->drawLine(x1, y1, x2, y2, CLASSIC_WHITE);
  }

  classicDrawRotatedText(display, cx, cy, 0 - heading,   r - 35, "N", 2, CLASSIC_WHITE);
  classicDrawRotatedText(display, cx, cy, 90 - heading,  r - 35, "E", 2, CLASSIC_WHITE);
  classicDrawRotatedText(display, cx, cy, 180 - heading, r - 35, "S", 2, CLASSIC_WHITE);
  classicDrawRotatedText(display, cx, cy, 270 - heading, r - 35, "W", 2, CLASSIC_WHITE);

  display->fillTriangle(
    cx,
    cy - r + 12,
    cx - 7,
    cy - r + 30,
    cx + 7,
    cy - r + 30,
    CLASSIC_WHITE
  );
}

static void classicDrawHeadingAircraft(
  Arduino_Canvas *display
) {
  int cx = CLASSIC_HDG_CX;
  int cy = CLASSIC_HDG_CY;

  display->fillTriangle(
    cx,
    cy - 18,
    cx - 3,
    cy - 5,
    cx + 3,
    cy - 5,
    CLASSIC_WHITE
  );

  display->fillRect(cx - 1, cy - 5, 3, 23, CLASSIC_WHITE);
  display->fillRect(cx - 24, cy - 3, 48, 5, CLASSIC_WHITE);
  display->fillRect(cx - 10, cy + 20, 20, 4, CLASSIC_WHITE);
  display->fillRect(cx - 1, cy + 15, 3, 7, CLASSIC_WHITE);
}

static void classicDrawHeadingIndicator(
  Arduino_Canvas *display
) {
  classicDrawHeadingCard(display);
  classicDrawHeadingAircraft(display);
}

// ----------------------------------------------------
// Main draw function called from main tab
// ----------------------------------------------------

void drawClassicAhiScreen(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
) {
  display->fillScreen(CLASSIC_BLACK);

  display->drawFastHLine(0, 139, CLASSIC_W, CLASSIC_DIM_GREY);

  classicDrawTopAhi(display, rollRad, pitchRad);
  classicDrawHeadingIndicator(display);
  classicDrawDecorativeScrews(display);
}