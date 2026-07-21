// ----------------------------------------------------
// Full-screen futuristic Tron-style portrait AHI
// ----------------------------------------------------
//
// Portrait canvas expected:
// - 240 x 280
//
// This is a separate selectable screen from screen_tron_ahi.ino.
//
// Uses only:
// - roll
// - pitch
//
// No airspeed / altitude / heading / GPS data.
// No title at the top.
// ----------------------------------------------------

// If the moving bank pointer moves the wrong way,
// change this from 1.0f to -1.0f.
const float TRON_FULL_ROLL_POINTER_SIGN = 1.0f;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void drawTronFullBackground(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t skyBase,
  uint16_t groundBase
);

void drawTronFullCornerFrame(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t cyanVeryDim
);

void drawTronFullGrid(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t skyGrid,
  uint16_t groundGrid
);

void drawTronFullPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t upperColour,
  uint16_t lowerColour
);

void drawTronFullHorizonLine(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t glowColour,
  uint16_t lineColour
);

void drawTronFullRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanDim,
  uint16_t cyanBright
);

void drawTronFullFixedZeroReference(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronFullMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t orangeBright,
  uint16_t panelBlack
);

void drawTronFullAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronFullSideMarkers(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t orangeBright,
  uint16_t panelBlack
);

void drawTronFullBottomDecor(
  Arduino_Canvas *display,
  uint16_t cyanVeryDim,
  uint16_t cyanDim,
  uint16_t cyanBright
);

void drawTronFullBoldText(
  Arduino_Canvas *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
);

void drawTronFullThickLine(
  Arduino_Canvas *display,
  int x1,
  int y1,
  int x2,
  int y2,
  int thickness,
  uint16_t colour
);

void drawTronFullRotatedInfiniteLineClipped(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float ux,
  float uy,
  float nx,
  float ny,
  float offset,
  uint16_t colour
);

bool tronFullLineSegmentWithinScreen(
  int x1,
  int y1,
  int x2,
  int y2,
  int w,
  int h
);

// ----------------------------------------------------
// Main renderer
// ----------------------------------------------------

void drawTronFullAhiScreen(Arduino_Canvas *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  const int cx = screenW / 2;
  const int cy = screenH / 2 + 6;

  const float pixelsPerDeg = 3.1f;

  float rollRad = -roll;
  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;

  uint16_t panelBlack   = rgb565(0, 4, 10);

  uint16_t cyanVeryDim  = rgb565(0, 28, 46);
  uint16_t cyanDim      = rgb565(0, 92, 140);
  uint16_t cyanMid      = rgb565(0, 170, 225);
  uint16_t cyanBright   = rgb565(80, 238, 255);

  uint16_t skyBase      = rgb565(0, 42, 128);
  uint16_t groundBase   = rgb565(30, 14, 0);

  uint16_t skyGrid      = rgb565(0, 78, 130);
  uint16_t groundGrid   = rgb565(115, 60, 0);

  uint16_t orangeBright = rgb565(255, 175, 35);

  display->fillScreen(GFX_BLACK);

  // Full-screen attitude field
  drawTronFullBackground(
    display,
    cx,
    cy,
    rollRad,
    pitchOffset,
    skyBase,
    groundBase
  );

  // Decorative grid
  drawTronFullGrid(
    display,
    cx,
    cy,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    skyGrid,
    groundGrid
  );

  // Pitch ladder
  drawTronFullPitchLadder(
    display,
    cx,
    cy,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    cyanBright,
    orangeBright
  );

  // Main horizon line
  drawTronFullHorizonLine(
    display,
    cx,
    cy,
    rollRad,
    pitchOffset,
    cyanMid,
    cyanBright
  );

  // Top roll scale
  const int rollCx = cx;
  const int rollCy = 88;
  const int rollRadius = 78;

  drawTronFullRollScale(
    display,
    rollCx,
    rollCy,
    rollRadius,
    cyanDim,
    cyanBright
  );

  drawTronFullFixedZeroReference(
    display,
    rollCx,
    rollCy,
    rollRadius,
    cyanBright,
    panelBlack
  );

  drawTronFullMovingBankPointer(
    display,
    rollCx,
    rollCy,
    rollRadius,
    roll,
    orangeBright,
    panelBlack
  );

  // Side horizon markers
  drawTronFullSideMarkers(
    display,
    cx,
    cy,
    orangeBright,
    panelBlack
  );

  // Aircraft cue
  drawTronFullAircraftCue(
    display,
    cx,
    cy,
    cyanDim,
    cyanBright,
    panelBlack
  );

  // Frame and bottom ornament last so they stay crisp
  drawTronFullCornerFrame(display, cyanDim, cyanBright, cyanVeryDim);
  drawTronFullBottomDecor(display, cyanVeryDim, cyanDim, cyanBright);
}

// ----------------------------------------------------
// Full-screen sky / ground fill
// ----------------------------------------------------

void drawTronFullBackground(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t skyBase,
  uint16_t groundBase
) {
  const int w = display->width();
  const int h = display->height();

  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  for (int y = 0; y < h; y++) {
    int skyBoost = map(y, 0, h - 1, 18, -12);
    int groundBoost = map(y, 0, h - 1, -8, 20);

    uint16_t skyShade = rgb565(
      0,
      constrain(42 + skyBoost, 0, 255),
      constrain(128 + skyBoost * 2, 0, 255)
    );

    uint16_t groundShade = rgb565(
      constrain(30 + groundBoost / 2, 0, 255),
      constrain(14 + groundBoost / 3, 0, 255),
      0
    );

    if (fabsf(nx) < 0.0001f) {
      float value = ny * ((float)y - cy - pitchOffset);
      uint16_t colour = (value < 0) ? skyShade : groundShade;
      display->drawFastHLine(0, y, w, colour);
    } else {
      float splitX = cx - (ny * ((float)y - cy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= 0) {
          display->drawFastHLine(0, y, w, groundShade);
        } else if (splitX >= (w - 1)) {
          display->drawFastHLine(0, y, w, skyShade);
        } else {
          int sx = (int)splitX;
          display->drawFastHLine(0, y, sx + 1, skyShade);
          display->drawFastHLine(sx + 1, y, w - (sx + 1), groundShade);
        }
      } else {
        if (splitX <= 0) {
          display->drawFastHLine(0, y, w, skyShade);
        } else if (splitX >= (w - 1)) {
          display->drawFastHLine(0, y, w, groundShade);
        } else {
          int sx = (int)splitX;
          display->drawFastHLine(0, y, sx + 1, groundShade);
          display->drawFastHLine(sx + 1, y, w - (sx + 1), skyShade);
        }
      }
    }
  }
}

// ----------------------------------------------------
// Decorative frame
// ----------------------------------------------------

void drawTronFullCornerFrame(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t cyanVeryDim
) {
  const int w = display->width();
  const int h = display->height();

  // Top left
  display->drawLine(8, 8, 42, 8, cyanBright);
  display->drawLine(8, 8, 8, 36, cyanBright);
  display->drawLine(8, 20, 24, 20, cyanDim);
  display->drawLine(20, 8, 20, 24, cyanDim);

  // Top right
  display->drawLine(w - 9, 8, w - 43, 8, cyanBright);
  display->drawLine(w - 9, 8, w - 9, 36, cyanBright);
  display->drawLine(w - 9, 20, w - 25, 20, cyanDim);
  display->drawLine(w - 21, 8, w - 21, 24, cyanDim);

  // Bottom left
  display->drawLine(8, h - 9, 42, h - 9, cyanBright);
  display->drawLine(8, h - 9, 8, h - 37, cyanBright);
  display->drawLine(8, h - 21, 24, h - 21, cyanDim);
  display->drawLine(20, h - 9, 20, h - 25, cyanDim);

  // Bottom right
  display->drawLine(w - 9, h - 9, w - 43, h - 9, cyanBright);
  display->drawLine(w - 9, h - 9, w - 9, h - 37, cyanBright);
  display->drawLine(w - 9, h - 21, w - 25, h - 21, cyanDim);
  display->drawLine(w - 21, h - 9, w - 21, h - 25, cyanDim);

  // Side rails
  display->drawLine(14, 58, 14, h - 58, cyanVeryDim);
  display->drawLine(18, 72, 18, h - 72, cyanDim);
  display->drawLine(w - 15, 58, w - 15, h - 58, cyanVeryDim);
  display->drawLine(w - 19, 72, w - 19, h - 72, cyanDim);
}

// ----------------------------------------------------
// Grid
// ----------------------------------------------------

void drawTronFullGrid(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t skyGrid,
  uint16_t groundGrid
) {
  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  // Sky horizontal grid lines
  for (int deg = 10; deg <= 40; deg += 10) {
    float offset = pitchOffset - deg * pixelsPerDeg;

    drawTronFullRotatedInfiniteLineClipped(
      display,
      cx,
      cy,
      ux,
      uy,
      nx,
      ny,
      offset,
      skyGrid
    );
  }

  // Ground horizontal grid lines
  for (int deg = -10; deg >= -40; deg -= 10) {
    float offset = pitchOffset - deg * pixelsPerDeg;

    drawTronFullRotatedInfiniteLineClipped(
      display,
      cx,
      cy,
      ux,
      uy,
      nx,
      ny,
      offset,
      groundGrid
    );
  }

  // Vertical sky grid
  for (int d = -50; d <= 50; d += 25) {
    if (d == 0) {
      continue;
    }

    int x1 = cx + nx * (d * 1.5f) - ux * 220;
    int y1 = cy + ny * (d * 1.5f) - uy * 220 - pitchOffset;

    int x2 = cx + nx * (d * 1.5f) + ux * 220;
    int y2 = cy + ny * (d * 1.5f) + uy * 220 - pitchOffset;

    display->drawLine(x1, y1, x2, y2, skyGrid);
  }

  // Ground perspective fan
  for (int x = -120; x <= 120; x += 24) {
    int x1 = cx + x;
    int y1 = cy + 18;

    int x2 = cx + x * 2;
    int y2 = display->height() - 1;

    display->drawLine(x1, y1, x2, y2, groundGrid);
  }
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

void drawTronFullPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t upperColour,
  uint16_t lowerColour
) {
  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  display->setTextSize(1);

  for (int deg = -30; deg <= 30; deg += 5) {
    if (deg == 0) {
      continue;
    }

    float offset = pitchOffset - deg * pixelsPerDeg;

    int mx = cx + nx * offset;
    int my = cy + ny * offset;

    uint16_t colour = (deg > 0) ? upperColour : lowerColour;

    int halfLen = (deg % 10 == 0) ? 48 : 20;

    int x1 = mx - ux * halfLen;
    int y1 = my - uy * halfLen;

    int x2 = mx + ux * halfLen;
    int y2 = my + uy * halfLen;

    // Avoid drawing labels/lines too close to top roll scale or bottom decor.
    if (!tronFullLineSegmentWithinScreen(x1, y1, x2, y2, display->width(), display->height())) {
      continue;
    }

    if (my < 40 || my > display->height() - 28) {
      continue;
    }

    drawTronFullThickLine(display, x1, y1, x2, y2, 1, colour);

    if (deg % 10 == 0) {
      display->fillCircle(x1, y1, 2, colour);
      display->fillCircle(x2, y2, 2, colour);

      char labelText[5];
      snprintf(labelText, sizeof(labelText), "%d", deg);

      int labelW = strlen(labelText) * 6;

      int lx1 = x1 - labelW - 10;
      int ly1 = y1 - 4;

      int lx2 = x2 + 10;
      int ly2 = y2 - 4;

      if (lx1 >= 0 && ly1 >= 0 && ly1 < display->height() - 8) {
        drawTronFullBoldText(display, lx1, ly1, labelText, colour);
      }

      if (lx2 < display->width() - labelW && ly2 >= 0 && ly2 < display->height() - 8) {
        drawTronFullBoldText(display, lx2, ly2, labelText, colour);
      }
    }
  }
}

// ----------------------------------------------------
// Main horizon line
// ----------------------------------------------------

void drawTronFullHorizonLine(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t glowColour,
  uint16_t lineColour
) {
  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  int mx = cx + nx * pitchOffset;
  int my = cy + ny * pitchOffset;

  int halfLen = 190;

  int x1 = mx - ux * halfLen;
  int y1 = my - uy * halfLen;

  int x2 = mx + ux * halfLen;
  int y2 = my + uy * halfLen;

  drawTronFullThickLine(display, x1, y1, x2, y2, 5, glowColour);
  drawTronFullThickLine(display, x1, y1, x2, y2, 2, lineColour);
}

// ----------------------------------------------------
// Roll scale
// ----------------------------------------------------

void drawTronFullRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanDim,
  uint16_t cyanBright
) {
  for (int deg = -90; deg <= 90; deg += 5) {
    float a = deg * DEG_TO_RAD;

    int outerR = radius;
    int innerR;

    if (deg % 30 == 0) {
      innerR = radius - 14;
    } else if (deg % 10 == 0) {
      innerR = radius - 10;
    } else {
      innerR = radius - 6;
    }

    uint16_t colour = (deg % 30 == 0) ? cyanBright : cyanDim;

    int x1 = cx + sinf(a) * outerR;
    int y1 = cy - cosf(a) * outerR;

    int x2 = cx + sinf(a) * innerR;
    int y2 = cy - cosf(a) * innerR;

    display->drawLine(x1, y1, x2, y2, colour);

    if (deg % 30 == 0) {
      display->drawLine(x1 + 1, y1, x2 + 1, y2, colour);
    }
  }

  // Roll labels
  const int labelR = radius - 26;

  for (int deg = -60; deg <= 60; deg += 30) {
    float a = deg * DEG_TO_RAD;

    char label[5];
    snprintf(label, sizeof(label), "%d", deg);

    int labelW = strlen(label) * 6;

    int lx = cx + sinf(a) * labelR - labelW / 2;
    int ly = cy - cosf(a) * labelR - 4;

    drawTronFullBoldText(display, lx, ly, label, cyanBright);
  }

  drawTronFullBoldText(display, cx - radius + 8, cy - 4, "-90", cyanBright);
  drawTronFullBoldText(display, cx + radius - 28, cy - 4, "90", cyanBright);
}

// ----------------------------------------------------
// Fixed top reference
// ----------------------------------------------------

void drawTronFullFixedZeroReference(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanBright,
  uint16_t panelBlack
) {
  int tipX = cx;
  int tipY = cy - radius + 4;

  int baseY = tipY + 13;
  int halfW = 8;

  display->fillTriangle(
    tipX,
    tipY,
    tipX - halfW,
    baseY,
    tipX + halfW,
    baseY,
    cyanBright
  );

  display->fillTriangle(
    tipX,
    tipY + 4,
    tipX - 4,
    baseY - 2,
    tipX + 4,
    baseY - 2,
    panelBlack
  );
}

// ----------------------------------------------------
// Moving bank pointer
// ----------------------------------------------------

void drawTronFullMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t orangeBright,
  uint16_t panelBlack
) {
  float bankDeg = roll * RAD_TO_DEG * TRON_FULL_ROLL_POINTER_SIGN;
  bankDeg = constrain(bankDeg, -60.0f, 60.0f);

  float a = bankDeg * DEG_TO_RAD;

  float rx = sinf(a);
  float ry = -cosf(a);

  float tx = cosf(a);
  float ty = sinf(a);

  int outerR = radius + 2;
  int innerR = radius - 14;
  int halfW = 8;

  int tipX = cx + rx * innerR;
  int tipY = cy + ry * innerR;

  int baseCX = cx + rx * outerR;
  int baseCY = cy + ry * outerR;

  int base1X = baseCX - tx * halfW;
  int base1Y = baseCY - ty * halfW;

  int base2X = baseCX + tx * halfW;
  int base2Y = baseCY + ty * halfW;

  display->fillTriangle(
    tipX,
    tipY,
    base1X,
    base1Y,
    base2X,
    base2Y,
    orangeBright
  );

  display->drawTriangle(
    tipX,
    tipY,
    base1X,
    base1Y,
    base2X,
    base2Y,
    panelBlack
  );
}

// ----------------------------------------------------
// Aircraft cue
// ----------------------------------------------------

void drawTronFullAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
) {
  const int wingY = cy;

  // Glow
  drawTronFullThickLine(display, cx - 60, wingY, cx - 18, wingY, 5, cyanDim);
  drawTronFullThickLine(display, cx + 18, wingY, cx + 60, wingY, 5, cyanDim);

  // Angular symbol
  display->drawLine(cx - 60, wingY, cx - 34, wingY, cyanBright);
  display->drawLine(cx - 34, wingY, cx - 24, wingY - 8, cyanBright);
  display->drawLine(cx - 24, wingY - 8, cx - 10, wingY - 8, cyanBright);
  display->drawLine(cx - 10, wingY - 8, cx, wingY - 18, cyanBright);

  display->drawLine(cx + 60, wingY, cx + 34, wingY, cyanBright);
  display->drawLine(cx + 34, wingY, cx + 24, wingY - 8, cyanBright);
  display->drawLine(cx + 24, wingY - 8, cx + 10, wingY - 8, cyanBright);
  display->drawLine(cx + 10, wingY - 8, cx, wingY - 18, cyanBright);

  display->drawLine(cx - 18, wingY + 6, cx - 7, wingY + 15, cyanBright);
  display->drawLine(cx + 18, wingY + 6, cx + 7, wingY + 15, cyanBright);
  display->drawLine(cx - 7, wingY + 15, cx + 7, wingY + 15, cyanBright);

  display->fillCircle(cx, wingY, 10, panelBlack);
  display->drawCircle(cx, wingY, 10, cyanDim);
  display->drawCircle(cx, wingY, 7, cyanBright);
  display->fillCircle(cx, wingY, 2, cyanBright);
}

// ----------------------------------------------------
// Side markers
// ----------------------------------------------------

void drawTronFullSideMarkers(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t orangeBright,
  uint16_t panelBlack
) {
  int leftX  = 26;
  int rightX = display->width() - 27;

  display->fillTriangle(
    leftX,
    cy,
    leftX - 13,
    cy - 7,
    leftX - 13,
    cy + 7,
    orangeBright
  );

  display->fillTriangle(
    leftX - 3,
    cy,
    leftX - 10,
    cy - 4,
    leftX - 10,
    cy + 4,
    panelBlack
  );

  display->fillTriangle(
    rightX,
    cy,
    rightX + 13,
    cy - 7,
    rightX + 13,
    cy + 7,
    orangeBright
  );

  display->fillTriangle(
    rightX + 3,
    cy,
    rightX + 10,
    cy - 4,
    rightX + 10,
    cy + 4,
    panelBlack
  );
}

// ----------------------------------------------------
// Bottom decoration
// ----------------------------------------------------

void drawTronFullBottomDecor(
  Arduino_Canvas *display,
  uint16_t cyanVeryDim,
  uint16_t cyanDim,
  uint16_t cyanBright
) {
  int w = display->width();
  int h = display->height();

  display->drawLine(42, h - 26, w - 43, h - 26, cyanVeryDim);
  display->drawLine(58, h - 20, w - 59, h - 20, cyanDim);
  display->drawLine(80, h - 14, w - 81, h - 14, cyanBright);

  // Center lower notch
  int cx = w / 2;

  display->drawLine(cx - 32, h - 30, cx - 14, h - 8, cyanDim);
  display->drawLine(cx + 32, h - 30, cx + 14, h - 8, cyanDim);
}

// ----------------------------------------------------
// Utility helpers
// ----------------------------------------------------

void drawTronFullBoldText(
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

void drawTronFullThickLine(
  Arduino_Canvas *display,
  int x1,
  int y1,
  int x2,
  int y2,
  int thickness,
  uint16_t colour
) {
  if (thickness <= 1) {
    display->drawLine(x1, y1, x2, y2, colour);
    return;
  }

  int half = thickness / 2;

  for (int dx = -half; dx <= half; dx++) {
    display->drawLine(x1 + dx, y1, x2 + dx, y2, colour);
  }

  for (int dy = -half; dy <= half; dy++) {
    display->drawLine(x1, y1 + dy, x2, y2 + dy, colour);
  }
}

void drawTronFullRotatedInfiniteLineClipped(
  Arduino_Canvas *display,
  int cx,
  int cy,
  float ux,
  float uy,
  float nx,
  float ny,
  float offset,
  uint16_t colour
) {
  int mx = cx + nx * offset;
  int my = cy + ny * offset;

  int halfLen = 260;

  int x1 = mx - ux * halfLen;
  int y1 = my - uy * halfLen;

  int x2 = mx + ux * halfLen;
  int y2 = my + uy * halfLen;

  display->drawLine(x1, y1, x2, y2, colour);
}

bool tronFullLineSegmentWithinScreen(
  int x1,
  int y1,
  int x2,
  int y2,
  int w,
  int h
) {
  if ((x1 < 0 && x2 < 0) || (x1 >= w && x2 >= w)) {
    return false;
  }

  if ((y1 < 0 && y2 < 0) || (y1 >= h && y2 >= h)) {
    return false;
  }

  return true;
}