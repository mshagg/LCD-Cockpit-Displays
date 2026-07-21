// ----------------------------------------------------
// Glass-style landscape AHI
// ----------------------------------------------------
//
// Built-in Waveshare ESP32-S3-LCD-1.69 display only.
//
// Landscape layout:
// - canvas size: 280 x 240
// - glass attitude field
// - fixed roll scale arc
// - fixed zero-bank reference triangle
// - moving bank pointer triangle
// - fixed aircraft cue
// ----------------------------------------------------

// ----------------------------------------------------
// Roll pointer direction
// ----------------------------------------------------
//
// If the moving yellow roll pointer moves the wrong way,
// change this from 1.0f to -1.0f.

const float GLASS_ROLL_POINTER_SIGN = 1.0f;

// ----------------------------------------------------
// Function declarations
// ----------------------------------------------------

void drawGlassAhiHorizonFillRect(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  int h,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
);

void drawGlassAhiPitchLadder(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  int h,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  float pixelsPerDeg,
  uint16_t colour
);

void drawGlassAhiBoldText(
  Arduino_Canvas *display,
  int x,
  int y,
  const char *text,
  uint16_t colour
);

void drawGlassAhiRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor
);

void drawGlassAhiFixedZeroRollReference(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor,
  uint16_t blackColor
);

void drawGlassAhiMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t pointerColor,
  uint16_t outlineColor
);

void drawGlassAhiAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t magentaColor,
  uint16_t blackColor
);

// ----------------------------------------------------
// Main renderer
// ----------------------------------------------------

void drawGlassAhiScreen(Arduino_Canvas *display, float roll, float pitch)
{
  const int screenW = display->width();
  const int screenH = display->height();

  uint16_t panelBlack   = rgb565(20, 20, 20);
  uint16_t panelGrey    = rgb565(40, 40, 40);
  uint16_t skyColor     = rgb565(0, 95, 205);
  uint16_t groundColor  = rgb565(95, 45, 10);
  uint16_t whiteColor   = GFX_WHITE;
  uint16_t yellowColor  = rgb565(220, 210, 35);
  uint16_t magentaColor = rgb565(180, 40, 170);
  uint16_t blackColor   = GFX_BLACK;

  display->fillScreen(panelBlack);

  // Landscape frame.
  //
  // Screen: 280 x 240
  // Top blank margin    = 12 px
  // Bottom blank margin = 12 px
  const int bezelX = 16;
  const int bezelY = 12;
  const int bezelW = 248;
  const int bezelH = 216;

  const int attX = bezelX + 6;
  const int attY = bezelY + 6;
  const int attW = bezelW - 12;
  const int attH = bezelH - 12;

  const int cx = attX + attW / 2;

  // Slight downward centre bias retained so the aircraft cue sits naturally.
  const int cy = attY + attH / 2 + 8;

  const float pixelsPerDeg = 2.9f;

  float rollRad = -roll;

  float pitchDeg = pitch * RAD_TO_DEG;
  float pitchOffset = pitchDeg * pixelsPerDeg;
  pitchOffset = constrain(pitchOffset, -screenH, screenH);

  // Bezel block
  display->fillRect(bezelX, bezelY, bezelW, bezelH, panelGrey);
  display->drawRect(bezelX, bezelY, bezelW, bezelH, GFX_WHITE);
  display->drawRect(bezelX + 1, bezelY + 1, bezelW - 2, bezelH - 2, panelBlack);

  // Attitude field
  drawGlassAhiHorizonFillRect(
    display,
    attX,
    attY,
    attW,
    attH,
    cx,
    cy,
    rollRad,
    pitchOffset,
    skyColor,
    groundColor
  );

  drawGlassAhiPitchLadder(
    display,
    attX,
    attY,
    attW,
    attH,
    cx,
    cy,
    rollRad,
    pitchOffset,
    pixelsPerDeg,
    whiteColor
  );

  // Mask outside the attitude field
  display->fillRect(0, 0, screenW, attY, panelBlack);
  display->fillRect(0, attY + attH, screenW, screenH - (attY + attH), panelBlack);
  display->fillRect(0, attY, attX, attH, panelBlack);
  display->fillRect(attX + attW, attY, screenW - (attX + attW), attH, panelBlack);

  // Re-draw bezel on top
  display->fillRect(bezelX, bezelY, bezelW, 6, panelGrey);
  display->fillRect(bezelX, bezelY + bezelH - 6, bezelW, 6, panelGrey);
  display->fillRect(bezelX, bezelY, 6, bezelH, panelGrey);
  display->fillRect(bezelX + bezelW - 6, bezelY, 6, bezelH, panelGrey);

  display->drawRect(bezelX, bezelY, bezelW, bezelH, GFX_WHITE);
  display->drawRect(bezelX + 1, bezelY + 1, bezelW - 2, bezelH - 2, panelBlack);

  // Roll scale.
  const int rollScaleRadius = 51;
  const int rollScaleCy = attY + rollScaleRadius + 2;

  drawGlassAhiRollScale(
    display,
    cx,
    rollScaleCy,
    rollScaleRadius,
    whiteColor
  );

  drawGlassAhiFixedZeroRollReference(
    display,
    cx,
    rollScaleCy,
    rollScaleRadius,
    whiteColor,
    blackColor
  );

  drawGlassAhiMovingBankPointer(
    display,
    cx,
    rollScaleCy,
    rollScaleRadius,
    roll,
    yellowColor,
    blackColor
  );

  // Aircraft cue
  drawGlassAhiAircraftCue(
    display,
    cx,
    cy + 10,
    yellowColor,
    magentaColor,
    blackColor
  );
}

// ----------------------------------------------------
// Horizon fill within rectangular field
// ----------------------------------------------------

void drawGlassAhiHorizonFillRect(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  int h,
  int cx,
  int cy,
  float rollRad,
  float pitchOffset,
  uint16_t skyColor,
  uint16_t groundColor
) {
  float ux = cosf(rollRad);
  float uy = sinf(rollRad);

  float nx = -uy;
  float ny = ux;

  for (int yy = y; yy < y + h; yy++) {
    int dy = yy - cy;

    if (fabsf(nx) < 0.0001f) {
      float value = ny * (dy - pitchOffset);
      uint16_t colour = (value < 0) ? skyColor : groundColor;
      display->drawFastHLine(x, yy, w, colour);
    } else {
      float splitX = cx - (ny * (dy - pitchOffset)) / nx;

      if (nx > 0) {
        if (splitX <= x) {
          display->drawFastHLine(x, yy, w, groundColor);
        } else if (splitX >= x + w - 1) {
          display->drawFastHLine(x, yy, w, skyColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(x, yy, sx - x + 1, skyColor);
          display->drawFastHLine(sx + 1, yy, (x + w - 1) - sx, groundColor);
        }
      } else {
        if (splitX <= x) {
          display->drawFastHLine(x, yy, w, skyColor);
        } else if (splitX >= x + w - 1) {
          display->drawFastHLine(x, yy, w, groundColor);
        } else {
          int sx = (int)splitX;

          display->drawFastHLine(x, yy, sx - x + 1, groundColor);
          display->drawFastHLine(sx + 1, yy, (x + w - 1) - sx, skyColor);
        }
      }
    }
  }
}

// ----------------------------------------------------
// Pitch ladder
// ----------------------------------------------------

void drawGlassAhiPitchLadder(
  Arduino_Canvas *display,
  int x,
  int y,
  int w,
  int h,
  int cx,
  int cy,
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

    int mx = cx + nx * lineOffset;
    int my = cy + ny * lineOffset;

    if (my < y - 20 || my > y + h + 20) {
      continue;
    }

    // Slightly wider ladder for the landscape field.
    int halfLen = (deg % 10 == 0) ? 38 : 19;

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

      const int labelGap = 10;

      int lx1 = x1 - labelW - labelGap;
      int ly1 = y1 - labelH / 2;

      int lx2 = x2 + labelGap;
      int ly2 = y2 - labelH / 2;

      if (lx1 > x + 4 && ly1 > y + 4 && ly1 < y + h - 12) {
        drawGlassAhiBoldText(display, lx1, ly1, labelText, colour);
      }

      if (lx2 < x + w - labelW - 4 && ly2 > y + 4 && ly2 < y + h - 12) {
        drawGlassAhiBoldText(display, lx2, ly2, labelText, colour);
      }
    }
  }
}

void drawGlassAhiBoldText(
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
// Roll scale
// ----------------------------------------------------

void drawGlassAhiRollScale(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor
) {
  for (int deg = -60; deg <= 60; deg += 10) {
    float a = deg * DEG_TO_RAD;

    int outerR = radius;
    int innerR = (deg % 30 == 0) ? radius - 14 : radius - 8;

    int x1 = cx + sinf(a) * outerR;
    int y1 = cy - cosf(a) * outerR;
    int x2 = cx + sinf(a) * innerR;
    int y2 = cy - cosf(a) * innerR;

    display->drawLine(x1, y1, x2, y2, whiteColor);
    display->drawLine(x1 + 1, y1, x2 + 1, y2, whiteColor);
  }
}

// ----------------------------------------------------
// Fixed zero-bank reference
// ----------------------------------------------------

void drawGlassAhiFixedZeroRollReference(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  uint16_t whiteColor,
  uint16_t blackColor
) {
  // Fixed white reference triangle at zero bank.
  // It stays at the top of the arc.
  int tipX = cx;
  int tipY = cy - radius + 3;

  int baseY = tipY + 13;
  int halfW = 8;

  display->fillTriangle(
    tipX,
    tipY,
    tipX - halfW,
    baseY,
    tipX + halfW,
    baseY,
    whiteColor
  );

  // Slight black inset to make it look less like a solid block.
  display->drawLine(tipX - 5, baseY - 2, tipX + 5, baseY - 2, blackColor);
}

// ----------------------------------------------------
// Moving bank pointer
// ----------------------------------------------------

void drawGlassAhiMovingBankPointer(
  Arduino_Canvas *display,
  int cx,
  int cy,
  int radius,
  float roll,
  uint16_t pointerColor,
  uint16_t outlineColor
) {
  float bankDeg = roll * RAD_TO_DEG * GLASS_ROLL_POINTER_SIGN;

  bankDeg = constrain(bankDeg, -60.0f, 60.0f);

  float a = bankDeg * DEG_TO_RAD;

  // Radial vector from roll-scale centre to pointer.
  float rx = sinf(a);
  float ry = -cosf(a);

  // Tangential vector around arc.
  float tx = cosf(a);
  float ty = sinf(a);

  // Triangle points inward toward the centre of the roll scale.
  int outerR = radius - 2;
  int innerR = radius - 17;
  int halfW  = 7;

  int tipX = cx + rx * innerR;
  int tipY = cy + ry * innerR;

  int baseCX = cx + rx * outerR;
  int baseCY = cy + ry * outerR;

  int base1X = baseCX - tx * halfW;
  int base1Y = baseCY - ty * halfW;

  int base2X = baseCX + tx * halfW;
  int base2Y = baseCY + ty * halfW;

  // Black outline/shadow first
  display->fillTriangle(
    tipX,
    tipY,
    base1X,
    base1Y,
    base2X,
    base2Y,
    outlineColor
  );

  // Slightly smaller yellow pointer
  int innerTipR = radius - 18;
  int innerBaseR = radius - 4;
  int innerHalfW = 5;

  int yTipX = cx + rx * innerTipR;
  int yTipY = cy + ry * innerTipR;

  int yBaseCX = cx + rx * innerBaseR;
  int yBaseCY = cy + ry * innerBaseR;

  int yBase1X = yBaseCX - tx * innerHalfW;
  int yBase1Y = yBaseCY - ty * innerHalfW;

  int yBase2X = yBaseCX + tx * innerHalfW;
  int yBase2Y = yBaseCY + ty * innerHalfW;

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
// Aircraft cue
// ----------------------------------------------------

void drawGlassAhiAircraftCue(
  Arduino_Canvas *display,
  int cx,
  int cy,
  uint16_t yellowColor,
  uint16_t magentaColor,
  uint16_t blackColor
) {
  uint16_t magentaDark = rgb565(95, 20, 95);

  // Left magenta wedge
  display->fillTriangle(
    cx - 57, cy + 8,
    cx - 8,  cy - 8,
    cx - 15, cy + 17,
    magentaColor
  );

  // Right magenta wedge
  display->fillTriangle(
    cx + 57, cy + 8,
    cx + 8,  cy - 8,
    cx + 15, cy + 17,
    magentaColor
  );

  // Magenta outlines / darker lower edges
  display->drawLine(cx - 57, cy + 8, cx - 15, cy + 17, magentaDark);
  display->drawLine(cx - 8,  cy - 8, cx - 15, cy + 17, magentaDark);

  display->drawLine(cx + 57, cy + 8, cx + 15, cy + 17, magentaDark);
  display->drawLine(cx + 8,  cy - 8, cx + 15, cy + 17, magentaDark);

  const int wingY = cy;
  const int wingH = 7;

  // Left black wing tip
  display->fillTriangle(
    cx - 72, wingY - 4,
    cx - 54, wingY - 4,
    cx - 54, wingY + 4,
    blackColor
  );

  // Right black wing tip
  display->fillTriangle(
    cx + 72, wingY - 4,
    cx + 54, wingY - 4,
    cx + 54, wingY + 4,
    blackColor
  );

  // Left yellow wing bar
  display->fillRoundRect(
    cx - 67,
    wingY - wingH / 2,
    44,
    wingH,
    2,
    yellowColor
  );

  // Right yellow wing bar
  display->fillRoundRect(
    cx + 23,
    wingY - wingH / 2,
    44,
    wingH,
    2,
    yellowColor
  );

  // Thin black underline/shadow on wings
  display->drawFastHLine(cx - 67, wingY + 4, 44, blackColor);
  display->drawFastHLine(cx + 23, wingY + 4, 44, blackColor);

  // Central yellow nose/caret shape
  display->fillTriangle(
    cx,
    wingY - 13,
    cx - 16,
    wingY + 7,
    cx + 16,
    wingY + 7,
    yellowColor
  );

  // Hollow the centre slightly so it looks like an outlined cue
  display->fillTriangle(
    cx,
    wingY - 6,
    cx - 8,
    wingY + 5,
    cx + 8,
    wingY + 5,
    blackColor
  );

  // Small yellow base line
  display->drawFastHLine(cx - 18, wingY + 8, 36, yellowColor);
  display->drawFastHLine(cx - 18, wingY + 9, 36, yellowColor);

  // Centre dot / pivot
  display->fillCircle(cx, wingY + 1, 4, yellowColor);
}