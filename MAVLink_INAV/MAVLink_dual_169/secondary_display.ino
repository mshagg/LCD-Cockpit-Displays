// ----------------------------------------------------
// Secondary display manager
// ----------------------------------------------------
//
// Hardware:
//   Secondary 1.69" ST7789 LCD
//
// Purpose:
//   - initialise secondary LCD
//   - manage secondary pages
//   - route drawing to separate page tabs
//   - skip disabled pages based on config.ino flags
//   - boot to configured default page
//
// Screen order:
//   0 - Preflight
//   1 - Flight status
//   2 - Navigation
//   3 - Battery numeric
//   4 - Battery graphical
//   5 - Gear / flaps
//   6 - Diagnostics
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ----------------------------------------------------
// Secondary page constants
// ----------------------------------------------------

#define SECONDARY_PAGE_PREFLIGHT           0
#define SECONDARY_PAGE_FLIGHT_STATUS       1
#define SECONDARY_PAGE_NAVIGATION          2
#define SECONDARY_PAGE_BATTERY             3
#define SECONDARY_PAGE_BATTERY_GRAPHICAL   4
#define SECONDARY_PAGE_GEAR_FLAPS          5
#define SECONDARY_PAGE_DIAGNOSTICS         6
#define SECONDARY_PAGE_COUNT               7

// ----------------------------------------------------
// Secondary LCD pins
// ----------------------------------------------------

#define SEC_LCD_DC       2
#define SEC_LCD_CS       3
#define SEC_LCD_RST     10
#define SEC_LCD_BL      11
#define SEC_LCD_MOSI    17
#define SEC_LCD_SCK     18

#define SEC_LCD_WIDTH   240
#define SEC_LCD_HEIGHT  280

// Change to 2 if upside down.
#define SEC_LCD_ROTATION 0

// ----------------------------------------------------
// Display objects
// ----------------------------------------------------

Arduino_DataBus *secBus = new Arduino_ESP32SPI(
  SEC_LCD_DC,
  SEC_LCD_CS,
  SEC_LCD_SCK,
  SEC_LCD_MOSI,
  -1
);

Arduino_GFX *secGfx = new Arduino_ST7789(
  secBus,
  SEC_LCD_RST,
  SEC_LCD_ROTATION,
  true,
  SEC_LCD_WIDTH,
  SEC_LCD_HEIGHT,
  0,
  20,
  0,
  0
);

// ----------------------------------------------------
// Secondary page state
// ----------------------------------------------------

uint8_t currentSecondaryPage = SECONDARY_PAGE_PREFLIGHT;

bool secondaryNeedsFullRedraw = true;
unsigned long secondaryLastUpdateMs = 0;

// ----------------------------------------------------
// Page function declarations
// ----------------------------------------------------

void drawSecondaryPreflightPage();
void drawSecondaryFlightStatusPage();
void drawSecondaryNavPage();
void drawSecondaryBatteryPage();
void drawSecondaryBatteryGraphicalPage();
void drawSecondaryGearFlapsPage();
void drawSecondaryDiagnosticsPage();

// ----------------------------------------------------
// Helper declarations
// ----------------------------------------------------

bool isSecondaryPageEnabled(uint8_t page);
uint8_t getFirstEnabledSecondaryPage();
uint8_t getConfiguredSecondaryDefaultPage();
const char* getSecondaryPageName();
void drawCurrentSecondaryPage();
void toggleSecondaryScreen();
void updateSecondaryDisplay();

// ----------------------------------------------------
// Page enable helpers
// ----------------------------------------------------

bool isSecondaryPageEnabled(uint8_t page)
{
  switch (page) {
    case SECONDARY_PAGE_PREFLIGHT:
      return CONFIG_SECONDARY_PAGE_PREFLIGHT_ENABLED;

    case SECONDARY_PAGE_FLIGHT_STATUS:
      return CONFIG_SECONDARY_PAGE_FLIGHT_STATUS_ENABLED;

    case SECONDARY_PAGE_NAVIGATION:
      return CONFIG_SECONDARY_PAGE_NAVIGATION_ENABLED;

    case SECONDARY_PAGE_BATTERY:
      return CONFIG_SECONDARY_PAGE_BATTERY_ENABLED;

    case SECONDARY_PAGE_BATTERY_GRAPHICAL:
      return CONFIG_SECONDARY_PAGE_BATTERY_GRAPHICAL_ENABLED;

    case SECONDARY_PAGE_GEAR_FLAPS:
      return CONFIG_SECONDARY_PAGE_GEAR_FLAPS_ENABLED;

    case SECONDARY_PAGE_DIAGNOSTICS:
      return CONFIG_SECONDARY_PAGE_DIAGNOSTICS_ENABLED;

    default:
      return false;
  }
}

uint8_t getFirstEnabledSecondaryPage()
{
  for (uint8_t page = 0; page < SECONDARY_PAGE_COUNT; page++) {
    if (isSecondaryPageEnabled(page)) {
      return page;
    }
  }

  // Safety fallback if every page is disabled.
  return SECONDARY_PAGE_PREFLIGHT;
}

uint8_t getConfiguredSecondaryDefaultPage()
{
  if (CONFIG_SECONDARY_DEFAULT_PAGE < SECONDARY_PAGE_COUNT &&
      isSecondaryPageEnabled(CONFIG_SECONDARY_DEFAULT_PAGE)) {
    return CONFIG_SECONDARY_DEFAULT_PAGE;
  }

  return getFirstEnabledSecondaryPage();
}

// ----------------------------------------------------
// Page name helper
// ----------------------------------------------------

const char* getSecondaryPageName()
{
  switch (currentSecondaryPage) {
    case SECONDARY_PAGE_PREFLIGHT:
      return "PREFLT";

    case SECONDARY_PAGE_FLIGHT_STATUS:
      return "FLIGHT";

    case SECONDARY_PAGE_NAVIGATION:
      return "NAV";

    case SECONDARY_PAGE_BATTERY:
      return "BATT";

    case SECONDARY_PAGE_BATTERY_GRAPHICAL:
      return "BATT G";

    case SECONDARY_PAGE_GEAR_FLAPS:
      return "GEAR";

    case SECONDARY_PAGE_DIAGNOSTICS:
      return "DIAG";

    default:
      return "UNKNOWN";
  }
}

// ----------------------------------------------------
// Draw router
// ----------------------------------------------------

void drawCurrentSecondaryPage()
{
  if (!isSecondaryPageEnabled(currentSecondaryPage)) {
    currentSecondaryPage = getFirstEnabledSecondaryPage();
    secondaryNeedsFullRedraw = true;
  }

  switch (currentSecondaryPage) {
    case SECONDARY_PAGE_PREFLIGHT:
      drawSecondaryPreflightPage();
      break;

    case SECONDARY_PAGE_FLIGHT_STATUS:
      drawSecondaryFlightStatusPage();
      break;

    case SECONDARY_PAGE_NAVIGATION:
      drawSecondaryNavPage();
      break;

    case SECONDARY_PAGE_BATTERY:
      drawSecondaryBatteryPage();
      break;

    case SECONDARY_PAGE_BATTERY_GRAPHICAL:
      drawSecondaryBatteryGraphicalPage();
      break;

    case SECONDARY_PAGE_GEAR_FLAPS:
      drawSecondaryGearFlapsPage();
      break;

    case SECONDARY_PAGE_DIAGNOSTICS:
      drawSecondaryDiagnosticsPage();
      break;

    default:
      currentSecondaryPage = getFirstEnabledSecondaryPage();
      secondaryNeedsFullRedraw = true;
      drawSecondaryPreflightPage();
      break;
  }
}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setupSecondaryDisplay()
{
  pinMode(SEC_LCD_BL, OUTPUT);
  digitalWrite(SEC_LCD_BL, HIGH);

  secGfx->begin();
  secGfx->fillScreen(RGB565(0, 0, 0));

  currentSecondaryPage = getConfiguredSecondaryDefaultPage();

  secondaryLastUpdateMs = millis();
  secondaryNeedsFullRedraw = true;

  drawCurrentSecondaryPage();

  secondaryNeedsFullRedraw = false;
}

// ----------------------------------------------------
// Page select
// ----------------------------------------------------

void toggleSecondaryScreen()
{
  uint8_t nextPage = currentSecondaryPage;

  for (uint8_t attempts = 0; attempts < SECONDARY_PAGE_COUNT; attempts++) {
    nextPage++;

    if (nextPage >= SECONDARY_PAGE_COUNT) {
      nextPage = 0;
    }

    if (isSecondaryPageEnabled(nextPage)) {
      currentSecondaryPage = nextPage;
      secondaryNeedsFullRedraw = true;
      return;
    }
  }

  currentSecondaryPage = getFirstEnabledSecondaryPage();
  secondaryNeedsFullRedraw = true;
}

// ----------------------------------------------------
// Update
// ----------------------------------------------------

void updateSecondaryDisplay()
{
  unsigned long nowMs = millis();

  if (!secondaryNeedsFullRedraw &&
      nowMs - secondaryLastUpdateMs < CONFIG_SECONDARY_DISPLAY_UPDATE_MS) {
    return;
  }

  drawCurrentSecondaryPage();

  secondaryNeedsFullRedraw = false;
  secondaryLastUpdateMs = nowMs;
}