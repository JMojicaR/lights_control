#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ────────────────────────────────────────
#define WIFI_SSID       "SSID"
#define WIFI_PASSWORD   "PASSWORD"

// ── Location & Timezone ─────────────────────────
// Used for the sunset API (sunrise-sunset.org)
#define LATITUDE        19.4326    // Mexico City (change to your location)
#define LONGITUDE       -99.1332
#define TIMEZONE        "America/Mexico_City"

// ── HTTP API Endpoints ──────────────────────────
#define TIME_API_URL    "https://timeapi.io/api/v1/time/current/zone?timezone="
#define SUNSET_API_URL  "https://api.sunrise-sunset.org/json"

// ── Pins ────────────────────────────────────────
// Presence: two HLK-LD2410B mmWave radars (24 GHz). Their "OUT" pin is HIGH
// when a person is present — moving OR stationary (the key advantage over PIR
// and the reason they replace the VL53L0X ToF sensors).
#define RADAR_BOTTOM_PIN   4   // bottom radar OUT → ESP32 GPIO 4
#define RADAR_TOP_PIN      6   // top radar OUT    → ESP32 GPIO 6
#define LED_MOSFET_PIN     5   // IRLZ44N gate for 12V LED strip
#define STATUS_LED_PIN     2   // Built-in LED (2 = most ESP32-S3 SuperMini)

// Ambient light: LDR photoresistor divider tap (brighter = higher ADC reading).
// Wiring: 3V3 ── LDR ──┬── GPIO1 (ADC) ──┐
//                      └── 10 kΩ ────────┴── GND
#define LDR_PIN            1         // ADC1 channel 0 (GPIO 1)
#define LDR_MAX_LUX        1000.0f   // full-scale rough-lux for the ADC mapping

// Optional: radar UART — used to read distance (cm) for the dashboard.
// Set USE_RADAR_UART 0 in the sketch if you only want the presence pin.
#define RADAR_UART_BAUD    256000    // HLK-LD2410 default
#define RADAR_BOTTOM_RX    7         // ESP32 RX ← bottom radar TX
#define RADAR_TOP_RX       8         // ESP32 RX ← top radar TX

// ── Light & Motion Thresholds ───────────────────
// LUX_THRESHOLD compares against the LDR's *scaled* lux (0 .. LDR_MAX_LUX).
// Calibrate: read the dashboard lux value in a "dark enough" room and set this
// just above it (lower = darker = more sensitive).
#define LUX_THRESHOLD       30    // Scaled "dark enough" threshold (LDR, calibrate)
#define DEFAULT_LIGHT_DURATION_SEC  90  // Keep lights ON this many seconds after last presence
#define MOTION_DEBOUNCE_MS          2000  // Ignore re-triggers within this window

// ── Timing ──────────────────────────────────────
#define POLL_INTERVAL_DEFAULT_SEC  5    // LDR polling interval (configurable via HTTP)
#define POLL_INTERVAL_MIN_SEC      1
#define POLL_INTERVAL_MAX_SEC      300
#define FADE_TICK_MS            250   // PWM fade step interval
#define LOOP_DELAY_MS           10    // Base loop delay
#define TIME_RESYNC_MIN     60    // Re-sync time from HTTP every N minutes
#define SUNSET_RESYNC_MIN   360   // Re-sync sunset from HTTP every 6 hours

// ── PWM Fade ─────────────────────────────────────
#define PWM_CHANNEL         0     // LEDC channel for MOSFET PWM
#define PWM_FREQ            5000  // PWM frequency (Hz) — above audible range
#define PWM_RES             8     // 8-bit resolution = 0-255
#define FADE_STEP           8     // Duty change per tick
#define FADE_ON_MS          500
#define FADE_OFF_MS         1500

// ── Duration limits (for HTTP endpoint) ──────────
#define DURATION_MIN_SEC    5
#define DURATION_MAX_SEC    1800  // 30 min

// ── Legacy distance-threshold limits (kept for API compatibility) ──
// With mmWave radar there is no code-configurable presence threshold (the
// radar's sensitivity is set via its own HLK app). These remain only so the
// /api/distance endpoint still validates; they no longer affect sensing.
#define DISTANCE_DEFAULT_MM      70
#define DISTANCE_MIN_MM          30
#define DISTANCE_MAX_MM          150

#endif
