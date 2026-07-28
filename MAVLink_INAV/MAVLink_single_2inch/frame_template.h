#pragma once

#include <Arduino_GFX_Library.h>

bool captureFrameTemplate(
  Arduino_GFX *display,
  uint16_t **frameTemplate
);

bool restoreFrameTemplate(
  Arduino_GFX *display,
  const uint16_t *frameTemplate
);

void resetFrameTemplateTiming();
uint32_t getFrameTemplateRestoreMicros();
uint32_t getFrameTemplateCaptureMicros();
