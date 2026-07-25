// ----------------------------------------------------
// Secondary page: gear / flaps
// ----------------------------------------------------
//
// Graphical landing gear and flap position page.
//
// Inputs:
//   gear  = RC channel configured in CONFIG_GEAR_FLAPS_GEAR_RC_CHANNEL
//   flaps = RC channel configured in CONFIG_GEAR_FLAPS_FLAP_RC_CHANNEL
//
// Gear logic:
//   low  = retracted / UP
//   high = extended / DOWN
//
// Gear transition:
//   When commanded gear state changes, the display shows:
//     - up arrow for retracting
//     - down arrow for extending
//   for GF_GEAR_TRANSITION_MS.
//
// Flap logic:
//   nearest configured RC value decides state:
//     CONFIG_FLAPS_UP_US
//     CONFIG_FLAPS_HALF_US
//     CONFIG_FLAPS_FULL_US
//
// This version avoids full-area redraws during refresh.
// Only the changing values/icons are redrawn.
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

extern float airspeed;
extern bool mavlinkVfrHudValid;

extern float batteryCellVoltage;
extern float batteryLowestCellVoltage;
extern bool batteryLowestCellVoltageValid;
extern bool mavlinkBatteryValid;

// ----------------------------------------------------
// Gear transition timing
// ----------------------------------------------------

static const unsigned long GF_GEAR_TRANSITION_MS = 2000UL;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t GF_BLACK      = RGB565(0, 0, 0);
static const uint16_t GF_PANEL      = RGB565(4, 13, 16);
static const uint16_t GF_PANEL2     = RGB565(4, 22, 28);

static const uint16_t GF_LINE       = RGB565(210, 210, 210);
static const uint16_t GF_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t GF_LABEL      = RGB565(0, 235, 255);
static const uint16_t GF_TEXT       = RGB565(230, 245, 245);
static const uint16_t GF_DIM        = RGB565(115, 150, 155);

static const uint16_t GF_GREEN      = RGB565(80, 255, 120);
static const uint16_t GF_WARN       = RGB565(255, 175, 40);
static const uint16_t GF_BAD        = RGB565(255, 65, 65);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int GF_W = 240;
static const int GF_H = 280;

static const int GF_TOP_BOX_Y = 34;
static const int GF_TOP_BOX_W = 76;
static const int GF_TOP_BOX_H = 52;

static const int GF_CELL_BOX_X = 4;
static const int GF_SPEED_BOX_X = 160;

static const int GF_AIRCRAFT_CX = 120;
static const int GF_AIRCRAFT_NOSE_Y = 86;
static const int GF_AIRCRAFT_TAIL_Y = 178;

static const int GF_FLAP_PANEL_X = 5;
static const int GF_FLAP_PANEL_Y = 190;
static const int GF_FLAP_PANEL_W = 230;
static const int GF_FLAP_PANEL_H = 84;

// ----------------------------------------------------
// State constants
// ----------------------------------------------------

static const uint8_t GF_GEAR_RETRACTED  = 0;
static const uint8_t GF_GEAR_EXTENDED   = 1;
static const uint8_t GF_GEAR_UNKNOWN    = 2;
static const uint8_t GF_GEAR_RETRACTING = 3;
static const uint8_t GF_GEAR_EXTENDING  = 4;

static const uint8_t GF_FLAP_UP      = 0;
static const uint8_t GF_FLAP_HALF    = 1;
static const uint8_t GF_FLAP_FULL    = 2;
static const uint8_t GF_FLAP_UNKNOWN = 3;

// ----------------------------------------------------
// Gear transition state
// ----------------------------------------------------

static bool gfGearInitialised = false;
static bool gfGearTransitionActive = false;

static uint8_t gfGearStableState = GF_GEAR_UNKNOWN;
static uint8_t gfGearTargetState = GF_GEAR_UNKNOWN;

static unsigned long gfGearTransitionStartMs = 0;

// ----------------------------------------------------
// Text helpers
// ----------------------------------------------------

static int gfTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void gfPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = gfTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(colour);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

static void gfDrawBox(
  int x,
  int y,
  int w,
  int h,
  uint16_t fillColour
) {
  secGfx->fillRect(x, y, w, h, fillColour);
  secGfx->drawRect(x, y, w, h, GF_LINE);
  secGfx->drawRect(x + 1, y + 1, w - 2, h - 2, GF_LINE_SOFT);
}

// ----------------------------------------------------
// RC helpers
// ----------------------------------------------------

static uint16_t gfGetRcChannelRaw(uint8_t channelNumber)
{
  if (channelNumber < 1 || channelNumber > 18) {
    return 0;
  }

  return rcChannelRaw[channelNumber - 1];
}

static bool gfRcRawValid(uint16_t raw)
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

static int gfAbsInt(int value)
{
  if (value < 0) {
    return -value;
  }

  return value;
}

static uint8_t gfGetGearCommandState()
{
  uint16_t raw =
    gfGetRcChannelRaw(CONFIG_GEAR_FLAPS_GEAR_RC_CHANNEL);

  if (!gfRcRawValid(raw)) {
    return GF_GEAR_UNKNOWN;
  }

  uint16_t threshold =
    (uint16_t)(((uint32_t)CONFIG_GEAR_RETRACTED_US +
                (uint32_t)CONFIG_GEAR_EXTENDED_US) / 2UL);

  if (CONFIG_GEAR_EXTENDED_US >= CONFIG_GEAR_RETRACTED_US) {
    if (raw >= threshold) {
      return GF_GEAR_EXTENDED;
    }

    return GF_GEAR_RETRACTED;
  }

  if (raw <= threshold) {
    return GF_GEAR_EXTENDED;
  }

  return GF_GEAR_RETRACTED;
}

static uint8_t gfGetGearDisplayState()
{
  uint8_t commandState = gfGetGearCommandState();
  unsigned long nowMs = millis();

  if (commandState == GF_GEAR_UNKNOWN) {
    gfGearInitialised = false;
    gfGearTransitionActive = false;
    gfGearStableState = GF_GEAR_UNKNOWN;
    gfGearTargetState = GF_GEAR_UNKNOWN;
    return GF_GEAR_UNKNOWN;
  }

  if (!gfGearInitialised) {
    gfGearInitialised = true;
    gfGearTransitionActive = false;
    gfGearStableState = commandState;
    gfGearTargetState = commandState;
    return commandState;
  }

  if (gfGearTransitionActive) {
    if (commandState != gfGearTargetState) {
      gfGearTargetState = commandState;
      gfGearTransitionStartMs = nowMs;
    }

    if (nowMs - gfGearTransitionStartMs >= GF_GEAR_TRANSITION_MS) {
      gfGearTransitionActive = false;
      gfGearStableState = gfGearTargetState;
      return gfGearStableState;
    }

    if (gfGearTargetState == GF_GEAR_RETRACTED) {
      return GF_GEAR_RETRACTING;
    }

    return GF_GEAR_EXTENDING;
  }

  if (commandState != gfGearStableState) {
    gfGearTargetState = commandState;
    gfGearTransitionStartMs = nowMs;
    gfGearTransitionActive = true;

    if (gfGearTargetState == GF_GEAR_RETRACTED) {
      return GF_GEAR_RETRACTING;
    }

    return GF_GEAR_EXTENDING;
  }

  return gfGearStableState;
}

static uint8_t gfGetFlapState()
{
  uint16_t raw =
    gfGetRcChannelRaw(CONFIG_GEAR_FLAPS_FLAP_RC_CHANNEL);

  if (!gfRcRawValid(raw)) {
    return GF_FLAP_UNKNOWN;
  }

  int diffUp =
    gfAbsInt((int)raw - (int)CONFIG_FLAPS_UP_US);

  int diffHalf =
    gfAbsInt((int)raw - (int)CONFIG_FLAPS_HALF_US);

  int diffFull =
    gfAbsInt((int)raw - (int)CONFIG_FLAPS_FULL_US);

  if (diffUp <= diffHalf && diffUp <= diffFull) {
    return GF_FLAP_UP;
  }

  if (diffHalf <= diffUp && diffHalf <= diffFull) {
    return GF_FLAP_HALF;
  }

  return GF_FLAP_FULL;
}

static const char* gfGearText(uint8_t state)
{
  switch (state) {
    case GF_GEAR_RETRACTED:
      return "UP";

    case GF_GEAR_EXTENDED:
      return "DOWN";

    case GF_GEAR_RETRACTING:
      return "UP";

    case GF_GEAR_EXTENDING:
      return "DOWN";

    default:
      return "UNK";
  }
}

static const char* gfFlapText(uint8_t state)
{
  switch (state) {
    case GF_FLAP_UP:
      return "UP";

    case GF_FLAP_HALF:
      return "HALF";

    case GF_FLAP_FULL:
      return "FULL";

    default:
      return "UNK";
  }
}

static bool gfGearIsMoving(uint8_t state)
{
  return state == GF_GEAR_RETRACTING ||
         state == GF_GEAR_EXTENDING;
}

static uint16_t gfGearStateColour(uint8_t state)
{
  if (state == GF_GEAR_UNKNOWN) {
    return GF_BAD;
  }

  if (gfGearIsMoving(state)) {
    return GF_WARN;
  }

  return GF_GREEN;
}

// ----------------------------------------------------
// Info panels
// ----------------------------------------------------

static float gfDisplayedCellVoltage()
{
  if (batteryLowestCellVoltageValid) {
    return batteryLowestCellVoltage;
  }

  return batteryCellVoltage;
}

static bool gfCellVoltageValid()
{
  if (batteryLowestCellVoltageValid) {
    return true;
  }

  return mavlinkBatteryValid && batteryCellVoltage > 0.0f;
}

static uint16_t gfCellColour(float cellV, bool valid)
{
  if (!valid) {
    return GF_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_CRITICAL_V) {
    return GF_BAD;
  }

  if (cellV <= CONFIG_BATTERY_CELL_WARN_V) {
    return GF_WARN;
  }

  return GF_GREEN;
}

static void gfDrawCellVoltagePanelStatic()
{
  gfDrawBox(
    GF_CELL_BOX_X,
    GF_TOP_BOX_Y,
    GF_TOP_BOX_W,
    GF_TOP_BOX_H,
    GF_PANEL
  );

  gfPrintCentered(
    GF_CELL_BOX_X,
    GF_TOP_BOX_Y + 5,
    GF_TOP_BOX_W,
    "CELL V",
    1,
    GF_LABEL
  );
}

static void gfUpdateCellVoltagePanel()
{
  char value[16];

  secGfx->fillRect(
    GF_CELL_BOX_X + 3,
    GF_TOP_BOX_Y + 17,
    GF_TOP_BOX_W - 6,
    32,
    GF_PANEL
  );

  float cellV = gfDisplayedCellVoltage();
  bool valid = gfCellVoltageValid();

  if (valid) {
    snprintf(value, sizeof(value), "%.2f", cellV);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  gfPrintCentered(
    GF_CELL_BOX_X,
    GF_TOP_BOX_Y + 19,
    GF_TOP_BOX_W,
    value,
    3,
    gfCellColour(cellV, valid)
  );

  gfPrintCentered(
    GF_CELL_BOX_X,
    GF_TOP_BOX_Y + 43,
    GF_TOP_BOX_W,
    "V/CELL",
    1,
    valid ? GF_GREEN : GF_BAD
  );
}

static void gfDrawAirspeedPanelStatic()
{
  gfDrawBox(
    GF_SPEED_BOX_X,
    GF_TOP_BOX_Y,
    GF_TOP_BOX_W,
    GF_TOP_BOX_H,
    GF_PANEL
  );

  gfPrintCentered(
    GF_SPEED_BOX_X,
    GF_TOP_BOX_Y + 5,
    GF_TOP_BOX_W,
    "AIRSPEED",
    1,
    GF_LABEL
  );
}

static void gfUpdateAirspeedPanel()
{
  char value[16];

  secGfx->fillRect(
    GF_SPEED_BOX_X + 3,
    GF_TOP_BOX_Y + 15,
    GF_TOP_BOX_W - 6,
    34,
    GF_PANEL
  );

  if (mavlinkVfrHudValid) {
    float displayedSpeed =
      airspeed * CONFIG_GLASS_SPEED_SCALE;

    snprintf(
      value,
      sizeof(value),
      "%ld",
      lroundf(displayedSpeed)
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  gfPrintCentered(
    GF_SPEED_BOX_X,
    GF_TOP_BOX_Y + 17,
    GF_TOP_BOX_W,
    value,
    4,
    mavlinkVfrHudValid ? GF_GREEN : GF_BAD
  );

  gfPrintCentered(
    GF_SPEED_BOX_X,
    GF_TOP_BOX_Y + 43,
    GF_TOP_BOX_W,
    CONFIG_GLASS_SPEED_LABEL,
    1,
    mavlinkVfrHudValid ? GF_GREEN : GF_BAD
  );
}

// ----------------------------------------------------
// Aircraft graphic
// ----------------------------------------------------

static void gfDrawAircraft()
{
  uint16_t body = RGB565(85, 92, 96);
  uint16_t bodyDim = RGB565(45, 52, 56);

  int cx = GF_AIRCRAFT_CX;

  secGfx->drawLine(cx - 6, 122, cx - 86, 140, body);
  secGfx->drawLine(cx - 86, 140, cx - 9, 151, body);
  secGfx->drawLine(cx - 9, 151, cx - 6, 122, body);

  secGfx->drawLine(cx + 6, 122, cx + 86, 140, body);
  secGfx->drawLine(cx + 86, 140, cx + 9, 151, body);
  secGfx->drawLine(cx + 9, 151, cx + 6, 122, body);

  secGfx->drawLine(cx, GF_AIRCRAFT_NOSE_Y, cx - 13, 164, body);
  secGfx->drawLine(cx, GF_AIRCRAFT_NOSE_Y, cx + 13, 164, body);
  secGfx->drawLine(cx - 13, 164, cx, GF_AIRCRAFT_TAIL_Y, body);
  secGfx->drawLine(cx + 13, 164, cx, GF_AIRCRAFT_TAIL_Y, body);

  secGfx->drawLine(cx, 91, cx, 174, bodyDim);
  secGfx->drawLine(cx - 5, 110, cx - 7, 160, bodyDim);
  secGfx->drawLine(cx + 5, 110, cx + 7, 160, bodyDim);

  secGfx->drawRect(cx - 7, 101, 14, 24, body);
  secGfx->drawLine(cx - 7, 113, cx + 7, 113, bodyDim);
  secGfx->drawLine(cx, 101, cx, 125, bodyDim);

  secGfx->drawLine(cx - 7, 163, cx - 42, 169, body);
  secGfx->drawLine(cx - 42, 169, cx - 9, 174, body);
  secGfx->drawLine(cx + 7, 163, cx + 42, 169, body);
  secGfx->drawLine(cx + 42, 169, cx + 9, 174, body);

  secGfx->drawPixel(cx, GF_AIRCRAFT_NOSE_Y, GF_LINE);
}

// ----------------------------------------------------
// Gear graphics
// ----------------------------------------------------

static void gfDrawGearArrow(
  int x,
  int y,
  bool arrowUp,
  uint16_t colour
) {
  int cx = x + 19;

  if (arrowUp) {
    secGfx->drawLine(cx, y + 29, cx, y + 10, colour);
    secGfx->drawLine(cx - 1, y + 29, cx - 1, y + 10, colour);
    secGfx->drawLine(cx + 1, y + 29, cx + 1, y + 10, colour);

    secGfx->fillTriangle(
      cx,
      y + 6,
      cx - 8,
      y + 16,
      cx + 8,
      y + 16,
      colour
    );
  } else {
    secGfx->drawLine(cx, y + 9, cx, y + 28, colour);
    secGfx->drawLine(cx - 1, y + 9, cx - 1, y + 28, colour);
    secGfx->drawLine(cx + 1, y + 9, cx + 1, y + 28, colour);

    secGfx->fillTriangle(
      cx,
      y + 32,
      cx - 8,
      y + 22,
      cx + 8,
      y + 22,
      colour
    );
  }
}

static void gfDrawGearIcon(
  int x,
  int y,
  uint8_t state,
  bool noseGear
) {
  uint16_t colour = gfGearStateColour(state);

  secGfx->fillRect(x, y, 38, 38, GF_BLACK);
  secGfx->drawRect(x, y, 38, 38, colour);
  secGfx->drawRect(x + 1, y + 1, 36, 36, colour);

  int cx = x + 19;
  int cy = y + 20;

  if (state == GF_GEAR_EXTENDED) {
    secGfx->drawLine(cx, y + 8, cx, y + 24, colour);
    secGfx->drawLine(cx - 7, y + 8, cx + 7, y + 8, colour);
    secGfx->drawCircle(cx, y + 28, 6, colour);
    secGfx->drawCircle(cx, y + 28, 2, colour);

    if (!noseGear) {
      secGfx->drawLine(cx - 5, y + 14, cx + 5, y + 14, colour);
      secGfx->drawLine(cx - 4, y + 18, cx + 4, y + 18, colour);
    }
  }
  else if (state == GF_GEAR_RETRACTED) {
    secGfx->drawCircle(cx, cy, 10, colour);
    secGfx->drawCircle(cx, cy, 5, colour);
    secGfx->fillCircle(cx, cy, 2, colour);
  }
  else if (state == GF_GEAR_RETRACTING) {
    gfDrawGearArrow(x, y, true, colour);
  }
  else if (state == GF_GEAR_EXTENDING) {
    gfDrawGearArrow(x, y, false, colour);
  }
  else {
    secGfx->drawLine(x + 10, y + 10, x + 28, y + 28, colour);
    secGfx->drawLine(x + 28, y + 10, x + 10, y + 28, colour);
  }
}

static void gfDrawGearNode(
  int x,
  int y,
  uint16_t colour
) {
  secGfx->fillCircle(x, y, 7, GF_BLACK);
  secGfx->drawCircle(x, y, 7, colour);
  secGfx->fillCircle(x, y, 3, colour);
}

static void gfDrawGearLabelsStatic()
{
  gfPrintCentered(82, 52, 76, "NOSE GEAR", 1, GF_LABEL);
  gfPrintCentered(10, 109, 80, "LEFT MAIN", 1, GF_LABEL);
  gfPrintCentered(150, 109, 80, "RIGHT MAIN", 1, GF_LABEL);
}

static void gfUpdateNoseGear(uint8_t state)
{
  uint16_t colour = gfGearStateColour(state);

  secGfx->fillRect(82, 63, 76, 10, GF_BLACK);

  gfPrintCentered(
    82,
    64,
    76,
    gfGearText(state),
    1,
    colour
  );

  gfDrawGearIcon(101, 76, state, true);

  secGfx->drawLine(120, 114, 120, 123, colour);
  gfDrawGearNode(120, 124, colour);
}

static void gfUpdateLeftMainGear(uint8_t state)
{
  uint16_t colour = gfGearStateColour(state);

  secGfx->fillRect(10, 120, 80, 10, GF_BLACK);

  gfPrintCentered(
    10,
    121,
    80,
    gfGearText(state),
    1,
    colour
  );

  gfDrawGearIcon(30, 137, state, false);

  secGfx->drawLine(68, 156, 91, 148, colour);
  secGfx->drawLine(91, 148, 100, 148, colour);
  gfDrawGearNode(100, 148, colour);
}

static void gfUpdateRightMainGear(uint8_t state)
{
  uint16_t colour = gfGearStateColour(state);

  secGfx->fillRect(150, 120, 80, 10, GF_BLACK);

  gfPrintCentered(
    150,
    121,
    80,
    gfGearText(state),
    1,
    colour
  );

  gfDrawGearIcon(172, 137, state, false);

  secGfx->drawLine(172, 156, 149, 148, colour);
  secGfx->drawLine(149, 148, 140, 148, colour);
  gfDrawGearNode(140, 148, colour);
}

// ----------------------------------------------------
// Flap graphic
// ----------------------------------------------------

static void gfDrawFlapSegment(
  int x,
  int y,
  int w,
  uint8_t flapState,
  uint8_t thisState
) {
  bool active = flapState == thisState;

  uint16_t boxFill = active ? GF_GREEN : GF_PANEL;
  uint16_t boxLine = active ? GF_GREEN : GF_LINE_SOFT;
  uint16_t valueColour = active ? GF_BLACK : GF_TEXT;

  secGfx->fillRect(x, y, w, 30, boxFill);
  secGfx->drawRect(x, y, w, 30, boxLine);

  gfPrintCentered(
    x,
    y + 8,
    w,
    gfFlapText(thisState),
    2,
    valueColour
  );
}

static void gfDrawFlapPanelStatic()
{
  gfDrawBox(
    GF_FLAP_PANEL_X,
    GF_FLAP_PANEL_Y,
    GF_FLAP_PANEL_W,
    GF_FLAP_PANEL_H,
    GF_PANEL2
  );

  gfPrintCentered(
    GF_FLAP_PANEL_X,
    GF_FLAP_PANEL_Y + 6,
    GF_FLAP_PANEL_W,
    "FLAP POSITION",
    2,
    GF_LABEL
  );
}

static void gfUpdateFlapPosition(uint8_t flapState)
{
  int y = GF_FLAP_PANEL_Y + 47;

  int upX = 18;
  int halfX = 91;
  int fullX = 164;
  int segW = 58;

  secGfx->fillRect(
    GF_FLAP_PANEL_X + 3,
    GF_FLAP_PANEL_Y + 30,
    GF_FLAP_PANEL_W - 6,
    GF_FLAP_PANEL_H - 34,
    GF_PANEL2
  );

  for (int x = upX + segW + 10; x < halfX - 8; x += 9) {
    secGfx->fillRect(x, y + 13, 3, 3, GF_DIM);
  }

  for (int x = halfX + segW + 10; x < fullX - 8; x += 9) {
    secGfx->fillRect(x, y + 13, 3, 3, GF_DIM);
  }

  gfDrawFlapSegment(upX, y, segW, flapState, GF_FLAP_UP);
  gfDrawFlapSegment(halfX, y, segW, flapState, GF_FLAP_HALF);
  gfDrawFlapSegment(fullX, y, segW, flapState, GF_FLAP_FULL);

  int pointerX = -1;

  if (flapState == GF_FLAP_UP) {
    pointerX = upX + segW / 2;
  }
  else if (flapState == GF_FLAP_HALF) {
    pointerX = halfX + segW / 2;
  }
  else if (flapState == GF_FLAP_FULL) {
    pointerX = fullX + segW / 2;
  }

  if (pointerX >= 0) {
    secGfx->fillTriangle(
      pointerX - 7,
      y - 9,
      pointerX + 7,
      y - 9,
      pointerX,
      y - 2,
      GF_LABEL
    );
  }
  else {
    gfPrintCentered(
      GF_FLAP_PANEL_X,
      GF_FLAP_PANEL_Y + 40,
      GF_FLAP_PANEL_W,
      "FLAP UNKNOWN",
      2,
      GF_BAD
    );
  }
}

// ----------------------------------------------------
// Static layout
// ----------------------------------------------------

static void gfDrawStaticLayout()
{
  secGfx->fillScreen(GF_BLACK);

  secGfx->drawRect(0, 0, GF_W, GF_H, GF_LINE_SOFT);
  secGfx->drawRect(1, 1, GF_W - 2, GF_H - 2, RGB565(35, 55, 60));

  gfPrintCentered(
    0,
    5,
    GF_W,
    "GEAR & FLAPS",
    2,
    GF_LABEL
  );

  secGfx->drawLine(72, 25, 110, 25, GF_LINE_SOFT);
  secGfx->drawLine(130, 25, 168, 25, GF_LINE_SOFT);
  secGfx->fillTriangle(112, 25, 128, 25, 120, 31, GF_LABEL);

  gfDrawCellVoltagePanelStatic();
  gfDrawAirspeedPanelStatic();

  gfDrawAircraft();
  gfDrawGearLabelsStatic();

  gfDrawFlapPanelStatic();
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryGearFlapsPage()
{
  uint8_t gearState = gfGetGearDisplayState();
  uint8_t flapState = gfGetFlapState();

  if (secondaryNeedsFullRedraw) {
    gfDrawStaticLayout();
  }

  gfUpdateCellVoltagePanel();
  gfUpdateAirspeedPanel();

  gfUpdateNoseGear(gearState);
  gfUpdateLeftMainGear(gearState);
  gfUpdateRightMainGear(gearState);

  gfUpdateFlapPosition(flapState);
}