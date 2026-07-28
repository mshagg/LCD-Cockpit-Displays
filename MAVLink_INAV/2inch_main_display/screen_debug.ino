// ----------------------------------------------------
// Lightweight debug screen for the main Waveshare LCD
// ----------------------------------------------------
//
// Purpose:
// - Keep the first display workload low
// - Let the larger secondary ST7789V2 keep rendering the AHI
// - Show secondary AHI FPS / frame time clearly
// ----------------------------------------------------

extern float mainFps;
extern float mainLastFrameMs;

extern bool secondaryDisplayReady;
extern bool secondaryCanvasReady;
extern float secondaryFps;
extern float secondaryLastFrameMs;

extern const char *getCurrentScreenName();

void drawDebugScreen(Arduino_Canvas *display, float roll, float pitch)
{
  uint16_t bgColor      = GFX_BLACK;
  uint16_t boxColor     = rgb565(25, 25, 25);
  uint16_t borderColor  = rgb565(0, 220, 220);
  uint16_t textColor    = GFX_WHITE;
  uint16_t titleColor   = rgb565(220, 210, 35);
  uint16_t okColor      = rgb565(0, 220, 80);
  uint16_t warnColor    = rgb565(230, 80, 40);

  int w = display->width();
  int h = display->height();

  display->fillScreen(bgColor);

  // Header
  display->fillRect(0, 0, w, 28, boxColor);
  display->drawFastHLine(0, 28, w, borderColor);

  display->setTextSize(2);
  display->setTextColor(titleColor);
  display->setCursor(8, 7);
  display->print("DEBUG");

  display->setTextSize(1);
  display->setTextColor(textColor);
  display->setCursor(88, 12);
  display->print("MAIN LIGHT LOAD");

  // Main display box
  drawDebugBox(display, 8, 40, w - 16, 72, "MAIN DISPLAY");

  display->setTextColor(textColor);
  display->setTextSize(1);

  char buf[40];

  snprintf(buf, sizeof(buf), "SCREEN: %s", getCurrentScreenName());
  display->setCursor(16, 62);
  display->print(buf);

  snprintf(buf, sizeof(buf), "FPS:    %5.1f", mainFps);
  display->setCursor(16, 78);
  display->print(buf);

  snprintf(buf, sizeof(buf), "FRAME:  %5.1f ms", mainLastFrameMs);
  display->setCursor(16, 94);
  display->print(buf);

  // Secondary display box
  drawDebugBox(display, 8, 122, w - 16, 88, "SECOND ST7789V2 AHI");

  display->setTextColor(textColor);

  display->setCursor(16, 144);
  display->print("STATUS: ");

  if (secondaryDisplayReady) {
    display->setTextColor(okColor);
    display->print("READY");
  } else {
    display->setTextColor(warnColor);
    display->print("NOT READY");
  }

  display->setTextColor(textColor);

  display->setCursor(16, 160);
  display->print("MODE:   ");

  if (secondaryCanvasReady) {
    display->setTextColor(okColor);
    display->print("BUFFERED");
  } else {
    display->setTextColor(warnColor);
    display->print("DIRECT");
  }

  display->setTextColor(textColor);

  snprintf(buf, sizeof(buf), "FPS:    %5.1f", secondaryFps);
  display->setCursor(16, 176);
  display->print(buf);

  snprintf(buf, sizeof(buf), "FRAME:  %5.1f ms", secondaryLastFrameMs);
  display->setCursor(16, 192);
  display->print(buf);

  // Attitude box
  drawDebugBox(display, 8, 220, w - 16, 48, "ATTITUDE");

  int rollDeg = (int)(roll * RAD_TO_DEG);
  int pitchDeg = (int)(pitch * RAD_TO_DEG);

  display->setTextColor(textColor);
  display->setTextSize(2);

  snprintf(buf, sizeof(buf), "R%+04d", rollDeg);
  display->setCursor(18, 242);
  display->print(buf);

  snprintf(buf, sizeof(buf), "P%+04d", pitchDeg);
  display->setCursor(118, 242);
  display->print(buf);
}

void drawDebugBox(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  int h,
  const char *title
) {
  uint16_t boxColor    = rgb565(18, 18, 18);
  uint16_t borderColor = rgb565(0, 130, 150);
  uint16_t titleColor  = rgb565(220, 210, 35);

  display->fillRect(x, y, w, h, boxColor);
  display->drawRect(x, y, w, h, borderColor);

  display->setTextSize(1);
  display->setTextColor(titleColor);
  display->setCursor(x + 8, y + 7);
  display->print(title);
}