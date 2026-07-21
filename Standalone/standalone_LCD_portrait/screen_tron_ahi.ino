// ----------------------------------------------------
// Futuristic Tron-style portrait AHI
// ----------------------------------------------------
//
// Built-in Waveshare ESP32-S3-LCD-1.69 display only.
//
// Portrait canvas expected:
// - 240 x 280
//
// This screen only uses data currently available from the standalone IMU:
// - roll
// - pitch
//
// It deliberately avoids:
// - airspeed
// - altitude
// - heading
// - GPS
// - vertical speed
// - flight mode
// - autopilot status
// ----------------------------------------------------

// If the moving bank pointer moves the wrong way,
// change this from 1.0f to -1.0f.
const float TRON_ROLL_POINTER_SIGN = 1.0f;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void drawTronOuterFrame(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronTitle(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronHorizonFillCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
);

void drawTronGrid(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t skyGridColor,
  uint16_t groundGridColor
);

void clearTronOutsideCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t backgroundColor
);

void drawTronHorizonLine(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  uint16_t cyanGlow,
  uint16_t cyanBright
);

void drawTronPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t cyanBright,
  uint16_t orangeBright
);

void drawTronRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanDim,
  uint16_t cyanBright
);

void drawTronFixedZeroReference(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t orangeBright,
  uint16_t panelBlack
);

void drawTronAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
);

void drawTronSideHorizonMarkers(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t orangeBright,
  uint16_t panelBlack
);

void drawTronBoldText(
  Arduino_Canvas *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
);

void drawTronThickLine(
  Arduino_Canvas *display,
  int x1,
  int y1,
  int x2,
  int y2,
  int thickness,
  uint16_t colour
);

bool tronPointInCircle(int x, int y, int cx, int cy, int radius);

// ----------------------------------------------------
// Main renderer
// ----------------------------------------------------

void drawTronAhiScreen(Arduino_Canvas *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  uint16_t panelBlack   = rgb565(0, 3, 8);
  uint16_t deepBlack    = GFX_BLACK;

  uint16_t cyanVeryDim  = rgb565(0, 25, 45);
  uint16_t cyanDim      = rgb565(0, 90, 135);
  uint16_t cyanMid      = rgb565(0, 170, 225);
  uint16_t cyanBright   = rgb565(80, 235, 255);

  uint16_t skyColor     = rgb565(0, 45, 135);
  uint16_t skyDeep      = rgb565(0, 20, 80);

  uint16_t groundColor  = rgb565(28, 14, 0);
  uint16_t orangeDim    = rgb565(130, 65, 0);
  uint16_t orangeBright = rgb565(255, 170, 35);

  display->fillScreen(deepBlack);

  drawTronOuterFrame(display, cyanDim, cyanBright, panelBlack);
  drawTronTitle(display, cyanDim, cyanBright, panelBlack);

  // Large central attitude sphere.
  //
  // Tuned for 240 x 280 portrait.
  const int cx = screenW / 2;
  const int cy = 143;
  const int radius = 101;

  const float pixelsPerDeg = 3.0f;

  float rollRad = -roll;

  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;
  pitchOffset = constrain(pitchOffset, -radius, radius);

  // Attitude field
  drawTronHorizonFillCircle(
    display,
    cx,
    cy,
    radius,
    rollRad,
    pitchOffset,
    skyColor,
    groundColor
  );

  // Subtle grid / terrain styling
  drawTronGrid(
    display,
    cx,
    cy,
    radius,
    rgb565(0, 75, 130),
    orangeDim
  );

  drawTronPitchLadder(
    display,
    cx,
    cy,
    radius,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    cyanBright,
    orangeBright
  );

  drawTronHorizonLine(
    display,
    cx,
    cy,
    radius,
    rollRad,
    pitchOffset,
    cyanMid,
    cyanBright
  );

  // Clip any escaping pitch/grid elements
  clearTronOutsideCircle(display, cx, cy, radius + 1, panelBlack);

  // Outer glowing sphere rings
  display->drawCircle(cx, cy, radius + 2, cyanDim);
  display->drawCircle(cx, cy, radius + 1, cyanBright);
  display->drawCircle(cx, cy, radius - 1, cyanDim);
  display->drawCircle(cx, cy, radius - 4, cyanVeryDim);

  // Fixed roll scale and moving bank pointer
  const int rollScaleRadius = radius + 7;

  drawTronRollScale(
    display,
    cx,
    cy,
    rollScaleRadius,
    cyanDim,
    cyanBright
  );

  drawTronFixedZeroReference(
    display,
    cx,
    cy,
    rollScaleRadius,
    cyanBright,
    panelBlack
  );

  drawTronMovingBankPointer(
    display,
    cx,
    cy,
    rollScaleRadius,
    roll,
    orangeBright,
    panelBlack
  );

  drawTronSideHorizonMarkers(
    display,
    cx,
    cy,
    radius,
    orangeBright,
    panelBlack
  );

  // Fixed aircraft reference symbol
  drawTronAircraftCue(
    display,
    cx,
    cy,
    cyanDim,
    cyanBright,
    panelBlack
  );

  // Small decorative lower empty panel, no unavailable data shown
  display->drawLine(62, 257, 178, 257, cyanVeryDim);
  display->drawLine(72, 263, 168, 263, cyanDim);
  display->drawLine(82, 269, 158, 269, cyanVeryDim);
}

// ----------------------------------------------------
// Outer frame
// ----------------------------------------------------

void drawTronOuterFrame(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
) {
  const int w = display->width();
  const int h = display->height();

  // Outer angled sci-fi frame
  display->drawRect(4, 4, w - 8, h - 8, cyanDim);
  display->drawRect(6, 6, w - 12, h - 12, cyanBright);

  // Corner bevels / black cuts
  display->fillTriangle(4, 4, 34, 4, 4, 34, panelBlack);
  display->fillTriangle(w - 5, 4, w - 35, 4, w - 5, 34, panelBlack);
  display->fillTriangle(4, h - 5, 34, h - 5, 4, h - 35, panelBlack);
  display->fillTriangle(w - 5, h - 5, w - 35, h - 5, w - 5, h - 35, panelBlack);

  // Corner neon strokes
  display->drawLine(12, 34, 34, 12, cyanBright);
  display->drawLine(w - 13, 34, w - 35, 12, cyanBright);
  display->drawLine(12, h - 35, 34, h - 13, cyanBright);
  display->drawLine(w - 13, h - 35, w - 35, h - 13, cyanBright);

  // Side decorative rails
  display->drawLine(14, 58, 14, 210, cyanDim);
  display->drawLine(18, 72, 18, 196, cyanBright);

  display->drawLine(w - 15, 58, w - 15, 210, cyanDim);
  display->drawLine(w - 19, 72, w - 19, 196, cyanBright);

  // Small circuit dots
  for (int y = 42; y <= 232; y += 18) {
    display->drawPixel(9, y, cyanDim);
    display->drawPixel(w - 10, y, cyanDim);
  }

  // Lower decorative angular panels
  display->drawLine(24, 246, 72, 246, cyanDim);
  display->drawLine(72, 246, 84, 256, cyanDim);
  display->drawLine(84, 256, 156, 256, cyanDim);
  display->drawLine(156, 256, 168, 246, cyanDim);
  display->drawLine(168, 246, 216, 246, cyanDim);
}

// ----------------------------------------------------
// Top title
// ----------------------------------------------------

void drawTronTitle(
  Arduino_Canvas *display,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
) {
  int cx = display->width() / 2;

  // Angular title plate
  display->fillRect(cx - 58, 8, 116, 24, panelBlack);

  display->drawLine(cx - 58, 20, cx - 46, 8, cyanDim);
  display->drawLine(cx - 46, 8, cx + 46, 8, cyanBright);
  display->drawLine(cx + 46, 8, cx + 58, 20, cyanDim);
  display->drawLine(cx + 58, 20, cx + 46, 32, cyanDim);
  display->drawLine(cx + 46, 32, cx - 46, 32, cyanBright);
  display->drawLine(cx - 46, 32, cx - 58, 20, cyanDim);

  display->setTextSize(2);
  display->setTextColor(cyanBright);
  display->setCursor(cx - 48, 13);
  display->print("ATTITUDE");

  // Small chevrons
  display->fillTriangle(cx - 68, 18, cx - 62, 14, cx - 62, 22, cyanBright);
  display->fillTriangle(cx + 68, 18, cx + 62, 14, cx + 62, 22, cyanBright);
}

// ----------------------------------------------------
// Horizon fill inside circular sphere
// ----------------------------------------------------

void drawTronHorizonFillCircle(
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

    // Subtle vertical shading
    int shade = map(y, cy - radius, cy + radius, 0, 50);
    uint16_t skyShade = rgb565(0, 45 + shade / 3, 135 + shade);
    uint16_t groundShade = rgb565(28 + shade / 4, 14 + shade / 6, 0);

    if (fabsf(nx) < 0.0001f) {
      float value = ny * (dy - pitchOffset);
      uint16_t colour = (value < 0) ? skyShade : groundShade;
      display->drawFastHLine(rowX1, y, rowW, colour);
    } else {
      float splitX = cx - (ny * (dy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= rowX1) {
          display->drawFastHLine(rowX1, y, rowW, groundShade);
        } else if (splitX >= rowX2) {
          display->drawFastHLine(rowX1, y, rowW, skyShade);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(rowX1, y, sx - rowX1 + 1, skyShade);
          display->drawFastHLine(sx + 1, y, rowX2 - sx, groundShade);
        }
      } else {
        if (splitX <= rowX1) {
          display->drawFastHLine(rowX1, y, rowW, skyShade);
        } else if (splitX >= rowX2) {
          display->drawFastHLine(rowX1, y, rowW, groundShade);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(rowX1, y, sx - rowX1 + 1, groundShade);
          display->drawFastHLine(sx + 1, y, rowX2 - sx, skyShade);
        }
      }
    }
  }
}

// ----------------------------------------------------
// Subtle grid / terrain motif
// ----------------------------------------------------

void drawTronGrid(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t skyGridColor,
  uint16_t groundGridColor
) {
  // Horizontal sky grid
  for (int y = cy - radius + 22; y < cy; y += 18) {
    int dy = y - cy;
    int dx = (int)sqrtf((radius * radius) - (dy * dy));

    display->drawFastHLine(cx - dx + 10, y, (dx * 2) - 20, skyGridColor);
  }

  // Vertical faint sky grid
  for (int x = cx - radius + 28; x <= cx + radius - 28; x += 28) {
    int dx = x - cx;
    int ySpan = (int)sqrtf((radius * radius) - (dx * dx));

    display->drawLine(x, cy - ySpan + 12, x, cy - 8, skyGridColor);
  }

  // Orange wireframe ground
  for (int x = cx - radius + 10; x <= cx + radius - 10; x += 22) {
    int dx = x - cx;
    int ySpan = (int)sqrtf((radius * radius) - (dx * dx));

    int bottomY = cy + ySpan - 10;
    display->drawLine(cx, cy + 26, x, bottomY, groundGridColor);
  }

  for (int y = cy + 24; y < cy + radius - 16; y += 18) {
    int dy = y - cy;
    int dx = (int)sqrtf((radius * radius) - (dy * dy));

    display->drawFastHLine(cx - dx + 8, y, (dx * 2) - 16, groundGridColor);
  }
}

// ----------------------------------------------------
// Circular clipping cleanup
// ----------------------------------------------------

void clearTronOutsideCircle(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t backgroundColor
) {
  int screenW = display->width();
  int screenH = display->height();

  display->fillRect(0, 34, screenW, max(0, cy - radius - 34), backgroundColor);
  display->fillRect(0, cy + radius + 1, screenW, screenH - (cy + radius + 1), backgroundColor);

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
// Bright horizon line
// ----------------------------------------------------

void drawTronHorizonLine(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  uint16_t cyanGlow,
  uint16_t cyanBright
) {
  if (fabsf(pitchOffset) >= radius - 4) {
    return;
  }

  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  int halfLen = (int)sqrtf((radius * radius) - (pitchOffset * pitchOffset));

  int mx = cx + nx * pitchOffset;
  int my = cy + ny * pitchOffset;

  int x1 = mx - ux * halfLen;
  int y1 = my - uy * halfLen;
  int x2 = mx + ux * halfLen;
  int y2 = my + uy * halfLen;

  drawTronThickLine(display, x1, y1, x2, y2, 5, cyanGlow);
  drawTronThickLine(display, x1, y1, x2, y2, 2, cyanBright);
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

void drawTronPitchLadder(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t cyanBright,
  uint16_t orangeBright
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

    float lineOffset = pitchOffset - deg * pixelsPerDeg;

    int mx = cx + nx * lineOffset;
    int my = cy + ny * lineOffset;

    int distX = mx - cx;
    int distY = my - cy;

    if ((distX * distX + distY * distY) > ((radius - 8) * (radius - 8))) {
      continue;
    }

    uint16_t colour = (deg > 0) ? cyanBright : orangeBright;

    int halfLen = (deg % 10 == 0) ? 36 : 18;

    int x1 = mx - ux * halfLen;
    int y1 = my - uy * halfLen;
    int x2 = mx + ux * halfLen;
    int y2 = my + uy * halfLen;

    drawTronThickLine(display, x1, y1, x2, y2, 1, colour);

    // Small glowing end caps on major lines
    if (deg % 10 == 0) {
      display->fillCircle(x1, y1, 2, colour);
      display->fillCircle(x2, y2, 2, colour);

      char labelText[5];
      snprintf(labelText, sizeof(labelText), "%d", deg);

      int labelW = strlen(labelText) * 6;
      int labelH = 8;

      int lx1 = x1 - labelW - 10;
      int ly1 = y1 - labelH / 2;

      int lx2 = x2 + 10;
      int ly2 = y2 - labelH / 2;

      if (tronPointInCircle(lx1, ly1, cx, cy, radius - 8)) {
        drawTronBoldText(display, lx1, ly1, labelText, colour);
      }

      if (tronPointInCircle(lx2 + labelW, ly2, cx, cy, radius - 8)) {
        drawTronBoldText(display, lx2, ly2, labelText, colour);
      }
    }
  }

  // Small centre vertical tick column
  for (int i = -5; i <= 5; i++) {
    if (i == 0) {
      continue;
    }

    int tickY = cy + i * 9;

    uint16_t colour = (i < 0) ? cyanBright : orangeBright;

    display->drawFastHLine(cx - 5, tickY, 10, colour);
  }
}

// ----------------------------------------------------
// Roll scale
// ----------------------------------------------------

void drawTronRollScale(
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
      innerR = radius - 13;
    } else if (deg % 10 == 0) {
      innerR = radius - 9;
    } else {
      innerR = radius - 5;
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

  // Numeric roll labels
  display->setTextSize(1);
  display->setTextColor(cyanBright);

  const int labelR = radius - 26;

  for (int deg = -60; deg <= 60; deg += 30) {
    float a = deg * DEG_TO_RAD;

    char label[5];
    snprintf(label, sizeof(label), "%d", deg);

    int labelW = strlen(label) * 6;

    int lx = cx + sinf(a) * labelR - labelW / 2;
    int ly = cy - cosf(a) * labelR - 4;

    drawTronBoldText(display, lx, ly, label, cyanBright);
  }

  // -90 and +90 side labels
  drawTronBoldText(display, cx - radius + 10, cy - 4, "-90", cyanBright);
  drawTronBoldText(display, cx + radius - 28, cy - 4, "90", cyanBright);
}

// ----------------------------------------------------
// Fixed zero-bank reference
// ----------------------------------------------------

void drawTronFixedZeroReference(
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
// Moving orange bank pointer
// ----------------------------------------------------

void drawTronMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t orangeBright,
  uint16_t panelBlack
) {
  float bankDeg = roll * RAD_TO_DEG * TRON_ROLL_POINTER_SIGN;

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
// Fixed aircraft cue
// ----------------------------------------------------

void drawTronAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t cyanDim,
  uint16_t cyanBright,
  uint16_t panelBlack
) {
  const int wingY = cy;

  // Outer glow
  drawTronThickLine(display, cx - 58, wingY, cx - 18, wingY, 5, cyanDim);
  drawTronThickLine(display, cx + 18, wingY, cx + 58, wingY, 5, cyanDim);

  // Angular wing shape
  display->drawLine(cx - 58, wingY, cx - 32, wingY, cyanBright);
  display->drawLine(cx - 32, wingY, cx - 22, wingY - 9, cyanBright);
  display->drawLine(cx - 22, wingY - 9, cx - 10, wingY - 9, cyanBright);
  display->drawLine(cx - 10, wingY - 9, cx, wingY - 18, cyanBright);

  display->drawLine(cx + 58, wingY, cx + 32, wingY, cyanBright);
  display->drawLine(cx + 32, wingY, cx + 22, wingY - 9, cyanBright);
  display->drawLine(cx + 22, wingY - 9, cx + 10, wingY - 9, cyanBright);
  display->drawLine(cx + 10, wingY - 9, cx, wingY - 18, cyanBright);

  // Lower center facets
  display->drawLine(cx - 20, wingY + 6, cx - 7, wingY + 15, cyanBright);
  display->drawLine(cx + 20, wingY + 6, cx + 7, wingY + 15, cyanBright);
  display->drawLine(cx - 7, wingY + 15, cx + 7, wingY + 15, cyanBright);

  // Central target
  display->fillCircle(cx, wingY, 10, panelBlack);
  display->drawCircle(cx, wingY, 10, cyanDim);
  display->drawCircle(cx, wingY, 7, cyanBright);
  display->fillCircle(cx, wingY, 2, cyanBright);
}

// ----------------------------------------------------
// Side horizon markers
// ----------------------------------------------------

void drawTronSideHorizonMarkers(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t orangeBright,
  uint16_t panelBlack
) {
  int leftX = cx - radius + 4;
  int rightX = cx + radius - 4;

  // Left marker
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

  // Right marker
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
// Utility helpers
// ----------------------------------------------------

void drawTronBoldText(
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

void drawTronThickLine(
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

bool tronPointInCircle(int x, int y, int cx, int cy, int radius)
{
  int dx = x - cx;
  int dy = y - cy;

  return ((dx * dx) + (dy * dy)) <= (radius * radius);
}