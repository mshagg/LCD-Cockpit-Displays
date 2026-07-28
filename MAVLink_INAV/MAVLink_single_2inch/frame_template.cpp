#include "frame_template.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

static uint32_t frameTemplateRestoreMicros = 0;
static uint32_t frameTemplateCaptureMicros = 0;

static size_t frameTemplateBytes(Arduino_GFX *display)
{
  return (size_t)display->width() *
         (size_t)display->height() *
         sizeof(uint16_t);
}

static Arduino_Canvas *frameTemplateCanvas(Arduino_GFX *display)
{
  // Both optimized pages are routed exclusively through the project's
  // full-screen Arduino_Canvas instance.
  return static_cast<Arduino_Canvas *>(display);
}

bool captureFrameTemplate(
  Arduino_GFX *display,
  uint16_t **frameTemplate
) {
  if (display == nullptr || frameTemplate == nullptr) {
    return false;
  }

  Arduino_Canvas *canvas = frameTemplateCanvas(display);
  uint16_t *source = canvas->getFramebuffer();

  if (source == nullptr) {
    return false;
  }

  size_t byteCount = frameTemplateBytes(display);

  if (*frameTemplate == nullptr) {
    *frameTemplate = (uint16_t *)heap_caps_malloc(
      byteCount,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
  }

  if (*frameTemplate == nullptr) {
    return false;
  }

  uint32_t copyStartUs = micros();
  memcpy(*frameTemplate, source, byteCount);
  frameTemplateCaptureMicros = micros() - copyStartUs;
  return true;
}

bool restoreFrameTemplate(
  Arduino_GFX *display,
  const uint16_t *frameTemplate
) {
  if (display == nullptr || frameTemplate == nullptr) {
    return false;
  }

  Arduino_Canvas *canvas = frameTemplateCanvas(display);
  uint16_t *destination = canvas->getFramebuffer();

  if (destination == nullptr) {
    return false;
  }

  uint32_t copyStartUs = micros();

  memcpy(
    destination,
    frameTemplate,
    frameTemplateBytes(display)
  );

  frameTemplateRestoreMicros = micros() - copyStartUs;
  return true;
}

void resetFrameTemplateTiming()
{
  frameTemplateRestoreMicros = 0;
  frameTemplateCaptureMicros = 0;
}

uint32_t getFrameTemplateRestoreMicros()
{
  return frameTemplateRestoreMicros;
}

uint32_t getFrameTemplateCaptureMicros()
{
  return frameTemplateCaptureMicros;
}
