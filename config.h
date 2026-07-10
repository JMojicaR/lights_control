#ifndef CONFIG_H
#define CONFIG_H

// ── WiFi ────────────────────────────────────────
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"

// ── Location & Timezone ─────────────────────────
// Used for the sunset API (sunrise-sunset.org)
#define LATITUDE        19.4326    // Mexico City (change to your location)
#define LONGITUDE       -99.1332
#define TIMEZONE        "America/Mexico_City"

// ── HTTP API Endpoints ──────────────────────────
// timeapi.io — returns current time in JSON (free, no API key)
#define TIME_API_URL    "https://timeapi.io/api/v1/time/current/zone?timezone="
// sunrise-sunset.org — returns sunrise/sunset times (free, no API key)
#define SUNSET_API_URL  "https://api.sunrise-sunset.org/json"

// ── Pins ────────────────────────────────────────
#define PIR_PIN         4       // HC-SR501 motion sensor (digital)
#define LED_MOSFET_PIN  5       // IRLZ44N gate for 12V LED strip
#define STATUS_LED_PIN  2       // Built-in LED (2 = most ESP32-S3 SuperMini)

// BH1750 uses I²C (default pins on ESP32-S3 SuperMini)
#define I2C_SDA         21
#define I2C_SCL         22

// ── Light & Motion Thresholds ───────────────────
#define LUX_THRESHOLD       30    // Lux below this = "dark enough" for lights
#define LIGHT_DURATION_SEC  120   // Keep lights ON this many seconds after last motion
#define MOTION_DEBOUNCE_MS  2000  // Ignore motion re-triggers within this window
#define PIR_RETRIGGER       false // HC-SR501: false = single trigger, true = repeat

// ── Timing ──────────────────────────────────────
#define SENSOR_POLL_MS      250   // How often to read sensors (milliseconds)
#define TIME_RESYNC_MIN     60    // Re-sync time from HTTP every N minutes
#define SUNSET_RESYNC_MIN   360   // Re-sync sunset from HTTP every 6 hours

// ── Sensor Calibration ──────────────────────────
// BH1750 modes (from BH1750::Mode enum):
//   BH1750::CONTINUOUS_HIGH_RES_MODE  — 1 lux resolution, 120ms
//   BH1750::CONTINUOUS_HIGH_RES_MODE2 — 0.5 lux resolution, 120ms
//   BH1750::CONTINUOUS_LOW_RES_MODE   — 4 lux resolution, 16ms
#define LIGHT_SENSOR_MODE  BH1750::CONTINUOUS_HIGH_RES_MODE

#endif
