// ----------------------------------------------------
// MAVLink LCD Portrait - main tab
// ----------------------------------------------------
//
// Hardware:
//   Waveshare ESP32-S3-LCD-1.69
//   240 x 280 ST7789V2
//
// Active primary screens:
//   0 - Full glass cockpit AHI
//   1 - Classic AHI + heading indicator
//   2 - Classic airspeed + altimeter
//   4 - Tron full-screen AHI
//
// Removed:
//   screen_glass_ahi.ino
//   screen_tron_ahi.ino
//
// Task split:
//   Task1 = primary display + MAVLink + buttons/page select
//   Task2 = secondary display only
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>

// ----------------------------------------------------
// Arduino_GFX compatibility helpers
// ----------------------------------------------------

#ifndef GFX_BLACK
#define GFX_BLACK RGB565(0, 0, 0)
#endif

#ifndef GFX_WHITE
#define GFX_WHITE RGB565(255, 255, 255)
#endif

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return RGB565(r, g, b);
}

// ----------------------------------------------------
// Primary LCD pins
// ----------------------------------------------------

#define LCD_DC        4
#define LCD_CS        5
#define LCD_SCK       6
#define LCD_MOSI      7
#define LCD_RST       8
#define LCD_BL        15

#define LCD_WIDTH     240
#define LCD_HEIGHT    280

// Change to 2 if the display is upside down.
#define LCD_ROTATION  0

// ----------------------------------------------------
// Task config
// ----------------------------------------------------

#define DISPLAY_TASK_CORE      1
#define DISPLAY_TASK_PRIORITY  1
#define DISPLAY_TASK_STACK     12000

#define SECONDARY_TASK_CORE      0
#define SECONDARY_TASK_PRIORITY  1
#define SECONDARY_TASK_STACK     8000

TaskHandle_t Task1;
TaskHandle_t Task2;

// ----------------------------------------------------
// Display objects
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
// Flight attitude globals
// ----------------------------------------------------

float roll = 0.0f;
float pitch = 0.0f;
float yaw = 0.0f;

// ----------------------------------------------------
// Display timing diagnostics
// ----------------------------------------------------

float displayFps = 0.0f;
float displayFrameMs = 0.0f;
float displayDrawMs = 0.0f;
float displayFlushMs = 0.0f;
float displayMavlinkMs = 0.0f;

uint32_t displayFrameCountWindow = 0;
uint64_t displayFrameUsAccum = 0;
uint64_t displayDrawUsAccum = 0;
uint64_t displayFlushUsAccum = 0;
uint64_t displayMavlinkUsAccum = 0;
unsigned long displayStatsLastMs = 0;

// ----------------------------------------------------
// Preferences
// ----------------------------------------------------

Preferences preferences;

const char *PREF_NAMESPACE = "mfd";
const char *PREF_AHI_SCREEN = "ahi";

// ----------------------------------------------------
// Primary screen selection
// ----------------------------------------------------
//
// Old removed IDs:
//   3 = old tron AHI

enum AhiScreen : uint8_t {
  SCREEN_FULL_AHI = 0,
  SCREEN_CLASSIC_AHI = 1,
  SCREEN_CLASSIC_INSTRUMENTS = 2,
  SCREEN_TRON_FULL_AHI = 4
};

AhiScreen currentAhiScreen = SCREEN_FULL_AHI;

// ----------------------------------------------------
// Local function declarations
// ----------------------------------------------------

void setupPrimaryDisplay();
void loadAhiScreenPreference();
void saveAhiScreenPreference();
bool isValidAhiScreenValue(uint8_t screenValue);
void toggleAhiScreen();
void drawCurrentScreen();

void updateDisplayStats(
  uint32_t frameStartUs,
  uint32_t mavlinkStartUs,
  uint32_t mavlinkEndUs,
  uint32_t drawStartUs,
  uint32_t flushStartUs,
  uint32_t frameEndUs
);

void Task1code(void *pvParameters);
void Task2code(void *pvParameters);

// ----------------------------------------------------
// Other-tab function declarations
// ----------------------------------------------------

void setupMavlinkTelemetry();
void get_mavlink_data();

void setupButtons();
void updateButtons();

void setupPageSelect();
void updatePageSelect();

void setupSecondaryDisplay();
void updateSecondaryDisplay();

void drawArtificialHorizonScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad
);

void drawClassicAhiScreen(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
);

void drawClassicInstrumentsScreen(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
);

void drawTronFullAhiScreen(
  Arduino_Canvas *display,
  float rollRad,
  float pitchRad
);

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(200);

  setupPrimaryDisplay();
  loadAhiScreenPreference();

  setupMavlinkTelemetry();
  setupButtons();
  setupPageSelect();
  setupSecondaryDisplay();

  displayStatsLastMs = millis();

  xTaskCreatePinnedToCore(
    Task1code,
    "Task1_Display",
    DISPLAY_TASK_STACK,
    NULL,
    DISPLAY_TASK_PRIORITY,
    &Task1,
    DISPLAY_TASK_CORE
  );

  xTaskCreatePinnedToCore(
    Task2code,
    "Task2_Secondary",
    SECONDARY_TASK_STACK,
    NULL,
    SECONDARY_TASK_PRIORITY,
    &Task2,
    SECONDARY_TASK_CORE
  );
}

// ----------------------------------------------------
// Main loop
// ----------------------------------------------------

void loop()
{
  delay(1000);
}

// ----------------------------------------------------
// Primary display setup
// ----------------------------------------------------

void setupPrimaryDisplay()
{
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  gfx->begin();
  canvas->begin();

  canvas->fillScreen(GFX_BLACK);
  canvas->flush();
}

// ----------------------------------------------------
// Screen preference
// ----------------------------------------------------

bool isValidAhiScreenValue(uint8_t screenValue)
{
  return screenValue == (uint8_t)SCREEN_FULL_AHI ||
         screenValue == (uint8_t)SCREEN_CLASSIC_AHI ||
         screenValue == (uint8_t)SCREEN_CLASSIC_INSTRUMENTS ||
         screenValue == (uint8_t)SCREEN_TRON_FULL_AHI;
}

void loadAhiScreenPreference()
{
  preferences.begin(PREF_NAMESPACE, true);

  uint8_t savedScreen =
    preferences.getUChar(PREF_AHI_SCREEN, (uint8_t)SCREEN_FULL_AHI);

  preferences.end();

  if (!isValidAhiScreenValue(savedScreen)) {
    savedScreen = (uint8_t)SCREEN_FULL_AHI;
  }

  currentAhiScreen = (AhiScreen)savedScreen;
}

void saveAhiScreenPreference()
{
  preferences.begin(PREF_NAMESPACE, false);
  preferences.putUChar(PREF_AHI_SCREEN, (uint8_t)currentAhiScreen);
  preferences.end();
}

// ----------------------------------------------------
// Main display screen switching
// ----------------------------------------------------

void toggleAhiScreen()
{
  switch (currentAhiScreen) {

    case SCREEN_FULL_AHI:
      currentAhiScreen = SCREEN_CLASSIC_AHI;
      break;

    case SCREEN_CLASSIC_AHI:
      currentAhiScreen = SCREEN_CLASSIC_INSTRUMENTS;
      break;

    case SCREEN_CLASSIC_INSTRUMENTS:
      currentAhiScreen = SCREEN_TRON_FULL_AHI;
      break;

    case SCREEN_TRON_FULL_AHI:
      currentAhiScreen = SCREEN_FULL_AHI;
      break;

    default:
      currentAhiScreen = SCREEN_FULL_AHI;
      break;
  }

  saveAhiScreenPreference();
}

// ----------------------------------------------------
// Draw selected primary screen
// ----------------------------------------------------

void drawCurrentScreen()
{
  switch (currentAhiScreen) {

    case SCREEN_FULL_AHI:
      drawArtificialHorizonScreen(
        canvas,
        roll,
        pitch
      );
      break;

    case SCREEN_CLASSIC_AHI:
      drawClassicAhiScreen(
        canvas,
        roll,
        pitch
      );
      break;

    case SCREEN_CLASSIC_INSTRUMENTS:
      drawClassicInstrumentsScreen(
        canvas,
        roll,
        pitch
      );
      break;

    case SCREEN_TRON_FULL_AHI:
      drawTronFullAhiScreen(
        canvas,
        roll,
        pitch
      );
      break;

    default:
      currentAhiScreen = SCREEN_FULL_AHI;

      drawArtificialHorizonScreen(
        canvas,
        roll,
        pitch
      );
      break;
  }
}

// ----------------------------------------------------
// Primary display task
// ----------------------------------------------------

void Task1code(void *pvParameters)
{
  for (;;) {
    uint32_t frameStartUs = micros();

    updateButtons();

    uint32_t mavlinkStartUs = micros();
    get_mavlink_data();
    uint32_t mavlinkEndUs = micros();

    updatePageSelect();

    uint32_t drawStartUs = micros();
    drawCurrentScreen();

    uint32_t flushStartUs = micros();
    canvas->flush();

    uint32_t frameEndUs = micros();

    updateDisplayStats(
      frameStartUs,
      mavlinkStartUs,
      mavlinkEndUs,
      drawStartUs,
      flushStartUs,
      frameEndUs
    );

    vTaskDelay(1);
  }
}

// ----------------------------------------------------
// Secondary display task
// ----------------------------------------------------

void Task2code(void *pvParameters)
{
  for (;;) {
    updateSecondaryDisplay();

    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// ----------------------------------------------------
// Display diagnostics
// ----------------------------------------------------

void updateDisplayStats(
  uint32_t frameStartUs,
  uint32_t mavlinkStartUs,
  uint32_t mavlinkEndUs,
  uint32_t drawStartUs,
  uint32_t flushStartUs,
  uint32_t frameEndUs
) {
  uint32_t frameUs = frameEndUs - frameStartUs;
  uint32_t mavlinkUs = mavlinkEndUs - mavlinkStartUs;
  uint32_t drawUs = flushStartUs - drawStartUs;
  uint32_t flushUs = frameEndUs - flushStartUs;

  displayFrameCountWindow++;

  displayFrameUsAccum += frameUs;
  displayMavlinkUsAccum += mavlinkUs;
  displayDrawUsAccum += drawUs;
  displayFlushUsAccum += flushUs;

  unsigned long nowMs = millis();
  unsigned long elapsedMs = nowMs - displayStatsLastMs;

  if (elapsedMs >= 1000) {
    displayFps =
      ((float)displayFrameCountWindow * 1000.0f) /
      (float)elapsedMs;

    if (displayFrameCountWindow > 0) {
      displayFrameMs =
        ((float)displayFrameUsAccum /
         (float)displayFrameCountWindow) /
        1000.0f;

      displayMavlinkMs =
        ((float)displayMavlinkUsAccum /
         (float)displayFrameCountWindow) /
        1000.0f;

      displayDrawMs =
        ((float)displayDrawUsAccum /
         (float)displayFrameCountWindow) /
        1000.0f;

      displayFlushMs =
        ((float)displayFlushUsAccum /
         (float)displayFrameCountWindow) /
        1000.0f;
    } else {
      displayFrameMs = 0.0f;
      displayMavlinkMs = 0.0f;
      displayDrawMs = 0.0f;
      displayFlushMs = 0.0f;
    }

    displayFrameCountWindow = 0;
    displayFrameUsAccum = 0;
    displayMavlinkUsAccum = 0;
    displayDrawUsAccum = 0;
    displayFlushUsAccum = 0;

    displayStatsLastMs = nowMs;
  }
}