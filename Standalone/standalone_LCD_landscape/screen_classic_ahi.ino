// ----------------------------------------------------
// Classic round AHI - landscape version
// ----------------------------------------------------
//
// Built-in Waveshare ESP32-S3-LCD-1.69 display only.
//
// Landscape canvas expected:
// - 280 x 240
//
// Features:
// - classic round attitude indicator
// - rotating horizon / pitch ladder
// - fixed aircraft symbol
// - fixed roll scale
// - moving bank pointer around roll arc
// ----------------------------------------------------

// ----------------------------------------------------
// Roll pointer direction
// ----------------------------------------------------
//
// If the moving roll pointer moves the wrong way,
// change this from 1.0f to -1.0f.

const float CLASSIC_ROLL_POINTER_SIGN = 1.0f;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void drawClassicAhiPanelBody(
  Arduino_Canvas *display,
  uint16_t panelGrey,
  uint16_t bezelDark
);

void drawClassicAhiHorizonFill(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
);

void drawClassicAhiPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t colour
);

void drawClassicAhiBoldText(
  Arduino_Canvas *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
);

void clearClassicAhiOutsideCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t backgroundColor
);

void drawClassicAhiBezel(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t bezelDark,
  uint16_t bezelGrey,
  uint16_t whiteColor
);

void drawClassicAhiRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor
);

void drawClassicAhiMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t pointerColor,
  uint16_t outlineColor
);

void drawClassicAhiAircraftSymbol(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t outlineColor
);

void drawClassicAhiScrews(Arduino_Canvas *display);
void drawClassicAhiScrew(Arduino_Canvas *display, int x, int y);
void drawClassicAhiBottomKnob(Arduino_Canvas *display, int cx);

void drawClassicAhiThickCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r,
  int thickness,
  uint16_t colour
);

// ----------------------------------------------------
// Main renderer
// ----------------------------------------------------

void drawClassicAhiScreen(Arduino_Canvas *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  uint16_t panelBlack   = rgb565(12, 12, 12);
  uint16_t panelGrey    = rgb565(42, 42, 42);
  uint16_t bezelDark    = rgb565(8, 8, 8);
  uint16_t bezelGrey    = rgb565(65, 65, 65);
  uint16_t skyColor     = rgb565(0, 85, 210);
  uint16_t groundColor  = rgb565(120, 65, 12);
  uint16_t whiteColor   = GFX_WHITE;
  uint16_t yellowColor  = rgb565(230, 220, 40);
  uint16_t blackColor   = GFX_BLACK;

  display->fillScreen(panelBlack);

  drawClassicAhiPanelBody(display, panelGrey, bezelDark);

  // Landscape 280 x 240 tuning.
  //
  // radius       = attitude ball radius
  // bezelRadius  = outer mechanical bezel
  // rollRadius   = roll scale / bank pointer radius
  const int cx = screenW / 2;
  const int cy = screenH / 2 + 2;

  const int radius = 82;
  const int bezelRadius = radius + 13;
  const int rollRadius = radius + 18;

  const float pixelsPerDeg = 2.5f;

  float rollRad = -roll;

  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;
  pitchOffset = constrain(pitchOffset, -screenH, screenH);

  // Attitude ball
  drawClassicAhiHorizonFill(
    display,
    cx,
    cy,
    radius,
    rollRad,
    pitchOffset,
    skyColor,
    groundColor
  );

  drawClassicAhiPitchLadder(
    display,
    cx,
    cy,
    radius,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    whiteColor
  );

  // Clean anything from the ladder that escaped the round ball
  clearClassicAhiOutsideCircle(display, cx, cy, radius, panelGrey);

  // Mechanical bezel and fixed scale
  drawClassicAhiBezel(
    display,
    cx,
    cy,
    bezelRadius,
    bezelDark,
    bezelGrey,
    whiteColor
  );

  drawClassicAhiRollScale(
    display,
    cx,
    cy,
    rollRadius,
    whiteColor
  );

  // Moving bank pointer
  drawClassicAhiMovingBankPointer(
    display,
    cx,
    cy,
    rollRadius,
    roll,
    whiteColor,
    blackColor
  );

  // Fixed aircraft reference
  drawClassicAhiAircraftSymbol(
    display,
    cx,
    cy,
    yellowColor,
    blackColor
  );

  drawClassicAhiScrews(display);
  drawClassicAhiBottomKnob(display, cx);
}

// ----------------------------------------------------
// Panel body
// ----------------------------------------------------

void drawClassicAhiPanelBody(
  Arduino_Canvas *display,
  uint16_t panelGrey,
  uint16_t bezelDark
) {
  int w = display->width();
  int h = display->height();

  display->fillScreen(panelGrey);

  // Slight vignette/frame
  display->drawRect(0, 0, w, h, bezelDark);
  display->drawRect(1, 1, w - 2, h - 2, bezelDark);
  display->drawRect(3, 3, w - 6, h - 6, rgb565(75, 75, 75));
}

// ----------------------------------------------------
// Horizon fill inside circular ball
// ----------------------------------------------------

void drawClassicAhiHorizonFill(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
) {
  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  for (int y = cy - radius; y <= cy + radius; y++) {
    int dy = y - cy;

    float inside = (radius * radius) - (dy * dy);

    if (inside < 0) {
      continue;
    }

    int dx = (int)sqrtf(inside);

    int rowX1 = cx - dx;
    int rowX2 = cx + dx;
    int rowW = rowX2 - rowX1 + 1;

    if (fabsf(nx) < 0.0001f) {
      float value = ny * (dy - pitchOffset);
      uint16_t colour = (value < 0) ? skyColor : groundColor;

      display->drawFastHLine(rowX1, y, rowW, colour);
    } else {
      float splitX = cx - (ny * (dy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= rowX1) {
          display->drawFastHLine(rowX1, y, rowW, groundColor);
        } else if (splitX >= rowX2) {
          display->drawFastHLine(rowX1, y, rowW, skyColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(rowX1, y, sx - rowX1 + 1, skyColor);
          display->drawFastHLine(sx + 1, y, rowX2 - sx, groundColor);
        }
      } else {
        if (splitX <= rowX1) {
          display->drawFastHLine(rowX1, y, rowW, skyColor);
        } else if (splitX >= rowX2) {
          display->drawFastHLine(rowX1, y, rowW, groundColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(rowX1, y, sx - rowX1 + 1, groundColor);
          display->drawFastHLine(sx + 1, y, rowX2 - sx, skyColor);
        }
      }
    }
  }
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

void drawClassicAhiPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t colour
) {
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

    int distX = mx - cx;
    int distY = my - cy;

    if ((distX * distX + distY * distY) > ((radius + 35) * (radius + 35))) {
      continue;
    }

    int halfLen = (deg % 10 == 0) ? 34 : 17;

    int x1 = mx - ux * halfLen;
    int y1 = my - uy * halfLen;
    int x2 = mx + ux * halfLen;
    int y2 = my + uy * halfLen;

    display->drawLine(x1, y1, x2, y2, colour);

    if (deg % 10 == 0) {
      char labelText[4];
      snprintf(labelText, sizeof(labelText), "%d", abs(deg));

      int labelW = strlen(labelText) * 6;
      int labelH = 8;

      int lx1 = x1 - labelW - 7;
      int ly1 = y1 - labelH / 2;

      int lx2 = x2 + 7;
      int ly2 = y2 - labelH / 2;

      drawClassicAhiBoldText(display, lx1, ly1, labelText, colour);
      drawClassicAhiBoldText(display, lx2, ly2, labelText, colour);
    }
  }

  // Horizon reference line emphasis
  int horizonOffset = pitchOffset;

  int mx = cx + nx * horizonOffset;
  int my = cy + ny * horizonOffset;

  int x1 = mx - ux * 44;
  int y1 = my - uy * 44;
  int x2 = mx + ux * 44;
  int y2 = my + uy * 44;

  display->drawLine(x1, y1, x2, y2, colour);
  display->drawLine(x1, y1 + 1, x2, y2 + 1, colour);
}

void drawClassicAhiBoldText(
  Arduino_Canvas *display,
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

// ----------------------------------------------------
// Circular clipping cleanup
// ----------------------------------------------------

void clearClassicAhiOutsideCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t backgroundColor
) {
  int screenW = display->width();
  int screenH = display->height();

  // Top/bottom bands
  display->fillRect(0, 0, screenW, cy - radius, backgroundColor);
  display->fillRect(0, cy + radius + 1, screenW, screenH - (cy + radius + 1), backgroundColor);

  // Left/right outside circular row
  for (int y = cy - radius; y <= cy + radius; y++) {
    int dy = y - cy;

    float inside = (radius * radius) - (dy * dy);

    if (inside < 0) {
      continue;
    }

    int dx = (int)sqrtf(inside);

    int leftEnd = cx - dx - 1;
    int rightStart = cx + dx + 1;

    if (leftEnd >= 0) {
      display->drawFastHLine(0, y, leftEnd + 1, backgroundColor);
    }

    if (rightStart < screenW) {
      display->drawFastHLine(rightStart, y, screenW - rightStart, backgroundColor);
    }
  }
}

// ----------------------------------------------------
// Bezel
// ----------------------------------------------------

void drawClassicAhiBezel(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t bezelDark,
  uint16_t bezelGrey,
  uint16_t whiteColor
) {
  // Outer dark rim
  drawClassicAhiThickCircle(display, cx, cy, radius + 6, 5, bezelDark);

  // Main grey bezel ring
  drawClassicAhiThickCircle(display, cx, cy, radius + 1, 7, bezelGrey);

  // White highlight rings
  display->drawCircle(cx, cy, radius + 4, whiteColor);
  display->drawCircle(cx, cy, radius - 6, whiteColor);

  // Inner black separator
  display->drawCircle(cx, cy, radius - 1, bezelDark);
}

// ----------------------------------------------------
// Fixed roll scale
// ----------------------------------------------------

void drawClassicAhiRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor
) {
  for (int deg = -60; deg <= 60; deg += 10) {
    float a = deg * DEG_TO_RAD;

    int outerR = radius;
    int innerR;

    if (deg == 0) {
      innerR = radius - 17;
    } else if (deg % 30 == 0) {
      innerR = radius - 14;
    } else {
      innerR = radius - 8;
    }

    int x1 = cx + sinf(a) * outerR;
    int y1 = cy - cosf(a) * outerR;
    int x2 = cx + sinf(a) * innerR;
    int y2 = cy - cosf(a) * innerR;

    display->drawLine(x1, y1, x2, y2, whiteColor);
    display->drawLine(x1 + 1, y1, x2 + 1, y2, whiteColor);
  }

  // Fixed zero-bank reference mark
  display->fillTriangle(
    cx,
    cy - radius - 1,
    cx - 7,
    cy - radius + 12,
    cx + 7,
    cy - radius + 12,
    whiteColor
  );
}

// ----------------------------------------------------
// Moving bank pointer
// ----------------------------------------------------

void drawClassicAhiMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t pointerColor,
  uint16_t outlineColor
) {
  float bankDeg = roll * RAD_TO_DEG * CLASSIC_ROLL_POINTER_SIGN;

  bankDeg = constrain(bankDeg, -60.0f, 60.0f);

  float a = bankDeg * DEG_TO_RAD;

  float rx = sinf(a);
  float ry = -cosf(a);

  float tx = cosf(a);
  float ty = sinf(a);

  // Triangle points inward toward centre.
  int outerR = radius - 4;
  int innerR = radius - 20;
  int halfW = 7;

  int tipX = cx + rx * innerR;
  int tipY = cy + ry * innerR;

  int baseCX = cx + rx * outerR;
  int baseCY = cy + ry * outerR;

  int base1X = baseCX - tx * halfW;
  int base1Y = baseCY - ty * halfW;

  int base2X = baseCX + tx * halfW;
  int base2Y = baseCY + ty * halfW;

  // Black outline
  display->fillTriangle(
    tipX,
    tipY,
    base1X,
    base1Y,
    base2X,
    base2Y,
    outlineColor
  );

  // White inner pointer
  int yOuterR = radius - 6;
  int yInnerR = radius - 19;
  int yHalfW = 5;

  int yTipX = cx + rx * yInnerR;
  int yTipY = cy + ry * yInnerR;

  int yBaseCX = cx + rx * yOuterR;
  int yBaseCY = cy + ry * yOuterR;

  int yBase1X = yBaseCX - tx * yHalfW;
  int yBase1Y = yBaseCY - ty * yHalfW;

  int yBase2X = yBaseCX + tx * yHalfW;
  int yBase2Y = yBaseCY + ty * yHalfW;

  display->fillTriangle(
    yTipX,
    yTipY,
    yBase1X,
    yBase1Y,
    yBase2X,
    yBase2Y,
    pointerColor
  );
}

// ----------------------------------------------------
// Aircraft symbol
// ----------------------------------------------------

void drawClassicAhiAircraftSymbol(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t outlineColor
) {
  // Black outline / shadow
  display->drawLine(cx - 70, cy,      cx - 24, cy,      outlineColor);
  display->drawLine(cx + 24, cy,      cx + 70, cy,      outlineColor);
  display->drawLine(cx - 24, cy,      cx,      cy + 10, outlineColor);
  display->drawLine(cx,      cy + 10, cx + 24, cy,      outlineColor);

  display->drawLine(cx - 70, cy + 1,  cx - 24, cy + 1,  outlineColor);
  display->drawLine(cx + 24, cy + 1,  cx + 70, cy + 1,  outlineColor);

  // Yellow wings
  display->drawLine(cx - 70, cy,      cx - 24, cy,      yellowColor);
  display->drawLine(cx - 24, cy,      cx,      cy + 10, yellowColor);
  display->drawLine(cx,      cy + 10, cx + 24, cy,      yellowColor);
  display->drawLine(cx + 24, cy,      cx + 70, cy,      yellowColor);

  display->drawLine(cx - 70, cy + 1,  cx - 24, cy + 1,  yellowColor);
  display->drawLine(cx + 24, cy + 1,  cx + 70, cy + 1,  yellowColor);

  display->drawLine(cx - 70, cy + 2,  cx - 24, cy + 2,  yellowColor);
  display->drawLine(cx + 24, cy + 2,  cx + 70, cy + 2,  yellowColor);

  // Centre hub
  display->fillCircle(cx, cy + 1, 4, yellowColor);
  display->drawCircle(cx, cy + 1, 6, yellowColor);
}

// ----------------------------------------------------
// Screws / knob
// ----------------------------------------------------

void drawClassicAhiScrews(Arduino_Canvas *display)
{
  int w = display->width();
  int h = display->height();

  drawClassicAhiScrew(display, 18, 18);
  drawClassicAhiScrew(display, w - 18, 18);
  drawClassicAhiScrew(display, 18, h - 18);
  drawClassicAhiScrew(display, w - 18, h - 18);
}

void drawClassicAhiScrew(
  Arduino_Canvas *display,
  int x,
  int y
) {
  uint16_t screwGrey = rgb565(95, 95, 95);
  uint16_t screwDark = rgb565(20, 20, 20);

  display->fillCircle(x, y, 5, screwGrey);
  display->drawCircle(x, y, 5, GFX_WHITE);
  display->drawLine(x - 3, y + 3, x + 3, y - 3, screwDark);
}

void drawClassicAhiBottomKnob(
  Arduino_Canvas *display,
  int cx
) {
  int h = display->height();

  uint16_t knobGrey = rgb565(70, 70, 70);
  uint16_t knobDark = rgb565(15, 15, 15);

  int knobY = h - 16;

  display->fillRoundRect(cx - 22, knobY - 7, 44, 14, 4, knobDark);
  display->fillRoundRect(cx - 18, knobY - 5, 36, 10, 4, knobGrey);
  display->drawRect(cx - 18, knobY - 5, 36, 10, GFX_WHITE);
}

// ----------------------------------------------------
// Utility drawing
// ----------------------------------------------------

void drawClassicAhiThickCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int r,
  int thickness,
  uint16_t colour
) {
  for (int i = 0; i < thickness; i++) {
    display->drawCircle(cx, cy, r - i, colour);
  }
}