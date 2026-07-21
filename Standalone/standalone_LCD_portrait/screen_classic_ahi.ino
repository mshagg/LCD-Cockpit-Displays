// ----------------------------------------------------
// Classic round AHI screen
// ----------------------------------------------------
//
// This is a stylised round attitude indicator inspired by
// traditional cockpit AHIs.
//
// It uses the same global roll/pitch source as the full-screen AHI.
// The main loop owns canvas->flush().
// ----------------------------------------------------

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void drawClassicAhiPanelBody(Arduino_Canvas *display, uint16_t panelGrey, uint16_t bezelDark);

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
  uint16_t whiteColor,
  uint16_t yellowColor
);

void drawClassicAhiAircraftSymbol(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t outlineColor
);

void drawClassicAhiOffFlag(Arduino_Canvas *display);
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

bool pointInsideClassicAhiCircle(
  int cx,
  int cy,
  int x,
  int y,
  int radius
);

// ----------------------------------------------------
// Main classic AHI renderer
// ----------------------------------------------------

void drawClassicAhiScreen(Arduino_Canvas *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  const int cx = screenW / 2;
  const int cy = 137;

  const int faceRadius = 101;
  const float pixelsPerDeg = 2.8f;

  uint16_t panelDark   = rgb565(32, 32, 28);
  uint16_t panelGrey   = rgb565(72, 70, 62);
  uint16_t bezelDark   = rgb565(18, 18, 18);
  uint16_t bezelGrey   = rgb565(95, 92, 82);

  uint16_t skyColor    = rgb565(55, 175, 235);
  uint16_t groundColor = rgb565(130, 85, 45);

  uint16_t yellowColor = rgb565(245, 220, 35);
  uint16_t whiteColor  = GFX_WHITE;
  uint16_t blackColor  = GFX_BLACK;

  float rollRad = -roll;

  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;
  pitchOffset = constrain(pitchOffset, -screenH, screenH);

  // Base panel
  display->fillScreen(panelDark);

  // Outer square-ish instrument body
  drawClassicAhiPanelBody(display, panelGrey, bezelDark);

  // Moving attitude ball
  drawClassicAhiHorizonFill(
    display,
    cx,
    cy,
    faceRadius,
    rollRad,
    pitchOffset,
    skyColor,
    groundColor
  );

  drawClassicAhiPitchLadder(
    display,
    cx,
    cy,
    faceRadius,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    whiteColor
  );

  // Hide anything outside the round face
  clearClassicAhiOutsideCircle(
    display,
    cx,
    cy,
    faceRadius,
    panelDark
  );

  // Fixed instrument features
  drawClassicAhiBezel(display, cx, cy, faceRadius, bezelDark, bezelGrey, whiteColor);
  drawClassicAhiRollScale(display, cx, cy, faceRadius, whiteColor, yellowColor);
  drawClassicAhiAircraftSymbol(display, cx, cy, yellowColor, blackColor);
  drawClassicAhiOffFlag(display);
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
  // Main dark-grey instrument block
  display->fillRect(5, 18, 230, 242, panelGrey);

  // Chamfered black corner impression
  display->fillTriangle(5, 18, 35, 18, 5, 48, bezelDark);
  display->fillTriangle(235, 18, 205, 18, 235, 48, bezelDark);
  display->fillTriangle(5, 260, 35, 260, 5, 230, bezelDark);
  display->fillTriangle(235, 260, 205, 260, 235, 230, bezelDark);

  // Inner dark recess
  display->fillCircle(120, 137, 118, bezelDark);
  display->fillCircle(120, 137, 111, panelGrey);
}

// ----------------------------------------------------
// Moving horizon ball
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

  for (int dy = -radius; dy <= radius; dy++) {
    int xSpan = sqrtf((radius * radius) - (dy * dy));

    int xStart = cx - xSpan;
    int xEnd   = cx + xSpan;
    int y      = cy + dy;

    if (fabsf(nx) < 0.0001f) {
      float value = ny * (dy - pitchOffset);
      uint16_t colour = (value < 0) ? skyColor : groundColor;

      display->drawFastHLine(xStart, y, xEnd - xStart + 1, colour);
    } else {
      float splitX = cx - (ny * (dy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= xStart) {
          display->drawFastHLine(xStart, y, xEnd - xStart + 1, groundColor);
        } else if (splitX >= xEnd) {
          display->drawFastHLine(xStart, y, xEnd - xStart + 1, skyColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(xStart, y, sx - xStart + 1, skyColor);
          display->drawFastHLine(sx + 1, y, xEnd - sx, groundColor);
        }
      } else {
        if (splitX <= xStart) {
          display->drawFastHLine(xStart, y, xEnd - xStart + 1, skyColor);
        } else if (splitX >= xEnd) {
          display->drawFastHLine(xStart, y, xEnd - xStart + 1, groundColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(xStart, y, sx - xStart + 1, groundColor);
          display->drawFastHLine(sx + 1, y, xEnd - sx, skyColor);
        }
      }
    }
  }

  // Horizon separator line
  int lineLen = radius + 30;

  int x1 = cx - ux * lineLen + nx * pitchOffset;
  int y1 = cy - uy * lineLen + ny * pitchOffset;
  int x2 = cx + ux * lineLen + nx * pitchOffset;
  int y2 = cy + uy * lineLen + ny * pitchOffset;

  display->drawLine(x1, y1, x2, y2, GFX_WHITE);
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

  for (int deg = -30; deg <= 30; deg += 5) {
    if (deg == 0) {
      continue;
    }

    float lineOffset = pitchOffset - deg * pixelsPerDeg;

    if (lineOffset < -(radius - 8) || lineOffset > (radius - 8)) {
      continue;
    }

    int requestedHalfLen = (deg % 10 == 0) ? 34 : 18;
    float availableHalfLen = sqrtf((radius - 8) * (radius - 8) - lineOffset * lineOffset);

    if (availableHalfLen <= 5) {
      continue;
    }

    int halfLen = min(requestedHalfLen, (int)availableHalfLen);

    int mx = cx + nx * lineOffset;
    int my = cy + ny * lineOffset;

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

      int lx1 = x1 - labelW - 8;
      int ly1 = y1 - labelH / 2;

      int lx2 = x2 + 8;
      int ly2 = y2 - labelH / 2;

      if (pointInsideClassicAhiCircle(cx, cy, lx1, ly1, radius - 10)) {
        drawClassicAhiBoldText(display, lx1, ly1, labelText, colour);
      }

      if (pointInsideClassicAhiCircle(cx, cy, lx2 + labelW, ly2, radius - 10)) {
        drawClassicAhiBoldText(display, lx2, ly2, labelText, colour);
      }
    }
  }
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
// Circular clipping/masking
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

  for (int y = 0; y < screenH; y++) {
    int dy = y - cy;

    if (abs(dy) > radius) {
      display->drawFastHLine(0, y, screenW, backgroundColor);
    } else {
      int xSpan = sqrtf((radius * radius) - (dy * dy));

      int circleLeft  = cx - xSpan;
      int circleRight = cx + xSpan;

      if (circleLeft > 0) {
        display->drawFastHLine(0, y, circleLeft, backgroundColor);
      }

      if (circleRight < screenW - 1) {
        display->drawFastHLine(circleRight + 1, y, screenW - circleRight - 1, backgroundColor);
      }
    }
  }
}

// ----------------------------------------------------
// Bezel / roll scale
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
  // Inner black rim
  drawClassicAhiThickCircle(display, cx, cy, radius + 1, 3, bezelDark);

  // Grey ring
  drawClassicAhiThickCircle(display, cx, cy, radius + 5, 3, bezelGrey);

  // Outer dark ring
  drawClassicAhiThickCircle(display, cx, cy, radius + 9, 4, bezelDark);

  // Thin white inner reference ring
  display->drawCircle(cx, cy, radius, whiteColor);
}

void drawClassicAhiRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor,
  uint16_t yellowColor
) {
  // Fixed roll tick marks across the upper arc
  for (int deg = -60; deg <= 60; deg += 10) {
    float a = deg * DEG_TO_RAD;

    int outerR = radius - 7;
    int innerR = (deg % 30 == 0) ? radius - 24 : radius - 16;

    int x1 = cx + sinf(a) * outerR;
    int y1 = cy - cosf(a) * outerR;
    int x2 = cx + sinf(a) * innerR;
    int y2 = cy - cosf(a) * innerR;

    display->drawLine(x1, y1, x2, y2, whiteColor);
    display->drawLine(x1 + 1, y1, x2 + 1, y2, whiteColor);
  }

  // Top fixed white triangle
  display->fillTriangle(
    cx,      cy - radius + 15,
    cx - 12, cy - radius - 14,
    cx + 12, cy - radius - 14,
    whiteColor
  );

  // Yellow reference triangle below it - slightly thicker
  int tx1 = cx;
  int ty1 = cy - radius + 24;
  int tx2 = cx - 12;
  int ty2 = cy - radius + 50;
  int tx3 = cx + 12;
  int ty3 = cy - radius + 50;

  display->drawTriangle(tx1, ty1, tx2, ty2, tx3, ty3, yellowColor);
  display->drawTriangle(tx1, ty1 + 1, tx2, ty2 + 1, tx3, ty3 + 1, yellowColor);
  display->drawTriangle(tx1 - 1, ty1, tx2 - 1, ty2, tx3 - 1, ty3, yellowColor);
  display->drawTriangle(tx1 + 1, ty1, tx2 + 1, ty2, tx3 + 1, ty3, yellowColor);
}

// ----------------------------------------------------
// Fixed yellow aircraft symbol
// ----------------------------------------------------

void drawClassicAhiAircraftSymbol(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t outlineColor
) {
  // Black outline / shadow
  display->drawLine(cx - 58, cy,      cx - 18, cy,      outlineColor);
  display->drawLine(cx - 58, cy + 1,  cx - 18, cy + 1,  outlineColor);

  display->drawLine(cx + 18, cy,      cx + 58, cy,      outlineColor);
  display->drawLine(cx + 18, cy + 1,  cx + 58, cy + 1,  outlineColor);

  display->drawLine(cx - 18, cy,      cx - 11, cy + 20, outlineColor);
  display->drawLine(cx - 17, cy,      cx - 10, cy + 20, outlineColor);

  display->drawLine(cx + 18, cy,      cx + 11, cy + 20, outlineColor);
  display->drawLine(cx + 17, cy,      cx + 10, cy + 20, outlineColor);

  display->drawLine(cx - 11, cy + 20, cx,      cy + 27, outlineColor);
  display->drawLine(cx - 10, cy + 20, cx + 1,  cy + 27, outlineColor);

  display->drawLine(cx,      cy + 27, cx + 11, cy + 20, outlineColor);
  display->drawLine(cx - 1,  cy + 27, cx + 10, cy + 20, outlineColor);

  // Yellow wings - slightly thicker
  display->drawLine(cx - 58, cy,      cx - 18, cy,      yellowColor);
  display->drawLine(cx - 58, cy + 1,  cx - 18, cy + 1,  yellowColor);
  display->drawLine(cx - 58, cy + 2,  cx - 18, cy + 2,  yellowColor);

  display->drawLine(cx + 18, cy,      cx + 58, cy,      yellowColor);
  display->drawLine(cx + 18, cy + 1,  cx + 58, cy + 1,  yellowColor);
  display->drawLine(cx + 18, cy + 2,  cx + 58, cy + 2,  yellowColor);

  // Yellow lower U / reference shape - slightly thicker
  display->drawLine(cx - 18, cy,      cx - 11, cy + 20, yellowColor);
  display->drawLine(cx - 17, cy,      cx - 10, cy + 20, yellowColor);

  display->drawLine(cx + 18, cy,      cx + 11, cy + 20, yellowColor);
  display->drawLine(cx + 17, cy,      cx + 10, cy + 20, yellowColor);

  display->drawLine(cx - 11, cy + 20, cx,      cy + 27, yellowColor);
  display->drawLine(cx - 10, cy + 20, cx + 1,  cy + 27, yellowColor);

  display->drawLine(cx,      cy + 27, cx + 11, cy + 20, yellowColor);
  display->drawLine(cx - 1,  cy + 27, cx + 10, cy + 20, yellowColor);

  // Centre dot slightly bolder
  display->fillCircle(cx, cy, 5, yellowColor);
  display->drawCircle(cx, cy, 6, yellowColor);
}

// ----------------------------------------------------
// OFF flag
// ----------------------------------------------------

void drawClassicAhiOffFlag(Arduino_Canvas *display)
{
  uint16_t redColor = rgb565(235, 55, 75);
  uint16_t whiteColor = GFX_WHITE;
  uint16_t blackColor = GFX_BLACK;

  const int x = 204;
  const int y = 58;
  const int w = 26;
  const int h = 56;

  display->fillRect(x, y, w, h, whiteColor);
  display->fillRect(x + 3, y + 3, w - 6, h - 6, redColor);
  display->drawRect(x, y, w, h, blackColor);

  display->setTextColor(blackColor);
  display->setTextSize(1);

  display->setCursor(x + 8, y + 8);
  display->print("O");

  display->setCursor(x + 8, y + 24);
  display->print("F");

  display->setCursor(x + 8, y + 40);
  display->print("F");
}

// ----------------------------------------------------
// Screws / knob
// ----------------------------------------------------

void drawClassicAhiScrews(Arduino_Canvas *display)
{
  drawClassicAhiScrew(display, 25, 38);
  drawClassicAhiScrew(display, 215, 38);
  drawClassicAhiScrew(display, 25, 238);
  drawClassicAhiScrew(display, 215, 238);
}

void drawClassicAhiScrew(
  Arduino_Canvas *display,
  int x,
  int y
) {
  uint16_t metal = rgb565(150, 150, 140);
  uint16_t dark  = rgb565(25, 25, 25);

  display->fillCircle(x, y, 10, metal);
  display->fillCircle(x, y, 7, dark);
  display->fillCircle(x, y, 5, metal);

  display->drawLine(x - 5, y, x + 5, y, dark);
  display->drawLine(x, y - 5, x, y + 5, dark);
}

void drawClassicAhiBottomKnob(
  Arduino_Canvas *display,
  int cx
) {
  uint16_t knobGrey = rgb565(80, 78, 70);
  uint16_t knobDark = rgb565(30, 30, 28);

  display->fillCircle(cx, 252, 18, knobDark);
  display->fillCircle(cx, 250, 15, knobGrey);
}

// ----------------------------------------------------
// Helpers
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
    display->drawCircle(cx, cy, r + i, colour);
  }
}

bool pointInsideClassicAhiCircle(
  int cx,
  int cy,
  int x,
  int y,
  int radius
) {
  int dx = x - cx;
  int dy = y - cy;

  return (dx * dx + dy * dy) < (radius * radius);
}