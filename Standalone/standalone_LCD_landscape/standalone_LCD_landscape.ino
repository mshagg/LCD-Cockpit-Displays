/*
  Waveshare ESP32-S3-LCD-1.69
  Built-in display only
  Landscape orientation
  Multiple AHI screen types selected with PWR/function button

  Added:
  - remembers last selected screen using ESP32 Preferences / NVS

  Tabs:
  - waveshare_horizon_test.ino          main setup / loop / screen switching
  - screen_artificial_horizon.ino       full-screen artificial horizon
  - screen_classic_ahi.ino              classic round AHI screen
  - screen_glass_ahi.ino                glass-style AHI screen
  - screen_tron_ahi.ino                 futuristic Tron-style AHI screen
  - imu_qmi8658.ino                     onboard IMU attitude source
  - buttons.ino                         power button screen switching

  Libraries required:
  - GFX_Library_for_Arduino
  - SensorLib
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <Preferences.h>
#include "SensorQMI8658.hpp"
#include <math.h>

// ----------------------------------------------------
// Built-in Waveshare display pins
// ----------------------------------------------------

#define LCD_DC     4
#define LCD_CS     5
#define LCD_SCK    6
#define LCD_MOSI   7
#define LCD_RST    8
#define LCD_BL     15

// Native physical panel is 240 x 280.
// In landscape, drawing surface is 280 x 240.
#define LCD_NATIVE_WIDTH   240
#define LCD_NATIVE_HEIGHT  280

#define LCD_WIDTH          280
#define LCD_HEIGHT         240

// Rotation 1 = landscape.
// If the display is upside down, change this to 3.
#define LCD_ROTATION       1

#define GFX_BLACK  0x0000
#define GFX_WHITE  0xFFFF

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295769f
#endif

#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.295779513082320876f
#endif

// ----------------------------------------------------
// Persistent settings
// ----------------------------------------------------

Preferences preferences;

const char *PREF_NAMESPACE = "ahi";
const char *PREF_SCREEN_KEY = "screen";

// ----------------------------------------------------
// Built-in display object
// ----------------------------------------------------

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
  LCD_ROTATION,
  true,
  LCD_NATIVE_WIDTH,
  LCD_NATIVE_HEIGHT,
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

float roll = 0.0f;
float pitch = 0.0f;

// ----------------------------------------------------
// Screen state
// ----------------------------------------------------

enum ScreenId {
  SCREEN_FULL_AHI,
  SCREEN_CLASSIC_AHI,
  SCREEN_GLASS_AHI,
  SCREEN_TRON_AHI
};

ScreenId currentScreen = SCREEN_TRON_AHI;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

void drawCurrentScreen();
void toggleAhiScreen();
const char *getCurrentScreenName();

void loadLastSelectedScreen();
void saveCurrentScreen();

void drawArtificialHorizonScreen(Arduino_GFX *display, float roll, float pitch);
void drawClassicAhiScreen(Arduino_Canvas *display, float roll, float pitch);
void drawGlassAhiScreen(Arduino_Canvas *display, float roll, float pitch);
void drawTronAhiScreen(Arduino_Canvas *display, float roll, float pitch);

void setupIMU();
void updateImuAttitude();

void setupButtons();
void updateButtons();

// ----------------------------------------------------
// Setup / loop
// ----------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(1000);

  preferences.begin(PREF_NAMESPACE, false);
  loadLastSelectedScreen();

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

  Serial.println("Built-in display ready - landscape mode");
  Serial.println("PWR/function button cycles: FULL -> CLASSIC -> GLASS -> TRON");

  Serial.print("Restored screen: ");
  Serial.println(getCurrentScreenName());
}

void loop()
{
  updateButtons();

  updateImuAttitude();

  drawCurrentScreen();
  canvas->flush();

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

    case SCREEN_TRON_AHI:
      drawTronAhiScreen(canvas, roll, pitch);
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
    currentScreen = SCREEN_TRON_AHI;
  } else {
    currentScreen = SCREEN_FULL_AHI;
  }

  saveCurrentScreen();

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

    case SCREEN_TRON_AHI:
      return "TRON AHI";

    default:
      return "UNKNOWN";
  }
}

// ----------------------------------------------------
// Persistent screen selection
// ----------------------------------------------------

void loadLastSelectedScreen()
{
  uint8_t savedScreen = preferences.getUChar(
    PREF_SCREEN_KEY,
    SCREEN_TRON_AHI
  );

  if (savedScreen <= SCREEN_TRON_AHI) {
    currentScreen = (ScreenId)savedScreen;
  } else {
    currentScreen = SCREEN_TRON_AHI;
  }
}

void saveCurrentScreen()
{
  preferences.putUChar(
    PREF_SCREEN_KEY,
    (uint8_t)currentScreen
  );
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
