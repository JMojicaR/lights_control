# Datasheets & Pinouts

Reference documents for every component in the staircase light controller.
All PDFs are the manufacturer's original datasheet (some fetched via GitHub
mirrors — see source note on each).

## Datasheets (PDF)

| Component | Role | File | Source |
|-----------|------|------|--------|
| **ESP32-S3** | Controller | `ESP32-S3_datasheet.pdf` | Espressif (87 pp) |
| **VL53L0X** | ToF presence sensor (×2) | `VL53L0X_datasheet.pdf` | STMicroelectronics (40 pp) |
| **BH1750(FVI)** | Ambient light sensor | `BH1750_datasheet.pdf` | ROHM (18 pp) |
| **IRLZ44N** | 12 V LED-strip MOSFET | `IRLZ44N_datasheet.pdf` | Infineon/IR (10 pp) |
| **P82B715** | I²C bus extender (optional) | `P82B715_datasheet.pdf` | Texas Instruments (24 pp) |

## Pinouts (images)

| Component | File | Notes |
|-----------|------|-------|
| ESP32-S3 SuperMini | `esp32-supermini-pinout.jpeg` | Board-level GPIO map |
| VL53L0X breakout | `VL53L0X_pinout.png` | XSHUT, GPIO1, I²C pins |
| BH1750 breakout | `BH1750_pinout.png` | ADDR, SDA/SCL, VCC/GND |
| IRLZ44N (TO-220) | `IRLZ44N_pinout.svg` | Gate / Drain / Source |
| P82B715 (DIP-8) | `P82B715_pinout.svg` | Sx/Sy + Lx/Ly sides |

## Note on provenance

The VL53L0X and BH1750 PDFs were downloaded from GitHub mirrors because the
manufacturers' sites (st.com, rohm.com) block direct command-line fetches. They
are verbatim copies of the official datasheets; re-download from the vendor if
you need a guaranteed-current revision:

- VL53L0X: <https://www.st.com/resource/en/datasheet/vl53l0x.pdf>
- BH1750FVI: <https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf>
- P82B715: <https://www.ti.com/lit/ds/symlink/p82b715.pdf>
- IRLZ44N: <https://www.infineon.com/dgdl/Infineon-IRLZ44N-DataSheet-v01_01-EN.pdf>
- ESP32-S3: <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf>

Wiring / pull-up / bus-speed guidance for the 3 m sensor run is in
[`../ELECTRICAL.md`](../ELECTRICAL.md).
