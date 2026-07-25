// ----------------------------------------------------
// MAVLink telemetry
// ----------------------------------------------------

#include <Arduino.h>
#include <MAVLink.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define MAVLINK_RX_PIN  16
#define MAVLINK_TX_PIN  -1
#define MAVLINK_BAUD    115200

HardwareSerial MavlinkSerial(1);

// Main attitude globals from main tab
extern float roll;
extern float pitch;
extern float yaw;

// ----------------------------------------------------
// MAVLink parser
// ----------------------------------------------------

mavlink_message_t msg;
mavlink_status_t status;

bool msgReceived = false;

// ----------------------------------------------------
// Attitude
// ----------------------------------------------------

float rawRoll = 0.0f;
float rawPitch = 0.0f;
float rawYaw = 0.0f;

bool attitudeSmoothingInitialised = false;
unsigned long lastAttitudeSmoothingUs = 0;

const float MAVLINK_ROLL_SMOOTHING_SECONDS  = 0.10f;
const float MAVLINK_PITCH_SMOOTHING_SECONDS = 0.12f;
const float MAVLINK_YAW_SMOOTHING_SECONDS   = 0.15f;

// ----------------------------------------------------
// Flight data
// ----------------------------------------------------

float airspeed = 0.0f;
float groundspeed = 0.0f;
float altitude_msl = 0.0f;
float climb_rate = 0.0f;
int16_t heading_deg = 0;

int16_t mavlinkThrottlePercent = -1;
bool mavlinkThrottleValid = false;

// ----------------------------------------------------
// Heartbeat / mode
// ----------------------------------------------------

bool vehicleArmed = false;

uint8_t heartbeatType = 0;
uint8_t heartbeatAutopilot = 0;
uint8_t heartbeatBaseMode = 0;
uint8_t heartbeatSystemStatus = 0;
uint32_t heartbeatCustomMode = 0;

char flightModeText[18] = "NO HB";

unsigned long lastMavlinkHeartbeatMs = 0;
bool mavlinkHeartbeatValid = false;

// ----------------------------------------------------
// Flight timer
// ----------------------------------------------------

bool flightTimerRunning = false;
unsigned long flightTimerStartMs = 0;
uint32_t flightTimeSeconds = 0;

// ----------------------------------------------------
// Battery
// ----------------------------------------------------

float batteryVoltage = 0.0f;
float batteryCellVoltage = 0.0f;
float batteryCurrentA = 0.0f;
float batteryPowerW = 0.0f;

int8_t batteryRemainingPercent = -1;

bool batteryVoltageValid = false;
bool batteryCurrentValid = false;
bool batteryPowerValid = false;

unsigned long lastMavlinkBatteryMs = 0;
bool mavlinkBatteryValid = false;

bool mavlinkBatteryStatusValid = false;
unsigned long lastMavlinkBatteryStatusMs = 0;

float batteryCellVoltages[10] = {0.0f};
bool batteryCellVoltageValid[10] = {false};

uint8_t batteryCellCountTelemetry = 0;

float batteryLowestCellVoltage = 0.0f;
bool batteryLowestCellVoltageValid = false;

float batteryConsumedMah = 0.0f;
bool batteryConsumedMahValid = false;

float batteryConsumedWh = 0.0f;
bool batteryConsumedWhValid = false;

float batteryTemperatureC = 0.0f;
bool batteryTemperatureValid = false;

float batteryMaxCurrentA = 0.0f;
bool batteryMaxCurrentValid = false;

float batteryReferenceVoltage = 0.0f;
bool batteryReferenceVoltageValid = false;

float batterySagV = 0.0f;
bool batterySagValid = false;

// ----------------------------------------------------
// GPS
// ----------------------------------------------------

uint8_t gpsFixType = 0;
uint8_t gpsSatellitesVisible = 0;
int32_t gpsLat = 0;
int32_t gpsLon = 0;
int32_t gpsAltMm = 0;

float gpsHdop = 0.0f;
bool gpsHdopValid = false;

unsigned long lastMavlinkGpsMs = 0;
bool mavlinkGpsValid = false;

// ----------------------------------------------------
// RC channels
// ----------------------------------------------------

uint16_t rcChannelRaw[18] = {0};

unsigned long lastMavlinkRcChannelsMs = 0;
bool mavlinkRcChannelsValid = false;

uint8_t rssiPercent = 0;
bool rssiValid = false;

// ----------------------------------------------------
// Telemetry health
// ----------------------------------------------------

unsigned long lastMavlinkAttitudeMs = 0;
bool mavlinkAttitudeValid = false;

unsigned long lastMavlinkVfrHudMs = 0;
bool mavlinkVfrHudValid = false;

// ----------------------------------------------------
// Diagnostics
// ----------------------------------------------------

uint32_t mavlinkConfiguredBaud = MAVLINK_BAUD;

uint32_t mavlinkBytesTotal = 0;
uint32_t mavlinkMessagesTotal = 0;

uint32_t mavlinkHeartbeatTotal = 0;
uint32_t mavlinkAttitudeTotal = 0;
uint32_t mavlinkVfrHudTotal = 0;
uint32_t mavlinkSysStatusTotal = 0;
uint32_t mavlinkGpsRawTotal = 0;
uint32_t mavlinkRcChannelsTotal = 0;
uint32_t mavlinkBatteryStatusTotal = 0;

uint32_t mavlinkBytesPerSecond = 0;
uint32_t mavlinkMessagesPerSecond = 0;

uint32_t mavlinkHeartbeatPerSecond = 0;
uint32_t mavlinkAttitudePerSecond = 0;
uint32_t mavlinkVfrHudPerSecond = 0;
uint32_t mavlinkSysStatusPerSecond = 0;
uint32_t mavlinkGpsRawPerSecond = 0;
uint32_t mavlinkRcChannelsPerSecond = 0;
uint32_t mavlinkBatteryStatusPerSecond = 0;

uint32_t mavlinkWindowBytes = 0;
uint32_t mavlinkWindowMessages = 0;
uint32_t mavlinkWindowHeartbeats = 0;
uint32_t mavlinkWindowAttitudes = 0;
uint32_t mavlinkWindowVfrHud = 0;
uint32_t mavlinkWindowSysStatus = 0;
uint32_t mavlinkWindowGpsRaw = 0;
uint32_t mavlinkWindowRcChannels = 0;
uint32_t mavlinkWindowBatteryStatus = 0;

uint32_t mavlinkParseCallsWindow = 0;
uint32_t mavlinkParseCallsPerSecond = 0;

uint32_t mavlinkParseMicrosLast = 0;
uint32_t mavlinkParseMicrosAvg = 0;
uint32_t mavlinkParseMicrosMax = 0;
uint32_t mavlinkParseMicrosMaxWindow = 0;
uint64_t mavlinkParseMicrosAccumWindow = 0;

uint16_t mavlinkSerialAvailableAtStart = 0;
uint16_t mavlinkSerialAvailableMax = 0;
uint16_t mavlinkSerialAvailableMaxWindow = 0;

uint16_t mavlinkParserDrops = 0;

unsigned long mavlinkStatsLastMs = 0;

// ----------------------------------------------------
// Forward declarations
// ----------------------------------------------------

void handleMavlinkMessage();

void decodeMavlinkHeartbeat();
void decodeMavlinkAttitude();
void decodeMavlinkVfrHud();
void decodeMavlinkSysStatus();
void decodeMavlinkGpsRawInt();
void decodeMavlinkRcChannels();
void decodeMavlinkBatteryStatus();

void updateBatteryDerivedValues();
void clearBatteryStatusOnlyFields();
void clearAllBatteryFields();

void updateFlightModeText();
const char* getPlaneModeName(uint32_t customMode);
const char* getCopterModeName(uint32_t customMode);
bool isPlaneType(uint8_t mavType);
bool isCopterType(uint8_t mavType);

void updateFlightTimerFromArmState(bool armedNow);
void serviceFlightTimer();
void resetFlightTimer();

void updateSmoothedAttitude(float targetRoll, float targetPitch, float targetYaw);
float smoothLinearValue(float currentValue, float targetValue, float alpha);
float smoothAngleRadians(float currentAngle, float targetAngle, float alpha);
float wrapAnglePi(float angleRad);

void updateMavlinkDiagnosticStats();

// ----------------------------------------------------
// Setup
// ----------------------------------------------------

void setupMavlinkTelemetry()
{
  MavlinkSerial.setRxBufferSize(2048);

  MavlinkSerial.begin(
    MAVLINK_BAUD,
    SERIAL_8N1,
    MAVLINK_RX_PIN,
    MAVLINK_TX_PIN
  );

  memset(&msg, 0, sizeof(msg));
  memset(&status, 0, sizeof(status));

  roll = 0.0f;
  pitch = 0.0f;
  yaw = 0.0f;

  rawRoll = 0.0f;
  rawPitch = 0.0f;
  rawYaw = 0.0f;

  attitudeSmoothingInitialised = false;
  lastAttitudeSmoothingUs = micros();

  airspeed = 0.0f;
  groundspeed = 0.0f;
  altitude_msl = 0.0f;
  climb_rate = 0.0f;
  heading_deg = 0;

  mavlinkThrottlePercent = -1;
  mavlinkThrottleValid = false;

  vehicleArmed = false;

  heartbeatType = 0;
  heartbeatAutopilot = 0;
  heartbeatBaseMode = 0;
  heartbeatSystemStatus = 0;
  heartbeatCustomMode = 0;

  snprintf(flightModeText, sizeof(flightModeText), "NO HB");

  flightTimerRunning = false;
  flightTimerStartMs = 0;
  flightTimeSeconds = 0;

  clearAllBatteryFields();

  gpsFixType = 0;
  gpsSatellitesVisible = 0;
  gpsLat = 0;
  gpsLon = 0;
  gpsAltMm = 0;
  gpsHdop = 0.0f;
  gpsHdopValid = false;

  for (int i = 0; i < 18; i++) {
    rcChannelRaw[i] = 0;
  }

  rssiPercent = 0;
  rssiValid = false;

  lastMavlinkHeartbeatMs = 0;
  lastMavlinkAttitudeMs = 0;
  lastMavlinkVfrHudMs = 0;
  lastMavlinkGpsMs = 0;
  lastMavlinkRcChannelsMs = 0;

  mavlinkHeartbeatValid = false;
  mavlinkAttitudeValid = false;
  mavlinkVfrHudValid = false;
  mavlinkGpsValid = false;
  mavlinkRcChannelsValid = false;

  mavlinkStatsLastMs = millis();
}

// ----------------------------------------------------
// MAVLink read
// ----------------------------------------------------

void get_mavlink_data()
{
  uint32_t parseStartUs = micros();

  int availableAtStart = MavlinkSerial.available();

  if (availableAtStart < 0) {
    availableAtStart = 0;
  }

  if (availableAtStart > 65535) {
    availableAtStart = 65535;
  }

  mavlinkSerialAvailableAtStart = (uint16_t)availableAtStart;

  if (mavlinkSerialAvailableAtStart > mavlinkSerialAvailableMaxWindow) {
    mavlinkSerialAvailableMaxWindow = mavlinkSerialAvailableAtStart;
  }

  mavlinkParseCallsWindow++;

  while (MavlinkSerial.available() > 0) {
    uint8_t c = (uint8_t)MavlinkSerial.read();

    mavlinkBytesTotal++;
    mavlinkWindowBytes++;

    msgReceived = mavlink_parse_char(
      MAVLINK_COMM_1,
      c,
      &msg,
      &status
    );

    if (msgReceived) {
      mavlinkMessagesTotal++;
      mavlinkWindowMessages++;
      handleMavlinkMessage();
    }
  }

  mavlinkParseMicrosLast = (uint32_t)(micros() - parseStartUs);
  mavlinkParseMicrosAccumWindow += mavlinkParseMicrosLast;

  if (mavlinkParseMicrosLast > mavlinkParseMicrosMaxWindow) {
    mavlinkParseMicrosMaxWindow = mavlinkParseMicrosLast;
  }

  mavlinkParserDrops = status.packet_rx_drop_count;

  unsigned long nowMs = millis();

  if (mavlinkHeartbeatValid && nowMs - lastMavlinkHeartbeatMs > 2000) {
    mavlinkHeartbeatValid = false;
    vehicleArmed = false;
    snprintf(flightModeText, sizeof(flightModeText), "NO HB");
    resetFlightTimer();
  }

  if (mavlinkAttitudeValid && nowMs - lastMavlinkAttitudeMs > 1000) {
    mavlinkAttitudeValid = false;
  }

  if (mavlinkVfrHudValid && nowMs - lastMavlinkVfrHudMs > 1500) {
    mavlinkVfrHudValid = false;
    mavlinkThrottleValid = false;
    mavlinkThrottlePercent = -1;
  }

  if (mavlinkBatteryValid && nowMs - lastMavlinkBatteryMs > 3000) {
    clearAllBatteryFields();
  }

  if (mavlinkBatteryStatusValid &&
      nowMs - lastMavlinkBatteryStatusMs > 3000) {
    clearBatteryStatusOnlyFields();
    updateBatteryDerivedValues();
  }

  if (mavlinkGpsValid && nowMs - lastMavlinkGpsMs > 3000) {
    mavlinkGpsValid = false;
    gpsHdopValid = false;
    gpsHdop = 0.0f;
  }

  if (mavlinkRcChannelsValid && nowMs - lastMavlinkRcChannelsMs > 1500) {
    mavlinkRcChannelsValid = false;
    rssiValid = false;
  }

  serviceFlightTimer();
  updateMavlinkDiagnosticStats();
}

// ----------------------------------------------------
// Message router
// ----------------------------------------------------

void handleMavlinkMessage()
{
  switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT:
      decodeMavlinkHeartbeat();
      break;

    case MAVLINK_MSG_ID_ATTITUDE:
      decodeMavlinkAttitude();
      break;

    case MAVLINK_MSG_ID_VFR_HUD:
      decodeMavlinkVfrHud();
      break;

    case MAVLINK_MSG_ID_SYS_STATUS:
      decodeMavlinkSysStatus();
      break;

    case MAVLINK_MSG_ID_GPS_RAW_INT:
      decodeMavlinkGpsRawInt();
      break;

    case MAVLINK_MSG_ID_RC_CHANNELS:
      decodeMavlinkRcChannels();
      break;

    case MAVLINK_MSG_ID_BATTERY_STATUS:
      decodeMavlinkBatteryStatus();
      break;

    default:
      break;
  }
}

// ----------------------------------------------------
// Decoders
// ----------------------------------------------------

void decodeMavlinkHeartbeat()
{
  mavlink_heartbeat_t heartbeat;
  mavlink_msg_heartbeat_decode(&msg, &heartbeat);

  heartbeatType = heartbeat.type;
  heartbeatAutopilot = heartbeat.autopilot;
  heartbeatBaseMode = heartbeat.base_mode;
  heartbeatSystemStatus = heartbeat.system_status;
  heartbeatCustomMode = heartbeat.custom_mode;

  bool armedNow =
    (heartbeatBaseMode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;

  vehicleArmed = armedNow;
  updateFlightTimerFromArmState(armedNow);
  updateFlightModeText();

  lastMavlinkHeartbeatMs = millis();
  mavlinkHeartbeatValid = true;

  mavlinkHeartbeatTotal++;
  mavlinkWindowHeartbeats++;
}

void decodeMavlinkAttitude()
{
  mavlink_attitude_t attitude;
  mavlink_msg_attitude_decode(&msg, &attitude);

  rawRoll = -attitude.roll;

  rawPitch =
    attitude.pitch +
    (CONFIG_AHI_PITCH_OFFSET_DEG * DEG_TO_RAD);

  rawYaw = attitude.yaw;

  updateSmoothedAttitude(rawRoll, rawPitch, rawYaw);

  lastMavlinkAttitudeMs = millis();
  mavlinkAttitudeValid = true;

  mavlinkAttitudeTotal++;
  mavlinkWindowAttitudes++;
}

void decodeMavlinkVfrHud()
{
  mavlink_vfr_hud_t vfrHud;
  mavlink_msg_vfr_hud_decode(&msg, &vfrHud);

  airspeed = vfrHud.airspeed;
  groundspeed = vfrHud.groundspeed;
  altitude_msl = vfrHud.alt;
  climb_rate = vfrHud.climb;
  heading_deg = vfrHud.heading;

  mavlinkThrottlePercent =
    (int16_t)constrain((int)vfrHud.throttle, 0, 100);

  mavlinkThrottleValid = true;

  lastMavlinkVfrHudMs = millis();
  mavlinkVfrHudValid = true;

  mavlinkVfrHudTotal++;
  mavlinkWindowVfrHud++;
}

void decodeMavlinkSysStatus()
{
  mavlink_sys_status_t sysStatus;
  mavlink_msg_sys_status_decode(&msg, &sysStatus);

  bool gotBatteryData = false;

  if (sysStatus.voltage_battery != UINT16_MAX &&
      sysStatus.voltage_battery > 0) {
    batteryVoltage =
      (float)sysStatus.voltage_battery / 1000.0f;

    batteryVoltageValid = true;
    gotBatteryData = true;
  }

  if (sysStatus.current_battery != -1) {
    batteryCurrentA =
      (float)sysStatus.current_battery / 100.0f;

    batteryCurrentValid = true;
    gotBatteryData = true;
  }

  if (sysStatus.battery_remaining >= 0) {
    batteryRemainingPercent = sysStatus.battery_remaining;
    gotBatteryData = true;
  }

  if (gotBatteryData) {
    lastMavlinkBatteryMs = millis();
    mavlinkBatteryValid = true;
  }

  updateBatteryDerivedValues();

  mavlinkSysStatusTotal++;
  mavlinkWindowSysStatus++;
}

void decodeMavlinkGpsRawInt()
{
  mavlink_gps_raw_int_t gps;
  mavlink_msg_gps_raw_int_decode(&msg, &gps);

  gpsFixType = gps.fix_type;
  gpsSatellitesVisible = gps.satellites_visible;
  gpsLat = gps.lat;
  gpsLon = gps.lon;
  gpsAltMm = gps.alt;

  if (gps.eph != UINT16_MAX && gps.eph > 0) {
    gpsHdop = (float)gps.eph / 100.0f;
    gpsHdopValid = true;
  } else {
    gpsHdop = 0.0f;
    gpsHdopValid = false;
  }

  lastMavlinkGpsMs = millis();
  mavlinkGpsValid = true;

  mavlinkGpsRawTotal++;
  mavlinkWindowGpsRaw++;
}

void decodeMavlinkRcChannels()
{
  mavlink_rc_channels_t rc;
  mavlink_msg_rc_channels_decode(&msg, &rc);

  rcChannelRaw[0]  = rc.chan1_raw;
  rcChannelRaw[1]  = rc.chan2_raw;
  rcChannelRaw[2]  = rc.chan3_raw;
  rcChannelRaw[3]  = rc.chan4_raw;
  rcChannelRaw[4]  = rc.chan5_raw;
  rcChannelRaw[5]  = rc.chan6_raw;
  rcChannelRaw[6]  = rc.chan7_raw;
  rcChannelRaw[7]  = rc.chan8_raw;
  rcChannelRaw[8]  = rc.chan9_raw;
  rcChannelRaw[9]  = rc.chan10_raw;
  rcChannelRaw[10] = rc.chan11_raw;
  rcChannelRaw[11] = rc.chan12_raw;
  rcChannelRaw[12] = rc.chan13_raw;
  rcChannelRaw[13] = rc.chan14_raw;
  rcChannelRaw[14] = rc.chan15_raw;
  rcChannelRaw[15] = rc.chan16_raw;
  rcChannelRaw[16] = rc.chan17_raw;
  rcChannelRaw[17] = rc.chan18_raw;

  if (rc.rssi != UINT8_MAX) {
    rssiPercent =
      (uint8_t)constrain(((int)rc.rssi * 100) / 255, 0, 100);

    rssiValid = true;
  } else {
    rssiValid = false;
  }

  lastMavlinkRcChannelsMs = millis();
  mavlinkRcChannelsValid = true;

  mavlinkRcChannelsTotal++;
  mavlinkWindowRcChannels++;
}

void decodeMavlinkBatteryStatus()
{
  mavlink_battery_status_t batteryStatus;
  mavlink_msg_battery_status_decode(&msg, &batteryStatus);

  bool gotBatteryData = false;

  float packVoltageFromCells = 0.0f;
  float lowestCell = 99.0f;
  uint8_t validCellCount = 0;

  for (int i = 0; i < 10; i++) {
    uint16_t cellMv = batteryStatus.voltages[i];

    if (cellMv != UINT16_MAX && cellMv > 0) {
      float cellV = (float)cellMv / 1000.0f;

      batteryCellVoltages[i] = cellV;
      batteryCellVoltageValid[i] = true;

      packVoltageFromCells += cellV;

      if (cellV < lowestCell) {
        lowestCell = cellV;
      }

      validCellCount++;
    } else {
      batteryCellVoltages[i] = 0.0f;
      batteryCellVoltageValid[i] = false;
    }
  }

  batteryCellCountTelemetry = validCellCount;

  if (validCellCount > 0) {
    batteryVoltage = packVoltageFromCells;
    batteryVoltageValid = true;

    batteryLowestCellVoltage = lowestCell;
    batteryLowestCellVoltageValid = true;

    batteryCellVoltage = batteryLowestCellVoltage;

    gotBatteryData = true;
  }

  if (batteryStatus.current_battery != -1) {
    batteryCurrentA =
      (float)batteryStatus.current_battery / 100.0f;

    batteryCurrentValid = true;
    gotBatteryData = true;
  }

  if (batteryStatus.current_consumed != -1) {
    batteryConsumedMah = (float)batteryStatus.current_consumed;
    batteryConsumedMahValid = true;
    gotBatteryData = true;
  } else {
    batteryConsumedMah = 0.0f;
    batteryConsumedMahValid = false;
  }

  if (batteryStatus.energy_consumed != -1) {
    batteryConsumedWh =
      (float)batteryStatus.energy_consumed / 36.0f;

    batteryConsumedWhValid = true;
    gotBatteryData = true;
  } else {
    batteryConsumedWh = 0.0f;
    batteryConsumedWhValid = false;
  }

  if (batteryStatus.temperature != INT16_MAX) {
    batteryTemperatureC =
      (float)batteryStatus.temperature / 100.0f;

    batteryTemperatureValid = true;
    gotBatteryData = true;
  } else {
    batteryTemperatureC = 0.0f;
    batteryTemperatureValid = false;
  }

  if (batteryStatus.battery_remaining >= 0) {
    batteryRemainingPercent = batteryStatus.battery_remaining;
    gotBatteryData = true;
  }

  if (gotBatteryData) {
    lastMavlinkBatteryMs = millis();
    lastMavlinkBatteryStatusMs = millis();

    mavlinkBatteryValid = true;
    mavlinkBatteryStatusValid = true;
  }

  updateBatteryDerivedValues();

  mavlinkBatteryStatusTotal++;
  mavlinkWindowBatteryStatus++;
}

// ----------------------------------------------------
// Battery helpers
// ----------------------------------------------------

void updateBatteryDerivedValues()
{
  if (!batteryLowestCellVoltageValid &&
      batteryVoltageValid &&
      CONFIG_BATTERY_CELL_COUNT > 0) {
    batteryCellVoltage =
      batteryVoltage / (float)CONFIG_BATTERY_CELL_COUNT;
  }

  if (batteryVoltageValid && batteryCurrentValid) {
    batteryPowerW = batteryVoltage * batteryCurrentA;
    batteryPowerValid = true;
  } else {
    batteryPowerW = 0.0f;
    batteryPowerValid = false;
  }

  if (batteryCurrentValid) {
    if (!batteryMaxCurrentValid ||
        batteryCurrentA > batteryMaxCurrentA) {
      batteryMaxCurrentA = batteryCurrentA;
      batteryMaxCurrentValid = true;
    }
  }

  if (batteryVoltageValid) {
    if (!batteryReferenceVoltageValid ||
        batteryVoltage > batteryReferenceVoltage) {
      batteryReferenceVoltage = batteryVoltage;
      batteryReferenceVoltageValid = true;
    }

    batterySagV = batteryReferenceVoltage - batteryVoltage;

    if (batterySagV < 0.0f) {
      batterySagV = 0.0f;
    }

    batterySagValid = true;
  } else {
    batterySagV = 0.0f;
    batterySagValid = false;
  }
}

void clearBatteryStatusOnlyFields()
{
  mavlinkBatteryStatusValid = false;
  lastMavlinkBatteryStatusMs = 0;

  for (int i = 0; i < 10; i++) {
    batteryCellVoltages[i] = 0.0f;
    batteryCellVoltageValid[i] = false;
  }

  batteryCellCountTelemetry = 0;

  batteryLowestCellVoltage = 0.0f;
  batteryLowestCellVoltageValid = false;

  batteryConsumedMah = 0.0f;
  batteryConsumedMahValid = false;

  batteryConsumedWh = 0.0f;
  batteryConsumedWhValid = false;

  batteryTemperatureC = 0.0f;
  batteryTemperatureValid = false;
}

void clearAllBatteryFields()
{
  batteryVoltage = 0.0f;
  batteryCellVoltage = 0.0f;
  batteryCurrentA = 0.0f;
  batteryPowerW = 0.0f;

  batteryRemainingPercent = -1;

  batteryVoltageValid = false;
  batteryCurrentValid = false;
  batteryPowerValid = false;

  lastMavlinkBatteryMs = 0;
  mavlinkBatteryValid = false;

  clearBatteryStatusOnlyFields();

  batteryMaxCurrentA = 0.0f;
  batteryMaxCurrentValid = false;

  batteryReferenceVoltage = 0.0f;
  batteryReferenceVoltageValid = false;

  batterySagV = 0.0f;
  batterySagValid = false;
}

// ----------------------------------------------------
// Mode decoder
// ----------------------------------------------------

void updateFlightModeText()
{
  const char *modeName = nullptr;

  if (isPlaneType(heartbeatType)) {
    modeName = getPlaneModeName(heartbeatCustomMode);
  }
  else if (isCopterType(heartbeatType)) {
    modeName = getCopterModeName(heartbeatCustomMode);
  }

  if (modeName != nullptr) {
    snprintf(flightModeText, sizeof(flightModeText), "%s", modeName);
  } else {
    snprintf(
      flightModeText,
      sizeof(flightModeText),
      "CUST %lu",
      (unsigned long)heartbeatCustomMode
    );
  }
}

bool isPlaneType(uint8_t mavType)
{
  return mavType == MAV_TYPE_FIXED_WING ||
         mavType == MAV_TYPE_VTOL_TILTROTOR;
}

bool isCopterType(uint8_t mavType)
{
  return mavType == MAV_TYPE_QUADROTOR ||
         mavType == MAV_TYPE_COAXIAL ||
         mavType == MAV_TYPE_HELICOPTER ||
         mavType == MAV_TYPE_HEXAROTOR ||
         mavType == MAV_TYPE_OCTOROTOR ||
         mavType == MAV_TYPE_TRICOPTER;
}

const char* getPlaneModeName(uint32_t customMode)
{
  switch (customMode) {
    case 0:  return "MANUAL";
    case 1:  return "CIRCLE";
    case 2:  return "STAB";
    case 3:  return "TRAIN";
    case 4:  return "ACRO";
    case 5:  return "ANGL";
    case 6:  return "FBWB";
    case 7:  return "CRUISE";
    case 8:  return "AUTOTUNE";
    case 10: return "AUTO";
    case 11: return "RTH";
    case 12: return "LOITER";
    case 13: return "TAKEOFF";
    case 15: return "GUIDED";
    default: return nullptr;
  }
}

const char* getCopterModeName(uint32_t customMode)
{
  switch (customMode) {
    case 0:  return "STAB";
    case 1:  return "ACRO";
    case 2:  return "ALTHOLD";
    case 3:  return "AUTO";
    case 4:  return "GUIDED";
    case 5:  return "LOITER";
    case 6:  return "RTL";
    case 7:  return "CIRCLE";
    case 9:  return "LAND";
    case 11: return "DRIFT";
    case 13: return "SPORT";
    case 15: return "AUTOTUNE";
    case 16: return "POSHOLD";
    case 17: return "BRAKE";
    case 18: return "THROW";
    default: return nullptr;
  }
}

// ----------------------------------------------------
// Flight timer
// ----------------------------------------------------

void updateFlightTimerFromArmState(bool armedNow)
{
  if (armedNow) {
    if (!flightTimerRunning) {
      flightTimerRunning = true;
      flightTimerStartMs = millis();
      flightTimeSeconds = 0;
    }

    serviceFlightTimer();
    return;
  }

  resetFlightTimer();
}

void serviceFlightTimer()
{
  if (!flightTimerRunning) {
    return;
  }

  flightTimeSeconds =
    (uint32_t)((millis() - flightTimerStartMs) / 1000UL);
}

void resetFlightTimer()
{
  flightTimerRunning = false;
  flightTimerStartMs = 0;
  flightTimeSeconds = 0;
}

// ----------------------------------------------------
// Attitude smoothing
// ----------------------------------------------------

void updateSmoothedAttitude(
  float targetRoll,
  float targetPitch,
  float targetYaw
) {
  unsigned long nowUs = micros();

  if (!attitudeSmoothingInitialised) {
    roll = targetRoll;
    pitch = targetPitch;
    yaw = targetYaw;

    attitudeSmoothingInitialised = true;
    lastAttitudeSmoothingUs = nowUs;
    return;
  }

  float dt =
    (float)(nowUs - lastAttitudeSmoothingUs) / 1000000.0f;

  lastAttitudeSmoothingUs = nowUs;

  if (dt <= 0.0f || dt > 0.5f) {
    roll = targetRoll;
    pitch = targetPitch;
    yaw = targetYaw;
    return;
  }

  float rollAlpha =
    1.0f - expf(-dt / MAVLINK_ROLL_SMOOTHING_SECONDS);

  float pitchAlpha =
    1.0f - expf(-dt / MAVLINK_PITCH_SMOOTHING_SECONDS);

  float yawAlpha =
    1.0f - expf(-dt / MAVLINK_YAW_SMOOTHING_SECONDS);

  roll = smoothAngleRadians(roll, targetRoll, rollAlpha);
  pitch = smoothLinearValue(pitch, targetPitch, pitchAlpha);
  yaw = smoothAngleRadians(yaw, targetYaw, yawAlpha);
}

float smoothLinearValue(
  float currentValue,
  float targetValue,
  float alpha
) {
  if (alpha < 0.0f) {
    alpha = 0.0f;
  }

  if (alpha > 1.0f) {
    alpha = 1.0f;
  }

  return currentValue + alpha * (targetValue - currentValue);
}

float smoothAngleRadians(
  float currentAngle,
  float targetAngle,
  float alpha
) {
  if (alpha < 0.0f) {
    alpha = 0.0f;
  }

  if (alpha > 1.0f) {
    alpha = 1.0f;
  }

  float delta = wrapAnglePi(targetAngle - currentAngle);

  return wrapAnglePi(currentAngle + alpha * delta);
}

float wrapAnglePi(float angleRad)
{
  while (angleRad > PI) {
    angleRad -= TWO_PI;
  }

  while (angleRad < -PI) {
    angleRad += TWO_PI;
  }

  return angleRad;
}

// ----------------------------------------------------
// RC helper
// ----------------------------------------------------

uint16_t getMavlinkRcChannelRaw(uint8_t channelNumber)
{
  if (channelNumber < 1 || channelNumber > 18) {
    return 0;
  }

  return rcChannelRaw[channelNumber - 1];
}

// ----------------------------------------------------
// Diagnostic stats
// ----------------------------------------------------

void updateMavlinkDiagnosticStats()
{
  unsigned long nowMs = millis();
  unsigned long elapsedMs = nowMs - mavlinkStatsLastMs;

  if (elapsedMs < 1000) {
    return;
  }

  mavlinkBytesPerSecond =
    (uint32_t)((mavlinkWindowBytes * 1000UL) / elapsedMs);

  mavlinkMessagesPerSecond =
    (uint32_t)((mavlinkWindowMessages * 1000UL) / elapsedMs);

  mavlinkHeartbeatPerSecond =
    (uint32_t)((mavlinkWindowHeartbeats * 1000UL) / elapsedMs);

  mavlinkAttitudePerSecond =
    (uint32_t)((mavlinkWindowAttitudes * 1000UL) / elapsedMs);

  mavlinkVfrHudPerSecond =
    (uint32_t)((mavlinkWindowVfrHud * 1000UL) / elapsedMs);

  mavlinkSysStatusPerSecond =
    (uint32_t)((mavlinkWindowSysStatus * 1000UL) / elapsedMs);

  mavlinkGpsRawPerSecond =
    (uint32_t)((mavlinkWindowGpsRaw * 1000UL) / elapsedMs);

  mavlinkRcChannelsPerSecond =
    (uint32_t)((mavlinkWindowRcChannels * 1000UL) / elapsedMs);

  mavlinkBatteryStatusPerSecond =
    (uint32_t)((mavlinkWindowBatteryStatus * 1000UL) / elapsedMs);

  mavlinkParseCallsPerSecond =
    (uint32_t)((mavlinkParseCallsWindow * 1000UL) / elapsedMs);

  if (mavlinkParseCallsWindow > 0) {
    mavlinkParseMicrosAvg =
      (uint32_t)(mavlinkParseMicrosAccumWindow / mavlinkParseCallsWindow);
  } else {
    mavlinkParseMicrosAvg = 0;
  }

  mavlinkParseMicrosMax = mavlinkParseMicrosMaxWindow;
  mavlinkSerialAvailableMax = mavlinkSerialAvailableMaxWindow;

  mavlinkWindowBytes = 0;
  mavlinkWindowMessages = 0;
  mavlinkWindowHeartbeats = 0;
  mavlinkWindowAttitudes = 0;
  mavlinkWindowVfrHud = 0;
  mavlinkWindowSysStatus = 0;
  mavlinkWindowGpsRaw = 0;
  mavlinkWindowRcChannels = 0;
  mavlinkWindowBatteryStatus = 0;

  mavlinkParseCallsWindow = 0;
  mavlinkParseMicrosAccumWindow = 0;
  mavlinkParseMicrosMaxWindow = 0;
  mavlinkSerialAvailableMaxWindow = 0;

  mavlinkStatsLastMs = nowMs;
}