# Electrical Notes — Two 3 m VL53L0X Cables + Local BH1750

Two **VL53L0X** ToF sensors each sit at the end of their **own 3 m cable**
(home-run back to the ESP32-S3). The **BH1750** ambient-light sensor sits
**~15 cm** from the ESP32 on short wires, effectively on the board. All three
share one I²C bus (SDA = GPIO 12, SCL = GPIO 13).

> **The golden rule:** on a long I²C run, **bus capacitance — not wire gauge —
> is what fails you.** The I²C spec allows **400 pF total** in standard mode
> (100 kHz). Two 3 m cables fit *inside* that budget (barely), which keeps this
> setup workable — but with thin margin, as §0 shows.

---

## 0. The budget (two 3 m cables, one local sensor)

| What adds capacitance | Value |
|-----------------------|-------|
| 2 × 3 m Cat6 @ ~50 pF/m | ~300 pF |
| BH1750 @ 15 cm | ~8 pF (negligible) |
| 2 × VL53L0X + 1 × BH1750 (input caps) | ~30 pF |
| **Total on SDA (and on SCL)** | **~340 pF** |

~340 pF is **under** the 400 pF budget, but there is little headroom. Rise-time
check with the on-board pull-ups (3 × 10 kΩ in parallel = 3.3 kΩ, §2):

| Pull-up (effective) | τ = R·C | Rise (10–90% ≈ 2.2 τ) | 100 kHz (needs ≤ 1 µs) |
|---------------------|---------|------------------------|------------------------|
| 3.3 kΩ (3 × 10 kΩ) | 1.12 µs | ~2.5 µs | ❌ too slow |
| 2.2 kΩ | 0.75 µs | ~1.7 µs | ❌ still too slow |
| 1.5 kΩ | 0.51 µs | ~1.1 µs | ⚠️ marginal |
| 1.0 kΩ (≈ floor) | 0.34 µs | ~0.75 µs | ✅ but at the sink limit |

**Conclusion:** 100 kHz is now *theoretically* reachable, but only with a pull-up
near the ~1 kΩ sink-current floor (~1.0–1.5 kΩ) — which means removing the
on-board 10 kΩ pull-ups and leaving no safety margin. The practical, no-hassle
choices are:

1. **50 kHz, leave the on-board pull-ups alone** (§3) — recommended, or
2. **P82B715** if you want 100 kHz with headroom, or plan to extend further (§4).

---

## 1. Cable type & per-sensor Cat6 wiring

**Cat6 (or Cat5e) is the recommended cable** — 4 twisted pairs, ~50 pF/m, cheap,
and available shielded (FTP/SFTP). Avoid flat ribbon, loose hookup wire, and zip
cord (no twist → crosstalk and uneven capacitance).

Only the **two VL53L0X** need long cables. Use the pairs deliberately — **each
fast signal (SDA, SCL) is twisted with a GND return**:

### VL53L0X cable (6 conductors used, 2 spare)

| Pair | Signals | Why |
|------|---------|-----|
| 1 | **SDA + GND** | Data line twisted with ground → tight, balanced loop. |
| 2 | **SCL + GND** | Same. Never twist SDA with SCL together — they'd couple into each other. |
| 3 | 3.3 V + GND | Sensor power (~10 mA); gauge is for robustness, not ampacity. |
| 4 | **XSHUT + GPIO1** | Both slow control signals — see §5 for details. |

### BH1750 (local, ~15 cm)

No long cable needed — mount it on the breakout near the ESP32 with short
(≤ 15 cm) hookup wires or a header. Its SDA/SCL/VCC/GND simply join the same
bus. If you do cable it, the same pair discipline applies, but at 15 cm the
capacitance is negligible (~8 pF).

> ⚠️ **Do NOT run the 12 V LED-strip power inside any sensor cable.** The LED
> MOSFET switching (PWM at 5 kHz) couples into the data lines. Keep the 12 V run
> in a separate cable, physically spaced from the signal cables.

---

## 2. Pull-up resistors — when the boards already have them

Both VL53L0X and BH1750 breakout boards usually ship with **10 kΩ pull-ups on
SDA and SCL** (some omit them, some use 4.7 kΩ). When several boards sit on the
same bus, their pull-ups are **in parallel**:

| Boards on bus | Each | Effective pull-up |
|---------------|------|-------------------|
| 3 | 10 kΩ | **3.3 kΩ** ✅ |
| 3 | 4.7 kΩ | 1.6 kΩ ⚠️ near floor |
| 3 | 2.2 kΩ | 0.73 kΩ ❌ below floor |
| 2 | 10 kΩ | 5 kΩ (too weak alone) |

### What this means in practice

1. **Verify first.** Confirm each board actually *has* the pull-up, its value
   (10 kΩ is usual), and that it pulls to **3.3 V** (not 5 V). If any board pulls
   to 5 V, remove it — it would back-drive the ESP32's 3.3 V rail.
2. **Three 10 kΩ in parallel already give 3.3 kΩ** — already in the sweet spot,
   so **do not blindly add 2.2 kΩ external resistors.** Adding 2.2 kΩ across
   3.3 kΩ gives ~1.3 kΩ (below the comfortable floor and near the sink limit).
3. **The on-board pull-ups are your floor.** You can only lower the effective
   value safely by *removing* some, not by stacking more. If you want 100 kHz
   (§0), remove all on-board pull-ups and install **one set** of ~1.5 kΩ at the
   ESP32 end — and accept the thin margin.
4. If the boards use 4.7 kΩ each (→ 1.6 kΩ effective), that's the *minimum*
   usable value — fine for 50 kHz. If they use 2.2 kΩ each, you **must** remove
   pull-ups (0.73 kΩ violates the sink spec).

**Bottom line:** with 3 × 10 kΩ on-board pull-ups, **leave them alone** — they
give the ~3.3 kΩ you want for 50 kHz. The fix for more speed is the **bus speed
(§3)** or the **extender (§4)**, *not* more resistors.

---

## 3. Bus speed

| Speed | Verdict for 2 × 3 m cables | Notes |
|-------|----------------------------|-------|
| 400 kHz | ❌ | Over the 400 pF budget. |
| 100 kHz | ⚠️ Marginal | ~340 pF needs a ~1.0–1.5 kΩ pull-up (near the floor) to meet the 1 µs rise spec. |
| **50 kHz** | ✅ **Default** | ~2.5 µs rise with the 3.3 kΩ on-board pull-ups is comfortable in a 20 µs period. |

The code currently drives **100 kHz** (`I2C_CLOCK_HZ` in `config.h`) with a 50 ms
`Wire.setTimeOut()`. **With the on-board 10 kΩ pull-ups, change `I2C_CLOCK_HZ` to
`50000`.** If you remove the on-board pull-ups and install ~1.5 kΩ at the ESP32,
you *can* stay at 100 kHz, but 50 kHz is the safer call. With a P82B715, keep
100 kHz.

---

## 4. Active extender (P82B715) — optional, for headroom

At ~340 pF, 50 kHz works without an extender. Add a **P82B715** if you want
**100 kHz with comfortable margin**, or if the cables get longer.

| Part | Type | Range | Notes |
|------|------|-------|-------|
| **P82B715** | Bus buffer (bidirectional) | ~10–50 m | **First choice.** Splits the bus into a 400 pF "local" side and a **3000 pF** "transmission" side. |
| P82B96 | Bus buffer | ~20 m | Similar, higher drive. |
| PCA9615 | Differential (I²C over twisted pair) | tens of metres | For >20 m or electrically noisy sites. |

### P82B715 wiring

```
                     ┌─────────[Lx/Ly]──────────╌ Cat6 ╌── VL53L0X bottom (3 m)
ESP32 ──[Sx/Sy]── P82B715 ──[Lx/Ly]────────────╌ Cat6 ╌── VL53L0X top    (3 m)
  (2.2 kΩ pull-ups)    └─────── BH1750 stays local (15 cm) on the Sx/Sy side
                     (remote pull-ups 470 Ω–1 kΩ to 3.3 V, at the P82B715)
```

- Put the **P82B715 at the ESP32 end**; the two 3 m VL53L0X cables hang off its
  **Lx/Ly side** (~300 pF is trivial inside the 3000 pF budget). The BH1750 can
  stay on the local Sx/Sy side.
- **Remote-side pull-ups: 470 Ω–1 kΩ** (the buffer provides the drive current).
  One set at the P82B715, not on every board.
- Run the P82B715 at **3.3 V** to match the sensors (VCC range 3–12 V).
- Leave the sensors' on-board 10 kΩ pull-ups in place — in parallel with 470 Ω
  they contribute negligibly and don't hurt.

---

## 5. XSHUT and GPIO1 over Cat6 (VL53L0X only)

These two signals are **not buffered by the P82B715** — they run as plain wires
inside the VL53L0X's Cat6 pair (pair 4). They're slow, so they're forgiving, but
the interrupt line deserves care. (The BH1750 has neither, so this only applies
to the two long VL53L0X cables.)

### XSHUT (enable input, active-high)

- **Function:** holds the sensor in shutdown when low. The ESP32 GPIO drives it.
- **Level when floating:** during ESP32 boot/reset the pin is high-impedance. Add
  a **10 kΩ pull-down to GND at the sensor** so it stays in shutdown until the
  firmware explicitly wakes it. (If the module already has a pull resistor, verify
  which way — a pull-**up** leaves the sensor running by default, usually fine too,
  but a defined level is the point.)
- **Reflection damping:** optional **100 Ω series resistor at the ESP32 end**; for
  a slow 3 m logic line it's rarely needed, but cheap insurance.

### GPIO1 (interrupt out, active-low, open-drain)

This is the **noise-sensitive one** — an open-drain line is high-impedance when
idle, so the 3 m wire behaves like an antenna.

- **Twist GPIO1 with GND** in its own pair (pair 4) — non-negotiable.
- **Pull-up at the ESP32 end: 4.7 kΩ to 3.3 V** (stronger than 10 kΩ for better
  noise immunity). One pull-up, at the controller, close to the interrupt pin.
- **RC glitch filter at the ESP32 end:** 100 Ω series + **1–10 nF** to GND (go up
  to 100 nF only if you see false triggers — presence detection is slow, so the
  extra filtering is invisible to the logic).
- Don't route GPIO1 in the same pair as SDA/SCL; keep it on the slow pair with
  XSHUT or its own GND.

---

## 6. Grounding & power

- Tie **all GNDs together at one point** (star ground) — ESP32, both VL53L0X,
  BH1750, P82B715 (if used), and the LED MOSFET source.
- Feed each VL53L0X **3.3 V from the ESP32's regulator** over its own cable; total
  draw is tiny (~10 mA per sensor). Add a **10–100 µF decoupling cap at each
  sensor end** for the 3 m run.
- The **12 V LED feed is a separate circuit** — its return current must not flow
  through the I²C ground returns. Keep the 12 V cable physically separate (§1).

---

## 7. Quick reference (2 × 3 m VL53L0X + local BH1750)

| Parameter | No extender | With P82B715 |
|-----------|-------------|--------------|
| Bus speed | **50 kHz** | **100 kHz** |
| Pull-ups (SDA + SCL) | On-board 3 × 10 kΩ = 3.3 kΩ (leave as-is) | Local 2.2 kΩ · Remote 470 Ω–1 kΩ |
| Minimum pull-up | ~1 kΩ (3 mA sink limit) | n/a (buffer drives) |
| Cable | Cat6 twisted pair, one per VL53L0X | Cat6 twisted pair, one per VL53L0X |
| BH1750 | Local, ≤ 15 cm hookup | Local, on Sx/Sy side |
| Pair assignment (VL53L0X) | SDA+GND, SCL+GND, 3.3V+GND, XSHUT+GPIO1 | same |
| XSHUT | 10 kΩ pull-down at sensor (+ optional 100 Ω series) | same |
| GPIO1 (IRQ) | 4.7 kΩ pull-up + 100 Ω / 1–10 nF filter at ESP32 | same |
| I²C timeout | 50 ms (`Wire.setTimeOut`) | 50 ms |
| Code change | `I2C_CLOCK_HZ = 50000` | keep `100000` |

Datasheets and pinout images for every component (including the P82B715) are in
`datasheets/`.
