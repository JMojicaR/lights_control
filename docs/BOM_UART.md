# Bill of Materials — UART / mmWave Option

Recommended build from `options.md`: two **HLK-LD2410B mmWave radars** for
presence (replacing the VL53L0X ToF) and a **LDR photoresistor** for ambient
light (replacing the BH1750). No I²C bus — presence is a digital pin, light is
an ADC pin, distance is UART. Prices are mid-range MercadoLibre México /
Amazon MX (Sept 2026, ~1 USD = 18 MXN).

## Main controller + sensors

| # | Component | Qty | Unit (MXN) | Total | Notes |
|---|-----------|-----|-----------|-------|-------|
| 1 | ESP32-S3 SuperMini | 1 | $200 | $200 | Unchanged controller |
| 2 | HLK-LD2410B mmWave radar (24 GHz) | 2 | $180 | $360 | Presence (moving + stationary), OUT pin + UART |
| 3 | LDR photoresistor (GL5528) + 10 kΩ | 1 | $10 | $10 | Ambient light (ADC divider) |
| 4 | IRLZ44N MOSFET (TO-220) | 1 | $30 | $30 | 12 V LED strip switch |
| 5 | 12 V LED strip 5 m (5050) | 1 | $250 | $250 | Staircase lighting |
| 6 | 12 V PSU 6 A (72 W) | 1 | $220 | $220 | Powers strip + ESP32 |
| 7 | Wiring + connectors (radar 3–5 wire) | 1 set | $60 | $60 | VCC/GND/OUT + optional UART |
| 8 | Enclosure + inline fuse + misc | 1 | $150 | $150 | Waterproof box, 3 A fuse |

## Totals

| Scenario | Per unit | Notes |
|----------|----------|-------|
| **Prototype (this BOM)** | **~$1,280** | + ~10% contingency → **~$1,410** |
| **Mass production** (custom PCB + bulk radar) | **~$690** | Radar bulk ~$80 each, PCB integration |

## Wiring summary (per radar)

| Radar pin | ESP32-S3 | Purpose |
|-----------|----------|---------|
| VCC (5 V) | 5 V rail | Power |
| GND | GND | Ground |
| **OUT** | GPIO 4 (bottom) / 6 (top) | Presence (HIGH = present) |
| TX (UART) | GPIO 7 (bottom RX) / 8 (top RX) | Distance in cm (optional) |

The LDR divider: `3V3 ── LDR ──┬── GPIO 1 (ADC) ── 10 kΩ ── GND`.

## Key advantage over the previous options

| | I²C (ToF + BH1750) | CAN | **UART (radar + LDR)** |
|---|---|---|---|
| BOM | ~$1,138 | ~$2,070 | **~$1,280** |
| I²C bus / address collision | yes | no (nodes) | **no** |
| Pull-up / termination tuning | yes | yes | **no** |
| Still-person detection | yes (narrow beam) | yes | **yes + micro-motion** |
| Distance on dashboard | mm | mm | **cm (UART)** |
