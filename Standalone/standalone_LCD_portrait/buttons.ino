// ----------------------------------------------------
// Button support
// ----------------------------------------------------
//
// Waveshare ESP32-S3-LCD-1.69 V2:
// - SYS_OUT / PWR function button = GPIO40, active LOW
// - SYS_EN / power hold           = GPIO41
//
// Older non-V2 board may use:
// - SYS_OUT = GPIO36
// - SYS_EN  = GPIO35
//
// This tab uses a short press of the PWR/function button
// to switch between the AHI screens.
// ----------------------------------------------------

#define BUTTON_PWR_PIN  40
#define SYS_EN_PIN      41

const unsigned long BUTTON_DEBOUNCE_MS        = 40;
const unsigned long BUTTON_SHORT_PRESS_MAX_MS = 700;

bool pwrRawLast = HIGH;
bool pwrStableState = HIGH;

unsigned long pwrLastDebounceTime = 0;
unsigned long pwrPressStartTime = 0;

void setupButtons()
{
  pinMode(SYS_EN_PIN, OUTPUT);
  digitalWrite(SYS_EN_PIN, HIGH);

  pinMode(BUTTON_PWR_PIN, INPUT_PULLUP);

  pwrRawLast = digitalRead(BUTTON_PWR_PIN);
  pwrStableState = pwrRawLast;

  Serial.println("Power/function button ready");
}

void updateButtons()
{
  updatePowerButton();
}

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
  Serial.println("PWR/function short press");
  toggleAhiScreen();
}