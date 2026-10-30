# Staircase Light Controller — ESP32-S3 (mmWave radar + BH1750)

Automatically controls a 12V LED strip on a 5m staircase using **HLK-LD2410B/C
mmWave radar** for presence detection, a **BH1750** ambient-light sensor, and
time-of-day logic (sunset API). Includes a **web dashboard** for live monitoring
and manual override.

> This is the **`uart_option`** variant. The radars detect a person who is
> **completely still** (micro-motion) — the key advantage over PIR — and they
> connect over simple digital lines instead of the old I²C ToF bus, so they can
> sit **tens of metres** from the controller over cheap **Cat6** cable.

## How It Works

```
Person on stairs?  ──┐
It's dark?          ──┤──▶ All 3 true? ──▶ LIGHTS ON
Past sunset?        ──┘

Any condition false ────────────────▶ LIGHTS OFF (after timeout)
```

**Decision logic:**
1. **mmWave radar OUT pin** (bottom + top of stairs) — HIGH when a person is
   present, moving **or stationary**. No I²C bus, no address collision, no
   pull-up tuning.
2. **BH1750** measures ambient light (lux) — ensures lights don't fire in daylight.
3. **sunrise-sunset.org API** provides sunset time — lights only at night.
4. Lights stay ON for a configurable duration (default 90 s), then fade OFF.
   A motion debounce (default 2 s) ignores re-triggers while the lights are on.
5. **Manual override** via web dashboard — force ON / OFF / AUTO.

Presence is read by polling each radar's OUT pin (debounced). The radar UART
also carries **configuration commands** — the sensing distance (detection range)
is set live from the dashboard via the radar's max-distance gate. The BH1750 is
polled (**5 s by default**, configurable), since lux changes slowly.

### Why mmWave radar instead of PIR?

| Feature | HC-SR501 PIR | HLK-LD2410B/C mmWave |
|---------|-------------|----------------------|
| Detection method | Passive infrared (heat movement) | 24 GHz radar (micro-motion) |
| Still person | ❌ Not detected | ✅ Detected |
| Hot environments | ❌ Fails (ambient ≈ body temp) | ✅ Works |
| Output | Binary (HIGH/LOW) | Binary OUT **+** UART distance |
| Range | ~5 m cone | Up to ~6 m, configurable |
| Interface | Single digital pin | OUT pin + UART (256 kbps) |
| Configurability | Fixed | Sensitivity/gates via HLK app or UART |

It also replaces the earlier **VL53L0X ToF** approach: the ToF needed a shared
I²C bus (address re-mapping + ~3 m cable ceiling), while the radar's push-pull
digital lines run 15–30 m with no extra hardware.

## Sensors & Specifications

### HLK-LD2410B / LD2410C — 24 GHz mmWave presence radar (×2)

| Parameter | Value |
|-----------|-------|
| Frequency | 24 GHz mmWave |
| Presence | **OUT** pin HIGH = person present (moving **or** stationary) |
| Supply | **5 V DC** (not 3.3 V) |
| Current | ~80–100 mA typical |
| Logic level | 3.3 V push-pull (OUT) |
| UART | 256 000 baud, 8N1 (distance + configuration) |
| Detection range | up to ~6 m, configurable (HLK app / UART) |
| OUT interface | Single digital pin |

**LD2410C vs LD2410B:** the **LD2410C** is a drop-in replacement — same OUT pin,
same UART protocol (256 kbps), same 5 V power — but adds **built-in BLE** for
configuration from the phone app and breadboard-friendly 2.54 mm pins.
**Verify the pin order** on the silkscreen, it can differ between B and C.

### BH1750 — ambient light sensor

| Parameter | Value |
|-----------|-------|
| Interface | I²C, address **0x23** |
| Output | Real lux (no calibration) |
| Mode | CONTINUOUS_HIGH_RES_MODE |
| Placement | **< 30 cm** from the ESP32 (short I²C — no pull-up/length tuning) |
| Wiring | SDA **GPIO 12**, SCL **GPIO 13** |

## Web Dashboard

Open `http://<esp32-ip>/` in any browser on the same network.

| Tile | Description |
|------|-------------|
| **Live status** | Lights ON/OFF, ambient lux, **motion location (bottom/top)**, local time, sunset |
| **Manual override** | Force ON, Force OFF, or return to AUTO mode |
| **Uptime & RSSI** | Device uptime and WiFi signal strength |
| **Duration / Poll / Sensing** | Set light duration (s), ambient-light (BH1750) polling interval (s), and radar **sensing distance (cm)** — all persisted to NVS |
| **JSON API** | `GET /api` returns machine-readable JSON |
| **Override API** | `GET /api/override?mode=on\|off\|auto` |
| **Duration API** | `POST /api/duration?seconds=N` |
| **Poll API** | `POST /api/poll?seconds=N` |
| **Sensing API** | `POST /api/sensing?cm=N` — sets the radar detection range live |
| **Auto-refresh** | Dashboard polls every 2 seconds |

> The dashboard shows **where motion was detected** (bottom / top) rather than
> the radar's distance reading. The radar's sensing distance is now changed from
> the dashboard (it sends a UART config command to the radar's max-distance gate).

### API Examples

```bash
curl http://192.168.1.42/api
# → {"lights_on":false,"duty":0,"lux":3.5,"presence_bottom":true,"presence_top":false,
#    "motion_bottom":true,"motion_top":false,"sensing_distance_cm":600,
#    "time":"14:30","sunset":"19:15",...}

curl "http://192.168.1.42/api/override?mode=on"    # → {"override":"on","lights_on":true}
curl "http://192.168.1.42/api/override?mode=auto"  # → {"override":"auto","lights_on":false}

curl -X POST "http://192.168.1.42/api/duration?seconds=120"  # set light duration
curl -X POST "http://192.168.1.42/api/poll?seconds=10"        # set BH1750 poll interval
```

## Hardware

| Component | Purpose | Pins |
|-----------|---------|------|
| ESP32-S3 SuperMini | Controller | — |
| HLK-LD2410B/C radar (bottom) | Presence detection | OUT → GPIO 4, TX → GPIO 7 (RX) |
| HLK-LD2410B/C radar (top) | Presence detection | OUT → GPIO 6, TX → GPIO 8 (RX) |
| BH1750 | Ambient light sensor | I²C SDA 12, SCL 13 |
| IRLZ44N MOSFET | Switch 12 V LED strip | Gate → GPIO 5 |
| 12 V LED strip (5 m) | Staircase lighting | MOSFET drain |
| 12 V DC PSU (≥6 A) | Power for LED strip + buck | — |

### Wiring (pin-by-pin)

```
ESP32-S3               Peripheral
────────               ──────────
GPIO 4   ───────────── Radar BOTTOM OUT   (presence, HIGH = present)
GPIO 7   ───────────── Radar BOTTOM TX    (UART RX ← radar, 256000 baud)
GPIO 9   ───────────── Radar BOTTOM RX    (UART TX → radar, config commands)
GPIO 6   ───────────── Radar TOP OUT
GPIO 8   ───────────── Radar TOP TX       (UART RX ← radar, 256000 baud)
GPIO 10  ───────────── Radar TOP RX       (UART TX → radar, config commands)
GPIO 12  ───────────── BH1750 SDA
GPIO 13  ───────────── BH1750 SCL
GPIO 5   ──[10 kΩ]──┬─ IRLZ44N GATE   (10 kΩ pulldown to GND keeps it OFF at boot)
                    └─ GND
5 V      ───────────── Radar VCC (×2)   ← NOT the ESP32 3V3 pin
3.3 V    ───────────── BH1750 VCC
GND      ───────────── Radar GND (×2), BH1750 GND

12 V PSU (+) ─── LED strip (+) ─── LED strip (−) ─── IRLZ44N DRAIN
12 V PSU (−) ─── IRLZ44N SOURCE ─── GND (common)
```

## Electrical & Wiring Considerations (Cat6 / Ethernet cable)

The radar `OUT` and `UART` lines are **3.3 V push-pull digital** — no I²C
open-drain/400 pF ceiling — so they run far longer than the old ToF bus.

### Distance limits

| Signal | Type | Comfortable | Max (with care) |
|--------|------|-------------|-----------------|
| Radar **OUT** (presence) | digital, slow | **20–30 m** | 50 m+ |
| Radar **UART** (256 kbps) | digital, push-pull | **15–20 m** | 30 m+ (lower baud) |

> **Staircase reality check:** top/bottom radars are at most ~5 m from a central
> ESP32 — well inside the comfort zone.

### Use Cat6 (or Cat5e) — one cable per radar

Cat6 gives you twisted pairs for free (each signal twisted with a GND return),
which is exactly what rejects EMI on a 15–30 m digital run.

| Factor | Cat6 | Separate 24 AWG |
|--------|------|-----------------|
| Noise immunity | ✅ 4 twisted pairs | ❌ must twist by hand |
| Conductors | ✅ 8 wires | buy 4+ strands |
| Gauge / power | ✅ 23 AWG solid (low 5 V drop) | 24 AWG (higher resistance) |
| Cost / convenience | ✅ one cable | multiple rolls |

### Per-radar Cat6 pair assignment

| Pair | Colour | Signals | Purpose |
|------|--------|---------|---------|
| 1 | orange | VCC (5 V) + GND | power |
| 2 | green | OUT + GND | presence |
| 3 | blue | TX + GND | UART distance |
| 4 | brown | RX + GND | spare (future config) |

### Power & electrical notes

- Radars run on **5 V** (not 3.3 V) — never feed them from the ESP32's 3V3 pin.
- At 20 m of 23 AWG the 5 V drop at ~80–100 mA is negligible (~0.15 V). For
  > 30 m, run 12 V and regulate to 5 V **at each radar**.
- UART at 256 kbps is trivial over 30 m; drop to 115200/9600 for noisy long runs.
- Fuse the **12 V feed (3 A)** and star-ground everything at one point.
- The MOSFET gate needs a **10 kΩ pulldown to GND** so the strip stays off while
  the ESP32 boots.
- **LD2410C is drop-in** for LD2410B (same OUT + UART + protocol); check the pin
  order on the silkscreen — it can differ between B and C.

## APIs Used

| API | Endpoint | Purpose | Key Required? |
|-----|----------|---------|---------------|
| timeapi.io | `timeapi.io/api/v1/time/current/zone?timezone={tz}` | Current time + timezone | No |
| sunrise-sunset.org | `api.sunrise-sunset.org/json` | Sunset/sunrise times | No |

## Configuration

Edit `config.h` before flashing:

```cpp
#define WIFI_SSID       "YOUR_SSID"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define LATITUDE        19.4326    // Your location
#define LONGITUDE       -99.1332
#define TIMEZONE        "America/Mexico_City"

#define RADAR_BOTTOM_PIN   4     // bottom radar OUT → GPIO 4
#define RADAR_TOP_PIN      6     // top radar OUT    → GPIO 6
#define LED_MOSFET_PIN     5     // IRLZ44N gate
#define I2C_SDA            12    // BH1750 SDA
#define I2C_SCL            13    // BH1750 SCL

#define RADAR_UART_BAUD    256000
#define RADAR_BOTTOM_RX    7     // ESP32 RX ← bottom radar TX
#define RADAR_TOP_RX       8     // ESP32 RX ← top radar TX

#define LUX_THRESHOLD      30    // lux below this = "dark enough"
#define DEFAULT_LIGHT_DURATION_SEC  90
#define MOTION_DEBOUNCE_MS  2000  // ignore re-triggers within this window
#define POLL_INTERVAL_DEFAULT_SEC  5
```

Light duration, the BH1750 polling interval, and the radar sensing distance
(detection range) can be changed live from the dashboard (persisted to NVS,
survive reboot). The sensing distance is applied by sending a UART config
command to each radar's max-distance gate.

## Build & Flash

```bash
arduino-cli lib install BH1750
arduino-cli lib install ArduinoJson

arduino-cli compile --fqbn esp32:esp32:esp32s3 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 .
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Serial Output

```
=== Staircase Light Controller ===
[✓] Radar bottom OUT on GPIO 4
[✓] Radar top OUT on GPIO 6
[✓] BH1750 ready
[✓] WiFi connected — IP: 192.168.1.42
[🌐] Dashboard: http://192.168.1.42/
--- Ready ---
[STATUS] 14:30:15 | Lux: 450 | Bottom: 2100mm | Top: 2050mm | Lights: OFF | Sunset: 19:15 | Mode: AUTO
[👣] Presence detected — bottom! (450 mm)
[💡] Decision: ON  (presence=1 dark=1 night=1 lux=3)
[💡] Lights → ON (fading in)
```

## Dependencies (Arduino Libraries)

| Library | Version | Install |
|---------|---------|---------|
| BH1750 | ≥1.3.0 | `arduino-cli lib install BH1750` |
| ArduinoJson | ≥7.0 | `arduino-cli lib install ArduinoJson` |

WiFi, HTTPClient, Wire, and WebServer are included with the ESP32 board package.
(No VL53L0X library — that sensor was replaced by the mmWave radars.)

## Enclosure Models

FreeCAD (.FCStd) models are in `enclosures/`:

| File | Description |
|------|-------------|
| `enclosures/main_box.FCStd` | Main controller enclosure (ESP32-S3 + MOSFET + terminals) |
| `enclosures/sensor_bottom.FCStd` | Radar housing — bottom of stairs |
| `enclosures/sensor_top.FCStd` | Radar housing — top of stairs |
| `enclosures/bh1750_housing.FCStd` | BH1750 ambient-light sensor housing |

Print in PETG or ABS for heat resistance (PLA may warp near the MOSFET).

## Other docs (this branch)

- `docs/distance_and_wiring.md` — distance limits + exact connection diagram + Cat6 pair assignment
- `docs/BOM_UART.md` — bill of materials
- `docs/UART_schematic.svg` — schematic
- `docs/cost_roi_UART.md` + `docs/market_analysis_UART.md` / `.pdf` — cost, ROI & market analysis (DIY kit)
