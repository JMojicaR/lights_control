# Distance Limits & Exact Connection Diagram (UART / mmWave Option)

How far the sensors can sit from the ESP32, and the exact pin-by-pin wiring.

---

## 1. Distance limits

The radar `OUT` and `UART` lines are **push-pull digital** signals — unlike the
old I²C bus (open-drain, 400 pF budget), so the 3 m ceiling is gone.

| Signal | Type | Comfortable | Max (with care) |
|--------|------|-------------|-----------------|
| **Radar OUT** (presence) | digital, slow | **20–30 m** | 50 m+ |
| **Radar UART** (distance, 256 kbps) | digital, push-pull | **15–20 m** | 30 m+ (lower baud) |
| **LDR** (ambient light) | analog | **5–10 m** | ~15 m (with RC filter) |

### What actually limits you (and how to push further)

1. **Noise / EMI** → twist each signal with a GND return; use shielded cable
   (FTP) for the longest runs.
2. **Power drop** → the radar draws only ~80 mA @ 5 V. At 20 m with 22 AWG the
   drop is ~0.2 V (negligible); 24 AWG ~0.35 V (still fine). For >30 m, run 12 V
   and regulate to 5 V at each radar.
3. **UART bit rate** → 256 kbps is trivial over 30 m of cable; drop to 115200 or
   9600 for extra margin on very long/noisy runs.
4. **LDR (analog)** → the real weak link. It is a *slow* "is it dark?" signal, so
   a 1 kΩ + 100 nF low-pass and longer averaging extends it to ~15 m. For long
   runs, keep the LDR **local** to the controller (it only measures ambient
   light, it doesn't need to be on the stairs).

> **Staircase reality check:** top/bottom radars are at most ~5 m from a central
> ESP32 — well inside the comfort zone, with large headroom.

### Why this beats the alternatives

| Link | Max practical distance |
|------|------------------------|
| I²C (old ToF + BH1750) | ~3 m (400 pF limit) |
| CAN (needs nodes + transceivers) | tens of metres |
| **UART/GPIO (this option)** | **15–30 m, no extra hardware** |

---

## 2. Exact connection diagram

### 2a. Pin-by-pin wiring table

| From | Pin | To | Pin | Wire |
|------|-----|----|-----|------|
| **Radar BOTTOM** | VCC (5 V) | 5 V rail | — | red |
| | GND | GND rail | — | black |
| | **OUT** | ESP32-S3 | **GPIO 4** | yellow |
| | **TX** | ESP32-S3 | **GPIO 7** (RX) | green |
| | **RX** | ESP32-S3 | **GPIO 9** (TX) | white |
| **Radar TOP** | VCC (5 V) | 5 V rail | — | red |
| | GND | GND rail | — | black |
| | **OUT** | ESP32-S3 | **GPIO 6** | yellow |
| | **TX** | ESP32-S3 | **GPIO 8** (RX) | green |
| | **RX** | ESP32-S3 | **GPIO 10** (TX) | white |
| **LDR divider** | 3V3 → LDR → tap → 10 kΩ → GND | ESP32-S3 | **GPIO 1** (ADC) | tap wire |
| **MOSFET** | ESP32-S3 **GPIO 5** → 10 kΩ → gate | IRLZ44N | gate | blue |
| | drain | LED strip | (−) | — |
| | source | GND | — | — |
| **LED strip** | 5 V rail (+) | strip | (+) | red |
| | strip (−) | MOSFET drain | — | — |
| **Power** | 5 V USB charger | → +5 V rail | → ESP32 5 V + radar VCC + strip (+) | — |

### 2b. ASCII wiring diagram

```
                    5 V USB charger (+) ──┬──────────────────► LED strip (+)
                                          ├──────────────────► ESP32 5 V pin
                                          └──────────────────► radar VCC (×2)
                    5 V USB charger (GND) ───────────────────► common GND

   ┌──────────┐                  ┌───────────┐                  ┌─────────────┐
   │ RADAR    │                  │ RADAR     │                  │  ESP32-S3   │
   │ BOTTOM   │                  │ TOP       │                  │  SuperMini  │
   │ LD2410B  │                  │ LD2410B   │                  │             │
   │ OUT ─────┼──► GPIO 4        │ OUT ──────┼──► GPIO 6        │  GPIO 4 ◄───┼── radar bottom OUT
   │ TX  ─────┼──► GPIO 7 (RX)   │ TX  ──────┼──► GPIO 8 (RX)   │  GPIO 6 ◄───┼── radar top OUT
   │ RX  ◄────┼──  GPIO 9 (TX)   │ RX  ◄─────┼──  GPIO 10 (TX)  │  GPIO 7 ◄───┼── radar bottom TX
   │ GND ─────┼──► GND           │ GND ──────┼──► GND           │  GPIO 8 ◄───┼── radar top TX
   └──────────┘                  └───────────┘                  │  GPIO 9 ────┼──► radar bottom RX (config)
                                                                 │  GPIO 10 ───┼──► radar top RX (config)
                                                                 │  GPIO 12 ◄──┼── BH1750 SDA
                                                                 │  GPIO 13 ◄──┼── BH1750 SCL
                                                                 │  GPIO 5 ────┼──► IRLZ44N gate (10kΩ)
                                                                 │  3V3 ───────┼──► BH1750 VCC
                                                                 │  GND        │
                                                                 └─────────────┘

    BH1750:   VCC ── 3V3     SDA ── GPIO 12     SCL ── GPIO 13     GND ── GND

    IRLZ44N:  gate ──(10kΩ pulldown to GND)── GPIO 5
              drain ──► LED strip (−)
              source ──► GND
```

### 2c. Notes

- **Radar `RX`** is wired to an ESP32 TX pin (GPIO 9 / 10) so the ESP32 can send
  UART config commands — this is how the sensing distance (max-distance gate) is
  changed from the dashboard.
- **LDR divider** gives *brighter = higher ADC*; `lux = (ADC/4095) × LDR_MAX_LUX`,
  threshold `LUX_THRESHOLD` calibrated in `config.h`.
- **Gate pulldown** (10 kΩ) keeps the MOSFET OFF while the ESP32 boots.
- **Single 5 V / 2 A USB charger** feeds everything (strip 1–1.5 A + radars +
  ESP32 ≈ 1.85 A max). Star-ground everything at one point.
- Radars run on **5 V** (not 3.3 V) — do not feed them from the ESP32's 3V3 pin.
- **LD2410C is a drop-in** for the LD2410B (same OUT + UART + 256 kbps protocol),
  with breadboard-friendly 2.54 mm pins and built-in BLE for app configuration.
  **Verify the pin order** on the silkscreen — it can differ between B and C.

---

## 3. Cable choice — Cat6 vs separate 24 AWG

**Use Cat6 (or Cat5e).** It is the better choice for this configuration:

| Factor | Cat6 | Separate 24 AWG |
|--------|------|-----------------|
| Noise immunity | ✅ twisted pairs (each signal + GND) | ❌ untwisted unless done by hand |
| Conductors | ✅ 8 wires (4 pairs) | must buy 4+ strands |
| Gauge / power | ✅ 23 AWG solid (less 5 V drop) | 24 AWG (higher resistance) |
| Cost / convenience | ✅ one cable | multiple rolls |

The OUT and TX lines are 3.3 V push-pull digital — reliable over 15–30 m only if
each is **twisted with a GND return** to reject EMI. Cat6 provides that for free.

### Per-radar Cat6 pair assignment

| Pair | Signals | Purpose |
|------|---------|---------|
| 1 (orange) | VCC (5 V) + GND | power |
| 2 (green) | OUT + GND | presence |
| 3 (blue) | TX + GND | UART distance (RX ← radar) |
| 4 (brown) | RX + GND | UART config (TX → radar, sensing distance) |

- One Cat6 cable **per radar** (bottom and top run back to the controller).
- 23 AWG solid handles the radar's ~80–100 mA with negligible drop over 20 m.

### When to use separate 24 AWG instead

- Short runs (< ~2 m) or bench prototyping, where flexibility beats noise.
- Where you need **stranded** wire for tight, repeated flexing.
