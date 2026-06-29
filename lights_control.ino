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
 * Lights stay ON for LIGHT_DURATION_SEC after last motion, then turn OFF.
 *
 * APIs used:
 *   - WorldTimeAPI  (http://worldtimeapi.org)  — current time
 *   - sunrise-sunset.org                        — sunset/sunrise times
 *
 * Hardware:
 *   - ESP32-S3 SuperMini
 *   - HC-SR501 PIR motion sensor (GPIO 4)
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

// ── Objects ─────────────────────────────────────
BH1750 lightMeter;

// ── Time tracking ───────────────────────────────
unsigned long lastTimeSync   = 0;      // millis() of last HTTP time sync
unsigned long lastSunsetSync = 0;      // millis() of last sunset sync
time_t        currentEpoch   = 0;      // current Unix time (from HTTP)
time_t        sunsetEpoch    = 0;      // today's sunset Unix time
time_t        sunriseEpoch   = 0;      // today's sunrise Unix time
bool          timeValid      = false;
bool          sunsetValid    = false;
const unsigned long TIME_RESYNC_MS   = TIME_RESYNC_MIN  * 60000UL;
const unsigned long SUNSET_RESYNC_MS = SUNSET_RESYNC_MIN * 60000UL;

// ── Motion tracking ─────────────────────────────
bool     motionActive    = false;
unsigned long lastMotionTime = 0;
unsigned long motionDebounceUntil = 0;

// ── Light state ─────────────────────────────────
bool     lightsOn        = false;
unsigned long lightsOnSince  = 0;

// ── Sensor readings ─────────────────────────────
float    lux             = 0.0;

// ─────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("\n=== Staircase Light Controller ===\n");

    // Pins
    pinMode(PIR_PIN, INPUT);
    pinMode(LED_MOSFET_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(LED_MOSFET_PIN, LOW);
    digitalWrite(STATUS_LED_PIN, LOW);

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

    Serial.println("\n--- Ready ---\n");
}

// ─────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────
void loop() {
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
// HTTP: Sync current time from WorldTimeAPI
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
            const char* dt = doc["datetime"];        // "2026-06-26T14:30:00.123456-06:00"
            int rawOffset = doc["raw_offset"];        // seconds offset from UTC
            int dstOffset = doc["dst_offset"];        // DST offset
            const char* abbrev = doc["abbreviation"]; // "CST", "CDT", etc.

            // Parse ISO datetime to epoch (simple approach: extract and use mktime)
            // Format: "2026-06-26T14:30:00.123456-06:00"
            struct tm t = {};
            t.tm_year = atoi(String(dt).substring(0, 4).c_str()) - 1900;
            t.tm_mon  = atoi(String(dt).substring(5, 7).c_str()) - 1;
            t.tm_mday = atoi(String(dt).substring(8, 10).c_str());
            t.tm_hour = atoi(String(dt).substring(11, 13).c_str());
            t.tm_min  = atoi(String(dt).substring(14, 16).c_str());
            t.tm_sec  = atoi(String(dt).substring(17, 19).c_str());
            t.tm_isdst = (dstOffset > 0) ? 1 : 0;

            currentEpoch = mktime(&t);
            timeValid = true;
            lastTimeSync = millis();

            Serial.printf("[⏰] Time synced: %s %s (UTC%+d)\n",
                          dt, abbrev, rawOffset / 3600);
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

            Serial.printf("[🌅] Sunset: %s UTC  |  Sunrise: %s UTC\n",
                          sunsetStr, sunriseStr);
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
    // PIR motion
    int pir = digitalRead(PIR_PIN);
    unsigned long now = millis();

    if (pir == HIGH && now >= motionDebounceUntil) {
        // Rising edge — motion detected
        if (!motionActive) {
            Serial.println("[👣] Motion detected!");
        }
        motionActive = true;
        lastMotionTime = now;
        motionDebounceUntil = now + MOTION_DEBOUNCE_MS;
    }

    // Motion timeout
    if (motionActive && (now - lastMotionTime > LIGHT_DURATION_SEC * 1000UL)) {
        motionActive = false;
        Serial.println("[👣] Motion timeout — no movement");
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

    // Condition 3: Was motion recently detected?
    bool hasMotion = motionActive;

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
// Light Control
// ═════════════════════════════════════════════════
void setLights(bool on) {
    if (on && !lightsOn) {
        digitalWrite(LED_MOSFET_PIN, HIGH);
        lightsOn = true;
        lightsOnSince = millis();
        Serial.println("[💡] Lights → ON");
    } else if (!on && lightsOn) {
        digitalWrite(LED_MOSFET_PIN, LOW);
        lightsOn = false;
        Serial.println("[💡] Lights → OFF");
    }
}

// ═════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════

// Convert ISO 8601 string to Unix epoch
// Input: "2026-06-27T01:15:00+00:00"
time_t isoToEpoch(const char* iso) {
    struct tm t = {};
    t.tm_year = atoi(String(iso).substring(0, 4).c_str()) - 1900;
    t.tm_mon  = atoi(String(iso).substring(5, 7).c_str()) - 1;
    t.tm_mday = atoi(String(iso).substring(8, 10).c_str());
    t.tm_hour = atoi(String(iso).substring(11, 13).c_str());
    t.tm_min  = atoi(String(iso).substring(14, 16).c_str());
    t.tm_sec  = atoi(String(iso).substring(17, 19).c_str());
    t.tm_isdst = 0;
    return mktime(&t);
}

// Print status report
void printStatus() {
    time_t now = currentEpoch + ((millis() - lastTimeSync) / 1000);
    struct tm* local = localtime(&now);

    char timeStr[16];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", local);

    char sunsetStr[16];
    struct tm* sun = localtime(&sunsetEpoch);
    strftime(sunsetStr, sizeof(sunsetStr), "%H:%M", sun);

    Serial.printf("[STATUS] %s | Lux: %.0f | Motion: %s | Lights: %s | Sunset: %s\n",
                  timeStr, lux,
                  motionActive ? "YES" : "no",
                  lightsOn ? "ON" : "OFF",
                  sunsetStr);
}
