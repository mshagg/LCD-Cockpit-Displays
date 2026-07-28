// ----------------------------------------------------
// Secondary 2.0" ST7789V2 TFT artificial horizon test
// ----------------------------------------------------
//
// Display:
// - Controller: ST7789V2
// - Resolution: 240(H) RGB x 320(V)
// - Interface: SPI
//
// Wired as:
//
// ST7789V2 pin      Waveshare pin
// --------------------------------
// VCC               3.3V
// GND               GND
// SCL               GPIO18   // SPI clock
// SDA               GPIO17   // SPI MOSI / data
// CS                GPIO16
// DC                GPIO3
// RST               GPIO2
//
// No separate backlight pin is required on this module.
// ----------------------------------------------------

// ----------------------------------------------------
// Secondary display pin setup
// ----------------------------------------------------

#define SECOND_TFT_SCK   18
#define SECOND_TFT_MOSI  17
#define SECOND_TFT_CS    16
#define SECOND_TFT_DC    3
#define SECOND_TFT_RST   2

#define SECOND_TFT_WIDTH   240
#define SECOND_TFT_HEIGHT  320

// Native panel is portrait 240 x 320.
// Rotation 1 gives landscape 320 x 240.
#define SECOND_TFT_ROTATION  1

// For performance testing.
// If unstable, reduce to 40000000 or 27000000.
#define SECOND_TFT_SPI_SPEED  80000000

// 0 = draw secondary AHI every loop.
// 33 = roughly 30 Hz cap.
// 100 = roughly 10 Hz cap.
// Leave at 0 for maximum-performance testing.
#define SECOND_AHI_FRAME_INTERVAL_MS  0

// Landscape canvas size after rotation.
#define SECOND_CANVAS_WIDTH   320
#define SECOND_CANVAS_HEIGHT  240

Arduino_DataBus *secondaryBus = new Arduino_ESP32SPI(
  SECOND_TFT_DC,
  SECOND_TFT_CS,
  SECOND_TFT_SCK,
  SECOND_TFT_MOSI,
  -1
);

Arduino_GFX *secondaryTft = new Arduino_ST7789(
  secondaryBus,
  SECOND_TFT_RST,
  SECOND_TFT_ROTATION,
  true,
  SECOND_TFT_WIDTH,
  SECOND_TFT_HEIGHT,
  0,
  0,
  0,
  0
);

// Preferred: buffered secondary display.
Arduino_Canvas *secondaryCanvas = new Arduino_Canvas(
  SECOND_CANVAS_WIDTH,
  SECOND_CANVAS_HEIGHT,
  secondaryTft
);

bool secondaryDisplayReady = false;
bool secondaryCanvasReady = false;

// These are read by screen_debug.ino
float secondaryFps = 0.0f;
float secondaryLastFrameMs = 0.0f;

// Internal performance counters
unsigned long secondaryLastFrameTime = 0;
unsigned long secondaryFpsWindowStart = 0;
unsigned long secondaryFrameCounter = 0;

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setupSecondaryDisplay()
{
  Serial.println("Starting secondary ST7789V2 AHI display...");

  pinMode(SECOND_TFT_CS, OUTPUT);
  pinMode(SECOND_TFT_DC, OUTPUT);
  pinMode(SECOND_TFT_RST, OUTPUT);

  digitalWrite(SECOND_TFT_CS, HIGH);
  digitalWrite(SECOND_TFT_DC, HIGH);

  // Manual reset pulse
  digitalWrite(SECOND_TFT_RST, HIGH);
  delay(20);
  digitalWrite(SECOND_TFT_RST, LOW);
  delay(50);
  digitalWrite(SECOND_TFT_RST, HIGH);
  delay(150);

  if (!secondaryTft->begin(SECOND_TFT_SPI_SPEED)) {
    Serial.println("secondaryTft->begin() failed");
    secondaryDisplayReady = false;
    return;
  }

  secondaryDisplayReady = true;

  secondaryTft->invertDisplay(false);
  secondaryTft->fillScreen(GFX_BLACK);

  Serial.print("Secondary ST7789V2 ready. Width=");
  Serial.print(secondaryTft->width());
  Serial.print(" Height=");
  Serial.println(secondaryTft->height());

  if (secondaryCanvas->begin()) {
    secondaryCanvasReady = true;

    secondaryCanvas->fillScreen(GFX_BLACK);
    secondaryCanvas->flush();

    Serial.println("Secondary canvas ready");
  } else {
    secondaryCanvasReady = false;

    secondaryTft->fillScreen(GFX_BLACK);
    secondaryTft->setTextColor(GFX_WHITE);
    secondaryTft->setTextSize(2);
    secondaryTft->setCursor(20, 80);
    secondaryTft->print("NO CANVAS");
    secondaryTft->setCursor(20, 110);
    secondaryTft->print("DIRECT MODE");

    Serial.println("Secondary canvas failed. Falling back to direct draw.");
  }

  secondaryLastFrameTime = millis();
  secondaryFpsWindowStart = millis();
}

// ----------------------------------------------------
// Update
// ----------------------------------------------------

void updateSecondaryDisplay()
{
  if (!secondaryDisplayReady) {
    return;
  }

  unsigned long now = millis();

  if (SECOND_AHI_FRAME_INTERVAL_MS > 0) {
    if (now - secondaryLastFrameTime < SECOND_AHI_FRAME_INTERVAL_MS) {
      return;
    }
  }

  secondaryLastFrameTime = now;

  unsigned long frameStartMicros = micros();

  Arduino_GFX *target;

  if (secondaryCanvasReady) {
    target = secondaryCanvas;
  } else {
    target = secondaryTft;
  }

  drawArtificialHorizonScreen(target, roll, pitch);

  drawSecondaryPerformanceOverlay(target);

  if (secondaryCanvasReady) {
    secondaryCanvas->flush();
  }

  unsigned long frameEndMicros = micros();

  secondaryLastFrameMs = (frameEndMicros - frameStartMicros) / 1000.0f;

  secondaryFrameCounter++;

  unsigned long fpsNow = millis();

  if (fpsNow - secondaryFpsWindowStart >= 1000) {
    secondaryFps =
      (secondaryFrameCounter * 1000.0f) /
      (float)(fpsNow - secondaryFpsWindowStart);

    secondaryFrameCounter = 0;
    secondaryFpsWindowStart = fpsNow;

    Serial.print("Secondary AHI FPS: ");
    Serial.print(secondaryFps, 1);
    Serial.print("  frame ms: ");
    Serial.println(secondaryLastFrameMs, 1);
  }
}

// ----------------------------------------------------
// Overlay
// ----------------------------------------------------

void drawSecondaryPerformanceOverlay(Arduino_GFX *display)
{
  int w = display->width();
  int h = display->height();

  uint16_t panelColor  = GFX_BLACK;
  uint16_t textColor   = GFX_WHITE;
  uint16_t accentColor = rgb565(220, 210, 35);
  uint16_t cyanColor   = rgb565(0, 220, 220);

  // Top-left black data panel
  display->fillRect(4, 4, 130, 52, panelColor);
  display->drawRect(4, 4, 130, 52, cyanColor);

  display->setTextSize(1);

  display->setTextColor(accentColor);
  display->setCursor(10, 10);
  display->print("SECOND AHI");

  display->setTextColor(textColor);

  char buf[32];

  snprintf(buf, sizeof(buf), "FPS %4.1f", secondaryFps);
  display->setCursor(10, 24);
  display->print(buf);

  snprintf(buf, sizeof(buf), "MS  %4.1f", secondaryLastFrameMs);
  display->setCursor(10, 38);
  display->print(buf);

  // Bottom attitude readout
  display->fillRect(4, h - 38, w - 8, 34, panelColor);
  display->drawRect(4, h - 38, w - 8, 34, cyanColor);

  int rollDeg = (int)(roll * RAD_TO_DEG);
  int pitchDeg = (int)(pitch * RAD_TO_DEG);

  display->setTextColor(textColor);
  display->setTextSize(2);

  snprintf(buf, sizeof(buf), "R%+03d", rollDeg);
  display->setCursor(14, h - 28);
  display->print(buf);

  snprintf(buf, sizeof(buf), "P%+03d", pitchDeg);
  display->setCursor(110, h - 28);
  display->print(buf);

  display->setTextSize(1);
  display->setTextColor(accentColor);

  if (secondaryCanvasReady) {
    display->setCursor(w - 52, h - 16);
    display->print("BUF");
  } else {
    display->setCursor(w - 52, h - 16);
    display->print("DIR");
  }
}