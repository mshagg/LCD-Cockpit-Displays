// ----------------------------------------------------
// Secondary page: battery
// ----------------------------------------------------
//
// Battery-themed telemetry page:
//   - large top field: cell voltage
//   - pack voltage, current, watts
//   - consumed mAh from MAVLink BATTERY_STATUS
//   - remaining %, max current, MAVLink throttle %, voltage sag
//
// Consumption is decoded from telemetry only.
// No local current integration is used.
//
// Throttle is decoded from MAVLink VFR_HUD.throttle.
// It is not calculated from an RC input channel.
//
// Colour scheme matches the navigation page.
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// Battery telemetry from mavlink_telemetry.ino
extern float batteryVoltage;
extern float batteryCellVoltage;
extern float batteryCurrentA;
extern float batteryPowerW;

extern int8_t batteryRemainingPercent;

extern bool batteryVoltageValid;
extern bool batteryCurrentValid;
extern bool batteryPowerValid;
extern bool mavlinkBatteryValid;

extern float batteryLowestCellVoltage;
extern bool batteryLowestCellVoltageValid;

extern float batteryConsumedMah;
extern bool batteryConsumedMahValid;

extern float batteryMaxCurrentA;
extern bool batteryMaxCurrentValid;

extern float batterySagV;
extern bool batterySagValid;

// MAVLink throttle telemetry from VFR_HUD
extern int16_t mavlinkThrottlePercent;
extern bool mavlinkThrottleValid;

// ----------------------------------------------------
// Colours - matched to nav page
// ----------------------------------------------------

static const uint16_t SEC_BATT_BLACK      = RGB565(0, 0, 0);
static const uint16_t SEC_BATT_PANEL      = RGB565(4, 13, 16);
static const uint16_t SEC_BATT_PANEL2     = RGB565(4, 22, 28);

static const uint16_t SEC_BATT_LINE       = RGB565(210, 210, 210);
static const uint16_t SEC_BATT_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t SEC_BATT_LABEL      = RGB565(0, 235, 255);
static const uint16_t SEC_BATT_CYAN       = RGB565(0, 245, 255);
static const uint16_t SEC_BATT_GREEN      = RGB565(80, 255, 120);
static const uint16_t SEC_BATT_WARN       = RGB565(255, 175, 40);
static const uint16_t SEC_BATT_BAD        = RGB565(255, 65, 65);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int SEC_BATT_TOP_X = 0;
static const int SEC_BATT_TOP_Y = 0;
static const int SEC_BATT_TOP_W = 240;
static const int SEC_BATT_TOP_H = 80;

static const int SEC_BATT_LEFT_X  = 0;
static const int SEC_BATT_RIGHT_X = 122;
static const int SEC_BATT_COL_W   = 118;
static const int SEC_BATT_TILE_H  = 46;

static const int SEC_BATT_ROW1_Y = 85;
static const int SEC_BATT_ROW2_Y = 134;
static const int SEC_BATT_ROW3_Y = 183;
static const int SEC_BATT_ROW4_Y = 232;

// ----------------------------------------------------
// Text helpers
// ----------------------------------------------------

static int secBattTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void secBattPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = secBattTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(colour);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

// ----------------------------------------------------
// Value helpers
// ----------------------------------------------------

static uint16_t secBattCellColour(float cellV, bool valid)
{
  if (!valid) {
    return SEC_BATT_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_CRITICAL_V) {
    return SEC_BATT_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_WARN_V) {
    return SEC_BATT_WARN;
  }

  return SEC_BATT_GREEN;
}

static uint16_t secBattValidColour(bool valid)
{
  return valid ? SEC_BATT_GREEN : SEC_BATT_BAD;
}

static uint16_t secBattPercentColour()
{
  if (batteryRemainingPercent < 0) {
    return SEC_BATT_BAD;
  }

  if (batteryRemainingPercent <= 20) {
    return SEC_BATT_BAD;
  }

  if (batteryRemainingPercent <= 35) {
    return SEC_BATT_WARN;
  }

  return SEC_BATT_GREEN;
}

static uint16_t secBattSagColour()
{
  if (!batterySagValid) {
    return SEC_BATT_BAD;
  }

  if (batterySagV >= 1.0f) {
    return SEC_BATT_BAD;
  }

  if (batterySagV >= 0.5f) {
    return SEC_BATT_WARN;
  }

  return SEC_BATT_GREEN;
}

static uint16_t secBattThrottleColour()
{
  if (!mavlinkThrottleValid || mavlinkThrottlePercent < 0) {
    return SEC_BATT_BAD;
  }

  if (mavlinkThrottlePercent >= 75) {
    return SEC_BATT_WARN;
  }

  return SEC_BATT_CYAN;
}

static float secBattDisplayedCellVoltage()
{
  if (batteryLowestCellVoltageValid) {
    return batteryLowestCellVoltage;
  }

  return batteryCellVoltage;
}

static bool secBattCellVoltageValid()
{
  if (batteryLowestCellVoltageValid) {
    return true;
  }

  return mavlinkBatteryValid && batteryCellVoltage > 0.0f;
}

// ----------------------------------------------------
// Drawing helpers
// ----------------------------------------------------

static void secBattDrawTileFrame(
  int x,
  int y,
  const char *label
) {
  secGfx->fillRect(
    x,
    y,
    SEC_BATT_COL_W,
    SEC_BATT_TILE_H,
    SEC_BATT_PANEL
  );

  secGfx->drawRect(
    x,
    y,
    SEC_BATT_COL_W,
    SEC_BATT_TILE_H,
    SEC_BATT_LINE
  );

  secGfx->drawRect(
    x + 1,
    y + 1,
    SEC_BATT_COL_W - 2,
    SEC_BATT_TILE_H - 2,
    SEC_BATT_LINE_SOFT
  );

  secGfx->setTextSize(2);
  secGfx->setTextColor(SEC_BATT_LABEL);
  secGfx->setCursor(x + 5, y + 3);
  secGfx->print(label);
}

static void secBattUpdateTileValue(
  int x,
  int y,
  const char *value,
  uint16_t valueColour
) {
  secGfx->fillRect(
    x + 3,
    y + 21,
    SEC_BATT_COL_W - 6,
    22,
    SEC_BATT_PANEL
  );

  secBattPrintCentered(
    x,
    y + 25,
    SEC_BATT_COL_W,
    value,
    2,
    valueColour
  );
}

static void secBattDrawStaticLayout()
{
  secGfx->fillScreen(SEC_BATT_BLACK);

  secGfx->fillRect(
    SEC_BATT_TOP_X,
    SEC_BATT_TOP_Y,
    SEC_BATT_TOP_W,
    SEC_BATT_TOP_H,
    SEC_BATT_PANEL2
  );

  secGfx->drawRect(
    SEC_BATT_TOP_X,
    SEC_BATT_TOP_Y,
    SEC_BATT_TOP_W,
    SEC_BATT_TOP_H,
    SEC_BATT_LINE
  );

  secGfx->drawRect(
    SEC_BATT_TOP_X + 1,
    SEC_BATT_TOP_Y + 1,
    SEC_BATT_TOP_W - 2,
    SEC_BATT_TOP_H - 2,
    SEC_BATT_LINE_SOFT
  );

  secBattPrintCentered(
    SEC_BATT_TOP_X,
    SEC_BATT_TOP_Y + 5,
    SEC_BATT_TOP_W,
    "CELL V",
    2,
    SEC_BATT_LABEL
  );

  secBattDrawTileFrame(SEC_BATT_LEFT_X,  SEC_BATT_ROW1_Y, "PACK V");
  secBattDrawTileFrame(SEC_BATT_RIGHT_X, SEC_BATT_ROW1_Y, "CURRENT");

  secBattDrawTileFrame(SEC_BATT_LEFT_X,  SEC_BATT_ROW2_Y, "POWER");
  secBattDrawTileFrame(SEC_BATT_RIGHT_X, SEC_BATT_ROW2_Y, "USED");

  secBattDrawTileFrame(SEC_BATT_LEFT_X,  SEC_BATT_ROW3_Y, "REMAIN");
  secBattDrawTileFrame(SEC_BATT_RIGHT_X, SEC_BATT_ROW3_Y, "MAX A");

  secBattDrawTileFrame(SEC_BATT_LEFT_X,  SEC_BATT_ROW4_Y, "THROTTLE");
  secBattDrawTileFrame(SEC_BATT_RIGHT_X, SEC_BATT_ROW4_Y, "SAG");
}

static void secBattUpdateTopCellVoltage()
{
  char value[18];

  secGfx->fillRect(
    SEC_BATT_TOP_X + 4,
    SEC_BATT_TOP_Y + 24,
    SEC_BATT_TOP_W - 8,
    51,
    SEC_BATT_PANEL2
  );

  float cellV = secBattDisplayedCellVoltage();
  bool valid = secBattCellVoltageValid();

  if (valid) {
    snprintf(value, sizeof(value), "%.2fV", cellV);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattPrintCentered(
    SEC_BATT_TOP_X,
    SEC_BATT_TOP_Y + 29,
    SEC_BATT_TOP_W,
    value,
    5,
    secBattCellColour(cellV, valid)
  );
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryBatteryPage()
{
  char value[24];

  if (secondaryNeedsFullRedraw) {
    secBattDrawStaticLayout();
  }

  secBattUpdateTopCellVoltage();

  if (batteryVoltageValid) {
    snprintf(value, sizeof(value), "%.1fV", batteryVoltage);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_LEFT_X,
    SEC_BATT_ROW1_Y,
    value,
    secBattValidColour(batteryVoltageValid)
  );

  if (batteryCurrentValid) {
    snprintf(value, sizeof(value), "%.1fA", batteryCurrentA);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_RIGHT_X,
    SEC_BATT_ROW1_Y,
    value,
    secBattValidColour(batteryCurrentValid)
  );

  if (batteryPowerValid) {
    snprintf(value, sizeof(value), "%.0fW", batteryPowerW);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_LEFT_X,
    SEC_BATT_ROW2_Y,
    value,
    secBattValidColour(batteryPowerValid)
  );

  if (batteryConsumedMahValid) {
    snprintf(value, sizeof(value), "%ldmAh", lroundf(batteryConsumedMah));
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_RIGHT_X,
    SEC_BATT_ROW2_Y,
    value,
    batteryConsumedMahValid ? SEC_BATT_CYAN : SEC_BATT_BAD
  );

  if (batteryRemainingPercent >= 0) {
    snprintf(value, sizeof(value), "%d%%", batteryRemainingPercent);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_LEFT_X,
    SEC_BATT_ROW3_Y,
    value,
    secBattPercentColour()
  );

  if (batteryMaxCurrentValid) {
    snprintf(value, sizeof(value), "%.1fA", batteryMaxCurrentA);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_RIGHT_X,
    SEC_BATT_ROW3_Y,
    value,
    batteryMaxCurrentValid ? SEC_BATT_CYAN : SEC_BATT_BAD
  );

  if (mavlinkThrottleValid && mavlinkThrottlePercent >= 0) {
    snprintf(value, sizeof(value), "%d%%", mavlinkThrottlePercent);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_LEFT_X,
    SEC_BATT_ROW4_Y,
    value,
    secBattThrottleColour()
  );

  if (batterySagValid) {
    if (batterySagV < 1.0f) {
      snprintf(value, sizeof(value), "%.2fV", batterySagV);
    } else {
      snprintf(value, sizeof(value), "%.1fV", batterySagV);
    }
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattUpdateTileValue(
    SEC_BATT_RIGHT_X,
    SEC_BATT_ROW4_Y,
    value,
    secBattSagColour()
  );
}