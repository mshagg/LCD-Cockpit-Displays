/*
  Waveshare ESP32-S3-LCD-1.69
  Built-in display only
  Multiple AHI screen types selected with PWR/function button

  Tabs:
  - waveshare_horizon_test.ino          main setup / loop / screen switching
  - screen_artificial_horizon.ino       full-screen artificial horizon
  - screen_classic_ahi.ino              classic round AHI screen
  - screen_glass_ahi.ino                glass-style full-screen AHI
  - imu_qmi8658.ino                     onboard IMU attitude source
  - buttons.ino                         power button screen switching

  Libraries required:
  - GFX_Library_for_Arduino
  - SensorLib
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
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
  0,
  true,
  LCD_WIDTH,
  LCD_HEIGHT,
  0,
  20,
  0,
  0
);

// Full-screen canvas for the built-in display
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
// Screen state
// ----------------------------------------------------

enum ScreenId {
  SCREEN_FULL_AHI,
  SCREEN_CLASSIC_AHI,
  SCREEN_GLASS_AHI
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
// This version accepts Arduino_GFX*, so Arduino_Canvas works too.
void drawArtificialHorizonScreen(Arduino_GFX *display, float roll, float pitch);

// From screen_classic_ahi.ino
void drawClassicAhiScreen(Arduino_Canvas *display, float roll, float pitch);

// From screen_glass_ahi.ino
void drawGlassAhiScreen(Arduino_Canvas *display, float roll, float pitch);

// From imu_qmi8658.ino
void setupIMU();
void updateImuAttitude();

// From buttons.ino
void setupButtons();
void updateButtons();

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

  Serial.println("Built-in display ready");
  Serial.println("PWR/function button cycles: FULL -> CLASSIC -> GLASS");
}

void loop()
{
  updateButtons();

  updateImuAttitude();

  drawCurrentScreen();
  canvas->flush();

  delay(20);
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
  }
}

void toggleAhiScreen()
{
  if (currentScreen == SCREEN_FULL_AHI) {
    currentScreen = SCREEN_CLASSIC_AHI;
  } else if (currentScreen == SCREEN_CLASSIC_AHI) {
    currentScreen = SCREEN_GLASS_AHI;
  } else {
    currentScreen = SCREEN_FULL_AHI;
  }

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