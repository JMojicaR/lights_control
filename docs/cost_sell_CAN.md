# Cost & Sell Analysis — CAN Option

This is a **delta analysis** against the base (I²C) build. The base build's cost
and market analysis live on the `electrical_adjustments` branch
(`ELECTRICAL.md`, `market_analysis_lights_control.pdf`). Here we answer: *what
does migrating the sensor link to CAN cost, and does it make the product more
sellable?*

---

## 1. Cost delta

| Line | I²C build | CAN build | Δ |
|------|-----------|-----------|---|
| Controller + LED + PSU + enclosure | ~$910 | ~$910 | — |
| Sensor hardware (2× ToF + BH1750) | $225 | $225 | — |
| Sensor link | 2× 3 m I²C cable + pull-ups ($40) | CAN bus + terminators + connectors ($155) | +$115 |
| **Smart nodes (MCU + transceiver + box)** | **—** | **$1,005** | **+$1,005** |
| **Total prototype** | **~$1,175** | **~$2,295** | **~+$1,120 (~95%)** |

**The CAN option roughly doubles the hardware cost**, almost entirely from the
three node microcontrollers + transceivers. This is inherent to the
"don't change the sensors" constraint — each sensor needs a local brain to
translate I²C → CAN.

### Cost reduction levers (same principles as the base analysis)

| Lever | Saving |
|-------|--------|
| Node MCU: ESP32 → STM32F103 (native CAN) | −$240 (3 nodes) |
| Node MCU: ESP32 → ATmega328P + MCP2515 | −$180 |
| Node enclosure: shared DIN-rail enclosure | −$100 |
| LED strip 5050 → 2835 + 3 A PSU | −$160 |
| ESP32-S3 → ESP32-C3 main (needs no CAN, it *is* the main) | −$100 |
| **Cost-reduced CAN build** | **~$1,700–1,800** |

At production scale (custom PCB integrating the MCU + transceiver + sensor on one
board per node), the node cost collapses to ~$80–120 each, bringing the CAN build
close to the I²C build while keeping all of CAN's advantages.

---

## 2. Unit economics (CAN, small batch)

Using the base analysis's retail pricing ($1,699 MXN) as the anchor:

```
Retail price:              $1,699
- COGS (CAN prototype):    $2,295   ← higher than retail!
- COGS (CAN, cost-reduced):$1,750
─────────────────────────────────────
```

**Key finding:** at prototype/small-batch cost, the CAN build **loses money** at
the $1,699 consumer price point. It only becomes viable at:

- **Production scale** (custom node PCB → COGS ≤ ~$900), or
- **A higher, industrial price point** ($2,999–3,999), where CAN's robustness is
  the selling point.

---

## 3. Sell analysis — the CAN version is a *different product*

The CAN migration doesn't make the *same* product cheaper — it makes it a
*different, more industrial* product. The market split:

| | I²C build | CAN build |
|---|---|---|
| **Target customer** | Homeowner (single staircase) | Building manager, contractor, commercial/industrial |
| **Value proposition** | "Auto stair light, $1,699" | "Reliable multi-sensor stair lighting over long/noisy runs, scalable, standard protocol" |
| **Price point** | $1,699 | **$2,999–3,999** |
| **Competitors** | Battery PIR lights, smart-home hubs | Industrial lighting controllers, PLC-based systems ($5K–20K) |
| **Key win** | Cheap, good enough | **CAN = standard**, integrates with building automation / vehicle systems |

### Why CAN is sellable at a premium

1. **Distance & noise:** commercial stairwells/parking garages need sensor runs
   far beyond I²C's 3 m. CAN does tens of metres natively.
2. **Scalability:** adding a 4th/5th sensor is a new node, not a re-wire.
3. **Standard protocol:** CAN is the lingua franca of building/vehicle
   automation — the product can be a *node* in a larger system, not a standalone
   island.
4. **Reliability:** differential signaling + CRC + retransmission mean it works
   in electrically noisy environments (fluorescent ballasts, motor drives) where
   I²C would corrupt.

### Verdict

- **For the original goal (sell a home staircase light):** stick with I²C — the
  CAN migration is a *cost* without a matching consumer benefit.
- **For a commercial/industrial stair-lighting product:** the CAN version is the
  *right* architecture and commands a 2–3× price premium. It competes against
  $5K–20K PLC/lighting-control systems, not $200 battery lights.

**Recommendation:** keep I²C for the consumer product; pursue CAN as a separate
"pro/industrial" SKU at $2,999+ with custom node PCBs (to close the cost gap) and
building-automation integration (Modbus/BACnet gateway via the same ESP32 main).
