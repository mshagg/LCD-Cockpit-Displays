#include "fast_trig.h"

#include <Arduino.h>
#include <math.h>

// One quadrant at one-degree spacing in signed Q15 format. Symmetry
// reconstructs the other three quadrants, keeping the table compact.
static const int16_t FAST_SIN_Q15[91] PROGMEM = {
  0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126,
  5690, 6252, 6813, 7371, 7927, 8481, 9032, 9580, 10126, 10668,
  11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886,
  16383, 16876, 17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621,
  21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964, 24351, 24730,
  25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087,
  28377, 28659, 28932, 29196, 29451, 29697, 29934, 30162, 30381, 30591,
  30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
  32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762,
  32767
};

static float fastSinDegInteger(int angleDeg)
{
  int angle = angleDeg % 360;
  if (angle < 0) {
    angle += 360;
  }

  int sign = 1;
  int index;

  if (angle <= 90) {
    index = angle;
  } else if (angle <= 180) {
    index = 180 - angle;
  } else if (angle <= 270) {
    index = angle - 180;
    sign = -1;
  } else {
    index = 360 - angle;
    sign = -1;
  }

  return ((float)(sign * (int)pgm_read_word(&FAST_SIN_Q15[index]))) /
         32767.0f;
}

void fastSinCosDeg(float angleDeg, float *sinValue, float *cosValue)
{
  int lowerDeg = (int)floorf(angleDeg);
  float fraction = angleDeg - (float)lowerDeg;

  float sin0 = fastSinDegInteger(lowerDeg);
  float sin1 = fastSinDegInteger(lowerDeg + 1);
  float cos0 = fastSinDegInteger(lowerDeg + 90);
  float cos1 = fastSinDegInteger(lowerDeg + 91);

  *sinValue = sin0 + ((sin1 - sin0) * fraction);
  *cosValue = cos0 + ((cos1 - cos0) * fraction);
}

void fastSinCosRad(float angleRad, float *sinValue, float *cosValue)
{
  fastSinCosDeg(angleRad * RAD_TO_DEG, sinValue, cosValue);
}

