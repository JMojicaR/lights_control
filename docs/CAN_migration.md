# CAN Migration — Research & Architecture

Can the staircase light controller use **CAN** instead of **I²C** for its sensor
links? **Yes** — and it is arguably the *better* choice for the 3 m sensor run,
at the cost of extra hardware. This document covers the research, the "adapter
circuit" that adapts the existing sensors to CAN without changing them, and the
code changes made.

---

## 1. CAN vs I²C — why it matters here

| Property | I²C (current) | CAN (proposed) |
|----------|---------------|----------------|
| Physical layer | 2 wires, single-ended, open-drain (SDA/SCL) | 2 wires, **differential** (CANH/CANL) + GND |
| Max practical length | ~1–3 m (400 pF budget) | **~40 m @ 1 Mbps, ~1 km @ 50 kbps** |
| Noise immunity | Poor (single-ended, no error detection) | **Excellent** (differential + 15-bit CRC + ACK) |
| Addressing | 7-bit device address (fixed per chip) | **Message-based** (any node sends/receives by ID) |
| Multi-master / priority | No real arbitration | **Built-in arbitration** (lowest ID wins) |
| Error handling | None (NAK on unaddressed) | Retransmission, error counters, bus-off recovery |
| Ecosystem | Sensors, EEPROMs, RTCs | **Automotive, industrial, OBD-II** |
| ESP32 support | Native `Wire` | Native **TWAI** (CAN 2.0) controller |

**Conclusion:** I²C is a short, board-level bus. Running it 3 m (as the current
build does, with pull-up/resistor tuning — see `ELECTRICAL.md` on the
`electrical_adjustments` branch) works but is fragile. CAN is *designed* for
exactly this: multi-node, differential, noise-immune communication over tens of
metres. For a staircase sensor run, CAN removes the capacitance/rise-time
problems entirely.

---

## 2. The constraint: sensors only speak I²C

The **VL53L0X** (ToF) and **BH1750** (ambient light) are I²C-only devices. They
have **no native CAN**, and they cannot be reflashed to speak CAN. So the
requirement *"do not change any sensor"* forces a **distributed smart-node**
architecture:

```
                    CAN bus (twisted pair, 120 Ω at each end)
    ┌────────────────┼────────────────┼──────────────────┐
    │                │                │                  │
 [MAIN]         [NODE A]        [NODE B]          [NODE C]
 ESP32-S3       ESP32 +        ESP32 +          ESP32 +
 + SN65HVD230   SN65HVD230     SN65HVD230       SN65HVD230
 (controller)      │               │                 │
               VL53L0X        VL53L0X           BH1750
               (bottom)       (top)             (ambient)
               via local I²C  via local I²C     via local I²C
                 (few cm)       (few cm)          (few cm)
```

Each sensor stays **exactly as-is**. The "adapter circuit" is a small node that
reads the sensor over a *short* local I²C link (centimetres, no long cable) and
publishes the measurement on the CAN bus.

---

## 3. The adapter circuit (per node)

A node is three things:

1. **A small microcontroller with a CAN controller.** The ESP32 (any variant
   with TWAI) is used here for code uniformity with the main. Cheaper
   alternatives with native CAN: STM32F103 ("Blue Pill"). For MCUs without CAN,
   add a standalone **MCP2515** SPI controller (the classic Arduino CAN approach).
2. **A CAN transceiver** — the **SN65HVD230** (3.3 V, ideal for ESP32) or
   TJA1050/MCP2551 (5 V). This converts the MCU's logic-level TX/RX to the
   differential CANH/CANL bus.
3. **The sensor**, wired to the node's I²C pins with short wires (its own 10 kΩ
   pull-ups are fine at this length).

### Node schematic (single VL53L0X node)

```
        ┌─────────────────────┐
        │   ESP32 (node)      │
        │                     │
        │  SDA (21) ──── VL53L0X SDA   (local, few cm)
        │  SCL (22) ──── VL53L0X SCL
        │  3V3     ──── VL53L0X VIN
        │  GND     ──── VL53L0X GND
        │                     │
        │  TX (5)  ──── SN65HVD230 CTX/D
        │  RX (4)  ──── SN65HVD230 CRX/R
        │  3V3     ──── SN65HVD230 VCC
        │  GND     ──── SN65HVD230 GND
        └──────────┬──────────┘
                   │
            SN65HVD230 CANH ──╌╌── CANH (bus)
            SN65HVD230 CANL ──╌╌── CANL (bus)
```

> ⚠️ **Bus termination:** CAN needs **120 Ω** between CANH and CANL at *each end*
> of the bus (two resistors total for a linear bus). The SN65HVD230 modules often
> include a jumper for an on-board 120 Ω — enable it only on the two end nodes.

---

## 4. CAN protocol (see `can_protocol.h`)

- **Rate:** 500 kbps (plenty of headroom at 3 m; good to ~100 m).
- **Frames:** standard 11-bit IDs.
- **Nodes:** main `0x01`, bottom ToF `0x10`, top ToF `0x11`, BH1750 `0x12`.

| Message ID | Direction | Payload |
|------------|-----------|---------|
| `0x110` | bottom ToF → main | `[presence][dist_mm lo][dist_mm hi][valid]` |
| `0x111` | top ToF → main | `[presence][dist_mm lo][dist_mm hi][valid]` |
| `0x112` | BH1750 → main | `[lux*10 lo][lux*10 hi]` |
| `0x120` | any node → main | `[node_id]` (heartbeat) |
| `0x201` | main → ToF node | `[target_node][threshold_mm lo][threshold_mm hi]` |

---

## 5. Code changes made (this branch)

### Main controller — `lights_control.ino`
- **Removed** I²C sensor code: `Wire`/`BH1750`/`VL53L0X` includes and objects,
  the XSHUT address-sequencing init, the GPIO1 interrupt ISRs, and the
  `arm/disarm/updateInterrupts()` + `readLux()` functions.
- **Added** `driver/twai.h` + `can_protocol.h`, and four functions:
  - `canSetup()` — initialises TWAI at 500 kbps.
  - `canPoll()` — consumes CAN frames, updates `presenceBottom/Top`,
    `distanceBottom/Top`, `lux`, and runs the presence timeout. It mirrors the
    old interrupt gating via `canAcceptPresence()` (night + lights-off + AUTO),
    so continued presence while the lights are ON still does **not** reset the
    countdown.
  - `canAcceptPresence()` — the "armed" condition, now checked per-frame.
  - `canSendThreshold()` — pushes a presence-threshold change to a ToF node
    (called on boot and when the dashboard changes the distance).
- **Everything else is unchanged** — WiFi, web dashboard, REST API, sunset/time,
  PWM fade, override, NVS persistence all work identically.

### New — `sensor_node/sensor_node.ino`
- One parameterised sketch (`NODE_TOF` or `NODE_LUX`) for the adapter nodes.
- `NODE_TOF`: reads the VL53L0X in continuous mode locally, publishes distance +
  presence at 10 Hz, and honours `SET_THRESHOLD` config messages.
- `NODE_LUX`: reads the BH1750 and publishes lux at 1 Hz.
- Both send a heartbeat every 2 s.

> The hardware-interrupt optimization from the I²C version is **not** needed here:
> the sensor is centimetres from the node, so local polling is trivial and fast.

---

## 6. Verdict

| | I²C (current) | CAN (this option) |
|---|---|---|
| Hardware cost | ~$1,138 MXN | ~$2,070 MXN |
| Max sensor distance | ~3 m (marginal) | tens of metres |
| Noise / reliability | Poor (needs careful pull-ups) | Excellent |
| Scalability (add sensors) | Limited (addresses, capacitance) | Trivial (add a node) |
| Standard / ecosystem | Board-level | Automotive + industrial |

**CAN is the right call** when the distance grows, the environment is noisy, or
the system needs to scale (e.g. commercial/industrial stair lighting). For a
single home staircase, the I²C version is cheaper and sufficient.
