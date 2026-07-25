// ----------------------------------------------------
// Secondary page: navigation
// ----------------------------------------------------
//
// Nav-themed GPS page:
//   - large distance-from-home field
//   - GPS fix, satellites, HDOP
//   - heading
//   - heading to home
//   - GPS altitude
//   - latitude / longitude
//
// Home point:
//   - captured from first valid 3D GPS fix after boot
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// From secondary_display.ino
extern Arduino_GFX *secGfx;
extern bool secondaryNeedsFullRedraw;

// MAVLink GPS / nav data
extern bool mavlinkGpsValid;
extern bool mavlinkVfrHudValid;

extern uint8_t gpsFixType;
extern uint8_t gpsSatellitesVisible;
extern int32_t gpsLat;
extern int32_t gpsLon;
extern int32_t gpsAltMm;

extern float gpsHdop;
extern bool gpsHdopValid;

extern int16_t heading_deg;

// ----------------------------------------------------
// Colours
// ----------------------------------------------------

static const uint16_t SEC_NAV_BLACK      = RGB565(0, 0, 0);
static const uint16_t SEC_NAV_PANEL      = RGB565(4, 13, 16);
static const uint16_t SEC_NAV_PANEL2     = RGB565(4, 22, 28);

static const uint16_t SEC_NAV_LINE       = RGB565(210, 210, 210);
static const uint16_t SEC_NAV_LINE_SOFT  = RGB565(95, 95, 95);

static const uint16_t SEC_NAV_DIM        = RGB565(115, 150, 155);
static const uint16_t SEC_NAV_LABEL      = RGB565(0, 235, 255);
static const uint16_t SEC_NAV_TEXT       = RGB565(230, 245, 245);
static const uint16_t SEC_NAV_CYAN       = RGB565(0, 245, 255);
static const uint16_t SEC_NAV_GREEN      = RGB565(80, 255, 120);
static const uint16_t SEC_NAV_WARN       = RGB565(255, 175, 40);
static const uint16_t SEC_NAV_BAD        = RGB565(255, 65, 65);
static const uint16_t SEC_NAV_PURPLE     = RGB565(190, 70, 255);

// ----------------------------------------------------
// Layout
// ----------------------------------------------------

static const int SEC_NAV_W = 240;

static const int SEC_NAV_HOME_X = 0;
static const int SEC_NAV_HOME_Y = 0;
static const int SEC_NAV_HOME_W = 240;
static const int SEC_NAV_HOME_H = 80;

static const int SEC_NAV_LEFT_X  = 0;
static const int SEC_NAV_RIGHT_X = 122;
static const int SEC_NAV_COL_W   = 118;
static const int SEC_NAV_TILE_H  = 46;

static const int SEC_NAV_ROW1_Y = 85;
static const int SEC_NAV_ROW2_Y = 134;
static const int SEC_NAV_ROW3_Y = 183;
static const int SEC_NAV_ROW4_Y = 232;

// ----------------------------------------------------
// Home capture state
// ----------------------------------------------------

static bool navHomeCaptured = false;
static int32_t navHomeLat = 0;
static int32_t navHomeLon = 0;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static int secNavTextWidth(const char *text, int textSize)
{
  if (text == nullptr) {
    return 0;
  }

  return (int)strlen(text) * 6 * textSize;
}

static void secNavPrintCentered(
  int x,
  int y,
  int w,
  const char *text,
  int textSize,
  uint16_t colour
) {
  int textW = secNavTextWidth(text, textSize);
  int textX = x + ((w - textW) / 2);

  if (textX < x + 2) {
    textX = x + 2;
  }

  secGfx->setTextSize(textSize);
  secGfx->setTextColor(colour);
  secGfx->setCursor(textX, y);
  secGfx->print(text);
}

static float secNavDegreesFromGpsInt(int32_t gpsValue)
{
  return (float)gpsValue / 10000000.0f;
}

static float secNavRadians(float degrees)
{
  return degrees * DEG_TO_RAD;
}

static float secNavDistanceMetres(
  int32_t lat1Raw,
  int32_t lon1Raw,
  int32_t lat2Raw,
  int32_t lon2Raw
) {
  const float earthRadiusM = 6371000.0f;

  float lat1 = secNavRadians(secNavDegreesFromGpsInt(lat1Raw));
  float lon1 = secNavRadians(secNavDegreesFromGpsInt(lon1Raw));
  float lat2 = secNavRadians(secNavDegreesFromGpsInt(lat2Raw));
  float lon2 = secNavRadians(secNavDegreesFromGpsInt(lon2Raw));

  float dLat = lat2 - lat1;
  float dLon = lon2 - lon1;

  float a =
    sinf(dLat * 0.5f) * sinf(dLat * 0.5f) +
    cosf(lat1) * cosf(lat2) *
    sinf(dLon * 0.5f) * sinf(dLon * 0.5f);

  float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

  return earthRadiusM * c;
}

static int secNavBearingDeg(
  int32_t lat1Raw,
  int32_t lon1Raw,
  int32_t lat2Raw,
  int32_t lon2Raw
) {
  float lat1 = secNavRadians(secNavDegreesFromGpsInt(lat1Raw));
  float lat2 = secNavRadians(secNavDegreesFromGpsInt(lat2Raw));

  float lon1 = secNavDegreesFromGpsInt(lon1Raw);
  float lon2 = secNavDegreesFromGpsInt(lon2Raw);
  float dLon = secNavRadians(lon2 - lon1);

  float y = sinf(dLon) * cosf(lat2);

  float x =
    cosf(lat1) * sinf(lat2) -
    sinf(lat1) * cosf(lat2) * cosf(dLon);

  int bearing = (int)lroundf(atan2f(y, x) * RAD_TO_DEG);

  while (bearing < 0) {
    bearing += 360;
  }

  while (bearing >= 360) {
    bearing -= 360;
  }

  return bearing;
}

static bool secNavGpsPositionUsable()
{
  if (!mavlinkGpsValid) {
    return false;
  }

  if (gpsFixType < 3) {
    return false;
  }

  if (gpsLat == 0 && gpsLon == 0) {
    return false;
  }

  return true;
}

static void secNavUpdateHomeCapture()
{
  if (navHomeCaptured) {
    return;
  }

  if (!secNavGpsPositionUsable()) {
    return;
  }

  navHomeLat = gpsLat;
  navHomeLon = gpsLon;
  navHomeCaptured = true;
}

static const char* secNavGpsFixText(uint8_t fixType)
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

static uint16_t secNavFixColour()
{
  if (!mavlinkGpsValid) {
    return SEC_NAV_BAD;
  }

  if (gpsFixType >= 3) {
    return SEC_NAV_GREEN;
  }

  if (gpsFixType == 2) {
    return SEC_NAV_WARN;
  }

  return SEC_NAV_BAD;
}

static uint16_t secNavHdopColour()
{
  if (!gpsHdopValid) {
    return SEC_NAV_BAD;
  }

  if (gpsHdop <= 1.5f) {
    return SEC_NAV_GREEN;
  }

  if (gpsHdop <= 2.5f) {
    return SEC_NAV_WARN;
  }

  return SEC_NAV_BAD;
}

static uint16_t secNavValidColour(bool valid)
{
  return valid ? SEC_NAV_GREEN : SEC_NAV_BAD;
}

static void secNavDrawTileFrame(
  int x,
  int y,
  const char *label
) {
  secGfx->fillRect(
    x,
    y,
    SEC_NAV_COL_W,
    SEC_NAV_TILE_H,
    SEC_NAV_PANEL
  );

  secGfx->drawRect(
    x,
    y,
    SEC_NAV_COL_W,
    SEC_NAV_TILE_H,
    SEC_NAV_LINE
  );

  secGfx->drawRect(
    x + 1,
    y + 1,
    SEC_NAV_COL_W - 2,
    SEC_NAV_TILE_H - 2,
    SEC_NAV_LINE_SOFT
  );

  // Larger/brighter field heading.
  // Long labels auto-shrink so they fit within the tile.
  int labelSize = 2;

  if (secNavTextWidth(label, labelSize) > SEC_NAV_COL_W - 10) {
    labelSize = 1;
  }

  secGfx->setTextSize(labelSize);
  secGfx->setTextColor(SEC_NAV_LABEL);
  secGfx->setCursor(x + 5, y + 3);
  secGfx->print(label);
}

static void secNavUpdateTileValue(
  int x,
  int y,
  const char *value,
  uint16_t valueColour,
  int textSize = 2
) {
  secGfx->fillRect(
    x + 3,
    y + 21,
    SEC_NAV_COL_W - 6,
    22,
    SEC_NAV_PANEL
  );

  secNavPrintCentered(
    x,
    y + 25,
    SEC_NAV_COL_W,
    value,
    textSize,
    valueColour
  );
}

static void secNavDrawStaticLayout()
{
  secGfx->fillScreen(SEC_NAV_BLACK);

  // Home distance panel.
  secGfx->fillRect(
    SEC_NAV_HOME_X,
    SEC_NAV_HOME_Y,
    SEC_NAV_HOME_W,
    SEC_NAV_HOME_H,
    SEC_NAV_PANEL2
  );

  secGfx->drawRect(
    SEC_NAV_HOME_X,
    SEC_NAV_HOME_Y,
    SEC_NAV_HOME_W,
    SEC_NAV_HOME_H,
    SEC_NAV_LINE
  );

  secGfx->drawRect(
    SEC_NAV_HOME_X + 1,
    SEC_NAV_HOME_Y + 1,
    SEC_NAV_HOME_W - 2,
    SEC_NAV_HOME_H - 2,
    SEC_NAV_LINE_SOFT
  );

  secNavPrintCentered(
    SEC_NAV_HOME_X,
    SEC_NAV_HOME_Y + 5,
    SEC_NAV_HOME_W,
    "HOME DIST",
    2,
    SEC_NAV_LABEL
  );

  // Two-column tiles.
  secNavDrawTileFrame(SEC_NAV_LEFT_X,  SEC_NAV_ROW1_Y, "GPS/SAT");
  secNavDrawTileFrame(SEC_NAV_RIGHT_X, SEC_NAV_ROW1_Y, "HDOP");

  secNavDrawTileFrame(SEC_NAV_LEFT_X,  SEC_NAV_ROW2_Y, "HDG");
  secNavDrawTileFrame(SEC_NAV_RIGHT_X, SEC_NAV_ROW2_Y, "HOME HDG");

  secNavDrawTileFrame(SEC_NAV_LEFT_X,  SEC_NAV_ROW3_Y, "ALT");
  secNavDrawTileFrame(SEC_NAV_RIGHT_X, SEC_NAV_ROW3_Y, "HOME");

  secNavDrawTileFrame(SEC_NAV_LEFT_X,  SEC_NAV_ROW4_Y, "LAT");
  secNavDrawTileFrame(SEC_NAV_RIGHT_X, SEC_NAV_ROW4_Y, "LON");
}

static void secNavUpdateHomeDistance()
{
  char value[18];

  secGfx->fillRect(
    SEC_NAV_HOME_X + 4,
    SEC_NAV_HOME_Y + 24,
    SEC_NAV_HOME_W - 8,
    51,
    SEC_NAV_PANEL2
  );

  if (secNavGpsPositionUsable() && navHomeCaptured) {
    float distM =
      secNavDistanceMetres(
        navHomeLat,
        navHomeLon,
        gpsLat,
        gpsLon
      );

    if (distM < 1000.0f) {
      snprintf(value, sizeof(value), "%ldM", lroundf(distM));
    } else {
      snprintf(value, sizeof(value), "%.1fKM", distM / 1000.0f);
    }

    int textSize = 5;

    if (distM >= 1000.0f) {
      textSize = 4;
    }

    secNavPrintCentered(
      SEC_NAV_HOME_X,
      SEC_NAV_HOME_Y + 29,
      SEC_NAV_HOME_W,
      value,
      textSize,
      SEC_NAV_PURPLE
    );
  } else {
    snprintf(value, sizeof(value), "---");

    secNavPrintCentered(
      SEC_NAV_HOME_X,
      SEC_NAV_HOME_Y + 29,
      SEC_NAV_HOME_W,
      value,
      5,
      SEC_NAV_BAD
    );
  }
}

// ----------------------------------------------------
// Draw page
// ----------------------------------------------------

void drawSecondaryNavPage()
{
  char value[24];

  secNavUpdateHomeCapture();

  if (secondaryNeedsFullRedraw) {
    secNavDrawStaticLayout();
  }

  secNavUpdateHomeDistance();

  // GPS / SAT.
  if (mavlinkGpsValid) {
    snprintf(
      value,
      sizeof(value),
      "%s %u",
      secNavGpsFixText(gpsFixType),
      gpsSatellitesVisible
    );
  } else {
    snprintf(value, sizeof(value), "LOST");
  }

  secNavUpdateTileValue(
    SEC_NAV_LEFT_X,
    SEC_NAV_ROW1_Y,
    value,
    secNavFixColour()
  );

  // HDOP.
  if (gpsHdopValid) {
    snprintf(value, sizeof(value), "%.1f", gpsHdop);
  } else {
    snprintf(value, sizeof(value), "--");
  }

  secNavUpdateTileValue(
    SEC_NAV_RIGHT_X,
    SEC_NAV_ROW1_Y,
    value,
    secNavHdopColour()
  );

  // Heading value.
  if (mavlinkVfrHudValid) {
    int hdg = heading_deg;

    while (hdg < 0) {
      hdg += 360;
    }

    while (hdg >= 360) {
      hdg -= 360;
    }

    snprintf(value, sizeof(value), "%03d", hdg);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secNavUpdateTileValue(
    SEC_NAV_LEFT_X,
    SEC_NAV_ROW2_Y,
    value,
    mavlinkVfrHudValid ? SEC_NAV_CYAN : SEC_NAV_BAD,
    2
  );

  // Heading to home.
  if (secNavGpsPositionUsable() && navHomeCaptured) {
    int homeHdg =
      secNavBearingDeg(
        gpsLat,
        gpsLon,
        navHomeLat,
        navHomeLon
      );

    snprintf(value, sizeof(value), "%03d", homeHdg);
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secNavUpdateTileValue(
    SEC_NAV_RIGHT_X,
    SEC_NAV_ROW2_Y,
    value,
    secNavValidColour(secNavGpsPositionUsable() && navHomeCaptured)
  );

  // Altitude.
  if (mavlinkGpsValid) {
    float gpsAltDisplayed =
      ((float)gpsAltMm / 1000.0f) * CONFIG_GLASS_ALT_SCALE;

    snprintf(
      value,
      sizeof(value),
      "%ld%s",
      lroundf(gpsAltDisplayed),
      CONFIG_GLASS_ALT_LABEL
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secNavUpdateTileValue(
    SEC_NAV_LEFT_X,
    SEC_NAV_ROW3_Y,
    value,
    secNavValidColour(mavlinkGpsValid)
  );

  // Home status.
  if (navHomeCaptured) {
    snprintf(value, sizeof(value), "SET");
  } else {
    snprintf(value, sizeof(value), "WAIT");
  }

  secNavUpdateTileValue(
    SEC_NAV_RIGHT_X,
    SEC_NAV_ROW3_Y,
    value,
    navHomeCaptured ? SEC_NAV_GREEN : SEC_NAV_WARN
  );

  // Latitude.
  if (mavlinkGpsValid) {
    snprintf(
      value,
      sizeof(value),
      "%.2f",
      secNavDegreesFromGpsInt(gpsLat)
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secNavUpdateTileValue(
    SEC_NAV_LEFT_X,
    SEC_NAV_ROW4_Y,
    value,
    secNavValidColour(mavlinkGpsValid)
  );

  // Longitude.
  if (mavlinkGpsValid) {
    snprintf(
      value,
      sizeof(value),
      "%.2f",
      secNavDegreesFromGpsInt(gpsLon)
    );
  } else {
    snprintf(value, sizeof(value), "---");
  }

  secNavUpdateTileValue(
    SEC_NAV_RIGHT_X,
    SEC_NAV_ROW4_Y,
    value,
    secNavValidColour(mavlinkGpsValid)
  );
}