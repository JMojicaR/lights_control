# Staircase Light Controller — ESP32-S3

Automatically controls a 12V LED strip on a 5m staircase using motion, ambient light, and time-of-day logic. Includes a **web dashboard** for live monitoring and manual override.

## How It Works

```
Motion detected?  ──┐
It's dark?        ──┤──▶ All 3 true? ──▶ LIGHTS ON
Past sunset?      ──┘

Any condition false ────────────────▶ LIGHTS OFF (after timeout)
```

**Decision logic:**
1. **HC-SR501 PIR** (bottom of stairs) detects motion at the bottom
2. **HC-SR501 PIR** (top of stairs) detects motion at the top
3. **BH1750** measures ambient light (lux) — ensures lights don't fire during daytime
4. **sunrise-sunset.org API** provides sunset time for your location — lights only at night
5. Lights stay ON for 2 minutes after last motion (from either sensor), then turn OFF
6. **Manual override** via web dashboard — force ON / OFF / AUTO

## Web Dashboard

Open `http://<esp32-ip>/` in any browser on the same network.

| Tile | Description |
|---------|-------------|
| **Live status** | Lights ON/OFF, ambient lux, motion (bottom/top), local time, sunset |
| **Manual override** | Force ON, Force OFF, or return to AUTO mode |
| **Uptime & RSSI** | Device uptime and WiFi signal strength |
| **JSON API** | `GET /api` returns machine-readable JSON |
| **Override API** | `GET /api/override?mode=on\|off\|auto` for programmatic control |
| **Auto-refresh** | Dashboard polls every 2 seconds |

### API Examples

```bash
# Get current status
curl http://192.168.1.42/api
# → {"lights_on":false,"duty":0,"lux":3.5,"motion_bottom":true,"motion_top":false,"time":"14:30","sunset":"19:15",...}

# Force lights on (e.g., from a script or home automation)
curl "http://192.168.1.42/api/override?mode=on"
# → {"override":"on","lights_on":true}

# Return to automatic mode
curl "http://192.168.1.42/api/override?mode=auto"
# → {"override":"auto","lights_on":false}
```

## Hardware

| Component | Purpose | GPIO |
|-----------|---------|------|
| ESP32-S3 SuperMini | Controller | — |
| HC-SR501 PIR (bottom) | Motion detection (bottom of stairs) | 4 (digital) |
| HC-SR501 PIR (top) | Motion detection (top of stairs) | 6 (digital) |
| BH1750 | Ambient light sensor | 21 (SDA), 22 (SCL) |
| IRLZ44N MOSFET | Switch 12V LED strip | 5 (PWM-capable) |
| 12V LED strip (5m) | Staircase lighting | MOSFET drain |
| 12V DC PSU (≥6A) | Power for LED strip | — |

### Wiring

```
ESP32-S3          Peripheral
────────          ──────────
GPIO 4   ──────── HC-SR501 OUT (bottom PIR)
GPIO 6   ──────── HC-SR501 OUT (top PIR)
GPIO 21  ──────── BH1750 SDA
GPIO 22  ──────── BH1750 SCL
3.3V    ──────── HC-SR501 VCC (×2), BH1750 VCC
GND     ──────── HC-SR501 GND (×2), BH1750 GND

MOSFET circuit:
GPIO 5 ──[10KΩ]──┬── IRLZ44N GATE
                 └── GND

12V PSU (+) ─── LED strip (+) ─── LED strip (-) ─── IRLZ44N DRAIN
12V PSU (-) ─── IRLZ44N SOURCE ─── GND (common)
```

## APIs Used

| API | Endpoint | Purpose | Key Required? |
|-----|----------|---------|---------------|
| timeapi.io | `timeapi.io/api/v1/time/current/zone?timezone={tz}` | Current time + timezone | No |
| sunrise-sunset.org | `api.sunrise-sunset.org/json` | Sunset/sunrise times | No |

Both are free, no API key needed.

## Configuration

Edit `config.h` before flashing:

```cpp
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define LATITUDE        19.4326    // Your location
#define LONGITUDE       -99.1332
#define TIMEZONE        "America/Mexico_City"
#define LUX_THRESHOLD   30         // Lux below this = "dark"
#define LIGHT_DURATION_SEC  120    // Seconds to keep lights on after motion
```

## Build & Flash

```bash
# Install dependencies (first time only)
arduino-cli lib install BH1750
arduino-cli lib install ArduinoJson

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 .

# Flash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 .

# Monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

**Flash usage:** ~1.1 MB (84%) of 1.28 MB program space, ~48 KB (14%) RAM. The web server (`WebServer.h`) is included with the ESP32 board package — no extra libraries needed.

## Serial Output

```
=== Staircase Light Controller ===
[✓] BH1750 ready
[✓] WiFi connected — IP: 192.168.1.42
[⏰] Time synced: 2026-07-10T16:19:26-06:00 (UTC-6, DST=no)
[🌅] Sunset: 2026-06-27T01:15:00 UTC  |  Sunrise: 2026-06-26T12:00:00 UTC
[🌐] Dashboard: http://192.168.1.42/
--- Ready ---
[STATUS] 14:30:15 | Lux: 450 | Motion bottom: no | Motion top: no | Lights: OFF | Sunset: 19:15 | Mode: AUTO
[👣] Motion detected — bottom!
[💡] Decision: ON  (motion=1 dark=1 night=1 lux=3)
[💡] Lights → ON
[🕹] Override → FORCE OFF
[💡] Lights → OFF (OVERRIDE)
[🕹] Override → AUTO
```

## Dependencies (Arduino Libraries)

| Library | Version | Install |
|---------|---------|---------|
| BH1750 | ≥1.3.0 | `arduino-cli lib install BH1750` |
| ArduinoJson | ≥7.0 | `arduino-cli lib install ArduinoJson` |

WiFi, HTTPClient, Wire, and **WebServer** are included with the ESP32 board package.
