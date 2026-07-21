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
// This is accelerometer-only attitude:
// - Good for bench testing board tilt
// - Not suitable as final aircraft attitude during real movement
// - Final aircraft attitude should come from MAVLink ATTITUDE
//
// Updated:
// - smoothing alpha increased from 0.12 to 0.35
// - this reduces apparent lag
// ----------------------------------------------------

#define IMU_SDA 11
#define IMU_SCL 10

SensorQMI8658 qmi;
IMUdata acc;

bool imuReady = false;

float imuRoll = 0.0f;
float imuPitch = 0.0f;

const float IMU_ROLL_SIGN  = 1.0f;
const float IMU_PITCH_SIGN = -1.0f;

const float IMU_ROLL_TRIM_DEG  = 0.0f;
const float IMU_PITCH_TRIM_DEG = 0.0f;

// Higher = faster response, more twitch.
// Lower = smoother response, more lag.
const float IMU_SMOOTHING_ALPHA = 0.35f;

const float SCREEN_NORMAL_X = 0.0f;
const float SCREEN_NORMAL_Y = 0.0f;
const float SCREEN_NORMAL_Z = -1.0f;

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

float dotVec(float ax, float ay, float az, float bx, float by, float bz)
{
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

  screenUpX = -gx;
  screenUpY = -gy;
  screenUpZ = -gz;
  normalizeVec(screenUpX, screenUpY, screenUpZ);

  screenNormalX = SCREEN_NORMAL_X;
  screenNormalY = SCREEN_NORMAL_Y;
  screenNormalZ = SCREEN_NORMAL_Z;
  normalizeVec(screenNormalX, screenNormalY, screenNormalZ);

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
}

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

  float gRight  = dotVec(gx, gy, gz, screenRightX,  screenRightY,  screenRightZ);
  float gUp     = dotVec(gx, gy, gz, screenUpX,     screenUpY,     screenUpZ);
  float gNormal = dotVec(gx, gy, gz, screenNormalX, screenNormalY, screenNormalZ);

  float rawRoll = atan2f(gRight, -gUp);

  gNormal = clampFloat(gNormal, -1.0f, 1.0f);
  float rawPitch = asinf(gNormal);

  rawRoll  *= IMU_ROLL_SIGN;
  rawPitch *= IMU_PITCH_SIGN;

  rawRoll  += IMU_ROLL_TRIM_DEG * DEG_TO_RAD;
  rawPitch += IMU_PITCH_TRIM_DEG * DEG_TO_RAD;

  imuRoll  = smoothAngle(imuRoll, rawRoll, IMU_SMOOTHING_ALPHA);
  imuPitch = imuPitch + IMU_SMOOTHING_ALPHA * (rawPitch - imuPitch);

  roll = imuRoll;
  pitch = imuPitch;
}