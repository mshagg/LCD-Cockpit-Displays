// ----------------------------------------------------
// Secondary page: flight status
// ----------------------------------------------------
//
// Layout:
//   - No header
//   - Large flight mode field at top
//   - Flight fields split into two large-text columns
//   - No RC15/16 row
//
// Static layout is drawn only when secondaryNeedsFullRedraw is true.
// Dynamic fields are updated in-place.
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// MAVLink flight data
extern bool mavlinkHeartbeatValid;
extern bool mavlinkVfrHudValid;
extern bool mavlinkBatteryValid;
extern bool mavlinkGpsValid;

extern bool vehicleArmed;
extern char flightModeText[18];

extern float airspeed;
extern float groundspeed;
extern float altitude_msl;

extern float batteryVoltage;
extern float batteryCellVoltage;
extern float batteryCurrentA;
extern int8_t batteryRemainingPercent;

extern uint8_t gpsFixType;
extern uint8_t gpsSatellitesVisible;

extern bool rssiValid;
extern uint8_t rssiPercent;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t SEC_FLIGHT_BLACK  = RGB565(0, 0, 0);
static const uint16_t SEC_FLIGHT_PANEL  = RGB565(5, 12, 18);
static const uint16_t SEC_FLIGHT_PANEL2 = RGB565(8, 18, 28);
static const uint16_t SEC_FLIGHT_LINE   = RGB565(40, 95, 120);
static const uint16_t SEC_FLIGHT_DIM    = RGB565(135, 150, 155);
static const uint16_t SEC_FLIGHT_GOOD   = RGB565(80, 255, 120);
static const uint16_t SEC_FLIGHT_WARN   = RGB565(255, 175, 40);
static const uint16_t SEC_FLIGHT_BAD    = RGB565(255, 65, 65);
static const uint16_t SEC_FLIGHT_PURPLE = RGB565(190, 70, 255);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int SEC_FLIGHT_W = 240;

static const int SEC_FLIGHT_MODE_X = 0;
static const int SEC_FLIGHT_MODE_Y = 0;
static const int SEC_FLIGHT_MODE_W = 240;
static const int SEC_FLIGHT_MODE_H = 80;

static const int SEC_FLIGHT_LEFT_X  = 0;
static const int SEC_FLIGHT_RIGHT_X = 122;
static const int SEC_FLIGHT_COL_W   = 118;
static const int SEC_FLIGHT_TILE_H  = 46;

static const int SEC_FLIGHT_ROW1_Y = 85;
static const int SEC_FLIGHT_ROW2_Y = 134;
static const int SEC_FLIGHT_ROW3_Y = 183;
static const int SEC_FLIGHT_ROW4_Y = 232;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static int secFlightTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void secFlightPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t color
) {
  int textW = secFlightTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(color);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

static int secFlightModeTextSize(const char *text)
{
  int len = strlen(text);

  if (len <= 4) {
    return 6;
  }

  if (len <= 6) {
    return 5;
  }

  if (len <= 9) {
    return 4;
  }

  return 3;
}

static const char* secFlightGpsFixText(uint8_t fixType)
{
  switch (fixType) {
    case 0:
      return "NO";

    case 1:
      return "NOFIX";

    case 2:
      return "2D";

    case 3:
      return "3D";

    case 4:
      return "DGPS";

    case 5:
      return "RTKF";

    case 6:
      return "RTK";

    default:
      return "FIX";
  }
}

static uint16_t secFlightGpsColor()
{
  if (!mavlinkGpsValid) {
    return SEC_FLIGHT_BAD;
  }

  if (gpsFixType >= 3) {
    return SEC_FLIGHT_GOOD;
  }

  if (gpsFixType == 2) {
    return SEC_FLIGHT_WARN;
  }

  return SEC_FLIGHT_BAD;
}

static uint16_t secFlightBatteryColor()
{
  if (!mavlinkBatteryValid) {
    return SEC_FLIGHT_BAD;
  }

  if (batteryCellVoltage <= CONFIG_BATTERY_CELL_CRITICAL_V) {
    return SEC_FLIGHT_BAD;
  }

  if (batteryCellVoltage <= CONFIG_BATTERY_CELL_WARN_V) {
    return SEC_FLIGHT_WARN;
  }

  return SEC_FLIGHT_GOOD;
}

static uint16_t secFlightRssiColor()
{
  if (!rssiValid) {
    return SEC_FLIGHT_BAD;
  }

  if (rssiPercent < 30) {
    return SEC_FLIGHT_BAD;
  }

  if (rssiPercent < 55) {
    return SEC_FLIGHT_WARN;
  }

  return SEC_FLIGHT_GOOD;
}

static uint16_t secFlightValidColor(bool valid)
{
  return valid ? SEC_FLIGHT_GOOD : SEC_FLIGHT_BAD;
}

static void secFlightDrawTileFrame(
  int x,
  int y,
  const char *label
) {
  secGfx->fillRect(
    x,
    y,
    SEC_FLIGHT_COL_W,
    SEC_FLIGHT_TILE_H,
    SEC_FLIGHT_PANEL
  );

  secGfx->drawRect(
    x,
    y,
    SEC_FLIGHT_COL_W,
    SEC_FLIGHT_TILE_H,
    SEC_FLIGHT_LINE
  );

  secGfx->setTextSize(1);
  secGfx->setTextColor(SEC_FLIGHT_DIM);
  secGfx->setCursor(x + 5, y + 5);
  secGfx->print(label);
}

static void secFlightUpdateTileValue(
  int x,
  int y,
  const char *value,
  uint16_t valueColor
) {
  secGfx->fillRect(
    x + 3,
    y + 19,
    SEC_FLIGHT_COL_W - 6,
    23,
    SEC_FLIGHT_PANEL
  );

  secFlightPrintCentered(
    x,
    y + 23,
    SEC_FLIGHT_COL_W,
    value,
    2,
    valueColor
  );
}

static void secFlightDrawStaticLayout()
{
  secGfx->fillScreen(SEC_FLIGHT_BLACK);

  // Flight mode panel.
  secGfx->fillRect(
    SEC_FLIGHT_MODE_X,
    SEC_FLIGHT_MODE_Y,
    SEC_FLIGHT_MODE_W,
    SEC_FLIGHT_MODE_H,
    SEC_FLIGHT_PANEL2
  );

  secGfx->drawRect(
    SEC_FLIGHT_MODE_X,
    SEC_FLIGHT_MODE_Y,
    SEC_FLIGHT_MODE_W,
    SEC_FLIGHT_MODE_H,
    SEC_FLIGHT_LINE
  );

  secFlightPrintCentered(
    SEC_FLIGHT_MODE_X,
    SEC_FLIGHT_MODE_Y + 7,
    SEC_FLIGHT_MODE_W,
    "MODE",
    1,
    SEC_FLIGHT_DIM
  );

  // Two-column tiles.
  secFlightDrawTileFrame(SEC_FLIGHT_LEFT_X,  SEC_FLIGHT_ROW1_Y, "GPS/SAT");
  secFlightDrawTileFrame(SEC_FLIGHT_RIGHT_X, SEC_FLIGHT_ROW1_Y, "RSSI");

  secFlightDrawTileFrame(SEC_FLIGHT_LEFT_X,  SEC_FLIGHT_ROW2_Y, "BAT");
  secFlightDrawTileFrame(SEC_FLIGHT_RIGHT_X, SEC_FLIGHT_ROW2_Y, "CELL");

  secFlightDrawTileFrame(SEC_FLIGHT_LEFT_X,  SEC_FLIGHT_ROW3_Y, "ALT");
  secFlightDrawTileFrame(SEC_FLIGHT_RIGHT_X, SEC_FLIGHT_ROW3_Y, "AIR");

  secFlightDrawTileFrame(SEC_FLIGHT_LEFT_X,  SEC_FLIGHT_ROW4_Y, "GS");
  secFlightDrawTileFrame(SEC_FLIGHT_RIGHT_X, SEC_FLIGHT_ROW4_Y, "ARM");
}

static void secFlightUpdateMode()
{
  secGfx->fillRect(
    SEC_FLIGHT_MODE_X + 4,
    SEC_FLIGHT_MODE_Y + 20,
    SEC_FLIGHT_MODE_W - 8,
    55,
    SEC_FLIGHT_PANEL2
  );

  const char *modeText = mavlinkHeartbeatValid ? flightModeText : "NO HB";

  int textSize = secFlightModeTextSize(modeText);

  secFlightPrintCentered(
    SEC_FLIGHT_MODE_X,
    SEC_FLIGHT_MODE_Y + 26,
    SEC_FLIGHT_MODE_W,
    modeText,
    textSize,
    mavlinkHeartbeatValid ? SEC_FLIGHT_PURPLE : SEC_FLIGHT_BAD
  );
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryFlightStatusPage()
{
  char value[24];

  if (secondaryNeedsFullRedraw) {
    secFlightDrawStaticLayout();
  }

  secFlightUpdateMode();

  // GPS / SAT.
  if (mavlinkGpsValid) {
    snprintf(
      value,
      sizeof(value),
      "%s %u",
      secFlightGpsFixText(gpsFixType),
      gpsSatellitesVisible
    );
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_LEFT_X,
    SEC_FLIGHT_ROW1_Y,
    value,
    secFlightGpsColor()
  );

  // RSSI.
  if (rssiValid) {
    snprintf(value, sizeof(value), "%u%%", rssiPercent);
  } else {
    snprintf(value, sizeof(value), "--");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_RIGHT_X,
    SEC_FLIGHT_ROW1_Y,
    value,
    secFlightRssiColor()
  );

  // Battery voltage.
  if (mavlinkBatteryValid) {
    snprintf(value, sizeof(value), "%.1fV", batteryVoltage);
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_LEFT_X,
    SEC_FLIGHT_ROW2_Y,
    value,
    secFlightBatteryColor()
  );

  // Cell voltage.
  if (mavlinkBatteryValid) {
    snprintf(value, sizeof(value), "%.2fV", batteryCellVoltage);
  } else {
    snprintf(value, sizeof(value), "--");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_RIGHT_X,
    SEC_FLIGHT_ROW2_Y,
    value,
    secFlightBatteryColor()
  );

  // Altitude.
  if (mavlinkVfrHudValid) {
    snprintf(
      value,
      sizeof(value),
      "%ld%s",
      lroundf(altitude_msl * CONFIG_GLASS_ALT_SCALE),
      CONFIG_GLASS_ALT_LABEL
    );
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_LEFT_X,
    SEC_FLIGHT_ROW3_Y,
    value,
    secFlightValidColor(mavlinkVfrHudValid)
  );

  // Airspeed.
  if (mavlinkVfrHudValid) {
    snprintf(
      value,
      sizeof(value),
      "%ld%s",
      lroundf(airspeed * CONFIG_GLASS_SPEED_SCALE),
      CONFIG_GLASS_SPEED_LABEL
    );
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_RIGHT_X,
    SEC_FLIGHT_ROW3_Y,
    value,
    secFlightValidColor(mavlinkVfrHudValid)
  );

  // Ground speed.
  if (mavlinkVfrHudValid) {
    snprintf(
      value,
      sizeof(value),
      "%ld%s",
      lroundf(groundspeed * CONFIG_GLASS_SPEED_SCALE),
      CONFIG_GLASS_SPEED_LABEL
    );
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_LEFT_X,
    SEC_FLIGHT_ROW4_Y,
    value,
    secFlightValidColor(mavlinkVfrHudValid)
  );

  // Arm state.
  if (mavlinkHeartbeatValid) {
    snprintf(value, sizeof(value), "%s", vehicleArmed ? "ARMED" : "SAFE");
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secFlightUpdateTileValue(
    SEC_FLIGHT_RIGHT_X,
    SEC_FLIGHT_ROW4_Y,
    value,
    mavlinkHeartbeatValid ?
      (vehicleArmed ? SEC_FLIGHT_WARN : SEC_FLIGHT_GOOD) :
      SEC_FLIGHT_BAD
  );
}