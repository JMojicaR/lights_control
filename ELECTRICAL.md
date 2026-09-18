# Electrical Notes — Three 3 m Sensor Cables (Star Topology)

Two **VL53L0X** ToF sensors and one **BH1750** ambient-light sensor each sit at
the end of their **own 3 m cable**, home-run (star topology) back to the
ESP32-S3. All three share one I²C bus (SDA = GPIO 12, SCL = GPIO 13), so every
cable's capacitance adds onto the same two nets.

> **The golden rule:** on a long I²C run, **bus capacitance — not wire gauge —
> is what fails you.** The I²C spec allows **400 pF total** in standard mode
> (100 kHz). Three 3 m cables **already blow that budget**, which changes the
> pull-up *and* speed advice below.

---

## 0. The budget problem (why three cables are different from one)

| What adds capacitance | Value |
|-----------------------|-------|
| 3 × 3 m Cat6 @ ~50 pF/m | ~450 pF |
| 2 × VL53L0X + 1 × BH1750 (input caps) | ~45 pF |
| **Total on SDA (and on SCL)** | **~500 pF** |

~500 pF is **over the 400 pF standard-mode budget**, so 100 kHz cannot be met
with passive pull-ups at this distance. The rise-time check makes it concrete:

| Pull-up (effective) | τ = R·C | Rise (10–90% ≈ 2.2 τ) | 100 kHz (needs ≤ 1 µs) |
|---------------------|---------|------------------------|------------------------|
| 3.3 kΩ (3 × 10 kΩ) | 1.65 µs | ~3.6 µs | ❌ too slow |
| 2.2 kΩ | 1.10 µs | ~2.4 µs | ❌ too slow |
| 1.0 kΩ (≈ floor) | 0.50 µs | ~1.1 µs | ⚠️ right at the limit |

To meet 100 kHz you'd need a pull-up near **~950 Ω**, which is *below* the
~1 kΩ floor set by the 3 mA I²C sink-current spec. **Conclusion: at ~500 pF you
cannot recover 100 kHz with passive pull-ups.** You have two real options:

1. **Drop the bus to 50 kHz** (passive — §3), or
2. **Add a P82B715** so the cables live on its 3000 pF transmission side (§4).

---

## 1. Cable type & per-sensor Cat6 wiring

**Cat6 (or Cat5e) is the recommended cable** — 4 twisted pairs, ~50 pF/m, cheap,
and available shielded (FTP/SFTP). Avoid flat ribbon, loose hookup wire, and zip
cord (no twist → crosstalk and uneven capacitance).

Each sensor gets its own cable. Use the pairs deliberately — **each fast signal
(SDA, SCL) is twisted with a GND return**:

### VL53L0X cable (6 conductors used, 2 spare)

| Pair | Signals | Why |
|------|---------|-----|
| 1 | **SDA + GND** | Data line twisted with ground → tight, balanced loop. |
| 2 | **SCL + GND** | Same. Never twist SDA with SCL together — they'd couple into each other. |
| 3 | 3.3 V + GND | Sensor power (~10 mA); gauge is for robustness, not ampacity. |
| 4 | **XSHUT + GPIO1** | Both slow control signals — see §5 for details. |

### BH1750 cable (4 conductors used)

| Pair | Signals |
|------|---------|
| 1 | SDA + GND |
| 2 | SCL + GND |
| 3 | 3.3 V + GND |
| 4 | spare (or ADDR + GND) |

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
2. **Three 10 kΩ in parallel already give 3.3 kΩ** — that is *already* in the
   sweet spot, so **do not blindly add 2.2 kΩ external resistors.** Adding
   2.2 kΩ across 3.3 kΩ gives ~1.3 kΩ (below the ~1.5 kΩ floor and near the sink
   limit).
3. **The on-board pull-ups are your floor.** You can only lower the effective
   value safely by *removing* some (to install a single set at the ESP32 end), not
   by stacking more. If the value is wrong, the clean fix is: remove all on-board
   pull-ups and install **one set** (SDA + SCL) at the ESP32 end.
4. If the boards use 4.7 kΩ each (→ 1.6 kΩ effective), that's the *minimum*
   usable value — fine for 50 kHz, but too low for comfort at higher drive. If
   they use 2.2 kΩ each, you **must** remove pull-ups (0.73 kΩ violates the sink
   spec).

**Bottom line:** with 3 × 10 kΩ on-board pull-ups, **leave them alone** — they
give the ~3.3 kΩ you want. The fix for a ~500 pF bus is the **bus speed (§3)** or
the **extender (§4)**, *not* more resistors.

---

## 3. Bus speed

| Speed | Verdict for 3 × 3 m cables | Notes |
|-------|----------------------------|-------|
| 400 kHz | ❌ | Way over the 400 pF budget. |
| 100 kHz | ⚠️ Only with P82B715 | ~500 pF + ~3.3 kΩ gives ~3.6 µs rise → fails the 1 µs spec (§0). |
| **50 kHz** | ✅ **Default (no extender)** | ~3.6 µs rise is comfortable within a 20 µs period. Plenty for two ToF + one light sensor. |

The code currently drives **100 kHz** (`I2C_CLOCK_HZ` in `config.h`) with a 50 ms
`Wire.setTimeOut()`. **Without the P82B715, change `I2C_CLOCK_HZ` to `50000`.** With
the extender you can stay at 100 kHz.

---

## 4. Active extender (P82B715) — recommended for this setup

At ~500 pF across three cables, the P82B715 is **no longer "optional for the
future" — it is the clean way to keep 100 kHz and gain headroom.**

| Part | Type | Range | Notes |
|------|------|-------|-------|
| **P82B715** | Bus buffer (bidirectional) | ~10–50 m | **First choice.** Splits the bus into a 400 pF "local" side and a **3000 pF** "transmission" side. |
| P82B96 | Bus buffer | ~20 m | Similar, higher drive. |
| PCA9615 | Differential (I²C over twisted pair) | tens of metres | For >20 m or electrically noisy sites. |

### P82B715 wiring (star topology)

```
                     ┌─────────[Lx/Ly]──────────╌ Cat6 ╌── VL53L0X bottom (3 m)
ESP32 ──[Sx/Sy]── P82B715 ──[Lx/Ly]────────────╌ Cat6 ╌── VL53L0X top    (3 m)
  (2.2 kΩ pull-ups)    │                          ╌ Cat6 ╌── BH1750        (3 m)
                     (remote pull-ups 470 Ω–1 kΩ to 3.3 V, at the P82B715)
```

- Put the **P82B715 at the ESP32 end**; all three sensor cables hang off its
  **Lx/Ly side** (~500 pF is well inside the 3000 pF budget).
- **Remote-side pull-ups: 470 Ω–1 kΩ** (the buffer provides the drive current, so
  these strong pull-ups charge the cable fast). One set at the P82B715, not on
  every board.
- Run the P82B715 at **3.3 V** to match the sensors (its VCC range is 3–12 V; 5 V
  also works but then keep remote pull-ups to the *same* rail the sensors use).
- Leave the sensors' on-board 10 kΩ pull-ups in place — in parallel with 470 Ω
  they contribute negligibly (~410 Ω total) and don't hurt.

---

## 5. XSHUT and GPIO1 over Cat6

These two signals are **not buffered by the P82B715** — they run as plain wires
inside the sensor's Cat6 pair (pair 4). They're slow, so they're forgiving, but
the interrupt line deserves care.

### XSHUT (enable input, active-high)

- **Function:** holds the sensor in shutdown when low. The ESP32 GPIO drives it.
- **Level when floating:** during ESP32 boot/reset the pin is high-impedance. Add
  a **10 kΩ pull-down to GND at the sensor** so it stays in shutdown until the
  firmware explicitly wakes it. (If the module already has a pull resistor, verify
  which way — a pull-**up** leaves the sensor running by default, which is usually
  fine too, but a defined level is the point.)
- **Reflection damping:** optional **100 Ω series resistor at the ESP32 end**; for
  a slow 3 m logic line it's rarely needed, but it's cheap insurance.

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
- Feed each sensor **3.3 V from the ESP32's regulator** over its own cable; total
  draw is tiny (~10 mA per sensor). Add a **10–100 µF decoupling cap at each
  sensor end** for a 3 m run.
- The **12 V LED feed is a separate circuit** — its return current must not flow
  through the I²C ground returns. Keep the 12 V cable physically separate (§1).

---

## 7. Quick reference (3 × 3 m cables)

| Parameter | No extender | With P82B715 |
|-----------|-------------|--------------|
| Bus speed | **50 kHz** | **100 kHz** |
| Pull-ups (SDA + SCL) | On-board 3 × 10 kΩ = 3.3 kΩ (leave as-is) | Local 2.2 kΩ · Remote 470 Ω–1 kΩ |
| Minimum pull-up | ~1 kΩ (3 mA sink limit) | n/a (buffer drives) |
| Cable | Cat6 twisted pair, one per sensor | Cat6 twisted pair, one per sensor |
| Pair assignment | SDA+GND, SCL+GND, 3.3V+GND, XSHUT+GPIO1 | same |
| XSHUT | 10 kΩ pull-down at sensor (+ optional 100 Ω series) | same |
| GPIO1 (IRQ) | 4.7 kΩ pull-up + 100 Ω / 1–10 nF filter at ESP32 | same |
| I²C timeout | 50 ms (`Wire.setTimeOut`) | 50 ms |
| Code change | `I2C_CLOCK_HZ = 50000` | keep `100000` |

Datasheets and pinout images for every component (including the P82B715) are in
`datasheets/`.
