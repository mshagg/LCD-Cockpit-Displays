// ----------------------------------------------------
// Artificial horizon screen
// ----------------------------------------------------
//
// This tab draws the artificial horizon only.
// It does not initialise the display.
// It does not call flush().
//
// roll and pitch are expected in radians.
//
// Updated:
// - accepts Arduino_GFX*
// - this allows the same gauge to draw to:
//     - main Arduino_Canvas
//     - secondary Arduino_Canvas
//     - direct TFT, if needed
// ----------------------------------------------------

void drawArtificialHorizonScreen(Arduino_GFX *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  const int cx = screenW / 2;
  const int cy = screenH / 2;

  const float pixelsPerDeg = 3.0f;

  uint16_t skyColor    = rgb565(0, 75, 230);
  uint16_t groundColor = rgb565(145, 85, 10);
  uint16_t yellowColor = rgb565(230, 220, 40);

  float rollRad = -roll;

  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;

  pitchOffset = constrain(pitchOffset, -screenH, screenH);

  drawFullScreenHorizonFill(display, cx, cy, rollRad, pitchOffset, skyColor, groundColor);
  drawFullScreenPitchLadder(display, cx, cy, rollRad, pitchOffset, pixelsPerDeg, GFX_WHITE);
  drawSimpleRollPointer(display, cx, GFX_WHITE);
  drawAircraftSymbol(display, cx, cy, yellowColor, GFX_BLACK);
}

void drawFullScreenHorizonFill(
  Arduino_GFX *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
) {
  const int screenW = display->width();
  const int screenH = display->height();

  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  for (int y = 0; y < screenH; y++) {
    int dy = y - cy;

    if (fabsf(nx) < 0.0001f) {
      float value = ny * (dy - pitchOffset);
      uint16_t colour = (value < 0) ? skyColor : groundColor;
      display->drawFastHLine(0, y, screenW, colour);
    } else {
      float splitX = cx - (ny * (dy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= 0) {
          display->drawFastHLine(0, y, screenW, groundColor);
        } else if (splitX >= screenW - 1) {
          display->drawFastHLine(0, y, screenW, skyColor);
        } else {
          int sx = (int)splitX;
          display->drawFastHLine(0, y, sx + 1, skyColor);
          display->drawFastHLine(sx + 1, y, screenW - sx - 1, groundColor);
        }
      } else {
        if (splitX <= 0) {
          display->drawFastHLine(0, y, screenW, skyColor);
        } else if (splitX >= screenW - 1) {
          display->drawFastHLine(0, y, screenW, groundColor);
        } else {
          int sx = (int)splitX;
          display->drawFastHLine(0, y, sx + 1, groundColor);
          display->drawFastHLine(sx + 1, y, screenW - sx - 1, skyColor);
        }
      }
    }
  }
}

void drawFullScreenPitchLadder(
  Arduino_GFX *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t colour
) {
  const int screenW = display->width();
  const int screenH = display->height();

  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  display->setTextColor(colour);
  display->setTextSize(1);

  for (int deg = -40; deg <= 40; deg += 5) {
    if (deg == 0) {
      continue;
    }

    float lineOffset = pitchOffset - deg * pixelsPerDeg;

    int mx = cx + nx * lineOffset;
    int my = cy + ny * lineOffset;

    if (mx < -90 || mx > screenW + 90 || my < -90 || my > screenH + 90) {
      continue;
    }

    int halfLen = (deg % 10 == 0) ? 36 : 18;

    int x1 = mx - ux * halfLen;
    int y1 = my - uy * halfLen;
    int x2 = mx + ux * halfLen;
    int y2 = my + uy * halfLen;

    display->drawLine(x1, y1, x2, y2, colour);

    if (deg % 10 == 0) {
      int labelValue = abs(deg);

      char labelText[4];
      snprintf(labelText, sizeof(labelText), "%d", labelValue);

      int charCount = strlen(labelText);
      int labelW = charCount * 6;
      int labelH = 8;

      int lx1 = x1 - labelW - 8;
      int ly1 = y1 - (labelH / 2);

      int lx2 = x2 + 8;
      int ly2 = y2 - (labelH / 2);

      if (lx1 > 0 && lx1 < screenW - labelW && ly1 > 0 && ly1 < screenH - labelH) {
        drawBoldPitchText(display, lx1, ly1, labelText, colour);
      }

      if (lx2 > 0 && lx2 < screenW - labelW && ly2 > 0 && ly2 < screenH - labelH) {
        drawBoldPitchText(display, lx2, ly2, labelText, colour);
      }
    }
  }

  display->setTextSize(1);
}

void drawBoldPitchText(
  Arduino_GFX *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
) {
  display->setTextColor(colour);
  display->setTextSize(1);

  display->setCursor(x, y);
  display->print(text);

  display->setCursor(x + 1, y);
  display->print(text);
}

void drawAircraftSymbol(
  Arduino_GFX *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t outlineColor
) {
  display->drawLine(cx - 70, cy,      cx - 22, cy,      outlineColor);
  display->drawLine(cx + 22, cy,      cx + 70, cy,      outlineColor);
  display->drawLine(cx - 22, cy,      cx,      cy + 10, outlineColor);
  display->drawLine(cx,      cy + 10, cx + 22, cy,      outlineColor);

  display->drawLine(cx - 70, cy,      cx - 22, cy,      yellowColor);
  display->drawLine(cx - 22, cy,      cx,      cy + 10, yellowColor);
  display->drawLine(cx,      cy + 10, cx + 22, cy,      yellowColor);
  display->drawLine(cx + 22, cy,      cx + 70, cy,      yellowColor);

  display->drawLine(cx - 70, cy + 1,  cx - 22, cy + 1,  yellowColor);
  display->drawLine(cx + 22, cy + 1,  cx + 70, cy + 1,  yellowColor);

  display->drawCircle(cx, cy, 3, yellowColor);
  display->drawCircle(cx, cy, 4, yellowColor);
}

void drawSimpleRollPointer(
  Arduino_GFX *display,
  int cx,
  uint16_t colour
) {
  const int y = 8;

  display->fillTriangle(
    cx,      y + 14,
    cx - 8,  y,
    cx + 8,  y,
    colour
  );
}