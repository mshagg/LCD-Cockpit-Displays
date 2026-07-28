// ----------------------------------------------------
// Page select - single-screen 2 inch fork
// ----------------------------------------------------
//
// Uses MAVLink RC channel 16 to cycle screen pages.
//
// Current pages:
//   0 = Classic glass AHI
//   1 = Gear / flaps
// ----------------------------------------------------

#include <Arduino.h>

// ----------------------------------------------------
// External MAVLink telemetry
// ----------------------------------------------------

extern bool mavlinkRcChannelsValid;
extern uint16_t getMavlinkRcChannelRaw(uint8_t channelNumber);

// ----------------------------------------------------
// External screen control
// ----------------------------------------------------

extern void toggleCurrentScreen();

// ----------------------------------------------------
// Page select configuration
// ----------------------------------------------------

static const bool PAGE_SELECT_ENABLED = true;

static const uint8_t PAGE_SELECT_RC_CHANNEL = 16;

static const uint16_t PAGE_SELECT_RC_VALID_LOW_US = 800;
static const uint16_t PAGE_SELECT_RC_VALID_HIGH_US = 2200;

static const uint16_t PAGE_SELECT_RC_LOW_US = 1300;
static const uint16_t PAGE_SELECT_RC_HIGH_US = 1700;

static const unsigned long PAGE_SELECT_MIN_INTERVAL_MS = 300;

// ----------------------------------------------------
// Page select state
// ----------------------------------------------------

static uint8_t pageSelectLastZone = 0;
static unsigned long pageSelectLastTriggerMs = 0;

// ----------------------------------------------------
// Helpers
// ----------------------------------------------------

static uint8_t pageSelectGetZone(uint16_t rawUs)
{
  if (rawUs < PAGE_SELECT_RC_VALID_LOW_US ||
      rawUs > PAGE_SELECT_RC_VALID_HIGH_US) {
    return 0;
  }

  if (rawUs <= PAGE_SELECT_RC_LOW_US) {
    return 1;
  }

  if (rawUs >= PAGE_SELECT_RC_HIGH_US) {
    return 3;
  }

  return 2;
}

// ----------------------------------------------------
// Public setup
// ----------------------------------------------------

void setupPageSelect()
{
  pageSelectLastZone = 0;
  pageSelectLastTriggerMs = 0;
}

// ----------------------------------------------------
// Public update
// ----------------------------------------------------

void updatePageSelect()
{
  if (!PAGE_SELECT_ENABLED) {
    return;
  }

  if (!mavlinkRcChannelsValid) {
    pageSelectLastZone = 0;
    return;
  }

  uint16_t rawUs =
    getMavlinkRcChannelRaw(PAGE_SELECT_RC_CHANNEL);

  uint8_t zone = pageSelectGetZone(rawUs);

  unsigned long nowMs = millis();

  // Trigger only on entry into the high zone.
  if (zone == 3 &&
      pageSelectLastZone != 3 &&
      nowMs - pageSelectLastTriggerMs >= PAGE_SELECT_MIN_INTERVAL_MS) {
    toggleCurrentScreen();
    pageSelectLastTriggerMs = nowMs;
  }

  pageSelectLastZone = zone;
}