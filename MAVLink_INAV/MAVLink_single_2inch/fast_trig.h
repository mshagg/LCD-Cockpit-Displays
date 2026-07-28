#pragma once

// Interpolated lookup-table trigonometry for display geometry.
// Angles may be outside the normal range; outputs are in [-1, 1].
void fastSinCosDeg(float angleDeg, float *sinValue, float *cosValue);
void fastSinCosRad(float angleRad, float *sinValue, float *cosValue);

