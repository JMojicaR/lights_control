# Bill of Materials — CAN Option

Distributed smart-sensor architecture: one ESP32-S3 main controller + three
smart-sensor nodes (2× VL53L0X, 1× BH1750) on a CAN bus. Prices are mid-range
MercadoLibre México / Amazon MX estimates (Sept 2026, ~1 USD = 18 MXN).

## Main controller

| # | Component | Qty | Unit (MXN) | Total | Notes |
|---|-----------|-----|-----------|-------|-------|
| 1 | ESP32-S3 SuperMini | 1 | $200 | $200 | Existing controller (unchanged) |
| 2 | SN65HVD230 CAN transceiver | 1 | $60 | $60 | 3.3 V, TWAI → CANH/CANL |
| 3 | IRLZ44N MOSFET (TO-220) | 1 | $30 | $30 | 12 V LED strip switch |
| 4 | 12 V LED strip 5 m (5050) | 1 | $250 | $250 | Staircase lighting |
| 5 | 12 V PSU 6 A | 1 | $220 | $220 | Powers strip + main |
| 6 | Main enclosure + fuse + terminals | 1 | $150 | $150 | Waterproof box, 3 A fuse |

## Sensor nodes (×3 — identical core, different sensor)

| # | Component | Qty | Unit | Total | Notes |
|---|-----------|-----|------|-------|-------|
| 7 | ESP32 DevKit (WROOM-32) | 3 | $150 | $450 | Node MCU (has TWAI/CAN) |
| 8 | SN65HVD230 CAN transceiver | 3 | $60 | $180 | One per node |
| 9 | VL53L0X ToF sensor | 2 | $75 | $150 | Bottom + top nodes |
| 10 | BH1750 ambient light sensor | 1 | $75 | $75 | Ambient-light node |
| 11 | Node enclosure + local I²C wiring | 3 | $50 | $150 | Small box, short I²C jumpers |

## CAN bus infrastructure

| # | Component | Qty | Unit | Total | Notes |
|---|-----------|-----|------|-------|-------|
| 12 | Twisted-pair cable (Cat5e, ~15 m) | 1 | $100 | $100 | CANH/CANL + GND |
| 13 | 120 Ω termination resistors | 2 | $3 | $5 | One at each bus end |
| 14 | Screw terminals / connectors | 1 set | $50 | $50 | Node drops off the bus |

## Totals

| Category | Cost (MXN) |
|----------|-----------|
| Main controller | $910 |
| Sensor nodes | $1,005 |
| CAN bus infrastructure | $155 |
| **Total (prototype)** | **~$2,070** |
| Contingency (~10%) | ~$200 |
| **Grand total** | **~$2,270** |

## Cheaper node options (if cost matters)

| Node MCU | Unit cost | CAN support | Note |
|----------|-----------|-------------|------|
| ESP32 DevKit | $150 | Native TWAI | Used above (code-uniform) |
| ESP32-C3 SuperMini | $100 | ❌ no CAN | Needs external MCP2515 |
| STM32F103 "Blue Pill" | $70 | ✅ native CAN | Cheaper, steeper toolchain |
| ATmega328P + MCP2515 | $90 | via SPI CAN controller | Classic, well-documented |

Swapping the 3 node MCUs from ESP32 ($150) to **STM32F103** ($70) saves
**$240** on the total. Swapping to ATmega328P+MCP2515 saves ~$180. The ESP32 is
kept here for code uniformity, but the node firmware's logic ports directly to
STM32/ATmega.
