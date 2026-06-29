# Staircase Light Controller — ESP32-S3

Automatically controls a 12V LED strip on a 5m staircase using motion, ambient light, and time-of-day logic.

## How It Works

```
Motion detected?  ──┐
It's dark?        ──┤──▶ All 3 true? ──▶ LIGHTS ON
Past sunset?      ──┘

Any condition false ────────────────▶ LIGHTS OFF (after timeout)
```

**Decision logic:**
1. **HC-SR501 PIR** detects motion on the stairs
2. **BH1750** measures ambient light (lux) — ensures lights don't fire during daytime
3. **sunrise-sunset.org API** provides sunset time for your location — lights only at night
4. Lights stay ON for 2 minutes after last motion, then turn OFF

## Hardware

| Component | Purpose | GPIO |
|-----------|---------|------|
| ESP32-S3 SuperMini | Controller | — |
| HC-SR501 PIR | Motion detection | 4 (digital) |
| BH1750 | Ambient light sensor | 21 (SDA), 22 (SCL) |
| IRLZ44N MOSFET | Switch 12V LED strip | 5 (PWM-capable) |
| 12V LED strip (5m) | Staircase lighting | MOSFET drain |
| 12V DC PSU (≥6A) | Power for LED strip | — |

### Wiring

```
ESP32-S3          Peripheral
────────          ──────────
GPIO 4   ──────── HC-SR501 OUT
GPIO 21  ──────── BH1750 SDA
GPIO 22  ──────── BH1750 SCL
3.3V    ──────── HC-SR501 VCC, BH1750 VCC
GND     ──────── HC-SR501 GND, BH1750 GND

MOSFET circuit:
GPIO 5 ──[10KΩ]──┬── IRLZ44N GATE
                 └── GND

12V PSU (+) ─── LED strip (+) ─── LED strip (-) ─── IRLZ44N DRAIN
12V PSU (-) ─── IRLZ44N SOURCE ─── GND (common)
```

## APIs Used

| API | Endpoint | Purpose | Key Required? |
|-----|----------|---------|---------------|
| WorldTimeAPI | `worldtimeapi.org/api/timezone/{tz}` | Current time + timezone | No |
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
[✓] BH1750 ready
[✓] WiFi connected — IP: 192.168.1.42
[⏰] Time synced: 2026-06-26T14:30:00 CST (UTC-6)
[🌅] Sunset: 2026-06-27T01:15:00 UTC  |  Sunrise: 2026-06-26T12:00:00 UTC
--- Ready ---
[STATUS] 14:30:15 | Lux: 450 | Motion: no | Lights: OFF | Sunset: 19:15
[👣] Motion detected!
[💡] Decision: ON  (motion=1 dark=1 night=1 lux=3)
[💡] Lights → ON
```

## Dependencies (Arduino Libraries)

| Library | Version | Install |
|---------|---------|---------|
| BH1750 | ≥1.3.0 | `arduino-cli lib install BH1750` |
| ArduinoJson | ≥7.0 | `arduino-cli lib install ArduinoJson` |

WiFi, HTTPClient, and Wire are included with the ESP32 board package.
