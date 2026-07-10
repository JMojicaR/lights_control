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
 * Web Dashboard: http://<esp32-ip>/
 *   - Live status: lux, motion, lights, time, sunset
 *   - Manual override: force ON / OFF / AUTO
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
#include <WebServer.h>

// ── Objects ─────────────────────────────────────
BH1750 lightMeter;
WebServer server(80);

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

    // ── Web server routes ─────────────────────────
    server.on("/", handleRoot);
    server.on("/api", handleAPI);
    server.on("/api/override", handleOverride);
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
        const char* mode = (overrideMode == 1) ? " (OVERRIDE)" : "";
        Serial.printf("[💡] Lights → ON%s\n", mode);
    } else if (!on && lightsOn) {
        digitalWrite(LED_MOSFET_PIN, LOW);
        lightsOn = false;
        const char* mode = (overrideMode == -1) ? " (OVERRIDE)" : "";
        Serial.printf("[💡] Lights → OFF%s\n", mode);
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

    Serial.printf("[STATUS] %s | Lux: %.0f | Motion: %s | Lights: %s | Sunset: %s | Mode: %s\n",
                  timeStr, lux,
                  motionActive ? "YES" : "no",
                  lightsOn ? "ON" : "OFF",
                  sunsetStr,
                  overrideMode == 1 ? "FORCE ON" : (overrideMode == -1 ? "FORCE OFF" : "AUTO"));
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
  .motion.active .value{color:#f87171}
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
    <div class="tile motion" id="motionTile">
      <div class="label">👣 Motion</div>
      <div class="value" id="motion">--</div>
    </div>
    <div class="tile time">
      <div class="label">🕐 Local Time</div>
      <div class="value" id="time">--</div>
    </div>
    <div class="tile sunset">
      <div class="label">🌅 Sunset</div>
      <div class="value" id="sunset">--</div>
    </div>
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

    // Motion
    const mTile = document.getElementById('motionTile');
    const mMetric = document.getElementById('motion');
    if (d.motion) {
      mTile.className = 'tile motion active';
      mMetric.textContent = 'Active';
    } else {
      mTile.className = 'tile motion';
      mMetric.textContent = 'Idle';
    }

    // Time
    document.getElementById('time').textContent = d.time;

    // Sunset
    document.getElementById('sunset').textContent = d.sunset;

    // Override mode
    currentOverride = d.override;
    updateButtons(currentOverride);

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
    struct tm* local = localtime(&now);

    char timeStr[8];  // "HH:MM" or "HH:MM:SS"
    strftime(timeStr, sizeof(timeStr), "%H:%M", local);

    char sunsetStr[8];
    struct tm* sun = localtime(&sunsetEpoch);
    strftime(sunsetStr, sizeof(sunsetStr), "%H:%M", sun);

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

    const char* overrideStr = (overrideMode == 1) ? "on"
                            : (overrideMode == -1) ? "off"
                            : "auto";

    String json = "{";
    json += "\"lights_on\":" + String(lightsOn ? "true" : "false") + ",";
    json += "\"lux\":" + String(lux, 1) + ",";
    json += "\"motion\":" + String(motionActive ? "true" : "false") + ",";
    json += "\"time\":\"" + String(timeStr) + "\",";
    json += "\"sunset\":\"" + String(sunsetStr) + "\",";
    json += "\"override\":\"" + String(overrideStr) + "\",";
    json += "\"uptime\":\"" + String(uptimeStr) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
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
