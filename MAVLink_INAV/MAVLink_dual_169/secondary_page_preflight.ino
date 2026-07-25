// ----------------------------------------------------
// Secondary page: preflight
// ----------------------------------------------------
//
// Preflight check/status page.
//
// Shows:
//   - GPS satellites
//   - GPS lock type
//   - cell voltage
//   - arm readiness
//   - flight mode
//   - flap position from configured RC channel
//   - gear position from configured RC channel
//
// Flap and gear RC values use the same config values as
// secondary_page_gear_flaps.ino.
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// MAVLink telemetry data
extern uint16_t rcChannelRaw[18];
extern bool mavlinkRcChannelsValid;

extern uint8_t gpsFixType;
extern uint8_t gpsSatellitesVisible;
extern bool mavlinkGpsValid;

extern float batteryCellVoltage;
extern float batteryLowestCellVoltage;
extern bool batteryLowestCellVoltageValid;
extern bool mavlinkBatteryValid;

extern bool mavlinkHeartbeatValid;
extern bool vehicleArmed;
extern char flightModeText[18];

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t PF_BLACK      = RGB565(0, 0, 0);
static const uint16_t PF_PANEL      = RGB565(4, 13, 16);
static const uint16_t PF_PANEL2     = RGB565(4, 22, 28);

static const uint16_t PF_LINE       = RGB565(210, 210, 210);
static const uint16_t PF_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t PF_LABEL      = RGB565(0, 235, 255);
static const uint16_t PF_TEXT       = RGB565(230, 245, 245);
static const uint16_t PF_DIM        = RGB565(115, 150, 155);

static const uint16_t PF_GREEN      = RGB565(80, 255, 120);
static const uint16_t PF_WARN       = RGB565(255, 175, 40);
static const uint16_t PF_BAD        = RGB565(255, 65, 65);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int PF_W = 240;
static const int PF_H = 280;

static const int PF_HEADER_X = 4;
static const int PF_HEADER_Y = 4;
static const int PF_HEADER_W = 232;
static const int PF_HEADER_H = 48;

static const int PF_ROW_X = 4;
static const int PF_ROW_W = 232;
static const int PF_ROW_H = 28;

static const int PF_ROW1_Y = 58;
static const int PF_ROW2_Y = 88;
static const int PF_ROW3_Y = 118;
static const int PF_ROW4_Y = 148;
static const int PF_ROW5_Y = 178;
static const int PF_ROW6_Y = 208;
static const int PF_ROW7_Y = 238;

// ----------------------------------------------------
// State constants
// ----------------------------------------------------

static const uint8_t PF_GEAR_RETRACTED = 0;
static const uint8_t PF_GEAR_EXTENDED  = 1;
static const uint8_t PF_GEAR_UNKNOWN   = 2;

static const uint8_t PF_FLAP_UP      = 0;
static const uint8_t PF_FLAP_HALF    = 1;
static const uint8_t PF_FLAP_FULL    = 2;
static const uint8_t PF_FLAP_UNKNOWN = 3;

static const uint8_t PF_CHECK_GOOD = 0;
static const uint8_t PF_CHECK_WARN = 1;
static const uint8_t PF_CHECK_BAD  = 2;

// ----------------------------------------------------
// Text helpers
// ----------------------------------------------------

static int pfTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void pfPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = pfTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(colour);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

static void pfDrawBox(
  int x,
  int y,
  int w,
  int h,
  uint16_t fillColour
) {
  secGfx->fillRect(x, y, w, h, fillColour);
  secGfx->drawRect(x, y, w, h, PF_LINE);
  secGfx->drawRect(x + 1, y + 1, w - 2, h - 2, PF_LINE_SOFT);
}

static uint16_t pfCheckColour(uint8_t checkState)
{
  if (checkState == PF_CHECK_GOOD) {
    return PF_GREEN;
  }

  if (checkState == PF_CHECK_WARN) {
    return PF_WARN;
  }

  return PF_BAD;
}

static const char* pfCheckText(uint8_t checkState)
{
  if (checkState == PF_CHECK_GOOD) {
    return "OK";
  }

  if (checkState == PF_CHECK_WARN) {
    return "CHK";
  }

  return "BAD";
}

// ----------------------------------------------------
// RC helpers
// ----------------------------------------------------

static uint16_t pfGetRcChannelRaw(uint8_t channelNumber)
{
  if (channelNumber < 1 || channelNumber > 18) {
    return 0;
  }

  return rcChannelRaw[channelNumber - 1];
}

static bool pfRcRawValid(uint16_t raw)
{
  if (!mavlinkRcChannelsValid) {
    return false;
  }

  if (raw < CONFIG_GEAR_FLAPS_RC_VALID_LOW_US) {
    return false;
  }

  if (raw > CONFIG_GEAR_FLAPS_RC_VALID_HIGH_US) {
    return false;
  }

  return true;
}

static int pfAbsInt(int value)
{
  if (value < 0) {
    return -value;
  }

  return value;
}

// ----------------------------------------------------
// Gear / flap helpers
// ----------------------------------------------------

static uint8_t pfGetGearState()
{
  uint16_t raw =
    pfGetRcChannelRaw(CONFIG_GEAR_FLAPS_GEAR_RC_CHANNEL);

  if (!pfRcRawValid(raw)) {
    return PF_GEAR_UNKNOWN;
  }

  uint16_t threshold =
    (uint16_t)(((uint32_t)CONFIG_GEAR_RETRACTED_US +
                (uint32_t)CONFIG_GEAR_EXTENDED_US) / 2UL);

  if (CONFIG_GEAR_EXTENDED_US >= CONFIG_GEAR_RETRACTED_US) {
    if (raw >= threshold) {
      return PF_GEAR_EXTENDED;
    }

    return PF_GEAR_RETRACTED;
  }

  if (raw <= threshold) {
    return PF_GEAR_EXTENDED;
  }

  return PF_GEAR_RETRACTED;
}

static const char* pfGearText(uint8_t gearState)
{
  switch (gearState) {
    case PF_GEAR_RETRACTED:
      return "UP";

    case PF_GEAR_EXTENDED:
      return "DOWN";

    default:
      return "UNKNOWN";
  }
}

static uint8_t pfGetFlapState()
{
  uint16_t raw =
    pfGetRcChannelRaw(CONFIG_GEAR_FLAPS_FLAP_RC_CHANNEL);

  if (!pfRcRawValid(raw)) {
    return PF_FLAP_UNKNOWN;
  }

  int diffUp =
    pfAbsInt((int)raw - (int)CONFIG_FLAPS_UP_US);

  int diffHalf =
    pfAbsInt((int)raw - (int)CONFIG_FLAPS_HALF_US);

  int diffFull =
    pfAbsInt((int)raw - (int)CONFIG_FLAPS_FULL_US);

  if (diffUp <= diffHalf && diffUp <= diffFull) {
    return PF_FLAP_UP;
  }

  if (diffHalf <= diffUp && diffHalf <= diffFull) {
    return PF_FLAP_HALF;
  }

  return PF_FLAP_FULL;
}

static const char* pfFlapText(uint8_t flapState)
{
  switch (flapState) {
    case PF_FLAP_UP:
      return "UP";

    case PF_FLAP_HALF:
      return "HALF";

    case PF_FLAP_FULL:
      return "FULL";

    default:
      return "UNKNOWN";
  }
}

// ----------------------------------------------------
// Telemetry helpers
// ----------------------------------------------------

static const char* pfGpsLockText()
{
  if (!mavlinkGpsValid) {
    return "NO GPS";
  }

  switch (gpsFixType) {
    case 0:
      return "NO GPS";

    case 1:
      return "NO FIX";

    case 2:
      return "2D";

    case 3:
      return "3D";

    case 4:
      return "DGPS";

    case 5:
      return "RTK F";

    case 6:
      return "RTK H";

    default:
      return "UNK";
  }
}

static uint8_t pfGpsSatsCheck()
{
  if (!mavlinkGpsValid) {
    return PF_CHECK_BAD;
  }

  if (gpsSatellitesVisible >= CONFIG_PREFLIGHT_MIN_GPS_SATS) {
    return PF_CHECK_GOOD;
  }

  if (gpsSatellitesVisible > 0) {
    return PF_CHECK_WARN;
  }

  return PF_CHECK_BAD;
}

static uint8_t pfGpsLockCheck()
{
  if (!mavlinkGpsValid) {
    return PF_CHECK_BAD;
  }

  if (CONFIG_PREFLIGHT_REQUIRE_3D_GPS) {
    if (gpsFixType >= 3) {
      return PF_CHECK_GOOD;
    }

    if (gpsFixType == 2) {
      return PF_CHECK_WARN;
    }

    return PF_CHECK_BAD;
  }

  if (gpsFixType >= 2) {
    return PF_CHECK_GOOD;
  }

  return PF_CHECK_BAD;
}

static float pfDisplayedCellVoltage()
{
  if (batteryLowestCellVoltageValid) {
    return batteryLowestCellVoltage;
  }

  return batteryCellVoltage;
}

static bool pfCellVoltageValid()
{
  if (batteryLowestCellVoltageValid) {
    return true;
  }

  return mavlinkBatteryValid && batteryCellVoltage > 0.0f;
}

static uint8_t pfCellCheck()
{
  float cellV = pfDisplayedCellVoltage();

  if (!pfCellVoltageValid()) {
    return PF_CHECK_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_CRITICAL_V) {
    return PF_CHECK_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_WARN_V) {
    return PF_CHECK_WARN;
  }

  return PF_CHECK_GOOD;
}

static uint8_t pfModeCheck()
{
  if (!mavlinkHeartbeatValid) {
    return PF_CHECK_BAD;
  }

  if (strcmp(flightModeText, "NO HB") == 0) {
    return PF_CHECK_BAD;
  }

  return PF_CHECK_GOOD;
}

static uint8_t pfFlapCheck(uint8_t flapState)
{
  if (flapState == PF_FLAP_UNKNOWN) {
    return PF_CHECK_BAD;
  }

  if (!CONFIG_PREFLIGHT_REQUIRE_FLAPS_UP) {
    return PF_CHECK_GOOD;
  }

  if (flapState == PF_FLAP_UP) {
    return PF_CHECK_GOOD;
  }

  return PF_CHECK_WARN;
}

static uint8_t pfGearCheck(uint8_t gearState)
{
  if (gearState == PF_GEAR_UNKNOWN) {
    return PF_CHECK_BAD;
  }

  if (!CONFIG_PREFLIGHT_REQUIRE_GEAR_DOWN) {
    return PF_CHECK_GOOD;
  }

  if (gearState == PF_GEAR_EXTENDED) {
    return PF_CHECK_GOOD;
  }

  return PF_CHECK_WARN;
}

static bool pfOverallReady(
  uint8_t gpsSatsCheck,
  uint8_t gpsLockCheck,
  uint8_t cellCheck,
  uint8_t modeCheck,
  uint8_t flapCheck,
  uint8_t gearCheck
) {
  if (vehicleArmed) {
    return false;
  }

  if (gpsSatsCheck != PF_CHECK_GOOD) {
    return false;
  }

  if (gpsLockCheck != PF_CHECK_GOOD) {
    return false;
  }

  if (cellCheck != PF_CHECK_GOOD) {
    return false;
  }

  if (modeCheck != PF_CHECK_GOOD) {
    return false;
  }

  if (flapCheck != PF_CHECK_GOOD) {
    return false;
  }

  if (gearCheck != PF_CHECK_GOOD) {
    return false;
  }

  return true;
}

// ----------------------------------------------------
// Drawing helpers
// ----------------------------------------------------

static void pfDrawStaticLayout()
{
  secGfx->fillScreen(PF_BLACK);

  secGfx->drawRect(0, 0, PF_W, PF_H, PF_LINE_SOFT);
  secGfx->drawRect(1, 1, PF_W - 2, PF_H - 2, RGB565(35, 55, 60));

  pfDrawBox(
    PF_HEADER_X,
    PF_HEADER_Y,
    PF_HEADER_W,
    PF_HEADER_H,
    PF_PANEL2
  );

  pfPrintCentered(
    PF_HEADER_X,
    PF_HEADER_Y + 5,
    PF_HEADER_W,
    "PREFLIGHT",
    2,
    PF_LABEL
  );

  pfDrawBox(PF_ROW_X, PF_ROW1_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW2_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW3_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW4_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW5_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW6_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);
  pfDrawBox(PF_ROW_X, PF_ROW7_Y, PF_ROW_W, PF_ROW_H, PF_PANEL);

  secGfx->setTextSize(2);
  secGfx->setTextColor(PF_LABEL);

  secGfx->setCursor(10, PF_ROW1_Y + 7);
  secGfx->print("GPS SAT");

  secGfx->setCursor(10, PF_ROW2_Y + 7);
  secGfx->print("GPS FIX");

  secGfx->setCursor(10, PF_ROW3_Y + 7);
  secGfx->print("CELL V");

  secGfx->setCursor(10, PF_ROW4_Y + 7);
  secGfx->print("ARM");

  secGfx->setCursor(10, PF_ROW5_Y + 7);
  secGfx->print("MODE");

  secGfx->setCursor(10, PF_ROW6_Y + 7);
  secGfx->print("FLAPS");

  secGfx->setCursor(10, PF_ROW7_Y + 7);
  secGfx->print("GEAR");
}

static void pfUpdateHeader(bool ready)
{
  secGfx->fillRect(
    PF_HEADER_X + 3,
    PF_HEADER_Y + 27,
    PF_HEADER_W - 6,
    18,
    PF_PANEL2
  );

  pfPrintCentered(
    PF_HEADER_X,
    PF_HEADER_Y + 28,
    PF_HEADER_W,
    ready ? "READY TO ARM" : "CHECK ITEMS",
    2,
    ready ? PF_GREEN : PF_WARN
  );
}

static void pfUpdateRow(
  int y,
  const char *value,
  uint8_t checkState
) {
  uint16_t colour = pfCheckColour(checkState);

  secGfx->fillRect(
    98,
    y + 3,
    88,
    PF_ROW_H - 6,
    PF_PANEL
  );

  secGfx->fillRect(
    188,
    y + 3,
    45,
    PF_ROW_H - 6,
    PF_PANEL
  );

  secGfx->setTextSize(2);
  secGfx->setTextColor(colour);
  secGfx->setCursor(100, y + 7);
  secGfx->print(value);

  pfPrintCentered(
    188,
    y + 7,
    45,
    pfCheckText(checkState),
    2,
    colour
  );
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryPreflightPage()
{
  char value[24];

  uint8_t gearState = pfGetGearState();
  uint8_t flapState = pfGetFlapState();

  uint8_t gpsSatsCheck = pfGpsSatsCheck();
  uint8_t gpsLockCheck = pfGpsLockCheck();
  uint8_t cellCheck = pfCellCheck();
  uint8_t modeCheck = pfModeCheck();
  uint8_t flapCheck = pfFlapCheck(flapState);
  uint8_t gearCheck = pfGearCheck(gearState);

  bool ready =
    pfOverallReady(
      gpsSatsCheck,
      gpsLockCheck,
      cellCheck,
      modeCheck,
      flapCheck,
      gearCheck
    );

  if (secondaryNeedsFullRedraw) {
    pfDrawStaticLayout();
  }

  pfUpdateHeader(ready);

  snprintf(
    value,
    sizeof(value),
    "%u",
    gpsSatellitesVisible
  );

  pfUpdateRow(
    PF_ROW1_Y,
    mavlinkGpsValid ? value : "---",
    gpsSatsCheck
  );

  pfUpdateRow(
    PF_ROW2_Y,
    pfGpsLockText(),
    gpsLockCheck
  );

  if (pfCellVoltageValid()) {
    snprintf(
      value,
      sizeof(value),
      "%.2fV",
      pfDisplayedCellVoltage()
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  pfUpdateRow(
    PF_ROW3_Y,
    value,
    cellCheck
  );

  if (vehicleArmed) {
    snprintf(value, sizeof(value), "ARMED");
  } else if (ready) {
    snprintf(value, sizeof(value), "READY");
  } else {
    snprintf(value, sizeof(value), "WAIT");
  }

  pfUpdateRow(
    PF_ROW4_Y,
    value,
    vehicleArmed ? PF_CHECK_WARN : (ready ? PF_CHECK_GOOD : PF_CHECK_WARN)
  );

  if (mavlinkHeartbeatValid) {
    snprintf(
      value,
      sizeof(value),
      "%s",
      flightModeText
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  pfUpdateRow(
    PF_ROW5_Y,
    value,
    modeCheck
  );

  pfUpdateRow(
    PF_ROW6_Y,
    pfFlapText(flapState),
    flapCheck
  );

  pfUpdateRow(
    PF_ROW7_Y,
    pfGearText(gearState),
    gearCheck
  );
}