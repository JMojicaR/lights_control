# Electrical Notes — 3 m Sensor Run

The two **VL53L0X** ToF sensors and the **BH1750** ambient-light sensor sit
**~3 m from the ESP32-S3** on a shared I²C bus (SDA = GPIO 12, SCL = GPIO 13).
This document covers the electrical choices that keep that long run reliable.

> **The golden rule:** on a long I²C run, **bus capacitance — not wire gauge —
> is what fails you.** The I²C specification allows **400 pF total** in standard
> mode (100 kHz). A typical cable is 50–100 pF/m, so a 3 m run alone consumes
> ~150–300 pF *before* you add the two VL53L0X and the BH1750 (each device adds
> ~10–15 pF). You have almost no headroom left, so every choice below is about
> clawing that headroom back.

---

## 1. Recommended cable type

| Choice | Verdict | Notes |
|--------|---------|-------|
| **Cat5e / Cat6 Ethernet** | ✅ **Recommended** | 4 twisted pairs, ~50 pF/m, cheap, shielded (STP/FTP if you can). |
| 24 AWG twisted pair (0.20 mm²) | ✅ Good | Fine for short-ish runs; the twist is what matters, not the gauge. |
| Flat ribbon / loose hookup wire | ❌ Avoid | Untwisted runs act as antennas and add uneven capacitance. |
| Speaker / zip cord | ❌ Avoid | No twist → crosstalk between SDA/SCL and the 12 V LED feed. |

**Pair assignment** (Cat5e has 4 pairs — use them deliberately):

| Pair | Signals | Why |
|------|---------|-----|
| 1 | **SDA + GND** | Each data line is twisted with a ground return → tight, balanced loop. |
| 2 | **SCL + GND** | Same. Never twist SDA with SCL together — they'd couple into each other. |
| 3 | 3.3 V + GND | Sensor power. Current is ~3 mA per sensor, so gauge is for robustness, not ampacity. |
| 4 | XSHUT (bottom) + XSHUT (top) | Slow digital enables; any twist is fine. |

The **IRQ lines** (VL53L0X GPIO1, active-low interrupt out) run 3 m too. They are
open-drain, so give each its own pull-up (see §3) and, ideally, route them in the
spare twist with a GND. The interrupt is a slow, edge-triggered signal, so it is
far more forgiving than SDA/SCL.

> ⚠️ **Do NOT run the 12 V LED-strip power in the same cable as SDA/SCL.** The
> LED MOSFET switching (PWM at 5 kHz) couples into the data lines. Keep the 12 V
> run in a separate cable, physically spaced from the I²C cable.

---

## 2. Bus speed

| Speed | Verdict for 3 m | Notes |
|-------|-----------------|-------|
| 400 kHz (fast mode) | ❌ Too fast | Needs ≤ ~1.2 µs rise time; a 3 m cable can't reach it without a buffer. |
| **100 kHz (standard)** | ✅ **Default** | Within the 400 pF budget *if* pull-ups are right (§3). Already set in `config.h` (`I2C_CLOCK_HZ`). |
| 50 kHz | ✅ Fallback | Halves the timing pressure. Use if you see intermittent `0xFF` reads / `[✗] not found`. |

The code already drives the bus at **explicit 100 kHz** and sets a **50 ms
fail-fast timeout** (`Wire.setTimeOut`), so a stuck bus reports an error instead
of hanging boot. If comms are flaky at 100 kHz, change `I2C_CLOCK_HZ` to `50000`
in `config.h` — speed is the first casualty of a long, capacitive bus, and 50 kHz
is still plenty for two ToF sensors plus a light sensor.

---

## 3. Pull-up resistors on the I²C bus

This is the single most important change for a 3 m run.

Most VL53L0X and BH1750 breakout boards ship with **10 kΩ pull-ups**. At 10 kΩ
the rise time constant is `τ = 10 kΩ × ~300 pF ≈ 3 µs` — the rising edge never
gets there in the ~1 µs a 100 kHz clock allows, so the bus reads `0xFF` and the
sensors "vanish."

| Pull-up value | Rise time (10–90% ≈ 2.2 τ, ~300 pF bus) | Verdict |
|---------------|------------------------------------------|---------|
| 10 kΩ | ~6.6 µs | ❌ Too slow (breaks 100 kHz) |
| 4.7 kΩ | ~3.1 µs | ⚠️ Marginal |
| **2.2 kΩ** | **~1.5 µs** | ✅ **Recommended** |
| 1.5 kΩ | ~1.0 µs | ✅ Floor — don't go lower |
| 1.0 kΩ | ~0.66 µs | ❌ Below the 3 mA I²C sink spec |

**Recommendation:** replace the breakout boards' 10 kΩ pull-ups with **2.2 kΩ to
3.3 kΩ** (to 3.3 V), one on SDA and one on SCL. Only put them **at one location**
— the ESP32 end — so you don't parallel-load the bus. If your breakouts have the
pull-ups hard-wired, either desolder them or pick a spot and accept the parallel
value (two 10 kΩ in parallel = 5 kΩ, still marginal).

Lower pull-ups charge the bus faster, but **never go below ~1.5 kΩ at 3.3 V** —
the I²C spec limits sink current to 3 mA, and `R_min ≈ 3.3 V / 3 mA ≈ 1.1 kΩ`.

---

## 4. Active extender (P82B715) — when and how

At 3 m with correct pull-ups, **you should not need an extender.** But if the run
grows, the cable is unshielded, or you keep seeing intermittent errors even at
50 kHz, add an active I²C buffer.

| Part | Type | Range | Notes |
|------|------|-------|-------|
| **P82B715** | Bus buffer (bidirectional) | ~10–20 m | **Your first choice.** Splits the bus into a low-capacitance "local" side and a high-drive "remote" side; needs *different* pull-ups on each side. DIP/SOIC, easy to dead-bug. |
| P82B96 | Bus buffer | ~20 m | Similar, higher drive. |
| PCA9615 | Differential (I²C over twisted pair) | tens of metres | Converts SDA/SCL to a differential pair — the robust option for >20 m or electrically noisy sites. |

### P82B715 wiring sketch

```
ESP32 ─── SDA ──┬───────────── P82B715 (local side, 2.2 kΩ pull-ups)
   (2.2 kΩ)     │                  │
                └── SCL ───────────┤
                                   │  ─── remote bus (up to ~20 m)
        sensors at 3 m ─── SDA ────┤  remote pull-ups: 470 Ω–1 kΩ (higher drive)
                          SCL ─────┘
```

The P82B715 lets the remote (long) side use **stronger pull-ups (down to ~470 Ω)**
because its buffers provide the current, so the cable capacitance is charged
aggressively while the ESP32's local side stays within the normal 3 mA budget.
Order through the usual distributors (it's an NXP/Texas Instruments part); the
datasheet is in `datasheets/`.

---

## 5. Interrupt (GPIO1) and XSHUT lines over 3 m

- **GPIO1 (IRQ out, active-low, open-drain):** needs a pull-up. 10 kΩ to 3.3 V is
  fine (it's a slow edge signal), but if the run picks up noise, lower it to
  4.7 kΩ and/or add a small RC filter at the ESP32 end (e.g. 100 nF to GND + a
  100 Ω series resistor right at the GPIO1 pin).
- **XSHUT (enable in):** plain logic-level input; the 3 m run is harmless. Keep
  the 10 kΩ pull-up the module includes so the pin never floats when the ESP32
  output is high-impedance during reset.

---

## 6. Grounding & power

- Tie **all GNDs together at one point** (star ground) — ESP32, both VL53L0X,
  BH1750, and the LED MOSFET source.
- Feed the sensors **3.3 V from the ESP32's regulator** over the twisted pair;
  total draw is tiny (~10 mA). If the 3.3 V rail sags over the run (unlikely at
  this current), add a 10–100 µF decoupling cap at each sensor end.
- The **12 V LED feed is a separate circuit**: its return current must not flow
  through the I²C ground return. Keep the 12 V cable and the signal cable apart
  (§1).

---

## 7. Quick reference

| Parameter | Value for 3 m run |
|-----------|-------------------|
| Bus speed | **100 kHz** (fallback 50 kHz) |
| Pull-ups (SDA + SCL) | **2.2–3.3 kΩ** to 3.3 V, at one end only |
| Minimum pull-up | ~1.5 kΩ (3 mA sink limit) |
| Cable | Cat5e/Cat6 twisted pair (~50 pF/m) |
| Pair assignment | SDA+GND, SCL+GND, 3.3V+GND, XSHUT pair |
| IRQ pull-up | 10 kΩ (4.7 kΩ + 100 nF RC if noisy) |
| Extender (if needed) | P82B715 (P82B96 alt, PCA9615 for >20 m) |
| I²C timeout | 50 ms (`Wire.setTimeOut`) |

Datasheets and pinout images for every component are in `datasheets/`.
