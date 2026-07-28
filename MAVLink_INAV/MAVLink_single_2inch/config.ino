// ----------------------------------------------------
// Project configuration
// ----------------------------------------------------
//
// Standalone ESP32-S3 + 2.0 inch ST7789VW fork
//
// MAVLink UART pins and baud are intentionally defined
// in mavlink_telemetry.ino, not here.
// ----------------------------------------------------

// ----------------------------------------------------
// Display configuration
// ----------------------------------------------------

#define CONFIG_DISPLAY_UPDATE_MS  16

// Full-frame LCD SPI clock. The ST7789 and wiring must be visually
// verified at this rate; use 40000000UL as the conservative baseline.
extern const uint32_t CONFIG_LCD_SPI_HZ = 80000000UL;

// ----------------------------------------------------
// AHI configuration
// ----------------------------------------------------

const float CONFIG_AHI_PITCH_OFFSET_DEG = 0.0f;
const float CONFIG_AHI_ROLL_POINTER_SIGN = 1.0f;

// MAVLink roll sign correction.
//
// Use one of these only:
//   1.0f  = normal MAVLink roll direction
//  -1.0f  = reversed MAVLink roll direction
//
// This affects the actual roll value used by all screens.
extern const float CONFIG_MAVLINK_ROLL_SIGN = 1.0f;

// ----------------------------------------------------
// Battery configuration
// ----------------------------------------------------

const uint8_t CONFIG_BATTERY_CELL_COUNT = 4;

// ----------------------------------------------------
// Units
// ----------------------------------------------------

const float CONFIG_GLASS_SPEED_SCALE = 3.6f;
const char *CONFIG_GLASS_SPEED_LABEL = "KMH";

const float CONFIG_GLASS_ALT_SCALE = 1.0f;
const char *CONFIG_GLASS_ALT_LABEL = "M";

// ----------------------------------------------------
// Analogue gauge ranges
// ----------------------------------------------------
//
// End-points for the analogue gauge page.
// Change these here rather than inside screen_analogue_gauges.ino.

extern const float CONFIG_ANALOGUE_SPEED_MIN_KMH = 0.0f;
extern const float CONFIG_ANALOGUE_SPEED_MAX_KMH = 200.0f;

extern const float CONFIG_ANALOGUE_ALT_MIN_M = 0.0f;
extern const float CONFIG_ANALOGUE_ALT_MAX_M = 250.0f;

extern const float CONFIG_ANALOGUE_CELL_MIN_V = 3.0f;
extern const float CONFIG_ANALOGUE_CELL_MAX_V = 4.2f;

extern const float CONFIG_ANALOGUE_CURRENT_MIN_A = 0.0f;
extern const float CONFIG_ANALOGUE_CURRENT_MAX_A = 100.0f;

extern const float CONFIG_ANALOGUE_REMAIN_MIN_PERCENT = 0.0f;
extern const float CONFIG_ANALOGUE_REMAIN_MAX_PERCENT = 100.0f;
