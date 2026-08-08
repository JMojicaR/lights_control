/**
 * Staircase Light Controller — ESP32-S3 SuperMini
 * ===============================================
 *
 * Automatically controls a 12V LED strip on a 5m staircase based on:
 *   1. Motion detection (HC-SR501 PIR)
 *   2. Ambient light level (BH1750 I²C lux sensor)
 *   3. Time of day vs sunset (HTTP APIs)
 *
 * Lights turn ON when: motion detected AND it's dark AND past sunset.
 * Lights stay ON for a configurable duration (default 120s) after last motion,
 *
 * Web Dashboard: http://<esp32-ip>/
 *   - Live status: lux, motion, lights, time, sunset
 *   - Manual override: force ON / OFF / AUTO
 *
 * APIs used:
 *   - timeapi.io          (https://timeapi.io)        — current time
 *   - sunrise-sunset.org                              — sunset/sunrise times
 *
 * Hardware:
 *   - ESP32-S3 SuperMini
 *   - HC-SR501 PIR motion sensor — bottom of stairs (GPIO 4)
 *   - HC-SR501 PIR motion sensor — top of stairs (GPIO 6)
 *   - BH1750 ambient light sensor (I²C: SDA 21, SCL 22)
 *   - IRLZ44N MOSFET switching 12V LED strip (GPIO 5)
 *   - 12V DC power supply (≥6A for 5m strip)
 */

#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <BH1750.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>
#include <Preferences.h>

// ── Objects ─────────────────────────────────────
BH1750 lightMeter;
WebServer server(80);
Preferences prefs;

const char* PREF_NAMESPACE = "stairs";
const char* PREF_KEY_DURATION = "dur_sec";

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

// ── Motion tracking ─────────────────────────────
bool     motionActiveBottom = false;
bool     motionActiveTop    = false;
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

    // Load persisted configured duration (fallback to compile-time default).
    if (prefs.begin(PREF_NAMESPACE, true)) {
      unsigned long saved = prefs.getULong(PREF_KEY_DURATION, DEFAULT_LIGHT_DURATION_SEC);
      prefs.end();

      if (saved >= DURATION_MIN_SEC && saved <= DURATION_MAX_SEC) {
        configuredDurationSec = saved;
      } else {
        configuredDurationSec = DEFAULT_LIGHT_DURATION_SEC;
        Serial.printf("[⚠] Stored duration invalid (%lu) — using default %lu\n",
                saved, configuredDurationSec);
      }
    } else {
      configuredDurationSec = DEFAULT_LIGHT_DURATION_SEC;
      Serial.printf("[⚠] Preferences unavailable — using default duration %lu\n",
              configuredDurationSec);
    }
    activeDurationSec = configuredDurationSec;
    Serial.printf("[⚙] Configured duration loaded: %lus\n", configuredDurationSec);

    // Pins — LED MOSFET uses PWM for fade
    pinMode(PIR_PIN_BOTTOM, INPUT);
    pinMode(PIR_PIN_TOP, INPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // PWM setup for LED MOSFET (8-bit, 5 kHz — silent, smooth fade)
    ledcAttach(LED_MOSFET_PIN, PWM_FREQ, PWM_RES);
    ledcWrite(LED_MOSFET_PIN, 0);

    // I²C for BH1750
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!lightMeter.begin(LIGHT_SENSOR_MODE, 0x23, &Wire)) {
        Serial.println("[✗] BH1750 not found — check wiring");
    } else {
        Serial.println("[✓] BH1750 ready");
    }

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
    server.handleClient();  // non-blocking — serves web requests

    unsigned long now = millis();

    // ── Periodic HTTP time re-sync ───────────────
    if (now - lastTimeSync >= TIME_RESYNC_MS || !timeValid) {
        syncTime();
    }

    // ── Periodic sunset re-sync ──────────────────
    if (now - lastSunsetSync >= SUNSET_RESYNC_MS || !sunsetValid) {
        syncSunset();
    }

    // ── Read sensors ─────────────────────────────
    readSensors();

    // ── Decision logic ───────────────────────────
    bool shouldLight = evaluate();

    // ── Control lights ───────────────────────────
    setLights(shouldLight);

    // ── PWM fade tick (non-blocking, steps each loop) ──
    updateFade();

    // ── Status LED heartbeat ─────────────────────
    digitalWrite(STATUS_LED_PIN, lightsOn ? HIGH : (now / 1000) % 2);

    // ── Serial report every 5 seconds ────────────
    static unsigned long lastReport = 0;
    if (now - lastReport >= 5000) {
        lastReport = now;
        printStatus();
    }

    delay(SENSOR_POLL_MS);
}

// ═════════════════════════════════════════════════
// WiFi
// ═════════════════════════════════════════════════
void connectWiFi() {
    Serial.printf("WiFi: connecting to %s", WIFI_SSID);

#ifdef WIFI_STATIC_IP
    IPAddress staticIP(WIFI_IP);
    IPAddress gateway(WIFI_GATEWAY);
    IPAddress subnet(WIFI_SUBNET);
    IPAddress dns1(WIFI_DNS1);
    IPAddress dns2(WIFI_DNS2);

    if (!WiFi.config(staticIP, gateway, subnet, dns1, dns2)) {
        Serial.println("\n[⚠] Static IP config failed — falling back to DHCP");
    } else {
        Serial.printf("\n[⚙] Static IP configured: %s", staticIP.toString().c_str());
    }
#endif

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
// Sensors
// ═════════════════════════════════════════════════
void readSensors() {
    unsigned long now = millis();
  bool sessionWasActive = (motionActiveBottom || motionActiveTop);

    // PIR — bottom of stairs
    int pirBottom = digitalRead(PIR_PIN_BOTTOM);
    if (pirBottom == HIGH && now >= motionDebounceUntil) {
        if (!motionActiveBottom) {
            Serial.println("[👣] Motion detected — bottom!");
        }
        motionActiveBottom = true;
        lastMotionTime = now;
        motionDebounceUntil = now + MOTION_DEBOUNCE_MS;
    }

    // PIR — top of stairs
    int pirTop = digitalRead(PIR_PIN_TOP);
    if (pirTop == HIGH && now >= motionDebounceUntil) {
        if (!motionActiveTop) {
            Serial.println("[👣] Motion detected — top!");
        }
        motionActiveTop = true;
        lastMotionTime = now;
        motionDebounceUntil = now + MOTION_DEBOUNCE_MS;
    }

    // Start a new motion session only when transitioning from idle -> active.
    bool sessionNowActive = (motionActiveBottom || motionActiveTop);
    if (!sessionWasActive && sessionNowActive) {
      activeDurationSec = configuredDurationSec;
      Serial.printf("[⏱] New motion session — duration locked at %lus\n", activeDurationSec);
    }

    // Motion timeout: clear both PIR states when no motion for duration
    bool anyMotion = (motionActiveBottom || motionActiveTop);
    if (anyMotion && (now - lastMotionTime > activeDurationSec * 1000UL)) {
        if (motionActiveBottom) Serial.println("[👣] Motion timeout — bottom");
        if (motionActiveTop)    Serial.println("[👣] Motion timeout — top");
        motionActiveBottom = false;
        motionActiveTop    = false;
    }

    // BH1750 — ambient light (lux)
    if (lightMeter.measurementReady()) {
        lux = lightMeter.readLightLevel();
        if (lux < 0) lux = 0;
    }
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

    // Advance the internal clock by elapsed millis
    time_t nowEpoch = currentEpoch + ((millis() - lastTimeSync) / 1000);

    // Condition 1: Is it dark enough?
    bool isDim = (lux >= 0 && lux < LUX_THRESHOLD);

    // Condition 2: Is it after sunset OR before sunrise?
    // Between sunrise and sunset = daytime → no lights needed
    bool isNight = (nowEpoch >= sunsetEpoch || nowEpoch < sunriseEpoch);

    // Condition 3: Was motion recently detected by either PIR?
    bool hasMotion = (motionActiveBottom || motionActiveTop);

    // Decision table:
    //   motion + dark + night → ON
    //   anything else → OFF
    bool shouldLight = hasMotion && isDim && isNight;

    static bool lastDecision = false;
    if (shouldLight != lastDecision) {
        Serial.printf("[💡] Decision: %s  (motion=%d dark=%d night=%d lux=%.0f)\n",
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

    Serial.printf("[STATUS] %s | Lux: %.0f | Motion bottom: %s | Motion top: %s | Lights: %s", 
                  timeStr, lux,
                  motionActiveBottom ? "YES" : "no",
                  motionActiveTop ? "YES" : "no",
                  currentDuty > 0 ? "ON" : "OFF");

    // Remaining time when lights are on
    if (currentDuty > 0 && (motionActiveBottom || motionActiveTop)) {
        unsigned long remaining = activeDurationSec - ((millis() - lastMotionTime) / 1000);
        Serial.printf(" | Remaining: %lus", remaining);
    }

      Serial.printf(" | Sunset: %s | Mode: %s | Duration(set): %lus | Duration(active): %lus\n",
                  sunsetStr,
                  overrideMode == 1 ? "FORCE ON" : (overrideMode == -1 ? "FORCE OFF" : "AUTO"),
              configuredDurationSec,
              activeDurationSec);
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
    <div class="tile motion" id="motionTileB">
      <div class="label">👣 Motion Bottom</div>
      <div class="value" id="motionB">--</div>
    </div>
    <div class="tile motion" id="motionTileT">
      <div class="label">👣 Motion Top</div>
      <div class="value" id="motionT">--</div>
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
let durationEditing = false;

const durInputEl = document.getElementById('durInput');
durInputEl.addEventListener('focus', () => { durationEditing = true; });
durInputEl.addEventListener('blur', () => { durationEditing = false; });

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

    // Motion — bottom
    const mTileB = document.getElementById('motionTileB');
    const mMetricB = document.getElementById('motionB');
    if (d.motion_bottom) {
      mTileB.className = 'tile motion active';
      mMetricB.textContent = 'Active';
    } else {
      mTileB.className = 'tile motion';
      mMetricB.textContent = 'Idle';
    }

    // Motion — top
    const mTileT = document.getElementById('motionTileT');
    const mMetricT = document.getElementById('motionT');
    if (d.motion_top) {
      mTileT.className = 'tile motion active';
      mMetricT.textContent = 'Active';
    } else {
      mTileT.className = 'tile motion';
      mMetricT.textContent = 'Idle';
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

    // Duration: do not overwrite user draft while editing.
    if (!durationEditing) {
      document.getElementById('durInput').value = d.duration_sec;
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
    if (currentDuty > 0 && (motionActiveBottom || motionActiveTop)) {
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
    json += "\"motion_bottom\":" + String(motionActiveBottom ? "true" : "false") + ",";
    json += "\"motion_top\":" + String(motionActiveTop ? "true" : "false") + ",";
    json += "\"time\":\"" + String(timeStr) + "\",";
    json += "\"sunset\":\"" + String(sunsetStr) + "\",";
    json += "\"override\":\"" + String(overrideStr) + "\",";
    json += "\"uptime\":\"" + String(uptimeStr) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"duration_sec\":" + String(configuredDurationSec) + ",";
    json += "\"active_duration_sec\":" + String(activeDurationSec) + ",";
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
