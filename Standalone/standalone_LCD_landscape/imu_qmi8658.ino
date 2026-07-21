// ----------------------------------------------------
// QMI8658 onboard IMU support
// ----------------------------------------------------
//
// Calibrated vertical-screen version:
//
// - Hold the screen vertically and level during startup
// - Startup calibration stores that as:
//     roll  = 0 degrees
//     pitch = 0 degrees
//
// This avoids guessing whether the board's screen-up axis is X or Y.
//
// This is accelerometer-only attitude:
// - Good for bench testing board tilt
// - Good for a stationary handheld / panel display
// - Not suitable as final aircraft attitude during real movement
// - Final aircraft attitude should still come from MAVLink ATTITUDE
// ----------------------------------------------------

// Waveshare ESP32-S3-LCD-1.69 onboard IMU I2C pins.
// If the IMU is not detected, try swapping these two values.
#define IMU_SDA 11
#define IMU_SCL 10

SensorQMI8658 qmi;
IMUdata acc;

bool imuReady = false;

// Filtered IMU attitude in radians
float imuRoll = 0.0f;
float imuPitch = 0.0f;

// ----------------------------------------------------
// Axis/sign tuning
// ----------------------------------------------------
//
// If roll moves the wrong way, change IMU_ROLL_SIGN to -1.0f.
// If pitch moves the wrong way, change IMU_PITCH_SIGN to -1.0f.

const float IMU_ROLL_SIGN  = 1.0f;
const float IMU_PITCH_SIGN = -1.0f;

// Fine trims after calibration.
// Example: if vertical/level reads +3 degrees pitch, set pitch trim to -3.
const float IMU_ROLL_TRIM_DEG  = 0.0f;
const float IMU_PITCH_TRIM_DEG = 0.0f;

// ----------------------------------------------------
// Screen normal axis
// ----------------------------------------------------
//
// For this board, the screen face-up/down axis appears to be IMU Z.
// Earlier testing showed screen face-up gives acc.z negative, so use -Z
// as the screen-normal reference.
//
// If pitch is inverted or strange after calibration, try changing
// SCREEN_NORMAL_Z from -1.0f to 1.0f.

const float SCREEN_NORMAL_X = 0.0f;
const float SCREEN_NORMAL_Y = 0.0f;
const float SCREEN_NORMAL_Z = -1.0f;

// Calibrated display basis vectors.
// These are calculated at startup.
float screenUpX = 0.0f;
float screenUpY = 0.0f;
float screenUpZ = 0.0f;

float screenRightX = 0.0f;
float screenRightY = 0.0f;
float screenRightZ = 0.0f;

float screenNormalX = 0.0f;
float screenNormalY = 0.0f;
float screenNormalZ = 0.0f;

bool imuCalibrated = false;

// ----------------------------------------------------
// Small vector helpers
// ----------------------------------------------------

float vecLength(float x, float y, float z)
{
  return sqrtf((x * x) + (y * y) + (z * z));
}

void normalizeVec(float &x, float &y, float &z)
{
  float len = vecLength(x, y, z);

  if (len < 0.0001f) {
    return;
  }

  x /= len;
  y /= len;
  z /= len;
}

float dotVec(
  float ax,
  float ay,
  float az,
  float bx,
  float by,
  float bz
) {
  return (ax * bx) + (ay * by) + (az * bz);
}

void crossVec(
  float ax,
  float ay,
  float az,
  float bx,
  float by,
  float bz,
  float &rx,
  float &ry,
  float &rz
) {
  rx = (ay * bz) - (az * by);
  ry = (az * bx) - (ax * bz);
  rz = (ax * by) - (ay * bx);
}

float clampFloat(float v, float lo, float hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float smoothAngle(float currentAngle, float targetAngle, float alpha)
{
  float delta = targetAngle - currentAngle;

  while (delta > PI) {
    delta -= TWO_PI;
  }

  while (delta < -PI) {
    delta += TWO_PI;
  }

  return currentAngle + (alpha * delta);
}

// ----------------------------------------------------
// IMU setup
// ----------------------------------------------------

void setupIMU()
{
  Serial.println("Starting QMI8658 IMU...");

  Wire.begin(IMU_SDA, IMU_SCL);

  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IMU_SDA, IMU_SCL)) {
    Serial.println("QMI8658 not found.");
    Serial.println("Try swapping IMU_SDA and IMU_SCL.");
    imuReady = false;
    return;
  }

  Serial.print("QMI8658 found. Chip ID: 0x");
  Serial.println(qmi.getChipID(), HEX);

  qmi.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_250Hz,
    SensorQMI8658::LPF_MODE_0
  );

  qmi.enableAccelerometer();

  imuReady = true;

  Serial.println("QMI8658 ready");
  Serial.println("Hold screen vertical and level...");
  delay(1500);

  calibrateVerticalIMU();
}

// ----------------------------------------------------
// Vertical calibration
// ----------------------------------------------------

void calibrateVerticalIMU()
{
  if (!imuReady) {
    return;
  }

  Serial.println("Calibrating vertical IMU reference...");

  float sumX = 0.0f;
  float sumY = 0.0f;
  float sumZ = 0.0f;

  int sampleCount = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < 1200) {
    if (qmi.getDataReady()) {
      if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
        sumX += acc.x;
        sumY += acc.y;
        sumZ += acc.z;
        sampleCount++;
      }
    }

    delay(5);
  }

  if (sampleCount < 10) {
    Serial.println("IMU calibration failed: not enough samples.");
    imuCalibrated = false;
    return;
  }

  float gx = sumX / sampleCount;
  float gy = sumY / sampleCount;
  float gz = sumZ / sampleCount;

  normalizeVec(gx, gy, gz);

  // At vertical/level calibration:
  // gravity points downward, so screen-up is opposite gravity.
  screenUpX = -gx;
  screenUpY = -gy;
  screenUpZ = -gz;
  normalizeVec(screenUpX, screenUpY, screenUpZ);

  // Preset screen-normal axis.
  screenNormalX = SCREEN_NORMAL_X;
  screenNormalY = SCREEN_NORMAL_Y;
  screenNormalZ = SCREEN_NORMAL_Z;
  normalizeVec(screenNormalX, screenNormalY, screenNormalZ);

  // Build a clean screen-right vector from calibrated up and screen normal.
  crossVec(
    screenUpX,
    screenUpY,
    screenUpZ,
    screenNormalX,
    screenNormalY,
    screenNormalZ,
    screenRightX,
    screenRightY,
    screenRightZ
  );

  normalizeVec(screenRightX, screenRightY, screenRightZ);

  // Rebuild screen-up so the three axes are square to each other.
  crossVec(
    screenNormalX,
    screenNormalY,
    screenNormalZ,
    screenRightX,
    screenRightY,
    screenRightZ,
    screenUpX,
    screenUpY,
    screenUpZ
  );

  normalizeVec(screenUpX, screenUpY, screenUpZ);

  imuRoll = 0.0f;
  imuPitch = 0.0f;

  roll = 0.0f;
  pitch = 0.0f;

  imuCalibrated = true;

  Serial.println("IMU vertical calibration complete.");

  Serial.print("Screen up vector: ");
  Serial.print(screenUpX, 3);
  Serial.print(", ");
  Serial.print(screenUpY, 3);
  Serial.print(", ");
  Serial.println(screenUpZ, 3);

  Serial.print("Screen right vector: ");
  Serial.print(screenRightX, 3);
  Serial.print(", ");
  Serial.print(screenRightY, 3);
  Serial.print(", ");
  Serial.println(screenRightZ, 3);
}

// ----------------------------------------------------
// IMU attitude update
// ----------------------------------------------------

void updateImuAttitude()
{
  if (!imuReady || !imuCalibrated) {
    return;
  }

  if (!qmi.getDataReady()) {
    return;
  }

  if (!qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
    return;
  }

  float gx = acc.x;
  float gy = acc.y;
  float gz = acc.z;

  normalizeVec(gx, gy, gz);

  // Project gravity onto the calibrated screen axes.
  float gRight  = dotVec(gx, gy, gz, screenRightX,  screenRightY,  screenRightZ);
  float gUp     = dotVec(gx, gy, gz, screenUpX,     screenUpY,     screenUpZ);
  float gNormal = dotVec(gx, gy, gz, screenNormalX, screenNormalY, screenNormalZ);

  // Roll:
  // At calibrated vertical/level, gRight = 0 and gUp = -1,
  // so roll = 0.
  float rawRoll = atan2f(gRight, -gUp);

  // Pitch:
  // At calibrated vertical/level, gNormal = 0.
  // Face-up/down gives about +/-90 degrees.
  gNormal = clampFloat(gNormal, -1.0f, 1.0f);
  float rawPitch = asinf(gNormal);

  rawRoll  *= IMU_ROLL_SIGN;
  rawPitch *= IMU_PITCH_SIGN;

  rawRoll  += IMU_ROLL_TRIM_DEG * DEG_TO_RAD;
  rawPitch += IMU_PITCH_TRIM_DEG * DEG_TO_RAD;

  const float alpha = 0.35f;

  imuRoll  = smoothAngle(imuRoll, rawRoll, alpha);
  imuPitch = imuPitch + alpha * (rawPitch - imuPitch);

  roll = imuRoll;
  pitch = imuPitch;
}