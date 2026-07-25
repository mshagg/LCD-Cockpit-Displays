// ----------------------------------------------------
// Secondary page: diagnostics
// ----------------------------------------------------
//
// This page avoids full-screen redraw on every refresh.
// Static layout is drawn only when secondaryNeedsFullRedraw is true.
// Dynamic fields are updated in-place.
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <stdio.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// Display diagnostics from main tab
extern float displayFps;
extern float displayFrameMs;
extern float displayDrawMs;
extern float displayFlushMs;
extern float displayMavlinkMs;

// MAVLink diagnostics
extern uint32_t mavlinkConfiguredBaud;

extern uint32_t mavlinkBytesPerSecond;
extern uint32_t mavlinkMessagesPerSecond;
extern uint32_t mavlinkHeartbeatPerSecond;
extern uint32_t mavlinkAttitudePerSecond;
extern uint32_t mavlinkVfrHudPerSecond;
extern uint32_t mavlinkSysStatusPerSecond;
extern uint32_t mavlinkGpsRawPerSecond;
extern uint32_t mavlinkRcChannelsPerSecond;

extern uint32_t mavlinkParseCallsPerSecond;
extern uint32_t mavlinkParseMicrosAvg;
extern uint32_t mavlinkParseMicrosMax;
extern uint16_t mavlinkSerialAvailableMax;
extern uint16_t mavlinkParserDrops;

extern bool mavlinkHeartbeatValid;
extern bool mavlinkAttitudeValid;
extern bool mavlinkVfrHudValid;
extern bool mavlinkBatteryValid;
extern bool mavlinkGpsValid;
extern bool mavlinkRcChannelsValid;

extern uint16_t pageSelectMainLastRaw;
extern uint16_t pageSelectSecondaryLastRaw;
extern uint32_t pageSelectMainTriggerCount;
extern uint32_t pageSelectSecondaryTriggerCount;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t SEC_DIAG_BLACK  = RGB565(0, 0, 0);
static const uint16_t SEC_DIAG_PANEL  = RGB565(5, 12, 18);
static const uint16_t SEC_DIAG_LINE   = RGB565(40, 95, 120);
static const uint16_t SEC_DIAG_TITLE  = RGB565(0, 220, 255);
static const uint16_t SEC_DIAG_TEXT   = RGB565(230, 240, 245);
static const uint16_t SEC_DIAG_DIM    = RGB565(135, 150, 155);
static const uint16_t SEC_DIAG_GOOD   = RGB565(80, 255, 120);
static const uint16_t SEC_DIAG_BAD    = RGB565(255, 65, 65);
static const uint16_t SEC_DIAG_PURPLE = RGB565(190, 70, 255);

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static const char* secDiagOkLost(bool valid)
{
  return valid ? "OK" : "LOST";
}

static uint16_t secDiagOkLostColor(bool valid)
{
  return valid ? SEC_DIAG_GOOD : SEC_DIAG_BAD;
}

static void secDiagDrawStaticLayout()
{
  secGfx->fillScreen(SEC_DIAG_BLACK);

  // Header.
  secGfx->fillRect(0, 0, 240, 24, SEC_DIAG_BLACK);
  secGfx->drawFastHLine(0, 24, 240, SEC_DIAG_LINE);

  secGfx->setTextSize(2);
  secGfx->setTextColor(SEC_DIAG_TITLE);
  secGfx->setCursor(6, 5);
  secGfx->print("DIAG");

  secGfx->setTextSize(1);
  secGfx->setTextColor(SEC_DIAG_DIM);
  secGfx->setCursor(158, 9);
  secGfx->print("MAVLINK");

  const char *labels[] = {
    "DISPLAY",
    "FRAME",
    "DRAW/FLUSH",
    "MAV TIME",
    "BAUD",
    "BYTES/s",
    "MSG/s",
    "HB/s",
    "ATT/s",
    "VFR/s",
    "SYS/s",
    "GPS/s",
    "RC/s",
    "PARSE us",
    "UART MAX",
    "DROPS",
    "VALID",
    "VALID",
    "RC15/16",
    "PAGE CNT"
  };

  int y = 30;

  for (int i = 0; i < 20; i++) {
    secGfx->fillRect(0, y, 240, 13, SEC_DIAG_PANEL);

    secGfx->setTextSize(1);
    secGfx->setTextColor(SEC_DIAG_DIM);
    secGfx->setCursor(6, y + 3);
    secGfx->print(labels[i]);

    y += 13;

    if (i == 3 || i == 12 || i == 15 || i == 17) {
      y += 4;
    }
  }

  // Footer.
  secGfx->fillRect(0, 264, 240, 16, SEC_DIAG_BLACK);
  secGfx->drawFastHLine(0, 263, 240, SEC_DIAG_LINE);

  secGfx->setTextSize(1);
  secGfx->setTextColor(SEC_DIAG_PURPLE);
  secGfx->setCursor(6, 268);
  secGfx->print("CH16 PAGE SELECT");
}

static void secDiagUpdateRowValue(
  int y,
  const char *value,
  uint16_t valueColor
) {
  secGfx->fillRect(86, y + 1, 150, 11, SEC_DIAG_PANEL);

  secGfx->setTextSize(1);
  secGfx->setTextColor(valueColor);
  secGfx->setCursor(88, y + 3);
  secGfx->print(value);
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryDiagnosticsPage()
{
  char value[40];

  if (secondaryNeedsFullRedraw) {
    secDiagDrawStaticLayout();
  }

  int y = 30;

  snprintf(value, sizeof(value), "%.1f FPS", displayFps);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%.1f ms", displayFrameMs);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%.1f / %.1f", displayDrawMs, displayFlushMs);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%.2f ms", displayMavlinkMs);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 17;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkConfiguredBaud);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkBytesPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkMessagesPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkHeartbeatPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkAttitudePerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkVfrHudPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkSysStatusPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkGpsRawPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%lu", (unsigned long)mavlinkRcChannelsPerSecond);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 17;

  snprintf(value, sizeof(value), "%lu avg %lu max",
           (unsigned long)mavlinkParseMicrosAvg,
           (unsigned long)mavlinkParseMicrosMax);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%u", mavlinkSerialAvailableMax);
  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(value, sizeof(value), "%u", mavlinkParserDrops);
  secDiagUpdateRowValue(
    y,
    value,
    mavlinkParserDrops == 0 ? SEC_DIAG_GOOD : SEC_DIAG_BAD
  );
  y += 17;

  snprintf(
    value,
    sizeof(value),
    "HB %s ATT %s VFR %s",
    secDiagOkLost(mavlinkHeartbeatValid),
    secDiagOkLost(mavlinkAttitudeValid),
    secDiagOkLost(mavlinkVfrHudValid)
  );

  secDiagUpdateRowValue(y, value, SEC_DIAG_TEXT);
  y += 13;

  snprintf(
    value,
    sizeof(value),
    "BAT %s GPS %s RC %s",
    secDiagOkLost(mavlinkBatteryValid),
    secDiagOkLost(mavlinkGpsValid),
    secDiagOkLost(mavlinkRcChannelsValid)
  );

  secDiagUpdateRowValue(
    y,
    value,
    (mavlinkBatteryValid && mavlinkGpsValid && mavlinkRcChannelsValid) ?
      SEC_DIAG_GOOD :
      SEC_DIAG_BAD
  );
  y += 17;

  snprintf(
    value,
    sizeof(value),
    "%u / %u",
    pageSelectMainLastRaw,
    pageSelectSecondaryLastRaw
  );

  secDiagUpdateRowValue(
    y,
    value,
    secDiagOkLostColor(mavlinkRcChannelsValid)
  );
  y += 13;

  snprintf(
    value,
    sizeof(value),
    "%lu / %lu",
    (unsigned long)pageSelectMainTriggerCount,
    (unsigned long)pageSelectSecondaryTriggerCount
  );

  secDiagUpdateRowValue(y, value, SEC_DIAG_PURPLE);
}