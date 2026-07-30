# Staircase Light Controller — ESP32-S3

Automatically controls a 12V LED strip on a 5m staircase using **VL53L0X ToF presence detection**, ambient light, and time-of-day logic. Includes a **web dashboard** for live monitoring and manual override.

## How It Works

```
Person on stairs?  ──┐
It's dark?          ──┤──▶ All 3 true? ──▶ LIGHTS ON
Past sunset?        ──┘

Any condition false ────────────────▶ LIGHTS OFF (after timeout)
```

**Decision logic:**
1. **VL53L0X ToF** (bottom of stairs) measures distance via laser — detects presence when someone is within range (default 1.2m)
2. **VL53L0X ToF** (top of stairs) — same, for the upper landing
3. **BH1750** measures ambient light (lux) — ensures lights don't fire during daytime
4. **sunrise-sunset.org API** provides sunset time for your location — lights only at night
5. Lights stay ON for a configurable duration (default 90s) after last detection, then turn OFF
6. **Manual override** via web dashboard — force ON / OFF / AUTO

### Why VL53L0X instead of PIR?

| Feature | HC-SR501 PIR | VL53L0X ToF |
|---------|-------------|-------------|
| Detection method | Passive infrared (heat movement) | Laser time-of-flight (distance) |
| Still person | ❌ Not detected | ✅ Detected |
| Hot environments | ❌ Fails (ambient ≈ body temp) | ✅ Works |
| Output | Binary (HIGH/LOW) | Distance in mm |
| Range | ~5m cone | Up to 2m (reliable ~1.2m) |
| Interface | Single digital pin | I²C (shared bus) |
| Multi-sensor | 1 pin each | XSHUT pin + unique I²C address |

## Web Dashboard

Open `http://<esp32-ip>/` in any browser on the same network.

| Tile | Description |
|---------|-------------|
| **Live status** | Lights ON/OFF, ambient lux, distance (cm) bottom/top, local time, sunset |
| **Manual override** | Force ON, Force OFF, or return to AUTO mode |
| **Uptime & RSSI** | Device uptime and WiFi signal strength |
| **JSON API** | `GET /api` returns machine-readable JSON |
| **Override API** | `GET /api/override?mode=on\|off\|auto` for programmatic control |
| **Auto-refresh** | Dashboard polls every 2 seconds |

### API Examples

```bash
# Get current status
curl http://192.168.1.42/api
# → {"lights_on":false,"duty":0,"lux":3.5,"presence_bottom":true,"presence_top":false,
#    "distance_bottom_mm":450,"distance_top_mm":2100,"time":"14:30","sunset":"19:15",...}

# Force lights on (e.g., from a script or home automation)
curl "http://192.168.1.42/api/override?mode=on"
# → {"override":"on","lights_on":true}

# Return to automatic mode
curl "http://192.168.1.42/api/override?mode=auto"
# → {"override":"auto","lights_on":false}
```

## Hardware

| Component | Purpose | GPIO / I²C |
|-----------|---------|------|
| ESP32-S3 SuperMini | Controller | — |
| VL53L0X ToF (bottom) | Presence detection (bottom of stairs) | I²C addr 0x29, XSHUT GPIO 4 |
| VL53L0X ToF (top) | Presence detection (top of stairs) | I²C addr 0x30, XSHUT GPIO 6 |
| BH1750 | Ambient light sensor | I²C 0x23 (SDA 12, SCL 13) |
| IRLZ44N MOSFET | Switch 12V LED strip | 5 (PWM-capable) |
| 12V LED strip (5m) | Staircase lighting | MOSFET drain |
| 12V DC PSU (≥6A) | Power for LED strip | — |

### Wiring

```
ESP32-S3              Peripheral
────────              ──────────
GPIO 4   ──────────── VL53L0X #1 XSHUT (bottom)
GPIO 6   ──────────── VL53L0X #2 XSHUT (top)
GPIO 12  ──────────── VL53L0X #1 SDA, VL53L0X #2 SDA, BH1750 SDA
GPIO 13  ──────────── VL53L0X #1 SCL, VL53L0X #2 SCL, BH1750 SCL
3.3V    ──────────── VL53L0X VIN (×2), BH1750 VCC
GND     ──────────── VL53L0X GND (×2), BH1750 GND

VL53L0X XSHUT pins must have 10KΩ pull-up to 3.3V (many modules include this).
All three I²C devices share the same SDA/SCL bus — unique addresses assigned at boot.

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
#define VL53L0X_PRESENCE_MM  1200  // Distance (mm) below which = person detected
#define DEFAULT_LIGHT_DURATION_SEC  90  // Seconds to keep lights on after detection
```

## Build & Flash

```bash
# Install dependencies (first time only)
arduino-cli lib install BH1750
arduino-cli lib install ArduinoJson
arduino-cli lib install VL53L0X

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3 .

# Flash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 .

# Monitor
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Serial Output

```
=== Staircase Light Controller ===
[✓] VL53L0X bottom ready (addr 0x29)
[✓] VL53L0X top ready (addr 0x30)
[✓] BH1750 ready
[✓] WiFi connected — IP: 192.168.1.42
[⏰] Time synced: 2026-07-10T16:19:26-06:00 (UTC-6, DST=no)
[🌅] Sunset: 2026-06-27T01:15:00 UTC  |  Sunrise: 2026-06-26T12:00:00 UTC
[🌐] Dashboard: http://192.168.1.42/
--- Ready ---
[STATUS] 14:30:15 | Lux: 450 | Bottom: clear (2100mm) | Top: clear (2050mm) | Lights: OFF | Sunset: 19:15 | Mode: AUTO
[👣] Presence detected — bottom! (450 mm)
[💡] Decision: ON  (presence=1 dark=1 night=1 lux=3)
[💡] Lights → ON (fading in)
```

## Dependencies (Arduino Libraries)

| Library | Version | Install |
|---------|---------|---------|
| VL53L0X | ≥1.3.0 | `arduino-cli lib install VL53L0X` |
| BH1750 | ≥1.3.0 | `arduino-cli lib install BH1750` |
| ArduinoJson | ≥7.0 | `arduino-cli lib install ArduinoJson` |

WiFi, HTTPClient, Wire, and **WebServer** are included with the ESP32 board package.

## Enclosure Models

FreeCAD (.FCStd) and STL models for 3D printing are in `enclosures/`:

| File | Description |
|------|-------------|
| `enclosures/main_box.fcstd` | Main controller enclosure (ESP32-S3 + MOSFET + terminals) |
| `enclosures/sensor_bottom.fcstd` | VL53L0X housing — bottom of stairs |
| `enclosures/sensor_top.fcstd` | VL53L0X housing — top of stairs |
| `enclosures/bh1750_housing.fcstd` | BH1750 ambient light sensor housing |

Print in PETG or ABS for heat resistance (PLA may warp near the MOSFET).
