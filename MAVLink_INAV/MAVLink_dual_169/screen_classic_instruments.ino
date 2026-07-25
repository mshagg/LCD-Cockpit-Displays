// ----------------------------------------------------
// Screen: classic airspeed + altimeter
// ----------------------------------------------------
//
// Same layout style as screen_classic_ahi:
//   - top half: airspeed indicator
//   - lower half: altimeter
//   - four decorative screws around each gauge
//
// Airspeed gauge:
//   - max speed configurable in config.ino
//   - red / green / yellow / red operating arcs configurable in config.ino
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From MAVLink telemetry.
extern float airspeed;
extern float altitude_msl;
extern bool mavlinkVfrHudValid;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t INST_BLACK      = RGB565(0, 0, 0);
static const uint16_t INST_WHITE      = RGB565(255, 255, 255);
static const uint16_t INST_GREY       = RGB565(120, 120, 120);
static const uint16_t INST_DIM_GREY   = RGB565(60, 60, 60);
static const uint16_t INST_DARK_GREY  = RGB565(28, 28, 28);
static const uint16_t INST_FACE       = RGB565(32, 32, 34);
static const uint16_t INST_FACE_DARK  = RGB565(18, 18, 20);
static const uint16_t INST_YELLOW     = RGB565(255, 210, 60);
static const uint16_t INST_RED        = RGB565(235, 40, 40);
static const uint16_t INST_GREEN      = RGB565(70, 220, 90);
static const uint16_t INST_BLUE       = RGB565(40, 150, 255);
static const uint16_t INST_SCREW      = RGB565(92, 92, 92);
static const uint16_t INST_SCREW_HI   = RGB565(165, 165, 165);
static const uint16_t INST_SCREW_LO   = RGB565(35, 35, 35);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int INST_W = 240;

// Top airspeed gauge.
static const int INST_ASI_CX = 120;
static const int INST_ASI_CY = 69;
static const int INST_ASI_R  = 62;

// Lower altimeter gauge.
static const int INST_ALT_CX = 120;
static const int INST_ALT_CY = 211;
static const int INST_ALT_R  = 67;

// Screw positions.
static const int INST_TOP_SCREW_L = 35;
static const int INST_TOP_SCREW_R = 205;
static const int INST_TOP_SCREW_T = 16;
static const int INST_TOP_SCREW_B = 124;

static const int INST_BOT_SCREW_L = 35;
static const int INST_BOT_SCREW_R = 205;
static const int INST_BOT_SCREW_T = 153;
static const int INST_BOT_SCREW_B = 266;

// Airspeed gauge sweep.
static const float INST_ASI_START_ANGLE = -135.0f;
static const float INST_ASI_END_ANGLE   = 135.0f;
static const float INST_ASI_SWEEP       = INST_ASI_END_ANGLE - INST_ASI_START_ANGLE;

// Altimeter scaling.
static const float INST_ALT_FULL_SCALE = 1000.0f;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static int instTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void instPrintCentered(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = instTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  display->setTextSize(textSize);
  display->setTextColor(colour);
  display->setCursor(textX, y);
  display->print(text);
}

static float instClampFloat(float value, float minValue, float maxValue)
{
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

static void instPolarPoint(
  int cx,
  int cy,
  float angleDeg,
  float radius,
  int &x,
  int &y
) {
  float a = angleDeg * DEG_TO_RAD;

  x = cx + (int)lroundf(sinf(a) * radius);
  y = cy - (int)lroundf(cosf(a) * radius);
}

static float instAirspeedToAngle(float speed)
{
  float maxSpeed = CONFIG_CLASSIC_ASI_MAX_SPEED;

  if (maxSpeed <= 0.0f) {
    maxSpeed = 1.0f;
  }

  float clampedSpeed = instClampFloat(speed, 0.0f, maxSpeed);
  float fraction = clampedSpeed / maxSpeed;

  return INST_ASI_START_ANGLE + (fraction * INST_ASI_SWEEP);
}

static void instDrawArcBand(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  int thickness,
  float startAngle,
  float endAngle,
  uint16_t colour
) {
  if (endAngle < startAngle) {
    float temp = startAngle;
    startAngle = endAngle;
    endAngle = temp;
  }

  for (float a = startAngle; a <= endAngle; a += 1.0f) {
    for (int t = 0; t < thickness; t++) {
      int x;
      int y;

      instPolarPoint(
        cx,
        cy,
        a,
        radius - t,
        x,
        y
      );

      display->drawPixel(x, y, colour);
    }
  }
}

static void instDrawAirspeedBand(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r,
  float speedStart,
  float speedEnd,
  uint16_t colour
) {
  float maxSpeed = CONFIG_CLASSIC_ASI_MAX_SPEED;

  if (maxSpeed <= 0.0f) {
    return;
  }

  speedStart = instClampFloat(speedStart, 0.0f, maxSpeed);
  speedEnd = instClampFloat(speedEnd, 0.0f, maxSpeed);

  if (speedEnd <= speedStart) {
    return;
  }

  float startAngle = instAirspeedToAngle(speedStart);
  float endAngle = instAirspeedToAngle(speedEnd);

  instDrawArcBand(
    display,
    cx,
    cy,
    r - 13,
    4,
    startAngle,
    endAngle,
    colour
  );
}

static void instDrawScrew(
  Arduino_Canvas *display,
  int x,
  int y,
  bool slotForward
) {
  display->fillCircle(x + 1, y + 1, 6, INST_BLACK);
  display->fillCircle(x, y, 6, INST_SCREW_LO);
  display->fillCircle(x, y, 5, INST_SCREW);
  display->drawCircle(x, y, 6, INST_SCREW_HI);
  display->drawCircle(x, y, 3, INST_DIM_GREY);

  if (slotForward) {
    display->drawLine(x - 4, y - 2, x + 4, y + 2, INST_BLACK);
    display->drawLine(x - 3, y - 2, x + 4, y + 1, INST_DARK_GREY);
    display->drawLine(x - 4, y - 1, x + 3, y + 2, INST_SCREW_HI);
  } else {
    display->drawLine(x - 4, y + 2, x + 4, y - 2, INST_BLACK);
    display->drawLine(x - 3, y + 2, x + 4, y - 1, INST_DARK_GREY);
    display->drawLine(x - 4, y + 1, x + 3, y - 2, INST_SCREW_HI);
  }
}

static void instDrawDecorativeScrews(
  Arduino_Canvas *display
) {
  // Top airspeed screws.
  instDrawScrew(display, INST_TOP_SCREW_L, INST_TOP_SCREW_T, true);
  instDrawScrew(display, INST_TOP_SCREW_R, INST_TOP_SCREW_T, false);
  instDrawScrew(display, INST_TOP_SCREW_L, INST_TOP_SCREW_B, false);
  instDrawScrew(display, INST_TOP_SCREW_R, INST_TOP_SCREW_B, true);

  // Lower altimeter screws.
  instDrawScrew(display, INST_BOT_SCREW_L, INST_BOT_SCREW_T, false);
  instDrawScrew(display, INST_BOT_SCREW_R, INST_BOT_SCREW_T, true);
  instDrawScrew(display, INST_BOT_SCREW_L, INST_BOT_SCREW_B, true);
  instDrawScrew(display, INST_BOT_SCREW_R, INST_BOT_SCREW_B, false);
}

static void instDrawGaugeFace(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r
) {
  display->fillCircle(cx, cy, r, INST_FACE);
  display->fillCircle(cx, cy, r - 8, INST_FACE_DARK);

  display->drawCircle(cx, cy, r, INST_WHITE);
  display->drawCircle(cx, cy, r - 2, INST_BLACK);
  display->drawCircle(cx, cy, r - 5, INST_WHITE);
}

static void instDrawNeedle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float angleDeg,
  int length,
  uint16_t colour
) {
  int tipX;
  int tipY;
  int tailX;
  int tailY;
  int leftX;
  int leftY;
  int rightX;
  int rightY;

  instPolarPoint(cx, cy, angleDeg, length, tipX, tipY);
  instPolarPoint(cx, cy, angleDeg + 180.0f, 10, tailX, tailY);
  instPolarPoint(cx, cy, angleDeg - 90.0f, 4, leftX, leftY);
  instPolarPoint(cx, cy, angleDeg + 90.0f, 4, rightX, rightY);

  display->fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, colour);
  display->drawLine(cx, cy, tipX, tipY, colour);
  display->drawLine(cx - 1, cy, tipX - 1, tipY, colour);
  display->drawLine(cx + 1, cy, tipX + 1, tipY, colour);
  display->drawLine(cx, cy, tailX, tailY, colour);

  display->fillCircle(cx, cy, 6, INST_BLACK);
  display->fillCircle(cx, cy, 4, colour);
}

// ----------------------------------------------------
// Airspeed indicator
// ----------------------------------------------------

static void instDrawAirspeedColourBands(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r
) {
  instDrawAirspeedBand(
    display,
    cx,
    cy,
    r,
    0.0f,
    CONFIG_CLASSIC_ASI_STALL_SPEED,
    INST_RED
  );

  instDrawAirspeedBand(
    display,
    cx,
    cy,
    r,
    CONFIG_CLASSIC_ASI_STALL_SPEED,
    CONFIG_CLASSIC_ASI_OPERATING_MAX_SPEED,
    INST_GREEN
  );

  instDrawAirspeedBand(
    display,
    cx,
    cy,
    r,
    CONFIG_CLASSIC_ASI_OPERATING_MAX_SPEED,
    CONFIG_CLASSIC_ASI_NEVER_EXCEED_SPEED,
    INST_YELLOW
  );

  instDrawAirspeedBand(
    display,
    cx,
    cy,
    r,
    CONFIG_CLASSIC_ASI_NEVER_EXCEED_SPEED,
    CONFIG_CLASSIC_ASI_MAX_SPEED,
    INST_RED
  );
}

static void instDrawAirspeedGauge(
  Arduino_Canvas *display
) {
  int cx = INST_ASI_CX;
  int cy = INST_ASI_CY;
  int r = INST_ASI_R;

  instDrawGaugeFace(display, cx, cy, r);
  instDrawAirspeedColourBands(display, cx, cy, r);

  float maxSpeed = CONFIG_CLASSIC_ASI_MAX_SPEED;

  if (maxSpeed <= 0.0f) {
    maxSpeed = 1.0f;
  }

  float majorStep = 20.0f;
  float minorStep = 10.0f;

  if (maxSpeed > 160.0f) {
    majorStep = 40.0f;
    minorStep = 20.0f;
  }

  if (maxSpeed <= 80.0f) {
    majorStep = 10.0f;
    minorStep = 5.0f;
  }

  for (float speed = 0.0f; speed <= maxSpeed + 0.1f; speed += minorStep) {
    float angle = instAirspeedToAngle(speed);

    bool majorTick =
      fabsf(fmodf(speed, majorStep)) < 0.1f ||
      fabsf(speed - maxSpeed) < 0.1f;

    int tickLen = majorTick ? 12 : 6;

    int x1;
    int y1;
    int x2;
    int y2;

    instPolarPoint(cx, cy, angle, r - 11, x1, y1);
    instPolarPoint(cx, cy, angle, r - 11 - tickLen, x2, y2);

    display->drawLine(x1, y1, x2, y2, INST_WHITE);

    if (majorTick) {
      char label[8];
      snprintf(label, sizeof(label), "%ld", lroundf(speed));

      int lx;
      int ly;
      instPolarPoint(cx, cy, angle, r - 31, lx, ly);

      display->setTextSize(1);
      display->setTextColor(INST_WHITE);
      display->setCursor(
        lx - (instTextWidth(label, 1) / 2),
        ly - 4
      );
      display->print(label);
    }
  }

  instPrintCentered(display, cx - 50, cy - 12, 100, "AIRSPEED", 1, INST_GREY);
  instPrintCentered(display, cx - 50, cy + 27, 100, CONFIG_GLASS_SPEED_LABEL, 1, INST_GREY);

  float speedDisplayed = airspeed * CONFIG_GLASS_SPEED_SCALE;

  if (!mavlinkVfrHudValid) {
    speedDisplayed = 0.0f;
  }

  float needleAngle = instAirspeedToAngle(speedDisplayed);

  instDrawNeedle(display, cx, cy, needleAngle, r - 21, INST_YELLOW);

  char value[12];

  if (mavlinkVfrHudValid) {
    snprintf(value, sizeof(value), "%ld", lroundf(speedDisplayed));
  } else {
    snprintf(value, sizeof(value), "---");
  }

  instPrintCentered(display, cx - 35, cy + 39, 70, value, 2, INST_WHITE);
}

// ----------------------------------------------------
// Altimeter
// ----------------------------------------------------

static void instDrawAltimeterGauge(
  Arduino_Canvas *display
) {
  int cx = INST_ALT_CX;
  int cy = INST_ALT_CY;
  int r = INST_ALT_R;

  instDrawGaugeFace(display, cx, cy, r);

  for (int altMark = 0; altMark < 1000; altMark += 50) {
    float fraction = (float)altMark / INST_ALT_FULL_SCALE;
    float angle = fraction * 360.0f;

    int tickLen = 6;

    if ((altMark % 100) == 0) {
      tickLen = 13;
    }

    int x1;
    int y1;
    int x2;
    int y2;

    instPolarPoint(cx, cy, angle, r - 11, x1, y1);
    instPolarPoint(cx, cy, angle, r - 11 - tickLen, x2, y2);

    display->drawLine(x1, y1, x2, y2, INST_WHITE);
  }

  // 0-9 labels around the altimeter.
  for (int i = 0; i <= 9; i++) {
    float angle = ((float)i / 10.0f) * 360.0f;

    char label[3];
    snprintf(label, sizeof(label), "%d", i);

    int lx;
    int ly;
    instPolarPoint(cx, cy, angle, r - 33, lx, ly);

    display->setTextSize(2);
    display->setTextColor(INST_WHITE);
    display->setCursor(
      lx - (instTextWidth(label, 2) / 2),
      ly - 8
    );
    display->print(label);
  }

  instPrintCentered(display, cx - 50, cy - 15, 100, "ALT", 1, INST_GREY);
  instPrintCentered(display, cx - 50, cy + 28, 100, CONFIG_GLASS_ALT_LABEL, 1, INST_GREY);

  float altDisplayed = altitude_msl * CONFIG_GLASS_ALT_SCALE;

  if (!mavlinkVfrHudValid) {
    altDisplayed = 0.0f;
  }

  float altWrapped = fmodf(altDisplayed, INST_ALT_FULL_SCALE);

  if (altWrapped < 0.0f) {
    altWrapped += INST_ALT_FULL_SCALE;
  }

  float needleAngle =
    (altWrapped / INST_ALT_FULL_SCALE) * 360.0f;

  instDrawNeedle(display, cx, cy, needleAngle, r - 20, INST_YELLOW);

  char value[14];

  if (mavlinkVfrHudValid) {
    snprintf(
      value,
      sizeof(value),
      "%ld%s",
      lroundf(altDisplayed),
      CONFIG_GLASS_ALT_LABEL
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  instPrintCentered(display, cx - 45, cy + 41, 90, value, 2, INST_WHITE);
}

// ----------------------------------------------------
// Main draw function called from main tab
// ----------------------------------------------------

void drawClassicInstrumentsScreen(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
) {
  (void)rollRad;
  (void)pitchRad;

  display->fillScreen(INST_BLACK);

  display->drawFastHLine(0, 139, INST_W, INST_DIM_GREY);

  instDrawAirspeedGauge(display);
  instDrawAltimeterGauge(display);
  instDrawDecorativeScrews(display);
}