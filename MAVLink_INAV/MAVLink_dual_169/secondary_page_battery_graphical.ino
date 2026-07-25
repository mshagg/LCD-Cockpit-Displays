// ----------------------------------------------------
// Secondary page: battery (graphical)
// ----------------------------------------------------
//
// Layout:
//   - large top field: cell voltage
//   - 4 horizontal bar charts below:
//       1) pack voltage
//       2) remaining battery capacity
//       3) current (0..100A)
//       4) throttle (0..100%)
//
// Notes:
//   - Pack voltage bar uses:
//       empty = 3.5V/cell
//       full  = 4.2V/cell
//   - Uses telemetry cell count when available,
//     otherwise falls back to CONFIG_BATTERY_CELL_COUNT
//   - Throttle comes from MAVLink VFR_HUD.throttle
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
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

extern uint8_t batteryCellCountTelemetry;

// MAVLink throttle telemetry
extern int16_t mavlinkThrottlePercent;
extern bool mavlinkThrottleValid;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t SEC_BATTG_BLACK      = RGB565(0, 0, 0);
static const uint16_t SEC_BATTG_PANEL      = RGB565(4, 13, 16);
static const uint16_t SEC_BATTG_PANEL2     = RGB565(4, 22, 28);

static const uint16_t SEC_BATTG_LINE       = RGB565(210, 210, 210);
static const uint16_t SEC_BATTG_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t SEC_BATTG_LABEL      = RGB565(0, 235, 255);
static const uint16_t SEC_BATTG_VALUE      = RGB565(240, 245, 250);
static const uint16_t SEC_BATTG_GREEN      = RGB565(80, 255, 120);
static const uint16_t SEC_BATTG_WARN       = RGB565(255, 175, 40);
static const uint16_t SEC_BATTG_BAD        = RGB565(255, 65, 65);
static const uint16_t SEC_BATTG_CYAN       = RGB565(0, 245, 255);
static const uint16_t SEC_BATTG_PURPLE     = RGB565(190, 70, 255);

static const uint16_t SEC_BATTG_BAR_BG     = RGB565(18, 28, 34);
static const uint16_t SEC_BATTG_BAR_EDGE   = RGB565(115, 125, 132);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int SEC_BATTG_TOP_X = 0;
static const int SEC_BATTG_TOP_Y = 0;
static const int SEC_BATTG_TOP_W = 240;
static const int SEC_BATTG_TOP_H = 80;

static const int SEC_BATTG_ROW_X = 0;
static const int SEC_BATTG_ROW_W = 240;
static const int SEC_BATTG_ROW_H = 45;
static const int SEC_BATTG_ROW_GAP = 4;

static const int SEC_BATTG_ROW1_Y = 86;
static const int SEC_BATTG_ROW2_Y = SEC_BATTG_ROW1_Y + SEC_BATTG_ROW_H + SEC_BATTG_ROW_GAP;
static const int SEC_BATTG_ROW3_Y = SEC_BATTG_ROW2_Y + SEC_BATTG_ROW_H + SEC_BATTG_ROW_GAP;
static const int SEC_BATTG_ROW4_Y = SEC_BATTG_ROW3_Y + SEC_BATTG_ROW_H + SEC_BATTG_ROW_GAP;

static const int SEC_BATTG_BAR_X = 8;
static const int SEC_BATTG_BAR_W = 224;
static const int SEC_BATTG_BAR_H = 14;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static int secBattGTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void secBattGPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = secBattGTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(colour);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

static float secBattGClamp01(float v)
{
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static uint8_t secBattGGetCellCount()
{
  if (batteryCellCountTelemetry > 0) {
    return batteryCellCountTelemetry;
  }

  if (CONFIG_BATTERY_CELL_COUNT > 0) {
    return CONFIG_BATTERY_CELL_COUNT;
  }

  return 6;
}

static float secBattGDisplayedCellVoltage()
{
  if (batteryLowestCellVoltageValid) {
    return batteryLowestCellVoltage;
  }

  return batteryCellVoltage;
}

static bool secBattGCellVoltageValid()
{
  if (batteryLowestCellVoltageValid) {
    return true;
  }

  return mavlinkBatteryValid && batteryCellVoltage > 0.0f;
}

static uint16_t secBattGCellColour(float cellV, bool valid)
{
  if (!valid) {
    return SEC_BATTG_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_CRITICAL_V) {
    return SEC_BATTG_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_WARN_V) {
    return SEC_BATTG_WARN;
  }

  return SEC_BATTG_GREEN;
}

static uint16_t secBattGPackBarColour(float ratio, bool valid)
{
  if (!valid) {
    return SEC_BATTG_BAD;
  }

  if (ratio <= 0.15f) {
    return SEC_BATTG_BAD;
  }

  if (ratio <= 0.35f) {
    return SEC_BATTG_WARN;
  }

  return SEC_BATTG_GREEN;
}

static uint16_t secBattGRemainBarColour(int percent, bool valid)
{
  if (!valid) {
    return SEC_BATTG_BAD;
  }

  if (percent <= 20) {
    return SEC_BATTG_BAD;
  }

  if (percent <= 35) {
    return SEC_BATTG_WARN;
  }

  return SEC_BATTG_GREEN;
}

static uint16_t secBattGCurrentBarColour(float currentA, bool valid)
{
  if (!valid) {
    return SEC_BATTG_BAD;
  }

  if (currentA >= 80.0f) {
    return SEC_BATTG_BAD;
  }

  if (currentA >= 60.0f) {
    return SEC_BATTG_WARN;
  }

  return SEC_BATTG_CYAN;
}

static uint16_t secBattGThrottleBarColour(int throttlePercent, bool valid)
{
  if (!valid) {
    return SEC_BATTG_BAD;
  }

  if (throttlePercent >= 75) {
    return SEC_BATTG_WARN;
  }

  return SEC_BATTG_PURPLE;
}

// ----------------------------------------------------
// Static layout
// ----------------------------------------------------

static void secBattGDrawTopPanelFrame()
{
  secGfx->fillRect(
    SEC_BATTG_TOP_X,
    SEC_BATTG_TOP_Y,
    SEC_BATTG_TOP_W,
    SEC_BATTG_TOP_H,
    SEC_BATTG_PANEL2
  );

  secGfx->drawRect(
    SEC_BATTG_TOP_X,
    SEC_BATTG_TOP_Y,
    SEC_BATTG_TOP_W,
    SEC_BATTG_TOP_H,
    SEC_BATTG_LINE
  );

  secGfx->drawRect(
    SEC_BATTG_TOP_X + 1,
    SEC_BATTG_TOP_Y + 1,
    SEC_BATTG_TOP_W - 2,
    SEC_BATTG_TOP_H - 2,
    SEC_BATTG_LINE_SOFT
  );

  secBattGPrintCentered(
    SEC_BATTG_TOP_X,
    SEC_BATTG_TOP_Y + 5,
    SEC_BATTG_TOP_W,
    "CELL V",
    2,
    SEC_BATTG_LABEL
  );
}

static void secBattGDrawBarRowFrame(
  int y,
  const char *label
) {
  secGfx->fillRect(
    SEC_BATTG_ROW_X,
    y,
    SEC_BATTG_ROW_W,
    SEC_BATTG_ROW_H,
    SEC_BATTG_PANEL
  );

  secGfx->drawRect(
    SEC_BATTG_ROW_X,
    y,
    SEC_BATTG_ROW_W,
    SEC_BATTG_ROW_H,
    SEC_BATTG_LINE
  );

  secGfx->drawRect(
    SEC_BATTG_ROW_X + 1,
    y + 1,
    SEC_BATTG_ROW_W - 2,
    SEC_BATTG_ROW_H - 2,
    SEC_BATTG_LINE_SOFT
  );

  secGfx->setTextSize(2);
  secGfx->setTextColor(SEC_BATTG_LABEL);
  secGfx->setCursor(8, y + 4);
  secGfx->print(label);

  secGfx->fillRect(
    SEC_BATTG_BAR_X,
    y + 24,
    SEC_BATTG_BAR_W,
    SEC_BATTG_BAR_H,
    SEC_BATTG_BAR_BG
  );

  secGfx->drawRect(
    SEC_BATTG_BAR_X,
    y + 24,
    SEC_BATTG_BAR_W,
    SEC_BATTG_BAR_H,
    SEC_BATTG_BAR_EDGE
  );
}

static void secBattGDrawStaticLayout()
{
  secGfx->fillScreen(SEC_BATTG_BLACK);

  secBattGDrawTopPanelFrame();
  secBattGDrawBarRowFrame(SEC_BATTG_ROW1_Y, "PACK V");
  secBattGDrawBarRowFrame(SEC_BATTG_ROW2_Y, "REMAIN");
  secBattGDrawBarRowFrame(SEC_BATTG_ROW3_Y, "CURRENT");
  secBattGDrawBarRowFrame(SEC_BATTG_ROW4_Y, "THROTTLE");
}

// ----------------------------------------------------
// Dynamic drawing
// ----------------------------------------------------

static void secBattGUpdateTopCellVoltage()
{
  char value[18];

  secGfx->fillRect(
    SEC_BATTG_TOP_X + 4,
    SEC_BATTG_TOP_Y + 24,
    SEC_BATTG_TOP_W - 8,
    51,
    SEC_BATTG_PANEL2
  );

  float cellV = secBattGDisplayedCellVoltage();
  bool valid = secBattGCellVoltageValid();

  if (valid) {
    snprintf(value, sizeof(value), "%.2fV", cellV);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secBattGPrintCentered(
    SEC_BATTG_TOP_X,
    SEC_BATTG_TOP_Y + 29,
    SEC_BATTG_TOP_W,
    value,
    5,
    secBattGCellColour(cellV, valid)
  );
}

static void secBattGUpdateBarRow(
  int y,
  const char *valueText,
  float ratio,
  uint16_t fillColour
) {
  secGfx->fillRect(124, y + 2, 108, 18, SEC_BATTG_PANEL);

  int textSize = 2;
  int textW = secBattGTextWidth(valueText, textSize);
  int textX = 232 - textW;
  if (textX < 124) {
    textX = 124;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(SEC_BATTG_VALUE);
  secGfx->setCursor(textX, y + 4);
  secGfx->print(valueText);

  secGfx->fillRect(
    SEC_BATTG_BAR_X + 1,
    y + 25,
    SEC_BATTG_BAR_W - 2,
    SEC_BATTG_BAR_H - 2,
    SEC_BATTG_BAR_BG
  );

  int fillW = (int)((SEC_BATTG_BAR_W - 2) * secBattGClamp01(ratio));

  if (fillW > 0) {
    secGfx->fillRect(
      SEC_BATTG_BAR_X + 1,
      y + 25,
      fillW,
      SEC_BATTG_BAR_H - 2,
      fillColour
    );
  }
}

// ----------------------------------------------------
// Page draw
// ----------------------------------------------------

void drawSecondaryBatteryGraphicalPage()
{
  if (secondaryNeedsFullRedraw) {
    secBattGDrawStaticLayout();
  }

  secBattGUpdateTopCellVoltage();

  // Pack voltage
  {
    char value[20];
    bool valid = batteryVoltageValid;
    float ratio = 0.0f;

    uint8_t cells = secBattGGetCellCount();
    float emptyV = 3.5f * (float)cells;
    float fullV  = 4.2f * (float)cells;

    if (valid && fullV > emptyV) {
      ratio = (batteryVoltage - emptyV) / (fullV - emptyV);
    }

    if (valid) {
      snprintf(value, sizeof(value), "%.1fV", batteryVoltage);
    } else {
      snprintf(value, sizeof(value), "---");
    }

    secBattGUpdateBarRow(
      SEC_BATTG_ROW1_Y,
      value,
      ratio,
      secBattGPackBarColour(ratio, valid)
    );
  }

  // Remaining
  {
    char value[20];
    bool valid = (batteryRemainingPercent >= 0);
    float ratio = 0.0f;

    if (valid) {
      ratio = (float)batteryRemainingPercent / 100.0f;
      snprintf(value, sizeof(value), "%d%%", batteryRemainingPercent);
    } else {
      snprintf(value, sizeof(value), "---");
    }

    secBattGUpdateBarRow(
      SEC_BATTG_ROW2_Y,
      value,
      ratio,
      secBattGRemainBarColour(batteryRemainingPercent, valid)
    );
  }

  // Current
  {
    char value[20];
    bool valid = batteryCurrentValid;
    float ratio = 0.0f;

    if (valid) {
      ratio = batteryCurrentA / 100.0f;
      snprintf(value, sizeof(value), "%.1fA", batteryCurrentA);
    } else {
      snprintf(value, sizeof(value), "---");
    }

    secBattGUpdateBarRow(
      SEC_BATTG_ROW3_Y,
      value,
      ratio,
      secBattGCurrentBarColour(batteryCurrentA, valid)
    );
  }

  // Throttle
  {
    char value[20];
    bool valid = mavlinkThrottleValid && mavlinkThrottlePercent >= 0;
    float ratio = 0.0f;

    if (valid) {
      ratio = (float)mavlinkThrottlePercent / 100.0f;
      snprintf(value, sizeof(value), "%d%%", mavlinkThrottlePercent);
    } else {
      snprintf(value, sizeof(value), "---");
    }

    secBattGUpdateBarRow(
      SEC_BATTG_ROW4_Y,
      value,
      ratio,
      secBattGThrottleBarColour(mavlinkThrottlePercent, valid)
    );
  }
}