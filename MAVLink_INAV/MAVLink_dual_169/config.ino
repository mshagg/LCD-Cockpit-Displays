// ----------------------------------------------------
// Project configuration
// ----------------------------------------------------

const float CONFIG_AHI_PITCH_OFFSET_DEG = 0.0f;

const float CONFIG_AHI_ROLL_POINTER_SIGN = 1.0f;

// ----------------------------------------------------
// CH15 main page select
// ----------------------------------------------------

const bool CONFIG_MAIN_PAGE_SELECT_RC_ENABLED = true;
const uint8_t CONFIG_MAIN_PAGE_SELECT_RC_CHANNEL = 15;
const uint16_t CONFIG_MAIN_PAGE_SELECT_RC_LOW_US = 1300;
const uint16_t CONFIG_MAIN_PAGE_SELECT_RC_HIGH_US = 1700;
const unsigned long CONFIG_MAIN_PAGE_SELECT_MIN_INTERVAL_MS = 300;

// ----------------------------------------------------
// CH16 secondary page select
// ----------------------------------------------------

const bool CONFIG_SECONDARY_PAGE_SELECT_RC_ENABLED = true;
const uint8_t CONFIG_SECONDARY_PAGE_SELECT_RC_CHANNEL = 16;
const uint16_t CONFIG_SECONDARY_PAGE_SELECT_RC_LOW_US = 1300;
const uint16_t CONFIG_SECONDARY_PAGE_SELECT_RC_HIGH_US = 1700;
const unsigned long CONFIG_SECONDARY_PAGE_SELECT_MIN_INTERVAL_MS = 300;

// ----------------------------------------------------
// Backward aliases
// ----------------------------------------------------

const bool CONFIG_PAGE_SELECT_RC_ENABLED = CONFIG_MAIN_PAGE_SELECT_RC_ENABLED;
const uint8_t CONFIG_PAGE_SELECT_RC_CHANNEL = CONFIG_MAIN_PAGE_SELECT_RC_CHANNEL;
const uint16_t CONFIG_PAGE_SELECT_RC_LOW_US = CONFIG_MAIN_PAGE_SELECT_RC_LOW_US;
const uint16_t CONFIG_PAGE_SELECT_RC_HIGH_US = CONFIG_MAIN_PAGE_SELECT_RC_HIGH_US;
const unsigned long CONFIG_PAGE_SELECT_MIN_INTERVAL_MS = CONFIG_MAIN_PAGE_SELECT_MIN_INTERVAL_MS;

// ----------------------------------------------------
// Secondary display
// ----------------------------------------------------

const unsigned long CONFIG_SECONDARY_DISPLAY_UPDATE_MS = 1000;

// ----------------------------------------------------
// Secondary page IDs
// ----------------------------------------------------
//
// Screen order:
//   0 = Preflight
//   1 = Flight status
//   2 = Navigation
//   3 = Battery numeric
//   4 = Battery graphical
//   5 = Gear / flaps
//   6 = Diagnostics
// ----------------------------------------------------

const uint8_t CONFIG_SECONDARY_PAGE_ID_PREFLIGHT           = 0;
const uint8_t CONFIG_SECONDARY_PAGE_ID_FLIGHT_STATUS       = 1;
const uint8_t CONFIG_SECONDARY_PAGE_ID_NAVIGATION          = 2;
const uint8_t CONFIG_SECONDARY_PAGE_ID_BATTERY             = 3;
const uint8_t CONFIG_SECONDARY_PAGE_ID_BATTERY_GRAPHICAL   = 4;
const uint8_t CONFIG_SECONDARY_PAGE_ID_GEAR_FLAPS          = 5;
const uint8_t CONFIG_SECONDARY_PAGE_ID_DIAGNOSTICS         = 6;

//
// Default page on boot.
// ----------------------------------------------------

const uint8_t CONFIG_SECONDARY_DEFAULT_PAGE =
  CONFIG_SECONDARY_PAGE_ID_PREFLIGHT;

// ----------------------------------------------------
// Secondary page availability
// ----------------------------------------------------
//
// Set false to remove a page from the CH16 page cycle.
// Disabled pages remain compiled, but are skipped.
// ----------------------------------------------------

const bool CONFIG_SECONDARY_PAGE_FLIGHT_STATUS_ENABLED = true;
const bool CONFIG_SECONDARY_PAGE_NAVIGATION_ENABLED    = true;
const bool CONFIG_SECONDARY_PAGE_BATTERY_ENABLED       = true;
const bool CONFIG_SECONDARY_PAGE_GEAR_FLAPS_ENABLED    = true;
const bool CONFIG_SECONDARY_PAGE_PREFLIGHT_ENABLED     = true;
const bool CONFIG_SECONDARY_PAGE_DIAGNOSTICS_ENABLED   = false;
const bool CONFIG_SECONDARY_PAGE_BATTERY_GRAPHICAL_ENABLED = true;

// ----------------------------------------------------
// Preflight page checks
// ----------------------------------------------------

const uint8_t CONFIG_PREFLIGHT_MIN_GPS_SATS = 6;

const bool CONFIG_PREFLIGHT_REQUIRE_3D_GPS = true;
const bool CONFIG_PREFLIGHT_REQUIRE_GEAR_DOWN = true;
const bool CONFIG_PREFLIGHT_REQUIRE_FLAPS_UP = true;

// ----------------------------------------------------
// Gear / flaps secondary page
// ----------------------------------------------------
//
// Channel numbers are 1-based RC channel numbers.
//
// Gear:
//   low  = retracted
//   high = extended
//
// Flaps:
//   low  = flaps up
//   mid  = half flaps
//   high = full flaps
// ----------------------------------------------------

const uint8_t CONFIG_GEAR_FLAPS_GEAR_RC_CHANNEL = 5;
const uint8_t CONFIG_GEAR_FLAPS_FLAP_RC_CHANNEL = 6;

const uint16_t CONFIG_GEAR_FLAPS_RC_VALID_LOW_US = 800;
const uint16_t CONFIG_GEAR_FLAPS_RC_VALID_HIGH_US = 2200;

const uint16_t CONFIG_GEAR_RETRACTED_US = 1000;
const uint16_t CONFIG_GEAR_EXTENDED_US = 2000;

const uint16_t CONFIG_FLAPS_UP_US = 1000;
const uint16_t CONFIG_FLAPS_HALF_US = 1500;
const uint16_t CONFIG_FLAPS_FULL_US = 2000;

// ----------------------------------------------------
// Battery / RSSI
// ----------------------------------------------------

const uint8_t CONFIG_BATTERY_CELL_COUNT = 4;
const float CONFIG_BATTERY_CELL_WARN_V = 3.60f;
const float CONFIG_BATTERY_CELL_CRITICAL_V = 3.40f;

const bool CONFIG_RSSI_USE_RC_CHANNEL = false;
const uint8_t CONFIG_RSSI_RC_CHANNEL = 0;
const uint16_t CONFIG_RSSI_RC_LOW_US = 1000;
const uint16_t CONFIG_RSSI_RC_HIGH_US = 2000;

// ----------------------------------------------------
// Units
// ----------------------------------------------------

const float CONFIG_GLASS_SPEED_SCALE = 3.6f;
const char *CONFIG_GLASS_SPEED_LABEL = "KMH";

const float CONFIG_GLASS_ALT_SCALE = 1.0f;
const char *CONFIG_GLASS_ALT_LABEL = "M";

// ----------------------------------------------------
// Glass cockpit / PFD tape config
// ----------------------------------------------------

const float CONFIG_GLASS_AHI_PITCH_PIXELS_PER_DEG = 3.6f;

const float CONFIG_GLASS_SPEED_HALF_RANGE = 40.0f;
const float CONFIG_GLASS_SPEED_MAJOR_STEP = 20.0f;
const float CONFIG_GLASS_SPEED_MINOR_STEP = 10.0f;

const float CONFIG_GLASS_ALT_HALF_RANGE = 100.0f;
const float CONFIG_GLASS_ALT_MAJOR_STEP = 50.0f;
const float CONFIG_GLASS_ALT_MINOR_STEP = 25.0f;

const int CONFIG_GLASS_HEADING_TAPE_HEIGHT = 28;
const float CONFIG_GLASS_HEADING_HALF_RANGE_DEG = 30.0f;
const int CONFIG_GLASS_HEADING_MAJOR_STEP_DEG = 10;
const int CONFIG_GLASS_HEADING_MINOR_STEP_DEG = 5;

// ----------------------------------------------------
// Classic instrument / airspeed gauge
// ----------------------------------------------------
//
// Speeds are in displayed units, currently KMH.
// ----------------------------------------------------

const float CONFIG_CLASSIC_ASI_MAX_SPEED = 140.0f;
const float CONFIG_CLASSIC_ASI_STALL_SPEED = 35.0f;
const float CONFIG_CLASSIC_ASI_OPERATING_MAX_SPEED = 95.0f;
const float CONFIG_CLASSIC_ASI_NEVER_EXCEED_SPEED = 150.0f;
