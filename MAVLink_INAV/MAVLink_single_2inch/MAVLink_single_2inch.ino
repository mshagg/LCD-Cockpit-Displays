// ----------------------------------------------------
// MAVLink single 2 inch ST7789VW - main tab
// ----------------------------------------------------
//
// Hardware:
//   Standalone ESP32-S3
//   2.0 inch ST7789VW LCD
//   Resolution: 240 x 320 physical
//   Runtime orientation: 320 x 240 landscape
//
// LCD wiring:
//   LCD VCC        -> ESP32-S3 3.3V
//   LCD GND        -> ESP32-S3 GND
//   LCD CD / CS    -> ESP32-S3 GPIO10
//   LCD DC         -> ESP32-S3 GPIO13
//   LCD RST        -> ESP32-S3 GPIO14
//   LCD SDA / MOSI -> ESP32-S3 GPIO11
//   LCD SCL / SCK  -> ESP32-S3 GPIO12
//
// FC wiring:
//   FC UART TX     -> ESP32-S3 GPIO40
//   FC GND         -> ESP32-S3 GND
//
// Core allocation:
//   Core 0 = MAVLink + page select
//   Core 1 = display drawing + LCD flush
// ----------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "frame_template.h"

// ----------------------------------------------------
// Colour helpers
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
// LCD pin configuration
// ----------------------------------------------------

#define LCD_CS        10    // LCD CD / CS
#define LCD_DC        13    // LCD DC
#define LCD_RST       14    // LCD RST
#define LCD_MOSI      11    // LCD SDA
#define LCD_SCK       12    // LCD SCL

#define LCD_WIDTH     240
#define LCD_HEIGHT    320

// 0 = portrait
// 1 = landscape
// 2 = portrait upside down
// 3 = landscape upside down
#define LCD_ROTATION  3

// ----------------------------------------------------
// Task configuration
// ----------------------------------------------------

#define TELEMETRY_TASK_CORE       0
#define DISPLAY_TASK_CORE         1

#define TELEMETRY_TASK_PRIORITY   2
#define DISPLAY_TASK_PRIORITY     1

#define TELEMETRY_TASK_STACK      8000
#define DISPLAY_TASK_STACK        12000

#define DISPLAY_UPDATE_MS         16    // about 30 fps

// Enable only while COM16 is being continuously monitored. USB serial
// output can block the display task when the host is not reading it.
#define DISPLAY_PERF_ENABLED      0

extern const uint32_t CONFIG_LCD_SPI_HZ;

// ----------------------------------------------------
// Screen IDs
// ----------------------------------------------------

#define SCREEN_CLASSIC_GLASS_AHI  0
#define SCREEN_GEAR_FLAPS         1
#define SCREEN_ANALOGUE_GAUGES    2
#define SCREEN_SPLIT_AHI          3
#define SCREEN_COUNT              4

volatile uint8_t currentScreen = SCREEN_CLASSIC_GLASS_AHI;

// ----------------------------------------------------
// MAVLink attitude globals
// ----------------------------------------------------
//
// These are written by mavlink_telemetry.ino and read by
// the screen drawing tabs.

float roll = 0.0f;
float pitch = 0.0f;
float yaw = 0.0f;

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
  0,
  0,
  0
);

Arduino_Canvas *canvas = nullptr;

// ----------------------------------------------------
// Task handles
// ----------------------------------------------------

TaskHandle_t TelemetryTaskHandle = nullptr;
TaskHandle_t DisplayTaskHandle = nullptr;

// ----------------------------------------------------
// Display performance diagnostics
// ----------------------------------------------------

struct DisplayPerfStats {
  uint32_t frames;
  uint32_t missedDeadlines;
  uint64_t templateRestoreUs;
  uint64_t drawUs;
  uint64_t flushUs;
  uint64_t totalUs;
  uint32_t templateRestoreMaxUs;
  uint32_t drawMaxUs;
  uint32_t flushMaxUs;
  uint32_t totalMaxUs;
};

DisplayPerfStats displayPerf[SCREEN_COUNT] = {};
unsigned long displayPerfLastReportMs = 0;

static void updateDisplayPerfMax(
  uint32_t value,
  uint32_t *maximum
) {
  if (value > *maximum) {
    *maximum = value;
  }
}

static const char *displayPerfScreenName(uint8_t screen)
{
  switch (screen) {
    case SCREEN_CLASSIC_GLASS_AHI: return "CLASSIC";
    case SCREEN_GEAR_FLAPS: return "GEAR";
    case SCREEN_ANALOGUE_GAUGES: return "GAUGES";
    case SCREEN_SPLIT_AHI: return "SPLIT";
    default: return "UNKNOWN";
  }
}

static void reportDisplayPerformance()
{
  unsigned long nowMs = millis();
  unsigned long elapsedMs = nowMs - displayPerfLastReportMs;

  if (elapsedMs < 1000) {
    return;
  }

  for (uint8_t screen = 0; screen < SCREEN_COUNT; screen++) {
    DisplayPerfStats &stats = displayPerf[screen];

    if (stats.frames == 0) {
      continue;
    }

    uint32_t avgRestore =
      (uint32_t)(stats.templateRestoreUs / stats.frames);
    uint32_t avgDraw =
      (uint32_t)(stats.drawUs / stats.frames);
    uint32_t avgFlush =
      (uint32_t)(stats.flushUs / stats.frames);
    uint32_t avgTotal =
      (uint32_t)(stats.totalUs / stats.frames);

    float fps =
      ((float)stats.frames * 1000.0f) / (float)elapsedMs;

    Serial.printf(
      "PERF %-7s fps=%5.1f frames=%lu miss=%lu "
      "restore=%lu/%lu draw=%lu/%lu flush=%lu/%lu total=%lu/%lu us\n",
      displayPerfScreenName(screen),
      fps,
      (unsigned long)stats.frames,
      (unsigned long)stats.missedDeadlines,
      (unsigned long)avgRestore,
      (unsigned long)stats.templateRestoreMaxUs,
      (unsigned long)avgDraw,
      (unsigned long)stats.drawMaxUs,
      (unsigned long)avgFlush,
      (unsigned long)stats.flushMaxUs,
      (unsigned long)avgTotal,
      (unsigned long)stats.totalMaxUs
    );

    stats = {};
  }

  displayPerfLastReportMs = nowMs;
}

// ----------------------------------------------------
// Forward declarations
// ----------------------------------------------------

void setupDisplay();
void drawCurrentScreen();

void setupMavlinkTelemetry();
void get_mavlink_data();
void serviceSmoothedAttitude();

void setupPageSelect();
void updatePageSelect();

void telemetryTaskCode(void *pvParameters);
void displayTaskCode(void *pvParameters);

void toggleCurrentScreen();
uint8_t getCurrentScreen();

void drawClassicGlassCockpitAhiScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad,
  float yawRad
);

void drawGearFlapsScreen(
  Arduino_GFX *display
);

void drawAnalogueGaugesScreen(
  Arduino_GFX *display
);

void drawSplitAhiScreen(
  Arduino_GFX *display,
  float rollRad,
  float pitchRad,
  float yawRad
);

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setup()
{
  Serial.begin(115200);
  delay(200);

  setupDisplay();

  drawBootSplashScreen(canvas);
  canvas->flush();
  delay(2000);

  setupMavlinkTelemetry();
  setupPageSelect();

  drawCurrentScreen();
  canvas->flush();

  xTaskCreatePinnedToCore(
    telemetryTaskCode,
    "Telemetry_Task",
    TELEMETRY_TASK_STACK,
    NULL,
    TELEMETRY_TASK_PRIORITY,
    &TelemetryTaskHandle,
    TELEMETRY_TASK_CORE
  );

  xTaskCreatePinnedToCore(
    displayTaskCode,
    "Display_Task",
    DISPLAY_TASK_STACK,
    NULL,
    DISPLAY_TASK_PRIORITY,
    &DisplayTaskHandle,
    DISPLAY_TASK_CORE
  );
}

// ----------------------------------------------------
// Main loop
// ----------------------------------------------------
//
// Work is done by FreeRTOS tasks.

void loop()
{
  delay(1000);
}

// ----------------------------------------------------
// Display setup
// ----------------------------------------------------

void setupDisplay()
{
  gfx->begin(CONFIG_LCD_SPI_HZ);
  gfx->fillScreen(GFX_BLACK);

  canvas = new Arduino_Canvas(
    gfx->width(),
    gfx->height(),
    gfx
  );

  // The physical display is already initialized at CONFIG_LCD_SPI_HZ.
  // Do not let Arduino_Canvas initialize it again at the 40 MHz default.
  canvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  canvas->fillScreen(GFX_BLACK);
  canvas->flush();
}

// ----------------------------------------------------
// Screen selection helpers
// ----------------------------------------------------

uint8_t getCurrentScreen()
{
  return currentScreen;
}

void toggleCurrentScreen()
{
  uint8_t nextScreen = currentScreen + 1;

  if (nextScreen >= SCREEN_COUNT) {
    nextScreen = 0;
  }

  currentScreen = nextScreen;
}

// ----------------------------------------------------
// Screen router
// ----------------------------------------------------

void drawCurrentScreen()
{
  uint8_t screenToDraw = getCurrentScreen();

  switch (screenToDraw) {
    case SCREEN_GEAR_FLAPS:
      drawGearFlapsScreen(canvas);
      break;

    case SCREEN_ANALOGUE_GAUGES:
      drawAnalogueGaugesScreen(canvas);
      break;

    case SCREEN_CLASSIC_GLASS_AHI:
    default:
      drawClassicGlassCockpitAhiScreen(
        canvas,
        roll,
        pitch,
        yaw
      );
      break;

          case SCREEN_SPLIT_AHI:
      drawSplitAhiScreen(
        canvas,
        roll,
        pitch,
        yaw
      );
      break;
  }
}

// ----------------------------------------------------
// Core 0 task: telemetry and page select
// ----------------------------------------------------

void telemetryTaskCode(void *pvParameters)
{
  (void)pvParameters;

  for (;;) {
    get_mavlink_data();
    updatePageSelect();

    vTaskDelay(1);
  }
}

// ----------------------------------------------------
// Core 1 task: display drawing
// ----------------------------------------------------

void displayTaskCode(void *pvParameters)
{
  (void)pvParameters;

  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t updateIntervalTicks =
    pdMS_TO_TICKS(DISPLAY_UPDATE_MS);

  for (;;) {
    // Advance attitude filtering at the display cadence so motion remains
    // smooth between MAVLink ATTITUDE packets.
    serviceSmoothedAttitude();

#if DISPLAY_PERF_ENABLED
    uint8_t screenDrawn = getCurrentScreen();
    uint32_t frameStartUs = micros();

    resetFrameTemplateTiming();

    uint32_t drawStartUs = micros();
    drawCurrentScreen();
    uint32_t drawUs = micros() - drawStartUs;

    uint32_t flushStartUs = micros();
    canvas->flush();
    uint32_t flushUs = micros() - flushStartUs;

    uint32_t totalUs = micros() - frameStartUs;
    uint32_t restoreUs = getFrameTemplateRestoreMicros();

    DisplayPerfStats &stats = displayPerf[screenDrawn];
    stats.frames++;
    stats.templateRestoreUs += restoreUs;
    stats.drawUs += drawUs;
    stats.flushUs += flushUs;
    stats.totalUs += totalUs;

    updateDisplayPerfMax(
      restoreUs,
      &stats.templateRestoreMaxUs
    );
    updateDisplayPerfMax(drawUs, &stats.drawMaxUs);
    updateDisplayPerfMax(flushUs, &stats.flushMaxUs);
    updateDisplayPerfMax(totalUs, &stats.totalMaxUs);

    if (totalUs > (DISPLAY_UPDATE_MS * 1000UL)) {
      stats.missedDeadlines++;
    }

    reportDisplayPerformance();
#else
    drawCurrentScreen();
    canvas->flush();
#endif

    vTaskDelayUntil(
      &lastWakeTime,
      updateIntervalTicks
    );
  }
}
