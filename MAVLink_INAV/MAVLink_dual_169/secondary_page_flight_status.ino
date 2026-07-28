// ----------------------------------------------------
// Secondary page: flight status
// ----------------------------------------------------
//
// Styled to match the other secondary pages:
//   - dark background
//   - dark-blue panels
//   - light grey borders
//   - cyan labels
//   - green / yellow / red status colours
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <stdio.h>
#include <string.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// MAVLink telemetry
extern char flightModeText[18];
extern bool vehicleArmed;
extern bool mavlinkHeartbeatValid;

extern float airspeed;
extern float groundspeed;
extern float altitude_msl;
extern float climb_rate;
extern int16_t heading_deg;
extern bool mavlinkVfrHudValid;

extern uint8_t gpsFixType;
extern uint8_t gpsSatellitesVisible;
extern bool mavlinkGpsValid;

extern uint8_t rssiPercent;
extern bool rssiValid;

extern float batteryCellVoltage;
extern float batteryLowestCellVoltage;
extern bool batteryLowestCellVoltageValid;
extern bool mavlinkBatteryValid;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t FS_BG         = RGB565(3, 8, 14);
static const uint16_t FS_PANEL      = RGB565(6, 18, 30);
static const uint16_t FS_PANEL_DARK = RGB565(2, 10, 18);

static const uint16_t FS_LINE       = RGB565(210, 210, 210);
static const uint16_t FS_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t FS_LABEL      = RGB565(0, 210, 255);
static const uint16_t FS_VALUE      = RGB565(245, 245, 245);

static const uint16_t FS_GREEN      = RGB565(0, 230, 90);
static const uint16_t FS_WARN       = RGB565(255, 190, 0);
static const uint16_t FS_BAD        = RGB565(255, 60, 50);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int FS_TITLE_X = 6;
static const int FS_TITLE_Y = 4;
static const int FS_TITLE_W = 228;
static const int FS_TITLE_H = 28;

static const int FS_MODE_X = 6;
static const int FS_MODE_Y = 38;
static const int FS_MODE_W = 228;
static const int FS_MODE_H = 52;

static const int FS_LEFT_X = 6;
static const int FS_RIGHT_X = 122;

static const int FS_TILE_W = 112;
static const int FS_TILE_H = 42;

static const int FS_ROW1_Y = 98;
static const int FS_ROW2_Y = 146;
static const int FS_ROW3_Y = 194;
static const int FS_ROW4_Y = 238;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static void fsDrawTileFrame(int x, int y, int w, int h, const char *label)
{
  secGfx->fillRoundRect(x, y, w, h, 4, FS_PANEL);
  secGfx->drawRoundRect(x, y, w, h, 4, FS_LINE);
  secGfx->drawLine(x + 1, y + 18, x + w - 2, y + 18, FS_LINE_SOFT);

  secGfx->setTextSize(1);
  secGfx->setTextColor(FS_LABEL, FS_PANEL);
  secGfx->setCursor(x + 6, y + 6);
  secGfx->print(label);
}

static void fsPrintCentered(
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

  secGfx->setTextSize(textSize);
  secGfx->getTextBounds(text, 0, 0, &x1, &y1, &textW, &textH);

  int drawX = x + ((w - (int)textW) / 2);

  if (drawX < x + 2) {
    drawX = x + 2;
  }

  secGfx->setTextColor(colour, bg);
  secGfx->setCursor(drawX, y);
  secGfx->print(text);
}

static void fsPrintTileValue(
  int x,
  int y,
  int w,
  int h,
  const char *value,
  uint16_t colour
) {
  secGfx->fillRect(x + 3, y + 20, w - 6, h - 23, FS_PANEL);
  fsPrintCentered(x, y + 24, w, value, 2, colour, FS_PANEL);
}

static void fsDrawStaticLayout()
{
  secGfx->fillScreen(FS_BG);

  secGfx->fillRoundRect(
    FS_TITLE_X,
    FS_TITLE_Y,
    FS_TITLE_W,
    FS_TITLE_H,
    4,
    FS_PANEL_DARK
  );

  secGfx->drawRoundRect(
    FS_TITLE_X,
    FS_TITLE_Y,
    FS_TITLE_W,
    FS_TITLE_H,
    4,
    FS_LINE
  );

  secGfx->setTextSize(2);
  secGfx->setTextColor(FS_LABEL, FS_PANEL_DARK);
  secGfx->setCursor(14, 10);
  secGfx->print("FLIGHT STATUS");

  fsDrawTileFrame(FS_MODE_X, FS_MODE_Y, FS_MODE_W, FS_MODE_H, "MODE / ARM");

  fsDrawTileFrame(FS_LEFT_X,  FS_ROW1_Y, FS_TILE_W, FS_TILE_H, "AIRSPEED");
  fsDrawTileFrame(FS_RIGHT_X, FS_ROW1_Y, FS_TILE_W, FS_TILE_H, "GND SPD");

  fsDrawTileFrame(FS_LEFT_X,  FS_ROW2_Y, FS_TILE_W, FS_TILE_H, "ALTITUDE");
  fsDrawTileFrame(FS_RIGHT_X, FS_ROW2_Y, FS_TILE_W, FS_TILE_H, "VERT SPD");

  fsDrawTileFrame(FS_LEFT_X,  FS_ROW3_Y, FS_TILE_W, FS_TILE_H, "HEADING");
  fsDrawTileFrame(FS_RIGHT_X, FS_ROW3_Y, FS_TILE_W, FS_TILE_H, "GPS");

  fsDrawTileFrame(FS_LEFT_X,  FS_ROW4_Y, FS_TILE_W, FS_TILE_H, "RSSI");
  fsDrawTileFrame(FS_RIGHT_X, FS_ROW4_Y, FS_TILE_W, FS_TILE_H, "CELL V");
}

static uint16_t fsHeartbeatColour()
{
  if (!mavlinkHeartbeatValid) {
    return FS_BAD;
  }

  if (vehicleArmed) {
    return FS_GREEN;
  }

  return FS_WARN;
}

static uint16_t fsGpsColour()
{
  if (!mavlinkGpsValid) {
    return FS_BAD;
  }

  if (gpsFixType >= 3 && gpsSatellitesVisible >= 6) {
    return FS_GREEN;
  }

  if (gpsFixType >= 2) {
    return FS_WARN;
  }

  return FS_BAD;
}

static uint16_t fsRssiColour()
{
  if (!rssiValid) {
    return FS_BAD;
  }

  if (rssiPercent < 35) {
    return FS_BAD;
  }

  if (rssiPercent < 60) {
    return FS_WARN;
  }

  return FS_GREEN;
}

static uint16_t fsCellColour(float cellV)
{
  if (!mavlinkBatteryValid || cellV <= 0.0f) {
    return FS_BAD;
  }

  if (cellV < 3.40f) {
    return FS_BAD;
  }

  if (cellV < 3.60f) {
    return FS_WARN;
  }

  return FS_GREEN;
}

// ----------------------------------------------------
// Dynamic drawing
// ----------------------------------------------------

static void fsUpdateModePanel()
{
  secGfx->fillRect(
    FS_MODE_X + 3,
    FS_MODE_Y + 20,
    FS_MODE_W - 6,
    FS_MODE_H - 23,
    FS_PANEL
  );

  const char *modeText = "NO LINK";

  if (mavlinkHeartbeatValid) {
    modeText = flightModeText;
  }

  const char *armText = "NO HB";

  if (mavlinkHeartbeatValid) {
    if (vehicleArmed) {
      armText = "ARMED";
    } else {
      armText = "SAFE";
    }
  }

  secGfx->setTextSize(2);
  secGfx->setTextColor(
    mavlinkHeartbeatValid ? FS_VALUE : FS_BAD,
    FS_PANEL
  );
  secGfx->setCursor(FS_MODE_X + 10, FS_MODE_Y + 28);
  secGfx->print(modeText);

  int16_t x1;
  int16_t y1;
  uint16_t textW;
  uint16_t textH;

  secGfx->setTextSize(2);
  secGfx->getTextBounds(armText, 0, 0, &x1, &y1, &textW, &textH);

  secGfx->setTextColor(fsHeartbeatColour(), FS_PANEL);
  secGfx->setCursor(FS_MODE_X + FS_MODE_W - 10 - textW, FS_MODE_Y + 28);
  secGfx->print(armText);
}

static void fsUpdateFlightTiles()
{
  char value[20];

  if (mavlinkVfrHudValid) {
    snprintf(value, sizeof(value), "%.0f", airspeed * 3.6f);
    fsPrintTileValue(FS_LEFT_X, FS_ROW1_Y, FS_TILE_W, FS_TILE_H, value, FS_VALUE);

    snprintf(value, sizeof(value), "%.0f", groundspeed * 3.6f);
    fsPrintTileValue(FS_RIGHT_X, FS_ROW1_Y, FS_TILE_W, FS_TILE_H, value, FS_VALUE);

    snprintf(value, sizeof(value), "%.0f", altitude_msl);
    fsPrintTileValue(FS_LEFT_X, FS_ROW2_Y, FS_TILE_W, FS_TILE_H, value, FS_VALUE);

    snprintf(value, sizeof(value), "%.1f", climb_rate);

    uint16_t climbColour = FS_VALUE;

    if (climb_rate > 1.0f) {
      climbColour = FS_GREEN;
    } else if (climb_rate < -1.0f) {
      climbColour = FS_WARN;
    }

    fsPrintTileValue(FS_RIGHT_X, FS_ROW2_Y, FS_TILE_W, FS_TILE_H, value, climbColour);

    snprintf(value, sizeof(value), "%03d", heading_deg);
    fsPrintTileValue(FS_LEFT_X, FS_ROW3_Y, FS_TILE_W, FS_TILE_H, value, FS_VALUE);
  } else {
    fsPrintTileValue(FS_LEFT_X,  FS_ROW1_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
    fsPrintTileValue(FS_RIGHT_X, FS_ROW1_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
    fsPrintTileValue(FS_LEFT_X,  FS_ROW2_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
    fsPrintTileValue(FS_RIGHT_X, FS_ROW2_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
    fsPrintTileValue(FS_LEFT_X,  FS_ROW3_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
  }
}

static void fsUpdateSystemTiles()
{
  char value[20];

  if (mavlinkGpsValid) {
    snprintf(value, sizeof(value), "%u/%u", gpsFixType, gpsSatellitesVisible);
    fsPrintTileValue(FS_RIGHT_X, FS_ROW3_Y, FS_TILE_W, FS_TILE_H, value, fsGpsColour());
  } else {
    fsPrintTileValue(FS_RIGHT_X, FS_ROW3_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
  }

  if (rssiValid) {
    snprintf(value, sizeof(value), "%u%%", rssiPercent);
    fsPrintTileValue(FS_LEFT_X, FS_ROW4_Y, FS_TILE_W, FS_TILE_H, value, fsRssiColour());
  } else {
    fsPrintTileValue(FS_LEFT_X, FS_ROW4_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
  }

  float cellV = batteryCellVoltage;

  if (batteryLowestCellVoltageValid) {
    cellV = batteryLowestCellVoltage;
  }

  if (mavlinkBatteryValid && cellV > 0.0f) {
    snprintf(value, sizeof(value), "%.2f", cellV);
    fsPrintTileValue(FS_RIGHT_X, FS_ROW4_Y, FS_TILE_W, FS_TILE_H, value, fsCellColour(cellV));
  } else {
    fsPrintTileValue(FS_RIGHT_X, FS_ROW4_Y, FS_TILE_W, FS_TILE_H, "---", FS_BAD);
  }
}

// ----------------------------------------------------
// Required page entry point
// ----------------------------------------------------

void drawSecondaryFlightStatusPage()
{
  if (secondaryNeedsFullRedraw) {
    fsDrawStaticLayout();
  }

  fsUpdateModePanel();
  fsUpdateFlightTiles();
  fsUpdateSystemTiles();
}