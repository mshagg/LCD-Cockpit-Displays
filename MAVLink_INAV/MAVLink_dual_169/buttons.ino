// ----------------------------------------------------
// Buttons
// ----------------------------------------------------
//
// PWR button:
//   short press = change main AHI screen
//
// GPIO40 = PWR button
// GPIO41 = SYS_EN, must be held HIGH
// ----------------------------------------------------

#include <Arduino.h>

#define BUTTON_PWR_PIN  40
#define SYS_EN_PIN      41

const unsigned long BUTTON_DEBOUNCE_MS        = 40;
const unsigned long BUTTON_SHORT_PRESS_MAX_MS = 700;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void updatePowerButton();
void onPowerButtonShortPress();

// From main tab.
void toggleAhiScreen();

// ----------------------------------------------------
// State
// ----------------------------------------------------

bool pwrRawLast = HIGH;
bool pwrStableState = HIGH;

unsigned long pwrLastDebounceTime = 0;
unsigned long pwrPressStartTime = 0;

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setupButtons()
{
  pinMode(SYS_EN_PIN, OUTPUT);
  digitalWrite(SYS_EN_PIN, HIGH);

  pinMode(BUTTON_PWR_PIN, INPUT_PULLUP);

  pwrRawLast = digitalRead(BUTTON_PWR_PIN);
  pwrStableState = pwrRawLast;

  pwrLastDebounceTime = millis();
  pwrPressStartTime = 0;
}

// ----------------------------------------------------
// Update
// ----------------------------------------------------

void updateButtons()
{
  updatePowerButton();
}

// ----------------------------------------------------
// PWR button
// ----------------------------------------------------

void updatePowerButton()
{
  bool rawReading = digitalRead(BUTTON_PWR_PIN);
  unsigned long now = millis();

  if (rawReading != pwrRawLast) {
    pwrLastDebounceTime = now;
    pwrRawLast = rawReading;
  }

  if ((now - pwrLastDebounceTime) < BUTTON_DEBOUNCE_MS) {
    return;
  }

  if (rawReading == pwrStableState) {
    return;
  }

  pwrStableState = rawReading;

  if (pwrStableState == LOW) {
    pwrPressStartTime = now;
    return;
  }

  unsigned long pressDuration = now - pwrPressStartTime;

  if (pressDuration <= BUTTON_SHORT_PRESS_MAX_MS) {
    onPowerButtonShortPress();
  }
}

void onPowerButtonShortPress()
{
  toggleAhiScreen();
}