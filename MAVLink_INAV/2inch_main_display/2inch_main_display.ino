/*
  Waveshare ESP32-S3-LCD-1.69
  Modular artificial horizon with onboard QMI8658 IMU
  plus secondary ST7789V2 display

  Tabs:
  - waveshare_horizon_test.ino          main setup / loop / screen switching
  - screen_artificial_horizon.ino       full-screen artificial horizon
  - screen_classic_ahi.ino              classic round AHI screen
  - screen_glass_ahi.ino                glass-style full-screen AHI
  - screen_debug.ino                    lightweight debug screen
  - imu_qmi8658.ino                     onboard IMU attitude source
  - buttons.ino                         power button screen switching
  - secondary_st7789.ino                second ST7789V2 display
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "SensorQMI8658.hpp"
#include <math.h>

// ----------------------------------------------------
// Main display pin setup - Waveshare ESP32-S3-LCD-1.69
// ----------------------------------------------------

#define LCD_DC     4
#define LCD_CS     5
#define LCD_SCK    6
#define LCD_MOSI   7
#define LCD_RST    8
#define LCD_BL     15

#define LCD_WIDTH  240
#define LCD_HEIGHT 280

#define GFX_BLACK  0x0000
#define GFX_WHITE  0xFFFF

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769f
#endif

#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876f
#endif

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC,
  LCD_CS,
  LCD_SCK,
  LCD_MOSI,
  -1
);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  0,
  true,
  LCD_WIDTH,
  LCD_HEIGHT,
  0,
  20,
  0,
  0
);

Arduino_Canvas *canvas = new Arduino_Canvas(
  LCD_WIDTH,
  LCD_HEIGHT,
  gfx
);

// ----------------------------------------------------
// Global attitude values
// ----------------------------------------------------
//
// Updated from onboard IMU.
// Roll and pitch are in radians.

float roll = 0.0f;
float pitch = 0.0f;

// ----------------------------------------------------
// Main display performance counters
// ----------------------------------------------------

float mainFps = 0.0f;
float mainLastFrameMs = 0.0f;

unsigned long mainFpsWindowStart = 0;
unsigned long mainFrameCounter = 0;

// ----------------------------------------------------
// Screen state
// ----------------------------------------------------

enum ScreenId {
  SCREEN_FULL_AHI,
  SCREEN_CLASSIC_AHI,
  SCREEN_GLASS_AHI,
  SCREEN_DEBUG
};

ScreenId currentScreen = SCREEN_GLASS_AHI;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

void drawCurrentScreen();
void toggleAhiScreen();
const char *getCurrentScreenName();

// From screen_artificial_horizon.ino
void drawArtificialHorizonScreen(Arduino_GFX *display, float roll, float pitch);

// From screen_classic_ahi.ino
void drawClassicAhiScreen(Arduino_Canvas *display, float roll, float pitch);

// From screen_glass_ahi.ino
void drawGlassAhiScreen(Arduino_Canvas *display, float roll, float pitch);

// From screen_debug.ino
void drawDebugScreen(Arduino_Canvas *display, float roll, float pitch);

// From imu_qmi8658.ino
void setupIMU();
void updateImuAttitude();

// From buttons.ino
void setupButtons();
void updateButtons();

// From secondary_st7789.ino
void setupSecondaryDisplay();
void updateSecondaryDisplay();

// ----------------------------------------------------
// Setup / loop
// ----------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed");
    while (true) {
      delay(1000);
    }
  }

  if (!canvas->begin()) {
    Serial.println("canvas->begin() failed");
    while (true) {
      delay(1000);
    }
  }

  canvas->fillScreen(GFX_BLACK);
  canvas->flush();

  setupIMU();
  setupButtons();
  setupSecondaryDisplay();

  mainFpsWindowStart = millis();

  Serial.println("Display ready");
  Serial.println("PWR/function button cycles: FULL -> CLASSIC -> GLASS -> DEBUG");
}

void loop()
{
  updateButtons();
  updateImuAttitude();

  unsigned long now = millis();

  // ----------------------------------------------------
  // Main display update control
  // ----------------------------------------------------
  //
  // Normal AHI modes:
  // - first display draws and flushes every loop
  //
  // DEBUG mode:
  // - first display updates only every 500 ms
  // - this avoids constantly pushing the 240x280 canvas
  // - gives cleaner performance data for the large second display

  static unsigned long lastDebugScreenUpdate = 0;

  bool shouldUpdateMainDisplay = true;

  if (currentScreen == SCREEN_DEBUG) {
    if (now - lastDebugScreenUpdate < 500) {
      shouldUpdateMainDisplay = false;
    } else {
      lastDebugScreenUpdate = now;
    }
  }

  if (shouldUpdateMainDisplay) {
    unsigned long mainFrameStartMicros = micros();

    drawCurrentScreen();
    canvas->flush();

    unsigned long mainFrameEndMicros = micros();

    mainLastFrameMs = (mainFrameEndMicros - mainFrameStartMicros) / 1000.0f;

    mainFrameCounter++;
  }

  // Update main FPS once per second.
  // In DEBUG mode this should show roughly 2 FPS because the first
  // display is intentionally throttled.
  if (now - mainFpsWindowStart >= 1000) {
    mainFps =
      (mainFrameCounter * 1000.0f) /
      (float)(now - mainFpsWindowStart);

    mainFrameCounter = 0;
    mainFpsWindowStart = now;
  }

  // ----------------------------------------------------
  // Secondary display update
  // ----------------------------------------------------
  //
  // The large 2.0" ST7789V2 continues rendering the AHI as fast as
  // secondary_st7789.ino allows.

  updateSecondaryDisplay();

  // Use a very small delay for performance testing.
  // This stops the task from fully starving background work but does
  // not impose the old ~50 Hz loop cap.
  delay(1);
}

// ----------------------------------------------------
// Main display dispatcher
// ----------------------------------------------------

void drawCurrentScreen()
{
  switch (currentScreen) {
    case SCREEN_FULL_AHI:
      drawArtificialHorizonScreen(canvas, roll, pitch);
      break;

    case SCREEN_CLASSIC_AHI:
      drawClassicAhiScreen(canvas, roll, pitch);
      break;

    case SCREEN_GLASS_AHI:
      drawGlassAhiScreen(canvas, roll, pitch);
      break;

    case SCREEN_DEBUG:
      drawDebugScreen(canvas, roll, pitch);
      break;
  }
}

void toggleAhiScreen()
{
  if (currentScreen == SCREEN_FULL_AHI) {
    currentScreen = SCREEN_CLASSIC_AHI;
  } else if (currentScreen == SCREEN_CLASSIC_AHI) {
    currentScreen = SCREEN_GLASS_AHI;
  } else if (currentScreen == SCREEN_GLASS_AHI) {
    currentScreen = SCREEN_DEBUG;
  } else {
    currentScreen = SCREEN_FULL_AHI;
  }

  // Force the newly selected screen to render immediately on next loop.
  mainFpsWindowStart = millis();
  mainFrameCounter = 0;

  Serial.print("Main screen: ");
  Serial.println(getCurrentScreenName());
}

const char *getCurrentScreenName()
{
  switch (currentScreen) {
    case SCREEN_FULL_AHI:
      return "FULL AHI";

    case SCREEN_CLASSIC_AHI:
      return "CLASSIC AHI";

    case SCREEN_GLASS_AHI:
      return "GLASS AHI";

    case SCREEN_DEBUG:
      return "DEBUG";

    default:
      return "UNKNOWN";
  }
}

// ----------------------------------------------------
// Utility
// ----------------------------------------------------

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) |
         ((g & 0xFC) << 3) |
         (b >> 3);
}