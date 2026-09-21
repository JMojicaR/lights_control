/**
 * Staircase Light Controller — ESP32-S3 SuperMini (mmWave radar + LDR option)
 * ===========================================================================
 *
 * Automatically controls a 12V LED strip on a 5m staircase based on:
 *   1. Presence detection (HLK-LD2410B mmWave radar — moving OR stationary)
 *   2. Ambient light level (LDR photoresistor)
 *   3. Time of day vs sunset (HTTP APIs)
 *
 * Lights turn ON when: person detected on stairs AND it's dark AND past sunset.
 * Presence comes from each radar's OUT pin (HIGH = present) — no I²C bus, no
 * address collision, no pull-up tuning. The radar detects a person who is
 * completely still (micro-motion), which PIR cannot.
 *
 * Web Dashboard: http://<esp32-ip>/
 *   - Live status: lux (scaled), distance (cm), lights, time, sunset
 *   - Manual override: force ON / OFF / AUTO
 *
 * APIs used:
 *   - timeapi.io          (https://timeapi.io)        — current time
 *   - sunrise-sunset.org                              — sunset/sunrise times
 *
 * Hardware:
 *   - ESP32-S3 SuperMini
 *   - HLK-LD2410B mmWave radar — bottom (OUT GPIO 4, UART RX GPIO 7)
 *   - HLK-LD2410B mmWave radar — top    (OUT GPIO 6, UART RX GPIO 8)
 *   - LDR photoresistor divider (ADC GPIO 1)
 *   - IRLZ44N MOSFET switching 12V LED strip (GPIO 5)
 *   - 12V DC power supply (≥6A for 5m strip)
 */

#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// Feature flag: read radar distance (cm) over UART for the dashboard.
// Set to 0 if you only want the presence OUT pin (simplest wiring).
#define USE_RADAR_UART  1

// ── Objects ─────────────────────────────────────
WebServer server(80);
Preferences prefs;

const char* PREF_NAMESPACE = "stairs";
const char* PREF_KEY_DURATION = "dur_sec";
const char* PREF_KEY_DIST_BOTTOM = "dist_bottom_mm";
const char* PREF_KEY_DIST_TOP = "dist_top_mm";
const char* PREF_KEY_POLL = "poll_sec";

// ── Time tracking ───────────────────────────────
unsigned long lastTimeSync   = 0;      // millis() of last HTTP time sync
unsigned long lastSunsetSync = 0;      // millis() of last sunset sync
time_t        currentEpoch   = 0;      // current Unix time (from HTTP)
time_t        sunsetEpoch    = 0;      // today's sunset Unix time
time_t        sunriseEpoch   = 0;      // today's sunrise Unix time
bool          timeValid      = false;
bool          sunsetValid    = false;
int           localUtcOffsetSec = -6 * 3600; // Updated from time API (Mexico City offset)
const unsigned long TIME_RESYNC_MS   = TIME_RESYNC_MIN  * 60000UL;
const unsigned long SUNSET_RESYNC_MS = SUNSET_RESYNC_MIN * 60000UL;

// ── Presence tracking (via mmWave radar OUT pins) ──
bool     presenceBottom  = false;
bool     presenceTop     = false;
uint16_t distanceBottom  = 0;        // cm (from radar UART, 0 if not parsed)
uint16_t distanceTop     = 0;        // cm
unsigned long lastMotionTime = 0;
unsigned long motionDebounceUntil = 0;

// ── Light state ─────────────────────────────────
bool     lightsOn        = false;
unsigned long lightsOnSince  = 0;

// ── PWM Fade ────────────────────────────────────
int      currentDuty     = 0;     // Current PWM duty cycle (0-255)
int      targetDuty      = 0;     // Desired PWM duty cycle
unsigned long fadeStartMs = 0;    // When the current fade began

// ── Dynamic light duration (changeable via HTTP) ─
unsigned long configuredDurationSec = DEFAULT_LIGHT_DURATION_SEC; // Used for new sessions
unsigned long activeDurationSec     = DEFAULT_LIGHT_DURATION_SEC; // Locked for current session

// ── Dynamic presence distance thresholds (changeable via HTTP) ─
unsigned long configuredDistanceBottomMm = DISTANCE_DEFAULT_MM;  // bottom sensor presence threshold
unsigned long configuredDistanceTopMm    = DISTANCE_DEFAULT_MM;  // top sensor presence threshold

// ── Dynamic sensor polling interval (changeable via HTTP) ─
unsigned long configuredPollIntervalSec = POLL_INTERVAL_DEFAULT_SEC;  // seconds between sensor reads

// ── Manual override ─────────────────────────────
//  0 = AUTO (use sensor/sunset logic)
//  1 = FORCE ON
// -1 = FORCE OFF
int      overrideMode    = 0;

// ── Sensor readings ─────────────────────────────
float    lux             = 0.0;

// ── Uptime tracking ─────────────────────────────
unsigned long bootMillis = 0;

// ─────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("\n=== Staircase Light Controller ===\n");

    // Load persisted settings (fallback to compile-time defaults).
    if (prefs.begin(PREF_NAMESPACE, true)) {
      unsigned long savedDur = prefs.getULong(PREF_KEY_DURATION, DEFAULT_LIGHT_DURATION_SEC);
      unsigned long savedBottom = prefs.getULong(PREF_KEY_DIST_BOTTOM, DISTANCE_DEFAULT_MM);
      unsigned long savedTop = prefs.getULong(PREF_KEY_DIST_TOP, DISTANCE_DEFAULT_MM);
      unsigned long savedPoll = prefs.getULong(PREF_KEY_POLL, POLL_INTERVAL_DEFAULT_SEC);
      prefs.end();

      if (savedDur >= DURATION_MIN_SEC && savedDur <= DURATION_MAX_SEC) {
        configuredDurationSec = savedDur;
      } else {
        configuredDurationSec = DEFAULT_LIGHT_DURATION_SEC;
        Serial.printf("[⚠] Stored duration invalid (%lu) — using default %lu\n",
                savedDur, configuredDurationSec);
      }

      configuredDistanceBottomMm = (savedBottom >= DISTANCE_MIN_MM && savedBottom <= DISTANCE_MAX_MM)
                                   ? savedBottom : DISTANCE_DEFAULT_MM;
      configuredDistanceTopMm    = (savedTop >= DISTANCE_MIN_MM && savedTop <= DISTANCE_MAX_MM)
                                   ? savedTop : DISTANCE_DEFAULT_MM;
      configuredPollIntervalSec  = (savedPoll >= POLL_INTERVAL_MIN_SEC && savedPoll <= POLL_INTERVAL_MAX_SEC)
                                   ? savedPoll : POLL_INTERVAL_DEFAULT_SEC;
    } else {
      configuredDurationSec = DEFAULT_LIGHT_DURATION_SEC;
      Serial.printf("[⚠] Preferences unavailable — using default duration %lu\n",
              configuredDurationSec);
    }
    activeDurationSec = configuredDurationSec;
    Serial.printf("[⚙] Loaded: duration %lus | dist bottom %lumm | dist top %lumm | poll %lus\n",
                  configuredDurationSec, configuredDistanceBottomMm,
                  configuredDistanceTopMm, configuredPollIntervalSec);

    // Pins — LED MOSFET uses PWM for fade
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // ── Presence radars (HLK-LD2410B) + LDR ambient light ──
    // Each radar's OUT pin is HIGH when a person is present (moving or
    // stationary). The LDR divider is read on the ADC. No I²C bus, no address
    // collision, no pull-up tuning — the whole point of this option.
    pinMode(RADAR_BOTTOM_PIN, INPUT_PULLDOWN);
    pinMode(RADAR_TOP_PIN, INPUT_PULLDOWN);
    analogReadResolution(12);
    pinMode(LDR_PIN, INPUT);

#if USE_RADAR_UART
    // Radar UART (distance in cm for the dashboard) — receive-only.
    Serial1.begin(RADAR_UART_BAUD, SERIAL_8N1, RADAR_BOTTOM_RX, -1);
    Serial2.begin(RADAR_UART_BAUD, SERIAL_8N1, RADAR_TOP_RX, -1);
    Serial.println("[📡] Radar UART enabled (distance reporting)");
#endif

    // PWM setup for LED MOSFET (8-bit, 5 kHz — silent, smooth fade)
    ledcAttach(LED_MOSFET_PIN, PWM_FREQ, PWM_RES);
    ledcWrite(LED_MOSFET_PIN, 0);

    // WiFi
    connectWiFi();

    // Initial time sync
    syncTime();
    syncSunset();

    // ── Web server routes ─────────────────────────
    server.on("/", handleRoot);
    server.on("/api", handleAPI);
    server.on("/api/override", handleOverride);
    server.on("/api/duration", handleDuration);
    server.on("/api/distance", handleDistance);
    server.on("/api/poll", handlePoll);
    server.onNotFound([]() {
        server.send(404, "application/json", "{\"error\":\"not found\"}");
    });
    server.begin();
    bootMillis = millis();
    Serial.printf("[🌐] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());

    Serial.println("\n--- Ready ---\n");
}

// ─────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────
void loop() {
    server.handleClient();  // non-blocking — serves web requests (runs every loop)

    unsigned long now = millis();

    // ── Periodic HTTP time re-sync ───────────────
    if (now - lastTimeSync >= TIME_RESYNC_MS || !timeValid) {
        syncTime();
    }

    // ── Periodic sunset re-sync ──────────────────
    if (now - lastSunsetSync >= SUNSET_RESYNC_MS || !sunsetValid) {
        syncSunset();
    }

    // ── Presence (mmWave radar OUT pins) + presence timeout ──
    processPresence();

    // ── Ambient light (LDR) polling (configurable interval, default 5s) ──
    static unsigned long lastLuxPoll = 0;
    unsigned long luxPollMs = configuredPollIntervalSec * 1000UL;
    if (lastLuxPoll == 0 || now - lastLuxPoll >= luxPollMs) {
        lastLuxPoll = now;
        readLux();
    }

#if USE_RADAR_UART
    // ── Radar distance (cm) for the dashboard ──
    readRadarDistance();
#endif

    // ── Decision logic + light control (every loop — keeps override responsive) ──
    setLights(evaluate());

    // ── PWM fade tick (fixed cadence → smooth fade regardless of polling) ──
    static unsigned long lastFadeTick = 0;
    if (now - lastFadeTick >= FADE_TICK_MS) {
        lastFadeTick = now;
        updateFade();
    }

    // ── Status LED heartbeat ─────────────────────
    digitalWrite(STATUS_LED_PIN, lightsOn ? HIGH : (now / 1000) % 2);

    // ── Serial report every 5 seconds ────────────
    static unsigned long lastReport = 0;
    if (now - lastReport >= 5000) {
        lastReport = now;
        printStatus();
    }

    delay(LOOP_DELAY_MS);
}

// ═════════════════════════════════════════════════
// WiFi
// ═════════════════════════════════════════════════
void connectWiFi() {
    Serial.printf("WiFi: connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[✓] WiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[✗] WiFi failed — will retry");
    }
}

// ═════════════════════════════════════════════════
// Time Helpers (UTC-safe)
// ═════════════════════════════════════════════════

long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

time_t buildUtcEpoch(int year, int month, int day, int hour, int minute, int second) {
  long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
  long long secs = (long long)days * 86400LL +
           (long long)hour * 3600LL +
           (long long)minute * 60LL +
           (long long)second;
  return (time_t)secs;
}

int parseIsoOffsetSeconds(const String& iso) {
  int n = iso.length();
  if (n >= 1 && iso.charAt(n - 1) == 'Z') {
    return 0;
  }
  if (n >= 6 && (iso.charAt(n - 6) == '+' || iso.charAt(n - 6) == '-') && iso.charAt(n - 3) == ':') {
    int sign = (iso.charAt(n - 6) == '-') ? -1 : 1;
    int offH = atoi(iso.substring(n - 5, n - 3).c_str());
    int offM = atoi(iso.substring(n - 2, n).c_str());
    return sign * ((offH * 3600) + (offM * 60));
  }
  return 0;
}

time_t isoToEpoch(const char* iso) {
  String s = String(iso);
  if (s.length() < 19) return 0;

  int year   = atoi(s.substring(0, 4).c_str());
  int month  = atoi(s.substring(5, 7).c_str());
  int day    = atoi(s.substring(8, 10).c_str());
  int hour   = atoi(s.substring(11, 13).c_str());
  int minute = atoi(s.substring(14, 16).c_str());
  int second = atoi(s.substring(17, 19).c_str());
  int offsetSec = parseIsoOffsetSeconds(s);

  // ISO timestamp encodes local wall time + offset. Convert to UTC epoch.
  time_t wallEpoch = buildUtcEpoch(year, month, day, hour, minute, second);
  return wallEpoch - offsetSec;
}

void formatHHMMFromEpoch(time_t epochUtc, int utcOffsetSec, char* out, size_t outSize) {
  time_t localEpoch = epochUtc + utcOffsetSec;
  struct tm tmUtc = {};
  gmtime_r(&localEpoch, &tmUtc);
  strftime(out, outSize, "%H:%M", &tmUtc);
}

void formatHHMMSSFromEpoch(time_t epochUtc, int utcOffsetSec, char* out, size_t outSize) {
  time_t localEpoch = epochUtc + utcOffsetSec;
  struct tm tmUtc = {};
  gmtime_r(&localEpoch, &tmUtc);
  strftime(out, outSize, "%H:%M:%S", &tmUtc);
}

// Local minutes-since-midnight for a UTC epoch (0..1439).
// Time-of-day comparisons are immune to the calendar-date offset between UTC
// and local time (a western timezone's sunset lands on the *next* UTC date).
int minutesOfDay(time_t epochUtc, int utcOffsetSec) {
  time_t localEpoch = epochUtc + utcOffsetSec;
  struct tm tmUtc = {};
  gmtime_r(&localEpoch, &tmUtc);
  return tmUtc.tm_hour * 60 + tmUtc.tm_min;
}

// Local calendar date (YYYY-MM-DD) for a UTC epoch — used to request today's
// sunrise/sunset from the API instead of letting it default to the UTC date.
void formatLocalDate(time_t epochUtc, int utcOffsetSec, char* out, size_t outSize) {
  time_t localEpoch = epochUtc + utcOffsetSec;
  struct tm tmUtc = {};
  gmtime_r(&localEpoch, &tmUtc);
  strftime(out, outSize, "%Y-%m-%d", &tmUtc);
}

// ═════════════════════════════════════════════════
// HTTP: Sync current time from timeapi.io
// ═════════════════════════════════════════════════
void syncTime() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        if (WiFi.status() != WL_CONNECTED) return;
    }

    HTTPClient http;
    String url = String(TIME_API_URL) + TIMEZONE;
    http.begin(url);
    http.setTimeout(8000);

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err) {
            const char* dt = doc["date_time"];       // "2026-07-10T16:19:26.627199-06:00"
            int utcOffset = doc["utc_offset_seconds"];// seconds offset from UTC
          bool dstActive = doc["dst_active"];

          time_t parsedEpoch = isoToEpoch(dt);
          if (parsedEpoch == 0) {
            Serial.println("[✗] Time parse error");
            http.end();
            return;
          }

          currentEpoch = parsedEpoch;
          localUtcOffsetSec = utcOffset;
            timeValid = true;
            lastTimeSync = millis();

          char localNow[16];
          formatHHMMSSFromEpoch(currentEpoch, localUtcOffsetSec, localNow, sizeof(localNow));
          Serial.printf("[⏰] Time synced: %s | Local: %s | UTC offset: %+d h (DST=%s)\n",
                  dt, localNow, utcOffset / 3600, dstActive ? "yes" : "no");
        } else {
            Serial.printf("[✗] Time JSON parse error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[✗] Time HTTP %d\n", code);
    }
    http.end();
}

// ═════════════════════════════════════════════════
// HTTP: Sync sunset/sunrise from sunrise-sunset.org
// ═════════════════════════════════════════════════
void syncSunset() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        if (WiFi.status() != WL_CONNECTED) return;
    }

    HTTPClient http;
    String url = String(SUNSET_API_URL) +
                 "?lat=" + String(LATITUDE, 4) +
                 "&lng=" + String(LONGITUDE, 4) +
                 "&formatted=0";  // return ISO 8601 UTC

    // Request today's LOCAL date explicitly. Without this the API defaults to
    // the current UTC date — for a UTC-6 timezone the local evening is already
    // the *next* UTC day, so it would return tomorrow's sunrise/sunset and the
    // night check would fire before sunset.
    if (timeValid) {
        char localDate[16];
        formatLocalDate(currentEpoch, localUtcOffsetSec, localDate, sizeof(localDate));
        url += "&date=" + String(localDate);
    }

    http.begin(url);
    http.setTimeout(8000);

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (!err && doc["status"] == "OK") {
            const char* sunsetStr  = doc["results"]["sunset"];   // "2026-06-27T01:15:00+00:00"
            const char* sunriseStr = doc["results"]["sunrise"];  // "2026-06-26T12:00:00+00:00"

            sunsetEpoch  = isoToEpoch(sunsetStr);
            sunriseEpoch = isoToEpoch(sunriseStr);
            sunsetValid  = true;
            lastSunsetSync = millis();

          char sunsetLocal[8];
          formatHHMMFromEpoch(sunsetEpoch, localUtcOffsetSec, sunsetLocal, sizeof(sunsetLocal));

          Serial.printf("[🌅] Sunset API(UTC): %s | Sunset local(MX): %s | Sunrise API(UTC): %s\n",
                  sunsetStr, sunsetLocal, sunriseStr);
        } else {
            Serial.printf("[✗] Sunset JSON error: %s\n", err.c_str());
        }
    } else {
        Serial.printf("[✗] Sunset HTTP %d\n", code);
    }
    http.end();
}

// ═════════════════════════════════════════════════
// Sensors — mmWave radar presence (HLK-LD2410B) + LDR
// ═════════════════════════════════════════════════

// ── Should a presence report be accepted right now? ──
// Mirrors the old interrupt gating: presence is acted on only at night, while
// the lights are OFF, and in AUTO mode — so continued presence while the lights
// are ON doesn't reset the countdown (the lights time out and re-arm).
bool canAcceptPresence() {
    return isNightNow() && !lightsOn && (overrideMode == 0);
}

// ── Process presence from the two radar OUT pins ──
// Each radar's OUT pin is HIGH when a person is present (moving OR stationary).
// Replaces the interrupt-driven VL53L0X presence handling entirely.
void processPresence() {
    bool sessionWasActive = (presenceBottom || presenceTop);
    unsigned long now = millis();
    bool accept = canAcceptPresence();

    if (digitalRead(RADAR_BOTTOM_PIN) == HIGH && accept && now >= motionDebounceUntil) {
        if (!presenceBottom) Serial.println("[👣] Presence detected — bottom!");
        presenceBottom = true;
        lastMotionTime = now;
        motionDebounceUntil = now + MOTION_DEBOUNCE_MS;
    }
    if (digitalRead(RADAR_TOP_PIN) == HIGH && accept && now >= motionDebounceUntil) {
        if (!presenceTop) Serial.println("[👣] Presence detected — top!");
        presenceTop = true;
        lastMotionTime = now;
        motionDebounceUntil = now + MOTION_DEBOUNCE_MS;
    }

    // Lock the light duration on the idle -> active transition.
    bool sessionNowActive = (presenceBottom || presenceTop);
    if (!sessionWasActive && sessionNowActive) {
        activeDurationSec = configuredDurationSec;
        Serial.printf("[⏱] New presence session — duration locked at %lus\n", activeDurationSec);
    }

    // Presence timeout: clear both when no presence for the configured duration.
    if ((presenceBottom || presenceTop) && (millis() - lastMotionTime > activeDurationSec * 1000UL)) {
        if (presenceBottom) Serial.println("[👣] Presence timeout — bottom");
        if (presenceTop)    Serial.println("[👣] Presence timeout — top");
        presenceBottom = false;
        presenceTop = false;
    }
}

// ── Ambient light (LDR photoresistor) ──
// Reads the LDR divider and maps it to a rough 0..LDR_MAX_LUX scale. Exact lux
// is not needed — only "is it dark enough?" — so a linear mapping plus a
// calibrated LUX_THRESHOLD (see config.h) is sufficient.
void readLux() {
    int adc = analogRead(LDR_PIN);             // 0..4095 (12-bit)
    lux = (float)adc / 4095.0f * LDR_MAX_LUX;
    if (lux < 0) lux = 0;
}

#if USE_RADAR_UART
// ── Radar distance (cm) via UART — best-effort ──
// Parses the HLK-LD2410 "report" frame:
//   AA FF 03 00 <len:2 LE> <data...> 55 CC
//   data[0] = target state (0 none, 1 moving, 2 stationary)
//   data[1..2] = distance cm (LE) when state != 0
// NOTE: frame layout can vary with firmware — verify against your unit; the
// presence OUT pin is the source of truth for the lights regardless.
struct RadarStream {
    uint8_t  st = 0;         // 0..3 = header sync, 4 = len lo, 5 = len hi, 6 = data
    uint16_t len = 0;
    uint8_t  idx = 0;
    uint8_t  buf[16];
    uint16_t distance = 0;
};

RadarStream radarBottom, radarTop;

void feedRadar(uint8_t which, uint8_t b) {
    RadarStream &r = (which == 0) ? radarBottom : radarTop;
    switch (r.st) {
        case 0: r.st = (b == 0xAA) ? 1 : 0; break;
        case 1: r.st = (b == 0xFF) ? 2 : 0; break;
        case 2: r.st = (b == 0x03) ? 3 : 0; break;
        case 3: r.st = (b == 0x00) ? 4 : 0; break;
        case 4: r.len = b; r.st = 5; break;
        case 5: r.len |= (uint16_t)b << 8; r.idx = 0; r.st = 6; break;
        case 6:
            if (r.idx < sizeof(r.buf)) r.buf[r.idx++] = b;
            if (r.idx >= r.len || r.idx >= sizeof(r.buf)) {
                if (r.buf[0] != 0 && r.idx >= 3) {          // a target is present
                    r.distance = r.buf[1] | ((uint16_t)r.buf[2] << 8);  // cm
                }
                r.st = 0;
            }
            break;
    }
}

void readRadarDistance() {
    while (Serial1.available()) feedRadar(0, (uint8_t)Serial1.read());
    while (Serial2.available()) feedRadar(1, (uint8_t)Serial2.read());
    distanceBottom = radarBottom.distance;
    distanceTop    = radarTop.distance;
}
#endif

// ── Night check (after sunset OR before sunrise) ──
// Compares local times-of-day rather than absolute epochs. Absolute-epoch
// comparison is fragile because sunrise/sunset are reported in UTC, so a
// western timezone's sunset lands on the NEXT UTC date — "now < sunrise" then
// stays true during the daytime and the lights come on before sunset.
bool isNightNow() {
    if (!timeValid || !sunsetValid) return false;
    time_t nowEpoch = currentEpoch + ((millis() - lastTimeSync) / 1000);
    int nowMin     = minutesOfDay(nowEpoch,     localUtcOffsetSec);
    int sunriseMin = minutesOfDay(sunriseEpoch, localUtcOffsetSec);
    int sunsetMin  = minutesOfDay(sunsetEpoch,  localUtcOffsetSec);
    return (nowMin >= sunsetMin) || (nowMin < sunriseMin);
}

// ═════════════════════════════════════════════════
// Decision Logic
// ═════════════════════════════════════════════════
bool evaluate() {
    // ── Manual override takes priority ──────────
    if (overrideMode == 1)  return true;   // FORCE ON
    if (overrideMode == -1) return false;  // FORCE OFF

    // ── AUTO mode: normal sensor/sunset logic ────

    // Fail-safe: if time/sunset data isn't valid yet, keep lights OFF
    if (!timeValid || !sunsetValid) {
        return false;
    }

    // Condition 1: Is it dark enough?
    bool isDim = (lux >= 0 && lux < LUX_THRESHOLD);

    // Condition 2: Is it after sunset OR before sunrise?
    // Between sunrise and sunset = daytime → no lights needed
    bool isNight = isNightNow();

    // Condition 3: Was presence recently detected by either ToF sensor?
    bool hasMotion = (presenceBottom || presenceTop);

    // Decision table:
    //   presence + dark + night → ON
    //   anything else → OFF
    bool shouldLight = hasMotion && isDim && isNight;

    static bool lastDecision = false;
    if (shouldLight != lastDecision) {
        Serial.printf("[💡] Decision: %s  (presence=%d dark=%d night=%d lux=%.0f)\n",
                      shouldLight ? "ON" : "OFF", hasMotion, isDim, isNight, lux);
        lastDecision = shouldLight;
    }

    return shouldLight;
}

// ═════════════════════════════════════════════════
// Light Control — PWM fade-based
// ═════════════════════════════════════════════════
void setLights(bool on) {
    if (on && !lightsOn) {
        // Begin fade-in: set target to full brightness
        targetDuty = 255;
        lightsOn = true;
        lightsOnSince = millis();
        const char* mode = (overrideMode == 1) ? " (OVERRIDE)" : "";
        Serial.printf("[💡] Lights → ON%s (fading in)\\n", mode);
    } else if (!on && lightsOn) {
        // Begin fade-out: set target to 0
        targetDuty = 0;
        lightsOn = false;
        const char* mode = (overrideMode == -1) ? " (OVERRIDE)" : "";
        Serial.printf("[💡] Lights → OFF%s (fading out)\\n", mode);
    }
}

// ═════════════════════════════════════════════════
// PWM Fade Tick — called every loop iteration
// ═════════════════════════════════════════════════
void updateFade() {
    if (currentDuty == targetDuty) return;  // nothing to do

    unsigned long now = millis();

    if (currentDuty < targetDuty) {
        // Fading IN
        currentDuty = min(currentDuty + FADE_STEP, targetDuty);
    } else {
        // Fading OUT
        currentDuty = max(currentDuty - FADE_STEP, 0);
    }
    ledcWrite(LED_MOSFET_PIN, currentDuty);

    // Log when fade completes
    if (currentDuty == targetDuty) {
        if (currentDuty == 255) {
            Serial.println("[💡] Fade-in complete → full brightness");
        } else if (currentDuty == 0) {
            Serial.println("[💡] Fade-out complete → lights off");
        }
    }
}

// ═════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════

// Print status report
void printStatus() {
    time_t now = currentEpoch + ((millis() - lastTimeSync) / 1000);

    char timeStr[16];
  formatHHMMSSFromEpoch(now, localUtcOffsetSec, timeStr, sizeof(timeStr));

    char sunsetStr[16];
  formatHHMMFromEpoch(sunsetEpoch, localUtcOffsetSec, sunsetStr, sizeof(sunsetStr));

    Serial.printf("[STATUS] %s | Lux: %.0f | Bottom: %s (%umm) | Top: %s (%umm) | Lights: %s", 
                  timeStr, lux,
                  presenceBottom ? "PRESENT" : "clear", distanceBottom,
                  presenceTop ? "PRESENT" : "clear", distanceTop,
                  currentDuty > 0 ? "ON" : "OFF");

    // Remaining time when lights are on (clamped — avoids unsigned underflow)
    if (currentDuty > 0 && (presenceBottom || presenceTop)) {
        long remaining = (long)activeDurationSec - (long)((millis() - lastMotionTime) / 1000);
        if (remaining < 0) remaining = 0;
        Serial.printf(" | Remaining: %lds", remaining);
    }

      Serial.printf(" | Sunset: %s | Mode: %s | Duration(set): %lus | Duration(active): %lus | Dist B/T: %lu/%lumm\n",
                  sunsetStr,
                  overrideMode == 1 ? "FORCE ON" : (overrideMode == -1 ? "FORCE OFF" : "AUTO"),
              configuredDurationSec,
              activeDurationSec,
              configuredDistanceBottomMm,
              configuredDistanceTopMm);
}

// ═════════════════════════════════════════════════
// Web Dashboard — HTML (dark theme, responsive)
// ═════════════════════════════════════════════════
void handleRoot() {
    String html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Staircase Lights</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:system-ui,-apple-system,sans-serif;background:#0b1120;color:#e2e8f0;min-height:100vh;display:flex;justify-content:center;align-items:center;padding:16px}
  .card{background:#111827;border:1px solid #1e293b;border-radius:20px;padding:32px 28px;max-width:420px;width:100%;box-shadow:0 25px 60px rgba(0,0,0,.5)}
  h1{font-size:1.35rem;font-weight:600;margin-bottom:2px}
  .sub{color:#64748b;font-size:.8rem;margin-bottom:24px}
  .light-status{text-align:center;padding:28px 0 20px}
  .light-dot{display:inline-block;width:64px;height:64px;border-radius:50%;transition:all .4s ease}
  .light-dot.on{background:radial-gradient(circle at 40% 40%,#fbbf24,#f59e0b 40%,#92400e);box-shadow:0 0 40px #f59e0b88,0 0 80px #f59e0b44}
  .light-dot.off{background:#334155;box-shadow:0 0 0 #0000}
  .light-label{font-size:1.1rem;font-weight:700;margin-top:12px;letter-spacing:.03em}
  .light-label.on{color:#fbbf24}
  .light-label.off{color:#64748b}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:20px}
  .tile{background:#1e293b;border-radius:12px;padding:14px;text-align:center}
  .tile .label{font-size:.7rem;text-transform:uppercase;letter-spacing:.06em;color:#94a3b8;margin-bottom:5px}
  .tile .value{font-size:1.5rem;font-weight:700;line-height:1.2}
  .lux .value{color:#38bdf8}
  .motion .value{color:#a78bfa}
  .motion.active .value{color:#f87171}
  .sunset .value{color:#fb923c}
  .time .value{color:#e2e8f0}
  .remaining .value{color:#34d399}
  .motion.active .value{color:#f87171}
  .remaining.warning .value{color:#fbbf24;animation:pulse 1s infinite}
  .remaining.urgent .value{color:#f87171;animation:pulse .5s infinite}
  .duration-row{display:flex;gap:8px;margin-top:12px;align-items:center}
  .duration-row label{font-size:.75rem;color:#94a3b8;white-space:nowrap}
  .duration-row input{flex:1;background:#0f172a;border:1px solid #334155;border-radius:8px;padding:8px 10px;color:#e2e8f0;font-size:.85rem;text-align:center}
  .duration-row input:focus{outline:none;border-color:#3b82f6}
  .duration-row .btn-sm{flex:0;padding:8px 14px;font-size:.8rem}
  .btn-row{display:flex;gap:8px;margin-top:4px}
  .btn{flex:1;background:#1e293b;border:1px solid #334155;border-radius:10px;padding:12px 8px;color:#cbd5e1;font-size:.85rem;font-weight:600;cursor:pointer;transition:all .15s}
  .btn:hover{background:#334155}
  .btn.active{border-color:#3b82f6;background:#1e3a5f;color:#60a5fa}
  .btn.force-on.active{border-color:#f59e0b;background:#3d2e0a;color:#fbbf24}
  .btn.force-off.active{border-color:#ef4444;background:#3b1010;color:#f87171}
  .footer{text-align:center;margin-top:16px;font-size:.72rem;color:#475569}
  .refresh{display:inline-block;width:6px;height:6px;border-radius:50%;background:#22c55e;margin-right:6px;animation:pulse 2s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
</style>
</head>
<body>
<div class="card">
  <h1>🏠 Staircase Lights</h1>
  <div class="sub" id="ip">—</div>

  <div class="light-status" id="lightStatus">
    <div class="light-dot off" id="lightDot"></div>
    <div class="light-label off" id="lightLabel">LIGHTS OFF</div>
  </div>

  <div class="grid">
    <div class="tile lux">
      <div class="label">☀️ Ambient Light</div>
      <div class="value" id="lux">--</div>
    </div>
    <div class="tile motion" id="sensorTileB">
      <div class="label">📏 Distance Bottom</div>
      <div class="value" id="distB">--</div>
    </div>
    <div class="tile motion" id="sensorTileT">
      <div class="label">📏 Distance Top</div>
      <div class="value" id="distT">--</div>
    </div>
    <div class="tile time">
      <div class="label">🕐 Local Time</div>
      <div class="value" id="time">--</div>
    </div>
    <div class="tile sunset">
      <div class="label">🌅 Sunset</div>
      <div class="value" id="sunset">--</div>
    </div>
    <div class="tile remaining" id="remainingTile" style="grid-column:1/-1">
      <div class="label">⏳ Remaining</div>
      <div class="value" id="remaining">--</div>
    </div>
  </div>

  <div class="duration-row">
    <label for="durInput">⏱ Duration:</label>
    <input type="number" id="durInput" min="5" max="1800" step="5" value="120">
    <button class="btn btn-sm" onclick="setDuration()">Set</button>
  </div>

  <div class="duration-row">
    <label for="distBottomInput">📏 Bottom (mm):</label>
    <input type="number" id="distBottomInput" min="30" max="150" step="1" value="70">
    <button class="btn btn-sm" onclick="setDistance('bottom')">Set</button>
  </div>
  <div class="duration-row">
    <label for="distTopInput">📏 Top (mm):</label>
    <input type="number" id="distTopInput" min="30" max="150" step="1" value="70">
    <button class="btn btn-sm" onclick="setDistance('top')">Set</button>
  </div>

  <div class="duration-row">
    <label for="pollInput">⏲ Poll (s):</label>
    <input type="number" id="pollInput" min="1" max="300" step="1" value="5">
    <button class="btn btn-sm" onclick="setPoll()">Set</button>
  </div>

  <div class="btn-row">
    <button class="btn force-on" id="btnOn" onclick="setOverride('on')">🔆 Force ON</button>
    <button class="btn active" id="btnAuto" onclick="setOverride('auto')">🔄 Auto</button>
    <button class="btn force-off" id="btnOff" onclick="setOverride('off')">🌙 Force OFF</button>
  </div>

  <div class="footer">
    <span class="refresh"></span><span id="uptime">—</span> &nbsp;|&nbsp; WiFi <span id="rssi">--</span>
  </div>
</div>

<script>
const ip = window.location.host;
document.getElementById('ip').textContent = 'http://' + ip + '/';

let currentOverride = 'auto';

const durInputEl = document.getElementById('durInput');
const distBottomEl = document.getElementById('distBottomInput');
const distTopEl = document.getElementById('distTopInput');
const pollInputEl = document.getElementById('pollInput');

async function fetchData() {
  try {
    const r = await fetch('/api');
    const d = await r.json();

    // Light status
    const dot = document.getElementById('lightDot');
    const lbl = document.getElementById('lightLabel');
    if (d.lights_on) {
      dot.className = 'light-dot on';
      lbl.className = 'light-label on';
      lbl.textContent = '💡 LIGHTS ON';
    } else {
      dot.className = 'light-dot off';
      lbl.className = 'light-label off';
      lbl.textContent = 'LIGHTS OFF';
    }

    // Lux
    document.getElementById('lux').innerHTML = d.lux.toFixed(0) + ' <small style="font-size:.65rem;opacity:.6">lux</small>';

    // Distance — bottom
    const sTileB = document.getElementById('sensorTileB');
    const sMetricB = document.getElementById('distB');
    const distB_cm = d.distance_bottom_mm ? d.distance_bottom_mm : '--';
    if (d.presence_bottom) {
      sTileB.className = 'tile motion active';
      sMetricB.innerHTML = distB_cm + ' <small style="font-size:.65rem;opacity:.6">cm · PRESENT</small>';
    } else if (d.distance_bottom_mm > 0) {
      sTileB.className = 'tile motion';
      sMetricB.innerHTML = distB_cm + ' <small style="font-size:.65rem;opacity:.6">cm · clear</small>';
    } else {
      sTileB.className = 'tile motion';
      sMetricB.textContent = '--';
    }

    // Distance — top
    const sTileT = document.getElementById('sensorTileT');
    const sMetricT = document.getElementById('distT');
    const distT_cm = d.distance_top_mm ? d.distance_top_mm : '--';
    if (d.presence_top) {
      sTileT.className = 'tile motion active';
      sMetricT.innerHTML = distT_cm + ' <small style="font-size:.65rem;opacity:.6">cm · PRESENT</small>';
    } else if (d.distance_top_mm > 0) {
      sTileT.className = 'tile motion';
      sMetricT.innerHTML = distT_cm + ' <small style="font-size:.65rem;opacity:.6">cm · clear</small>';
    } else {
      sTileT.className = 'tile motion';
      sMetricT.textContent = '--';
    }

    // Time
    document.getElementById('time').textContent = d.time;

    // Sunset
    document.getElementById('sunset').textContent = d.sunset;

    // Override mode
    currentOverride = d.override;
    updateButtons(currentOverride);

    // Remaining time
    const remTile = document.getElementById('remainingTile');
    const remEl = document.getElementById('remaining');
    if (d.lights_on && d.remaining_sec > 0) {
      const m = Math.floor(d.remaining_sec / 60);
      const s = d.remaining_sec % 60;
      remEl.textContent = m + 'm ' + s + 's';
      remTile.className = 'tile remaining' + (d.remaining_sec <= 30 ? ' urgent' : (d.remaining_sec <= 60 ? ' warning' : ''));
    } else if (d.lights_on) {
      remEl.textContent = 'Fading...';
      remTile.className = 'tile remaining';
    } else {
      remEl.textContent = '—';
      remTile.className = 'tile remaining';
    }

    // Settings inputs: only refresh values that aren't being actively edited.
    if (document.activeElement !== durInputEl) {
      durInputEl.value = d.duration_sec;
    }
    if (document.activeElement !== distBottomEl) {
      distBottomEl.value = d.distance_bottom_mm_set;
    }
    if (document.activeElement !== distTopEl) {
      distTopEl.value = d.distance_top_mm_set;
    }
    if (document.activeElement !== pollInputEl) {
      pollInputEl.value = d.poll_interval_sec;
    }

    // Uptime
    document.getElementById('uptime').textContent = 'Uptime ' + d.uptime;

    // RSSI
    document.getElementById('rssi').textContent = d.rssi + ' dBm';
  } catch(e) {
    console.error('API fetch error:', e);
  }
}

function updateButtons(mode) {
  document.getElementById('btnOn').className = 'btn force-on' + (mode === 'on' ? ' active' : '');
  document.getElementById('btnAuto').className = 'btn' + (mode === 'auto' ? ' active' : '');
  document.getElementById('btnOff').className = 'btn force-off' + (mode === 'off' ? ' active' : '');
}

async function setOverride(mode) {
  try {
    const r = await fetch('/api/override?mode=' + mode);
    const d = await r.json();
    currentOverride = d.override;
    updateButtons(currentOverride);
  } catch(e) {
    console.error('Override error:', e);
  }
}

async function setDuration() {
  const secs = document.getElementById('durInput').value;
  try {
    const r = await fetch('/api/duration?seconds=' + secs, {method:'POST'});
    const d = await r.json();
    if (d.ok) {
      document.getElementById('durInput').value = d.duration_sec;
    } else if (d.error) {
      alert(d.error);
    }
  } catch(e) {
    console.error('Duration error:', e);
  }
}

async function setDistance(position) {
  const el = (position === 'bottom')
    ? document.getElementById('distBottomInput')
    : document.getElementById('distTopInput');
  const mm = el.value;
  try {
    const r = await fetch('/api/distance?position=' + position + '&mm=' + mm, {method:'POST'});
    const d = await r.json();
    if (d.ok) {
      el.value = d.distance_mm;
    } else if (d.error) {
      alert(d.error);
    }
  } catch(e) {
    console.error('Distance error:', e);
  }
}

async function setPoll() {
  const secs = pollInputEl.value;
  try {
    const r = await fetch('/api/poll?seconds=' + secs, {method:'POST'});
    const d = await r.json();
    if (d.ok) {
      pollInputEl.value = d.poll_interval_sec;
    } else if (d.error) {
      alert(d.error);
    }
  } catch(e) {
    console.error('Poll error:', e);
  }
}

fetchData();
setInterval(fetchData, 2000);
</script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

// ═════════════════════════════════════════════════
// JSON API — live sensor / state / time data
// ═════════════════════════════════════════════════
void handleAPI() {
    time_t now = currentEpoch + ((millis() - lastTimeSync) / 1000);

    char timeStr[8];  // "HH:MM" or "HH:MM:SS"
  formatHHMMFromEpoch(now, localUtcOffsetSec, timeStr, sizeof(timeStr));

    char sunsetStr[8];
  formatHHMMFromEpoch(sunsetEpoch, localUtcOffsetSec, sunsetStr, sizeof(sunsetStr));

    // Uptime as human-readable string
    unsigned long uptimeSec = (millis() - bootMillis) / 1000;
    char uptimeStr[16];
    if (uptimeSec < 60)
        snprintf(uptimeStr, sizeof(uptimeStr), "%lus", uptimeSec);
    else if (uptimeSec < 3600)
        snprintf(uptimeStr, sizeof(uptimeStr), "%lum", uptimeSec / 60);
    else if (uptimeSec < 86400)
        snprintf(uptimeStr, sizeof(uptimeStr), "%luh %lum",
                 uptimeSec / 3600, (uptimeSec % 3600) / 60);
    else
        snprintf(uptimeStr, sizeof(uptimeStr), "%lud %luh",
                 uptimeSec / 86400, (uptimeSec % 86400) / 3600);

    // Compute remaining time (seconds) when lights are on
    long remainingSec = 0;
    if (currentDuty > 0 && (presenceBottom || presenceTop)) {
        long elapsed = (millis() - lastMotionTime) / 1000;
      remainingSec = (long)activeDurationSec - elapsed;
        if (remainingSec < 0) remainingSec = 0;
    }

    const char* overrideStr = (overrideMode == 1) ? "on"
                            : (overrideMode == -1) ? "off"
                            : "auto";

    String json = "{";
    json += "\"lights_on\":" + String(currentDuty > 0 ? "true" : "false") + ",";
    json += "\"duty\":" + String(currentDuty) + ",";
    json += "\"lux\":" + String(lux, 1) + ",";
    json += "\"presence_bottom\":" + String(presenceBottom ? "true" : "false") + ",";
    json += "\"presence_top\":" + String(presenceTop ? "true" : "false") + ",";
    json += "\"distance_bottom_mm\":" + String(distanceBottom) + ",";
    json += "\"distance_top_mm\":" + String(distanceTop) + ",";
    json += "\"distance_bottom_mm_set\":" + String(configuredDistanceBottomMm) + ",";
    json += "\"distance_top_mm_set\":" + String(configuredDistanceTopMm) + ",";
    json += "\"time\":\"" + String(timeStr) + "\",";
    json += "\"sunset\":\"" + String(sunsetStr) + "\",";
    json += "\"override\":\"" + String(overrideStr) + "\",";
    json += "\"uptime\":\"" + String(uptimeStr) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"duration_sec\":" + String(configuredDurationSec) + ",";
    json += "\"active_duration_sec\":" + String(activeDurationSec) + ",";
    json += "\"poll_interval_sec\":" + String(configuredPollIntervalSec) + ",";
    json += "\"remaining_sec\":" + String(remainingSec);
    json += "}";

    server.send(200, "application/json", json);
}

// ═════════════════════════════════════════════════
// Manual override endpoint — /api/override?mode=on|off|auto
// ═════════════════════════════════════════════════
void handleOverride() {
    if (!server.hasArg("mode")) {
        server.send(400, "application/json",
                    "{\"error\":\"missing ?mode=on|off|auto\"}");
        return;
    }

    String mode = server.arg("mode");
    mode.toLowerCase();

    if (mode == "on") {
        overrideMode = 1;
        Serial.println("[🕹] Override → FORCE ON");
    } else if (mode == "off") {
        overrideMode = -1;
        Serial.println("[🕹] Override → FORCE OFF");
    } else if (mode == "auto") {
        overrideMode = 0;
        Serial.println("[🕹] Override → AUTO");
    } else {
        server.send(400, "application/json",
                    "{\"error\":\"invalid mode — use on, off, or auto\"}");
        return;
    }

    const char* overrideStr = (overrideMode == 1) ? "on"
                            : (overrideMode == -1) ? "off"
                            : "auto";

    server.send(200, "application/json",
                "{\"override\":\"" + String(overrideStr) + "\",\"lights_on\":" +
                String(lightsOn ? "true" : "false") + "}");
}

// ═════════════════════════════════════════════════
// Duration endpoint — /api/duration
//   GET          → returns current duration in seconds
//   POST ?seconds=N  → sets new duration (clamped)
// ═════════════════════════════════════════════════
void handleDuration() {
    if (server.method() == HTTP_POST || server.hasArg("seconds")) {
        // Set new duration
        String val = server.arg("seconds");
        long newDuration = val.toInt();

        if (newDuration < DURATION_MIN_SEC || newDuration > DURATION_MAX_SEC) {
            char err[128];
            snprintf(err, sizeof(err),
                     "{\"error\":\"duration must be %d–%d seconds\"}",
                     DURATION_MIN_SEC, DURATION_MAX_SEC);
            server.send(400, "application/json", err);
            return;
        }

        configuredDurationSec = (unsigned long)newDuration;
        Serial.printf("[⚙] Duration set to %lus (HTTP, applies to next session)\\n", configuredDurationSec);

        bool persisted = false;
        if (prefs.begin(PREF_NAMESPACE, false)) {
          persisted = (prefs.putULong(PREF_KEY_DURATION, configuredDurationSec) > 0);
          prefs.end();
        }
        if (!persisted) {
          Serial.println("[⚠] Failed to persist duration to NVS");
        }

        char resp[192];
        snprintf(resp, sizeof(resp),
           "{\"duration_sec\":%lu,\"active_duration_sec\":%lu,\"persisted\":%s,\"ok\":true}",
           configuredDurationSec, activeDurationSec, persisted ? "true" : "false");
        server.send(200, "application/json", resp);
    } else {
        // GET — return current duration
        char resp[192];
        snprintf(resp, sizeof(resp),
             "{\"duration_sec\":%lu,\"active_duration_sec\":%lu}",
             configuredDurationSec, activeDurationSec);
        server.send(200, "application/json", resp);
    }
}

// ═════════════════════════════════════════════════
// Distance endpoint — /api/distance
//   GET  → returns current presence distance thresholds (mm)
//   POST ?position=bottom|top&mm=N → sets threshold (clamped)
// ═════════════════════════════════════════════════
void handleDistance() {
    String position = server.arg("position");
    position.toLowerCase();
    bool isBottom = (position == "bottom");
    bool isTop    = (position == "top");
    if (!isBottom && !isTop) {
        server.send(400, "application/json",
                    "{\"error\":\"missing ?position=bottom|top\"}");
        return;
    }

    if (server.method() == HTTP_POST || server.hasArg("mm")) {
        long newDist = server.arg("mm").toInt();
        if (newDist < DISTANCE_MIN_MM || newDist > DISTANCE_MAX_MM) {
            char err[128];
            snprintf(err, sizeof(err),
                     "{\"error\":\"distance must be %d–%d mm\"}",
                     DISTANCE_MIN_MM, DISTANCE_MAX_MM);
            server.send(400, "application/json", err);
            return;
        }

        // NOTE: with mmWave radar there is no code-configurable presence
        // threshold — the radar's sensitivity is set via its own HLK app. This
        // value is kept for API compatibility only (no longer affects sensing).
        if (isBottom) {
            configuredDistanceBottomMm = (unsigned long)newDist;
        } else {
            configuredDistanceTopMm = (unsigned long)newDist;
        }

        Serial.printf("[⚙] Distance %s set to %lumm (HTTP)\n",
                      isBottom ? "bottom" : "top",
                      isBottom ? configuredDistanceBottomMm : configuredDistanceTopMm);

        if (prefs.begin(PREF_NAMESPACE, false)) {
            prefs.putULong(PREF_KEY_DIST_BOTTOM, configuredDistanceBottomMm);
            prefs.putULong(PREF_KEY_DIST_TOP, configuredDistanceTopMm);
            prefs.end();
        }

        char resp[160];
        snprintf(resp, sizeof(resp),
                 "{\"position\":\"%s\",\"distance_mm\":%lu,\"ok\":true}",
                 isBottom ? "bottom" : "top",
                 isBottom ? configuredDistanceBottomMm : configuredDistanceTopMm);
        server.send(200, "application/json", resp);
    } else {
        char resp[160];
        snprintf(resp, sizeof(resp),
                 "{\"distance_bottom_mm\":%lu,\"distance_top_mm\":%lu}",
                 configuredDistanceBottomMm, configuredDistanceTopMm);
        server.send(200, "application/json", resp);
    }
}

// ═════════════════════════════════════════════════
// Poll interval endpoint — /api/poll
//   GET  → returns current sensor polling interval (seconds)
//   POST ?seconds=N → sets interval (clamped)
// ═════════════════════════════════════════════════
void handlePoll() {
    if (server.method() == HTTP_POST || server.hasArg("seconds")) {
        long newPoll = server.arg("seconds").toInt();
        if (newPoll < POLL_INTERVAL_MIN_SEC || newPoll > POLL_INTERVAL_MAX_SEC) {
            char err[128];
            snprintf(err, sizeof(err),
                     "{\"error\":\"poll interval must be %d–%d seconds\"}",
                     POLL_INTERVAL_MIN_SEC, POLL_INTERVAL_MAX_SEC);
            server.send(400, "application/json", err);
            return;
        }

        configuredPollIntervalSec = (unsigned long)newPoll;
        Serial.printf("[⚙] Poll interval set to %lus (HTTP)\n", configuredPollIntervalSec);

        if (prefs.begin(PREF_NAMESPACE, false)) {
            prefs.putULong(PREF_KEY_POLL, configuredPollIntervalSec);
            prefs.end();
        }

        char resp[128];
        snprintf(resp, sizeof(resp),
                 "{\"poll_interval_sec\":%lu,\"ok\":true}",
                 configuredPollIntervalSec);
        server.send(200, "application/json", resp);
    } else {
        char resp[128];
        snprintf(resp, sizeof(resp),
                 "{\"poll_interval_sec\":%lu}",
                 configuredPollIntervalSec);
        server.send(200, "application/json", resp);
    }
}
