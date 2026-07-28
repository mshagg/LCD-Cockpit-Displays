// ----------------------------------------------------
// Screen design: gear / flaps
// ----------------------------------------------------
//
// Standalone ESP32-S3 + 2.0 inch ST7789VW fork.
//
// Display orientation expected:
//   320 x 240 landscape
//
// Original display style, scaled/adapted:
//   - title removed
//   - top telemetry, gear stations and aircraft moved upward
//   - flap panel remains at bottom
//   - gear icons change with gear state:
//       UP   = detailed wheel-only icon
//       DOWN = detailed strut + fork + wheel icon
//   - transition arrows show direction of gear travel
//
// Functionality:
//   - Gear from RC channel 5
//   - Flaps from RC channel 6
//   - Page select handled separately by CH16
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include "frame_template.h"

// ----------------------------------------------------
// External MAVLink telemetry
// ----------------------------------------------------

extern bool mavlinkRcChannelsValid;
extern uint16_t getMavlinkRcChannelRaw(uint8_t channelNumber);

extern float batteryCellVoltage;
extern bool batteryVoltageValid;

extern float airspeed;
extern bool mavlinkVfrHudValid;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t GF_BLACK       = RGB565(0, 0, 0);
static const uint16_t GF_WHITE       = RGB565(240, 245, 245);
static const uint16_t GF_SOFT_WHITE  = RGB565(170, 185, 190);

static const uint16_t GF_CYAN        = RGB565(0, 210, 255);
static const uint16_t GF_GREEN       = RGB565(85, 255, 0);
static const uint16_t GF_DARK_GREEN  = RGB565(8, 45, 8);
static const uint16_t GF_YELLOW      = RGB565(255, 210, 0);
static const uint16_t GF_RED         = RGB565(255, 55, 40);

static const uint16_t GF_PANEL       = RGB565(8, 18, 28);
static const uint16_t GF_PANEL_GREY  = RGB565(64, 83, 95);
static const uint16_t GF_BOX_GREY    = RGB565(48, 61, 72);
static const uint16_t GF_DARK_GREY   = RGB565(30, 38, 45);
static const uint16_t GF_MID_GREY    = RGB565(92, 104, 112);

// ----------------------------------------------------
// RC configuration
// ----------------------------------------------------

static const uint8_t GF_GEAR_RC_CHANNEL = 5;
static const uint8_t GF_FLAP_RC_CHANNEL = 6;

static const uint16_t GF_RC_VALID_LOW_US = 800;
static const uint16_t GF_RC_VALID_HIGH_US = 2200;

static const uint16_t GF_GEAR_RETRACTED_US = 1000;
static const uint16_t GF_GEAR_EXTENDED_US  = 2000;

static const uint16_t GF_FLAPS_UP_US   = 1111;
static const uint16_t GF_FLAPS_HALF_US = 1352;
static const uint16_t GF_FLAPS_FULL_US = 1536;

static const unsigned long GF_GEAR_TRANSITION_MS = 2000;

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int GF_AIRCRAFT_CX = 160;
static const int GF_AIRCRAFT_CY = 112;

// ----------------------------------------------------
// State constants
// ----------------------------------------------------

static const uint8_t GF_GEAR_UNKNOWN    = 0;
static const uint8_t GF_GEAR_RETRACTED  = 1;
static const uint8_t GF_GEAR_EXTENDED   = 2;

static const uint8_t GF_FLAPS_UNKNOWN   = 0;
static const uint8_t GF_FLAPS_UP        = 1;
static const uint8_t GF_FLAPS_HALF      = 2;
static const uint8_t GF_FLAPS_FULL      = 3;

static const uint8_t GF_TRANSITION_NONE    = 0;
static const uint8_t GF_TRANSITION_RETRACT = 1;
static const uint8_t GF_TRANSITION_EXTEND  = 2;

static uint8_t gfLastGearState = GF_GEAR_UNKNOWN;
static uint8_t gfTransitionDirection = GF_TRANSITION_NONE;
static unsigned long gfTransitionStartMs = 0;

static uint16_t *gfFrameTemplate = nullptr;
static bool gfFrameTemplateReady = false;

// ----------------------------------------------------
// General helpers
// ----------------------------------------------------

static bool gfRcValueValid(uint16_t rawUs)
{
  return rawUs >= GF_RC_VALID_LOW_US &&
         rawUs <= GF_RC_VALID_HIGH_US;
}

static uint8_t gfGetGearState(uint16_t rawUs)
{
  if (!gfRcValueValid(rawUs)) {
    return GF_GEAR_UNKNOWN;
  }

  uint16_t midPoint =
    (GF_GEAR_RETRACTED_US + GF_GEAR_EXTENDED_US) / 2;

  if (rawUs >= midPoint) {
    return GF_GEAR_EXTENDED;
  }

  return GF_GEAR_RETRACTED;
}

static uint8_t gfGetFlapState(uint16_t rawUs)
{
  if (!gfRcValueValid(rawUs)) {
    return GF_FLAPS_UNKNOWN;
  }

  uint16_t upHalfBoundary =
    (GF_FLAPS_UP_US + GF_FLAPS_HALF_US) / 2;

  uint16_t halfFullBoundary =
    (GF_FLAPS_HALF_US + GF_FLAPS_FULL_US) / 2;

  if (rawUs <= upHalfBoundary) {
    return GF_FLAPS_UP;
  }

  if (rawUs <= halfFullBoundary) {
    return GF_FLAPS_HALF;
  }

  return GF_FLAPS_FULL;
}

static void gfUpdateGearTransition(uint8_t gearState)
{
  if (gearState == GF_GEAR_UNKNOWN) {
    return;
  }

  if (gfLastGearState == GF_GEAR_UNKNOWN) {
    gfLastGearState = gearState;
    return;
  }

  if (gearState == gfLastGearState) {
    return;
  }

  if (gearState == GF_GEAR_EXTENDED) {
    gfTransitionDirection = GF_TRANSITION_EXTEND;
  } else {
    gfTransitionDirection = GF_TRANSITION_RETRACT;
  }

  gfTransitionStartMs = millis();
  gfLastGearState = gearState;
}

static bool gfTransitionActive()
{
  if (gfTransitionDirection == GF_TRANSITION_NONE) {
    return false;
  }

  if (millis() - gfTransitionStartMs <= GF_GEAR_TRANSITION_MS) {
    return true;
  }

  gfTransitionDirection = GF_TRANSITION_NONE;
  return false;
}

static uint16_t gfCellColour()
{
  if (!batteryVoltageValid) {
    return GF_RED;
  }

  if (batteryCellVoltage >= 3.60f) {
    return GF_GREEN;
  }

  if (batteryCellVoltage >= 3.40f) {
    return GF_YELLOW;
  }

  return GF_RED;
}

static uint16_t gfGearColour(uint8_t gearState)
{
  if (gearState == GF_GEAR_UNKNOWN) {
    return GF_RED;
  }

  return GF_GREEN;
}

static void gfPrintCentered(
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
}

// ----------------------------------------------------
// Top telemetry boxes
// ----------------------------------------------------

static void gfDrawTelemetryBoxFrame(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h
) {
  display->fillRect(x, y, w, h, GF_PANEL);
  display->drawRect(x, y, w, h, GF_SOFT_WHITE);
  display->drawRect(x + 1, y + 1, w - 2, h - 2, GF_MID_GREY);
}

static void gfDrawCellBox(Arduino_GFX *display)
{
  char cellText[12];

  if (batteryVoltageValid) {
    snprintf(
      cellText,
      sizeof(cellText),
      "%.2f",
      batteryCellVoltage
    );
  } else {
    snprintf(cellText, sizeof(cellText), "--");
  }

  gfDrawTelemetryBoxFrame(display, 5, 6, 82, 55);

  gfPrintCentered(display, 5, 11, 82, "CELL V", 1, GF_CYAN, GF_PANEL);

  gfPrintCentered(
    display,
    5,
    27,
    82,
    cellText,
    3,
    gfCellColour(),
    GF_PANEL
  );

  gfPrintCentered(display, 5, 52, 82, "V/CELL", 1, GF_GREEN, GF_PANEL);
}

static void gfDrawAirspeedBox(Arduino_GFX *display)
{
  char speedText[12];

  if (mavlinkVfrHudValid) {
    snprintf(
      speedText,
      sizeof(speedText),
      "%.0f",
      airspeed * 3.6f
    );
  } else {
    snprintf(speedText, sizeof(speedText), "--");
  }

  int x = display->width() - 87;

  gfDrawTelemetryBoxFrame(display, x, 6, 82, 55);

  gfPrintCentered(display, x, 11, 82, "AIRSPEED", 1, GF_CYAN, GF_PANEL);

  gfPrintCentered(
    display,
    x,
    26,
    82,
    speedText,
    3,
    mavlinkVfrHudValid ? GF_GREEN : GF_RED,
    GF_PANEL
  );

  gfPrintCentered(display, x, 52, 82, "KMH", 1, GF_GREEN, GF_PANEL);
}

static void gfDrawTelemetryValues(Arduino_GFX *display)
{
  char cellText[12];
  char speedText[12];

  if (batteryVoltageValid) {
    snprintf(cellText, sizeof(cellText), "%.2f", batteryCellVoltage);
  } else {
    snprintf(cellText, sizeof(cellText), "--");
  }

  if (mavlinkVfrHudValid) {
    snprintf(speedText, sizeof(speedText), "%.0f", airspeed * 3.6f);
  } else {
    snprintf(speedText, sizeof(speedText), "--");
  }

  gfPrintCentered(
    display, 5, 27, 82, cellText, 3,
    gfCellColour(), GF_PANEL
  );

  int speedX = display->width() - 87;
  gfPrintCentered(
    display, speedX, 26, 82, speedText, 3,
    mavlinkVfrHudValid ? GF_GREEN : GF_RED, GF_PANEL
  );
}

// ----------------------------------------------------
// Gear icon helpers
// ----------------------------------------------------

static const char *gfGearStateText(uint8_t gearState)
{
  if (gearState == GF_GEAR_RETRACTED) {
    return "UP";
  }

  if (gearState == GF_GEAR_EXTENDED) {
    return "DOWN";
  }

  return "--";
}

static void gfDrawDetailedWheel(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t colour
) {
  display->fillCircle(cx, cy, 13, GF_BLACK);
  display->drawCircle(cx, cy, 13, colour);
  display->drawCircle(cx, cy, 12, colour);

  display->drawCircle(cx, cy, 8, GF_SOFT_WHITE);
  display->drawCircle(cx, cy, 7, colour);

  display->fillCircle(cx, cy, 3, colour);
  display->drawCircle(cx, cy, 4, GF_WHITE);

  display->drawLine(cx - 9, cy - 8, cx - 4, cy - 11, colour);
  display->drawLine(cx + 4, cy - 11, cx + 9, cy - 8, colour);
  display->drawLine(cx - 9, cy + 8, cx - 4, cy + 11, colour);
  display->drawLine(cx + 4, cy + 11, cx + 9, cy + 8, colour);
}

static void gfDrawGearRetractedIcon(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t colour
) {
  display->drawRoundRect(cx - 18, cy - 13, 36, 26, 5, GF_MID_GREY);
  display->drawLine(cx - 14, cy - 13, cx + 14, cy - 13, colour);

  gfDrawDetailedWheel(display, cx, cy, colour);
}

static void gfDrawGearExtendedIcon(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t colour
) {
  display->fillRect(cx - 8, cy - 19, 17, 4, colour);

  display->drawLine(cx - 2, cy - 18, cx - 2, cy + 1, GF_WHITE);
  display->drawLine(cx + 2, cy - 18, cx + 2, cy + 1, GF_WHITE);
  display->drawLine(cx, cy - 18, cx, cy + 1, colour);

  display->drawLine(cx - 2, cy - 8, cx - 10, cy + 1, colour);
  display->drawLine(cx + 2, cy - 8, cx + 10, cy + 1, colour);
  display->drawLine(cx - 10, cy + 1, cx + 2, cy + 7, colour);
  display->drawLine(cx + 10, cy + 1, cx - 2, cy + 7, colour);

  display->drawLine(cx - 8, cy + 4, cx - 12, cy + 14, GF_WHITE);
  display->drawLine(cx + 8, cy + 4, cx + 12, cy + 14, GF_WHITE);
  display->drawLine(cx - 12, cy + 14, cx + 12, cy + 14, colour);

  gfDrawDetailedWheel(display, cx, cy + 15, colour);
}

static void gfDrawGearTransitionArrow(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint8_t transitionDirection
) {
  if (transitionDirection == GF_TRANSITION_EXTEND) {
    display->fillRect(cx - 3, cy - 16, 7, 21, GF_CYAN);
    display->fillTriangle(
      cx,
      cy + 17,
      cx - 10,
      cy + 4,
      cx + 10,
      cy + 4,
      GF_CYAN
    );
  } else if (transitionDirection == GF_TRANSITION_RETRACT) {
    display->fillRect(cx - 3, cy - 5, 7, 21, GF_YELLOW);
    display->fillTriangle(
      cx,
      cy - 17,
      cx - 10,
      cy - 4,
      cx + 10,
      cy - 4,
      GF_YELLOW
    );
  }
}

static void gfDrawGearBox(
  Arduino_GFX *display,
  int x,
  int y,
  int size,
  uint8_t gearState,
  bool transitionNow
) {
  uint16_t colour = gfGearColour(gearState);

  display->fillRect(x, y, size, size, GF_DARK_GREEN);
  display->drawRect(x, y, size, size, colour);
  display->drawRect(x + 1, y + 1, size - 2, size - 2, colour);

  int cx = x + (size / 2);
  int cy = y + (size / 2);

  if (gearState == GF_GEAR_UNKNOWN) {
    display->drawLine(x + 8, y + 8, x + size - 8, y + size - 8, GF_RED);
    display->drawLine(x + size - 8, y + 8, x + 8, y + size - 8, GF_RED);
    return;
  }

  if (transitionNow) {
    gfDrawGearTransitionArrow(
      display,
      cx,
      cy,
      gfTransitionDirection
    );
    return;
  }

  if (gearState == GF_GEAR_EXTENDED) {
    gfDrawGearExtendedIcon(display, cx, cy - 6, colour);
  } else {
    gfDrawGearRetractedIcon(display, cx, cy, colour);
  }
}

static void gfDrawGearLabel(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  const char *label,
  uint8_t gearState
) {
  uint16_t colour = gfGearColour(gearState);

  gfPrintCentered(display, x, y, w, label, 1, GF_CYAN, GF_BLACK);

  gfPrintCentered(
    display,
    x,
    y + 12,
    w,
    gfGearStateText(gearState),
    1,
    colour,
    GF_BLACK
  );
}

// ----------------------------------------------------
// Aircraft drawing
// ----------------------------------------------------

static void gfDrawAircraftFrame(Arduino_GFX *display)
{
  int cx = GF_AIRCRAFT_CX;
  int cy = GF_AIRCRAFT_CY;

  display->drawLine(cx, cy - 50, cx - 12, cy + 38, GF_SOFT_WHITE);
  display->drawLine(cx, cy - 50, cx + 12, cy + 38, GF_SOFT_WHITE);
  display->drawLine(cx - 12, cy + 38, cx, cy + 54, GF_SOFT_WHITE);
  display->drawLine(cx + 12, cy + 38, cx, cy + 54, GF_SOFT_WHITE);

  display->drawLine(cx, cy - 50, cx, cy + 54, GF_MID_GREY);

  display->drawLine(cx - 74, cy + 9, cx - 12, cy + 7, GF_SOFT_WHITE);
  display->drawLine(cx + 74, cy + 9, cx + 12, cy + 7, GF_SOFT_WHITE);
  display->drawLine(cx - 74, cy + 9, cx - 12, cy + 28, GF_SOFT_WHITE);
  display->drawLine(cx + 74, cy + 9, cx + 12, cy + 28, GF_SOFT_WHITE);

  display->drawLine(cx - 42, cy + 53, cx - 7, cy + 39, GF_SOFT_WHITE);
  display->drawLine(cx + 42, cy + 53, cx + 7, cy + 39, GF_SOFT_WHITE);
  display->drawLine(cx - 42, cy + 53, cx, cy + 54, GF_SOFT_WHITE);
  display->drawLine(cx + 42, cy + 53, cx, cy + 54, GF_SOFT_WHITE);
}

static void gfDrawAircraftGearIndicators(
  Arduino_GFX *display,
  uint8_t gearState
) {
  int cx = GF_AIRCRAFT_CX;
  int cy = GF_AIRCRAFT_CY;
  uint16_t gearColour = gfGearColour(gearState);

  display->drawCircle(cx, cy - 12, 8, gearColour);
  display->drawCircle(cx - 27, cy + 23, 8, gearColour);
  display->drawCircle(cx + 27, cy + 23, 8, gearColour);

  display->fillCircle(cx, cy - 12, 3, gearColour);
  display->fillCircle(cx - 27, cy + 23, 3, gearColour);
  display->fillCircle(cx + 27, cy + 23, 3, gearColour);
}

// ----------------------------------------------------
// Gear layout
// ----------------------------------------------------

static void gfDrawGearStations(
  Arduino_GFX *display,
  uint8_t gearState
) {
  uint16_t gearColour = gfGearColour(gearState);
  bool transitionNow = gfTransitionActive();

  gfDrawGearLabel(display, 125, 30, 70, "NOSE GEAR", gearState);

  gfDrawGearBox(
    display,
    135,
    56,
    50,
    gearState,
    transitionNow
  );

  display->drawLine(160, 106, 160, 96, gearColour);

  gfDrawGearLabel(display, 30, 89, 92, "LEFT MAIN", gearState);

  gfDrawGearBox(
    display,
    48,
    117,
    50,
    gearState,
    transitionNow
  );

  display->drawLine(98, 142, 133, 135, gearColour);

  gfDrawGearLabel(display, 198, 89, 92, "RIGHT MAIN", gearState);

  gfDrawGearBox(
    display,
    222,
    117,
    50,
    gearState,
    transitionNow
  );

  display->drawLine(222, 142, 187, 135, gearColour);
}

// ----------------------------------------------------
// Flap panel
// ----------------------------------------------------

static void gfDrawFlapButton(
  Arduino_GFX *display,
  int x,
  int y,
  int w,
  int h,
  const char *label,
  bool active,
  uint16_t activeColour
) {
  uint16_t fillColour = active ? activeColour : GF_BOX_GREY;
  uint16_t edgeColour = active ? activeColour : GF_SOFT_WHITE;
  uint16_t textColour = active ? GF_BLACK : GF_WHITE;

  display->fillRect(x, y, w, h, fillColour);
  display->drawRect(x, y, w, h, edgeColour);
  display->drawRect(x + 1, y + 1, w - 2, h - 2, edgeColour);

  gfPrintCentered(
    display,
    x,
    y + 8,
    w,
    label,
    2,
    textColour,
    fillColour
  );
}

static void gfDrawFlapPanel(
  Arduino_GFX *display,
  uint8_t flapState
) {
  int panelX = 6;
  int panelY = 190;
  int panelW = display->width() - 12;
  int panelH = 48;

  int buttonY = panelY + 22;

  gfDrawFlapButton(
    display,
    34,
    buttonY,
    64,
    25,
    "UP",
    flapState == GF_FLAPS_UP,
    GF_GREEN
  );

  gfDrawFlapButton(
    display,
    124,
    buttonY,
    72,
    25,
    "HALF",
    flapState == GF_FLAPS_HALF,
    GF_YELLOW
  );

  gfDrawFlapButton(
    display,
    222,
    buttonY,
    72,
    25,
    "FULL",
    flapState == GF_FLAPS_FULL,
    GF_CYAN
  );

  if (flapState == GF_FLAPS_UNKNOWN) {
    gfPrintCentered(
      display,
      panelX,
      panelY + 21,
      panelW,
      "UNKNOWN",
      1,
      GF_RED,
      GF_PANEL_GREY
    );
  }
}

static void gfDrawStaticTemplate(Arduino_GFX *display)
{
  display->fillScreen(GF_BLACK);

  gfDrawTelemetryBoxFrame(display, 5, 6, 82, 55);
  gfPrintCentered(display, 5, 11, 82, "CELL V", 1, GF_CYAN, GF_PANEL);
  gfPrintCentered(display, 5, 52, 82, "V/CELL", 1, GF_GREEN, GF_PANEL);

  int speedX = display->width() - 87;
  gfDrawTelemetryBoxFrame(display, speedX, 6, 82, 55);
  gfPrintCentered(display, speedX, 11, 82, "AIRSPEED", 1, GF_CYAN, GF_PANEL);
  gfPrintCentered(display, speedX, 52, 82, "KMH", 1, GF_GREEN, GF_PANEL);

  gfPrintCentered(display, 125, 30, 70, "NOSE GEAR", 1, GF_CYAN, GF_BLACK);
  gfPrintCentered(display, 30, 89, 92, "LEFT MAIN", 1, GF_CYAN, GF_BLACK);
  gfPrintCentered(display, 198, 89, 92, "RIGHT MAIN", 1, GF_CYAN, GF_BLACK);

  int panelX = 6;
  int panelY = 190;
  int panelW = display->width() - 12;
  int panelH = 48;

  display->fillRect(panelX, panelY, panelW, panelH, GF_PANEL_GREY);
  display->drawRect(panelX, panelY, panelW, panelH, GF_SOFT_WHITE);
  display->drawRect(panelX + 1, panelY + 1, panelW - 2, panelH - 2, GF_MID_GREY);

  gfPrintCentered(
    display,
    panelX,
    panelY + 4,
    panelW,
    "FLAP POSITION",
    2,
    GF_CYAN,
    GF_PANEL_GREY
  );
}

// ----------------------------------------------------
// Public screen entry point
// ----------------------------------------------------

void drawGearFlapsScreen(Arduino_GFX *display)
{
  if (!gfFrameTemplateReady) {
    gfDrawStaticTemplate(display);
    gfFrameTemplateReady =
      captureFrameTemplate(display, &gfFrameTemplate);
  } else {
    restoreFrameTemplate(display, gfFrameTemplate);
  }

  // If PSRAM allocation failed, retain a complete safe redraw.
  if (!gfFrameTemplateReady) {
    gfDrawStaticTemplate(display);
  }

  gfDrawTelemetryValues(display);

  uint16_t gearRaw = 0;
  uint16_t flapRaw = 0;

  if (mavlinkRcChannelsValid) {
    gearRaw = getMavlinkRcChannelRaw(GF_GEAR_RC_CHANNEL);
    flapRaw = getMavlinkRcChannelRaw(GF_FLAP_RC_CHANNEL);
  }

  uint8_t gearState = GF_GEAR_UNKNOWN;
  uint8_t flapState = GF_FLAPS_UNKNOWN;

  if (mavlinkRcChannelsValid) {
    gearState = gfGetGearState(gearRaw);
    flapState = gfGetFlapState(flapRaw);
  }

  gfUpdateGearTransition(gearState);

  gfDrawGearStations(display, gearState);
  // Preserve the original layering: the aircraft is drawn over the
  // gear stations after their dynamic boxes and connector lines.
  gfDrawAircraftFrame(display);
  gfDrawAircraftGearIndicators(display, gearState);
  gfDrawFlapPanel(display, flapState);
}
