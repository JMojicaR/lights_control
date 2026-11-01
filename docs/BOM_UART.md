# Bill of Materials — UART / mmWave Option (5V USB)

Recommended build: two **HLK-LD2410B/C mmWave radars** for presence, a **BH1750**
for ambient light, and a **5 V USB LED strip**. The whole system runs from one
**5 V / 2 A USB phone charger** — no 12 V, no buck. Prices are mid-range
MercadoLibre México / Amazon MX (Oct 2026, ~1 USD = 18 MXN).

## Main controller + sensors

| # | Component | Qty | Unit (MXN) | Total | Notes |
|---|-----------|-----|-----------|-------|-------|
| 1 | ESP32-S3 SuperMini | 1 | $200 | $200 | Socketed controller |
| 2 | HLK-LD2410B/C mmWave radar (24 GHz) | 2 | $180 | $360 | Presence (moving + stationary), OUT + UART |
| 3 | BH1750 ambient light sensor | 1 | $50 | $50 | Real lux (I²C 0x23) |
| 4 | IRLZ44N MOSFET (TO-220) | 1 | $30 | $30 | 5 V LED strip switch (low-side) |
| 5 | 5 V LED strip (USB-A plug) | 1 | ~$200 | $200 | White/warm-white; 1–1.5 A |
| 6 | 5 V / 2 A USB phone charger | 1 | ~$120 | $120 | Powers everything (often already owned) |
| 7 | Cat6 cable + RJ45 connectors | 2 | $50 | $100 | One cable per radar (5V+GND+OUT+TX+RX) |
| 8 | PCB (custom `hardware/`) or stripboard | 1 | $150 | $150 | Optional — controller board |
| 9 | Enclosure + wiring + misc | 1 | $100 | $100 | Box, headers, heat-shrink |

## Totals

| Scenario | Per unit | Notes |
|----------|----------|-------|
| **Prototype (this BOM)** | **~$1,310** | + ~10% contingency → **~$1,440** |
| **Mass production** (custom PCB + bulk radar) | **~$600** | Radar bulk ~$80 each, PCB integration, no charger |

## Power budget (5 V / 2 A)

| Load | Current |
|------|---------|
| LED strip | 1–1.5 A |
| 2× radar | ≈0.2 A |
| ESP32-S3 + BH1750 | ≈0.15 A |
| **Total (max)** | **≈1.85 A** (within 2 A) |

## Wiring summary (per radar)

| Radar pin | ESP32-S3 | Purpose |
|-----------|----------|---------|
| VCC (5 V) | 5 V rail | Power |
| GND | GND | Ground |
| **OUT** | GPIO 4 (bottom) / 6 (top) | Presence (HIGH = present) |
| TX (UART) | GPIO 7 (bottom RX) / 8 (top RX) | Distance (optional) |
| RX (UART) | GPIO 9 (bottom TX) / 10 (top TX) | Config (sensing distance) |

The BH1750: `SDA → GPIO 12`, `SCL → GPIO 13`, `VCC → 3V3`, `GND → GND`.

The LED strip is switched **low-side**: strip `+` → +5 V, strip `−` → MOSFET
drain → source → GND (gate → GPIO 5 via 10 kΩ pulldown).

## Key advantage over the previous options

| | I²C (ToF + BH1750) | CAN | **UART (radar + BH1750)** |
|---|---|---|---|
| BOM | ~$1,138 | ~$2,070 | **~$1,310** |
| I²C bus / address collision | yes | no (nodes) | **no** |
| Pull-up / termination tuning | yes | yes | **no** |
| Still-person detection | yes (narrow beam) | yes | **yes + micro-motion** |
| Power | 12 V + buck | 12 V | **5 V USB (single supply)** |
