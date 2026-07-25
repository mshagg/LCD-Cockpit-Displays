// ----------------------------------------------------
// RC channel page select
// ----------------------------------------------------
//
// CH15 = main display page select
// CH16 = secondary display page select
//
// Both use LOW -> HIGH transition logic.
// They do not repeat while held HIGH.
// ----------------------------------------------------

#include <Arduino.h>

// From mavlink_telemetry.ino
extern bool mavlinkRcChannelsValid;
extern unsigned long lastMavlinkRcChannelsMs;

uint16_t getMavlinkRcChannelRaw(uint8_t channelNumber);

// From main tab
void toggleAhiScreen();

// From secondary_display.ino
void toggleSecondaryScreen();

// ----------------------------------------------------
// Page select state constants
// ----------------------------------------------------

const uint8_t PAGE_SELECT_RC_UNKNOWN = 0;
const uint8_t PAGE_SELECT_RC_LOW     = 1;
const uint8_t PAGE_SELECT_RC_HIGH    = 2;

// ----------------------------------------------------
// Main display page select state
// ----------------------------------------------------

uint8_t mainPageSelectState = PAGE_SELECT_RC_UNKNOWN;
uint16_t mainPageSelectRaw = 0;
unsigned long mainPageSelectLastActionMs = 0;
uint32_t mainPageSelectTriggerCountInternal = 0;

// ----------------------------------------------------
// Secondary display page select state
// ----------------------------------------------------

uint8_t secondaryPageSelectState = PAGE_SELECT_RC_UNKNOWN;
uint16_t secondaryPageSelectRaw = 0;
unsigned long secondaryPageSelectLastActionMs = 0;
uint32_t secondaryPageSelectTriggerCountInternal = 0;

// ----------------------------------------------------
// Diagnostics exposed to secondary pages
// ----------------------------------------------------

uint16_t pageSelectMainLastRaw = 0;
uint16_t pageSelectSecondaryLastRaw = 0;

uint32_t pageSelectMainTriggerCount = 0;
uint32_t pageSelectSecondaryTriggerCount = 0;

// ----------------------------------------------------
// Helper
// ----------------------------------------------------

void updateOnePageSelect(
  uint8_t &state,
  uint16_t &lastRaw,
  unsigned long &lastActionMs,
  uint32_t &triggerCount,
  bool enabled,
  uint8_t channel,
  uint16_t lowUs,
  uint16_t highUs,
  unsigned long minIntervalMs,
  void (*action)()
) {
  if (!enabled) {
    return;
  }

  if (channel < 1 || channel > 18) {
    return;
  }

  if (!mavlinkRcChannelsValid) {
    return;
  }

  if (millis() - lastMavlinkRcChannelsMs > 1500) {
    return;
  }

  uint16_t raw = getMavlinkRcChannelRaw(channel);
  lastRaw = raw;

  // Ignore missing or invalid RC values.
  if (raw < 800 || raw > 2200) {
    return;
  }

  // Establish initial state without triggering.
  // This prevents a page change on boot if the switch is already high.
  if (state == PAGE_SELECT_RC_UNKNOWN) {
    if (raw <= lowUs) {
      state = PAGE_SELECT_RC_LOW;
    }
    else if (raw >= highUs) {
      state = PAGE_SELECT_RC_HIGH;
    }

    return;
  }

  // Rearm when channel returns low.
  if (raw <= lowUs) {
    state = PAGE_SELECT_RC_LOW;
    return;
  }

  // Trigger once on LOW -> HIGH transition.
  if (raw >= highUs && state == PAGE_SELECT_RC_LOW) {
    unsigned long nowMs = millis();

    if (nowMs - lastActionMs >= minIntervalMs) {
      if (action) {
        action();
      }

      lastActionMs = nowMs;
      triggerCount++;
    }

    state = PAGE_SELECT_RC_HIGH;
  }
}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setupPageSelect()
{
  mainPageSelectState = PAGE_SELECT_RC_UNKNOWN;
  mainPageSelectRaw = 0;
  mainPageSelectLastActionMs = 0;
  mainPageSelectTriggerCountInternal = 0;

  secondaryPageSelectState = PAGE_SELECT_RC_UNKNOWN;
  secondaryPageSelectRaw = 0;
  secondaryPageSelectLastActionMs = 0;
  secondaryPageSelectTriggerCountInternal = 0;

  pageSelectMainLastRaw = 0;
  pageSelectSecondaryLastRaw = 0;

  pageSelectMainTriggerCount = 0;
  pageSelectSecondaryTriggerCount = 0;
}

// ----------------------------------------------------
// Update
// ----------------------------------------------------

void updatePageSelect()
{
  updateOnePageSelect(
    mainPageSelectState,
    mainPageSelectRaw,
    mainPageSelectLastActionMs,
    mainPageSelectTriggerCountInternal,
    CONFIG_MAIN_PAGE_SELECT_RC_ENABLED,
    CONFIG_MAIN_PAGE_SELECT_RC_CHANNEL,
    CONFIG_MAIN_PAGE_SELECT_RC_LOW_US,
    CONFIG_MAIN_PAGE_SELECT_RC_HIGH_US,
    CONFIG_MAIN_PAGE_SELECT_MIN_INTERVAL_MS,
    toggleAhiScreen
  );

  updateOnePageSelect(
    secondaryPageSelectState,
    secondaryPageSelectRaw,
    secondaryPageSelectLastActionMs,
    secondaryPageSelectTriggerCountInternal,
    CONFIG_SECONDARY_PAGE_SELECT_RC_ENABLED,
    CONFIG_SECONDARY_PAGE_SELECT_RC_CHANNEL,
    CONFIG_SECONDARY_PAGE_SELECT_RC_LOW_US,
    CONFIG_SECONDARY_PAGE_SELECT_RC_HIGH_US,
    CONFIG_SECONDARY_PAGE_SELECT_MIN_INTERVAL_MS,
    toggleSecondaryScreen
  );

  pageSelectMainLastRaw = mainPageSelectRaw;
  pageSelectSecondaryLastRaw = secondaryPageSelectRaw;

  pageSelectMainTriggerCount = mainPageSelectTriggerCountInternal;
  pageSelectSecondaryTriggerCount = secondaryPageSelectTriggerCountInternal;
}