// ----------------------------------------------------
// Screen design: analogue flight gauges
// ----------------------------------------------------
//
// Standalone ESP32-S3 + 2.0 inch ST7789VW fork.
//
// Display orientation expected:
//   320 x 240 landscape
//
// Gauge ranges are configured in config.ino.
//
// Gauge style:
//   - simplified analogue round dial
//   - clean square gauge panels
//   - decorative screw heads
//   - outward scale labels
//   - faux-bold labels and numbers
//   - very thin coloured caution/warning arcs
//   - moving needle
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include "fast_trig.h"
#include "frame_template.h"

// ----------------------------------------------------
// External configuration
// ----------------------------------------------------

extern const float CONFIG_ANALOGUE_SPEED_MIN_KMH;
extern const float CONFIG_ANALOGUE_SPEED_MAX_KMH;

extern const float CONFIG_ANALOGUE_ALT_MIN_M;
extern const float CONFIG_ANALOGUE_ALT_MAX_M;

extern const float CONFIG_ANALOGUE_CELL_MIN_V;
extern const float CONFIG_ANALOGUE_CELL_MAX_V;

extern const float CONFIG_ANALOGUE_CURRENT_MIN_A;
extern const float CONFIG_ANALOGUE_CURRENT_MAX_A;

extern const float CONFIG_ANALOGUE_REMAIN_MIN_PERCENT;
extern const float CONFIG_ANALOGUE_REMAIN_MAX_PERCENT;

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

// ----------------------------------------------------
// Colours
// ----------------------------------------------------
//
// Precomputed RGB565 values. No RGB565() macro calls are
// used in this tab.

static const uint16_t AG_BLACK        = 0x0000;
static const uint16_t AG_FACE         = 0x0041;

static const uint16_t AG_PANEL        = 0x1082;
static const uint16_t AG_PANEL_EDGE   = 0x6B4D;
static const uint16_t AG_PANEL_DARK   = 0x2104;
static const uint16_t AG_PANEL_LIGHT  = 0x8C51;
static const uint16_t AG_PANEL_GAP    = 0x18E3;

static const uint16_t AG_BEZEL_DARK   = 0x2125;
static const uint16_t AG_BEZEL_MID    = 0x5B0C;
static const uint16_t AG_BEZEL_LIGHT  = 0x9D14;

static const uint16_t AG_WHITE        = 0xEF7D;
static const uint16_t AG_SOFT_WHITE   = 0xAD96;
static const uint16_t AG_GREY         = 0x5B0C;
static const uint16_t AG_DARK_GREY    = 0x2145;

static const uint16_t AG_GREEN        = 0x4728;
static const uint16_t AG_YELLOW       = 0xFE64;
static const uint16_t AG_RED          = 0xF1A5;

static const uint16_t AG_NEEDLE       = 0xF79D;
static const uint16_t AG_NEEDLE_EDGE  = 0x73CF;
static const uint16_t AG_HUB          = 0x2966;

// ----------------------------------------------------
// Gauge geometry
// ----------------------------------------------------

static const float AG_PI = 3.14159265f;
static const float AG_DEG_TO_RAD = 0.0174532925f;

static const float AG_START_ANGLE_DEG = 140.0f;
static const float AG_END_ANGLE_DEG   = 400.0f;

static uint16_t *agFrameTemplate = nullptr;
static bool agFrameTemplateReady = false;

// ----------------------------------------------------
// Forward declarations
// ----------------------------------------------------

static float agClampFloat(
  float value,
  float low,
  float high
);

static float agRangePoint(
  float minValue,
  float maxValue,
  float fraction
);

static float agValueToAngleDeg(
  float value,
  float minValue,
  float maxValue
);

static void agPolarPoint(
  int cx,
  int cy,
  float angleDeg,
  float radius,
  int *x,
  int *y
);

static void agFormatValue(
  char *buffer,
  size_t bufferSize,
  float value,
  bool valid,
  uint8_t decimals
);

static void agPrintCenteredBold(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  const char *text,
  uint8_t textSize,
  uint16_t colour,
  uint16_t bg
);

static void agPrintAtBold(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text,
  uint8_t textSize,
  uint16_t colour,
  uint16_t bg
);

static void agDrawPanelScrew(
  Arduino_GFX *display,
  int x,
  int y
);

static void agDrawGaugePanel(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  bool largePanel
);

static void agDrawGaugeBody(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r
);

static void agDrawColourBand(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float bandStartValue,
  float bandEndValue,
  uint16_t colour
);

static void agDrawTicks(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float majorStep,
  float minorStep,
  uint8_t labelDecimals
);

static void agDrawNeedle(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float value,
  bool valid
);

static void agDrawDigitalText(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  const char *title,
  float value,
  bool valid,
  uint8_t valueDecimals
);

static void agDrawGauge(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  const char *title,
  float minValue,
  float maxValue,
  float value,
  bool valid,
  float majorStep,
  float minorStep,
  uint8_t labelDecimals,
  uint8_t valueDecimals,
  uint8_t gaugeType,
  bool staticPass
);

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static float agClampFloat(
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

static float agRangePoint(
  float minValue,
  float maxValue,
  float fraction
) {
  return minValue + ((maxValue - minValue) * fraction);
}

static float agValueToAngleDeg(
  float value,
  float minValue,
  float maxValue
) {
  if (maxValue <= minValue) {
    return AG_START_ANGLE_DEG;
  }

  float clampedValue =
    agClampFloat(value, minValue, maxValue);

  float fraction =
    (clampedValue - minValue) /
    (maxValue - minValue);

  return AG_START_ANGLE_DEG +
         fraction * (AG_END_ANGLE_DEG - AG_START_ANGLE_DEG);
}

static void agPolarPoint(
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

static void agFormatValue(
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

static void agPrintCenteredBold(
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

  display->setTextColor(colour);
  display->setCursor(drawX + 1, y);
  display->print(text);
}

static void agPrintAtBold(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text,
  uint8_t textSize,
  uint16_t colour,
  uint16_t bg
) {
  display->setTextSize(textSize);

  display->setTextColor(colour, bg);
  display->setCursor(x, y);
  display->print(text);

  display->setTextColor(colour);
  display->setCursor(x + 1, y);
  display->print(text);
}

// ----------------------------------------------------
// Gauge panel and body
// ----------------------------------------------------

static void agDrawPanelScrew(
  Arduino_GFX *display,
  int x,
  int y
) {
  display->fillCircle(x, y, 3, AG_PANEL_DARK);
  display->drawCircle(x, y, 3, AG_PANEL_LIGHT);
  display->fillCircle(x, y, 1, AG_BEZEL_LIGHT);

  display->drawLine(x - 2, y, x + 2, y, AG_PANEL_EDGE);
}

static void agDrawGaugePanel(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  bool largePanel
) {
  int panelX;
  int panelY;
  int panelW;
  int panelH;

  if (largePanel) {
    panelX = cx - 64;
    panelY = cy - 57;
    panelW = 128;
    panelH = 124;
  } else {
    panelX = cx - 50;
    panelY = cy - 48;
    panelW = 100;
    panelH = 96;
  }

  display->fillRect(panelX, panelY, panelW, panelH, AG_PANEL);

  // Clean square border. No black bottom/right shadow lines.
  display->drawRect(panelX, panelY, panelW, panelH, AG_PANEL_EDGE);
  display->drawRect(panelX + 1, panelY + 1, panelW - 2, panelH - 2, AG_PANEL_DARK);

  display->drawLine(
    panelX + 2,
    panelY + 2,
    panelX + panelW - 3,
    panelY + 2,
    AG_PANEL_LIGHT
  );

  display->drawLine(
    panelX + 2,
    panelY + 2,
    panelX + 2,
    panelY + panelH - 3,
    AG_PANEL_LIGHT
  );

  display->drawLine(
    panelX + 2,
    panelY + panelH - 3,
    panelX + panelW - 3,
    panelY + panelH - 3,
    AG_PANEL_DARK
  );

  display->drawLine(
    panelX + panelW - 3,
    panelY + 2,
    panelX + panelW - 3,
    panelY + panelH - 3,
    AG_PANEL_DARK
  );

  agDrawPanelScrew(display, panelX + 8, panelY + 8);
  agDrawPanelScrew(display, panelX + panelW - 9, panelY + 8);
  agDrawPanelScrew(display, panelX + 8, panelY + panelH - 9);
  agDrawPanelScrew(display, panelX + panelW - 9, panelY + panelH - 9);
}

static void agDrawGaugeBody(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r
) {
  display->fillCircle(cx, cy, r + 3, AG_BEZEL_DARK);
  display->drawCircle(cx, cy, r + 3, AG_BEZEL_LIGHT);
  display->drawCircle(cx, cy, r + 1, AG_BEZEL_MID);

  display->fillCircle(cx, cy, r - 2, AG_FACE);
  display->drawCircle(cx, cy, r - 3, AG_DARK_GREY);
}

static void agDrawColourBand(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float bandStartValue,
  float bandEndValue,
  uint16_t colour
) {
  float startDeg =
    agValueToAngleDeg(bandStartValue, minValue, maxValue);

  float endDeg =
    agValueToAngleDeg(bandEndValue, minValue, maxValue);

  if (endDeg < startDeg) {
    float temp = startDeg;
    startDeg = endDeg;
    endDeg = temp;
  }

  float radius = (float)r - 9.0f;

  for (float angleDeg = startDeg;
       angleDeg <= endDeg;
       angleDeg += 1.0f) {
    int x;
    int y;

    agPolarPoint(cx, cy, angleDeg, radius, &x, &y);
    display->drawPixel(x, y, colour);
  }
}

static void agDrawTicks(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  float minValue,
  float maxValue,
  float majorStep,
  float minorStep,
  uint8_t labelDecimals
) {
  if (minorStep <= 0.0f || majorStep <= 0.0f) {
    return;
  }

  for (float value = minValue;
       value <= maxValue + (minorStep * 0.5f);
       value += minorStep) {
    float majorRemainder =
      fmodf(value - minValue, majorStep);

    bool isMajor =
      fabsf(majorRemainder) < 0.01f ||
      fabsf(majorRemainder - majorStep) < 0.01f;

    float angleDeg =
      agValueToAngleDeg(value, minValue, maxValue);

    int tickOuterX;
    int tickOuterY;
    int tickInnerX;
    int tickInnerY;

    float outerRadius = (float)r - 6.0f;
    float innerRadius = isMajor ?
      (float)r - 16.0f :
      (float)r - 11.0f;

    agPolarPoint(
      cx,
      cy,
      angleDeg,
      outerRadius,
      &tickOuterX,
      &tickOuterY
    );

    agPolarPoint(
      cx,
      cy,
      angleDeg,
      innerRadius,
      &tickInnerX,
      &tickInnerY
    );

    display->drawLine(
      tickInnerX,
      tickInnerY,
      tickOuterX,
      tickOuterY,
      isMajor ? AG_WHITE : AG_SOFT_WHITE
    );

    if (isMajor) {
      char label[10];

      agFormatValue(
        label,
        sizeof(label),
        value,
        true,
        labelDecimals
      );

      int labelX;
      int labelY;

      agPolarPoint(
        cx,
        cy,
        angleDeg,
        (float)r - 23.0f,
        &labelX,
        &labelY
      );

      int16_t x1;
      int16_t y1;
      uint16_t textW;
      uint16_t textH;

      display->setTextSize(1);
      display->getTextBounds(label, 0, 0, &x1, &y1, &textW, &textH);

      int labelDrawX = labelX - ((int)textW / 2);
      int labelDrawY = labelY - ((int)textH / 2);

      agPrintAtBold(
        display,
        labelDrawX,
        labelDrawY,
        label,
        1,
        AG_WHITE,
        AG_FACE
      );
    }
  }
}

static void agDrawNeedle(
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
    display->drawLine(cx - 9, cy - 9, cx + 9, cy + 9, AG_RED);
    display->drawLine(cx + 9, cy - 9, cx - 9, cy + 9, AG_RED);
    return;
  }

  float angleDeg =
    agValueToAngleDeg(value, minValue, maxValue);

  float s;
  float c;
  fastSinCosDeg(angleDeg, &s, &c);

  float needleLength = (float)r * 0.72f;
  float tailLength = (float)r * 0.12f;
  float halfWidth = (float)r * 0.052f;

  int tipX = cx + (int)roundf(c * needleLength);
  int tipY = cy + (int)roundf(s * needleLength);

  int tailX = cx - (int)roundf(c * tailLength);
  int tailY = cy - (int)roundf(s * tailLength);

  int baseX1 = tailX - (int)roundf(s * halfWidth);
  int baseY1 = tailY + (int)roundf(c * halfWidth);

  int baseX2 = tailX + (int)roundf(s * halfWidth);
  int baseY2 = tailY - (int)roundf(c * halfWidth);

  display->fillTriangle(
    tipX,
    tipY,
    baseX1,
    baseY1,
    baseX2,
    baseY2,
    AG_NEEDLE
  );

  display->drawLine(cx, cy, tipX, tipY, AG_NEEDLE_EDGE);

  display->fillCircle(cx, cy, 6, AG_HUB);
  display->drawCircle(cx, cy, 7, AG_BLACK);
  display->drawCircle(cx, cy, 5, AG_BEZEL_MID);
}

static void agDrawDigitalText(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  const char *title,
  float value,
  bool valid,
  uint8_t valueDecimals
) {
  char valueText[14];

  agFormatValue(
    valueText,
    sizeof(valueText),
    value,
    valid,
    valueDecimals
  );

  bool largeGauge = r >= 50;

  int textW = r * 2 - 12;
  int textX = cx - (textW / 2);

  int titleY = largeGauge ? cy + 12 : cy + 5;
  int valueY = largeGauge ? cy + 33 : cy + 25;

  agPrintCenteredBold(
    display,
    textX,
    titleY,
    textW,
    title,
    2,
    AG_WHITE,
    AG_FACE
  );

  agPrintCenteredBold(
    display,
    textX,
    valueY,
    textW,
    valueText,
    2,
    valid ? AG_GREEN : AG_RED,
    AG_FACE
  );
}

static void agDrawGauge(
  Arduino_GFX *display,
  int cx,
  int cy,
  int r,
  const char *title,
  float minValue,
  float maxValue,
  float value,
  bool valid,
  float majorStep,
  float minorStep,
  uint8_t labelDecimals,
  uint8_t valueDecimals,
  uint8_t gaugeType,
  bool staticPass
) {
  bool largePanel = r >= 50;

  float rangeLow = minValue;
  float rangeHigh = maxValue;

  if (rangeHigh <= rangeLow) {
    rangeHigh = rangeLow + 1.0f;
  }

  if (staticPass) {
    agDrawGaugePanel(
      display,
      cx,
      cy,
      r,
      largePanel
    );

    agDrawGaugeBody(display, cx, cy, r);
  }

  if (staticPass && gaugeType == 0) {
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, rangeLow, agRangePoint(rangeLow, rangeHigh, 0.60f), AG_GREEN);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.60f), agRangePoint(rangeLow, rangeHigh, 0.85f), AG_YELLOW);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.85f), rangeHigh, AG_RED);
  } else if (staticPass && gaugeType == 1) {
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, rangeLow, rangeHigh, AG_GREY);
  } else if (staticPass && gaugeType == 2) {
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, rangeLow, 3.40f, AG_RED);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, 3.40f, 3.60f, AG_YELLOW);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, 3.60f, rangeHigh, AG_GREEN);
  } else if (staticPass && gaugeType == 3) {
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, rangeLow, agRangePoint(rangeLow, rangeHigh, 0.50f), AG_GREEN);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.50f), agRangePoint(rangeLow, rangeHigh, 0.75f), AG_YELLOW);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.75f), rangeHigh, AG_RED);
  } else if (staticPass && gaugeType == 4) {
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, rangeLow, agRangePoint(rangeLow, rangeHigh, 0.20f), AG_RED);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.20f), agRangePoint(rangeLow, rangeHigh, 0.35f), AG_YELLOW);
    agDrawColourBand(display, cx, cy, r, rangeLow, rangeHigh, agRangePoint(rangeLow, rangeHigh, 0.35f), rangeHigh, AG_GREEN);
  }

  if (staticPass) {
    agDrawTicks(
      display,
      cx,
      cy,
      r,
      rangeLow,
      rangeHigh,
      majorStep,
      minorStep,
      labelDecimals
    );

    agPrintCenteredBold(
      display,
      cx - ((r * 2 - 12) / 2),
      r >= 50 ? cy + 12 : cy + 5,
      r * 2 - 12,
      title,
      2,
      AG_WHITE,
      AG_FACE
    );

    return;
  }

  agDrawNeedle(
    display,
    cx,
    cy,
    r,
    rangeLow,
    rangeHigh,
    value,
    valid
  );

  char valueText[14];
  agFormatValue(
    valueText,
    sizeof(valueText),
    value,
    valid,
    valueDecimals
  );

  int textW = r * 2 - 12;
  int valueY = r >= 50 ? cy + 33 : cy + 25;

  agPrintCenteredBold(
    display,
    cx - (textW / 2),
    valueY,
    textW,
    valueText,
    2,
    valid ? AG_GREEN : AG_RED,
    AG_FACE
  );
}

// ----------------------------------------------------
// Public screen entry point
// ----------------------------------------------------
//
// This is the function called by the main screen router.
// It must remain non-static.

static void agDrawPagePass(
  Arduino_GFX *display,
  bool staticPass
)
{
  // Use dark grey rather than pure black so there is no harsh
  // black divider between adjacent square panels.
  if (staticPass) {
    display->fillScreen(AG_PANEL_GAP);
  }

  float speedKmh = airspeed * 3.6f;
  bool speedValid = mavlinkVfrHudValid;

  float altitudeM = altitude_msl;
  bool altitudeValid = mavlinkVfrHudValid;

  float cellVoltage = batteryCellVoltage;
  bool cellValid = batteryVoltageValid;

  float currentA = batteryCurrentA;
  bool currentValid = batteryCurrentValid;

  float remainingPercent = (float)batteryRemainingPercent;
  bool remainingValid =
    batteryRemainingPercent >= 0 &&
    (mavlinkBatteryValid || mavlinkBatteryStatusValid);

  if (currentA < 0.0f) {
    currentA = 0.0f;
  }

  agDrawGauge(
    display,
    80,
    61,
    54,
    "SPD",
    CONFIG_ANALOGUE_SPEED_MIN_KMH,
    CONFIG_ANALOGUE_SPEED_MAX_KMH,
    speedKmh,
    speedValid,
    50.0f,
    10.0f,
    0,
    0,
    0,
    staticPass
  );

  agDrawGauge(
    display,
    240,
    61,
    54,
    "ALT",
    CONFIG_ANALOGUE_ALT_MIN_M,
    CONFIG_ANALOGUE_ALT_MAX_M,
    altitudeM,
    altitudeValid,
    50.0f,
    25.0f,
    0,
    0,
    1,
    staticPass
  );

  agDrawGauge(
    display,
    53,
    191,
    45,
    "CELL",
    CONFIG_ANALOGUE_CELL_MIN_V,
    CONFIG_ANALOGUE_CELL_MAX_V,
    cellVoltage,
    cellValid,
    0.6f,
    0.1f,
    1,
    2,
    2,
    staticPass
  );

  agDrawGauge(
    display,
    160,
    191,
    45,
    "AMP",
    CONFIG_ANALOGUE_CURRENT_MIN_A,
    CONFIG_ANALOGUE_CURRENT_MAX_A,
    currentA,
    currentValid,
    50.0f,
    10.0f,
    0,
    1,
    3,
    staticPass
  );

  agDrawGauge(
    display,
    267,
    191,
    45,
    "REM",
    CONFIG_ANALOGUE_REMAIN_MIN_PERCENT,
    CONFIG_ANALOGUE_REMAIN_MAX_PERCENT,
    remainingPercent,
    remainingValid,
    50.0f,
    10.0f,
    0,
    0,
    4,
    staticPass
  );
}

void drawAnalogueGaugesScreen(Arduino_GFX *display)
{
  if (!agFrameTemplateReady) {
    agDrawPagePass(display, true);
    agFrameTemplateReady =
      captureFrameTemplate(display, &agFrameTemplate);
  } else {
    restoreFrameTemplate(display, agFrameTemplate);
  }

  // If PSRAM allocation failed, retain the original safe full redraw.
  if (!agFrameTemplateReady) {
    agDrawPagePass(display, true);
  }

  agDrawPagePass(display, false);
}
