# Hardware — Controller PCB (uart_option)

KiCad 10 schematic + PCB for the **lights_control `uart_option`** controller board:
ESP32-S3 SuperMini + 2× HLK-LD2410C mmWave radar (remote, via RJ45) + BH1750.

## Design

- **Modular / socketed** (per requirement): the **ESP32-S3 SuperMini** sits in a
  **2×9 female header socket**, the **BH1750** in a **1×4 socket** — no direct
  solder of those two.
- **2× RJ45 (8P8C)** jacks, one per remote LD2410C radar (Cat6 run carries
  5 V + GND + OUT + TX + RX over the four twisted pairs).
- **Integrated buck**: MP1584EN steps 12 V → 5 V for the radars + ESP32.
- **IRLZ44N** MOSFET switches the 12 V LED strip (screw-terminal out).
- Screw terminals for **12 V in** and **LED strip out**; status LED on GPIO 2.

### RJ45 pin mapping (per radar, T568B pairs)

| RJ45 pin | Pair | Signal |
|----------|------|--------|
| 1 | orange | +5 V |
| 2 | orange | GND |
| 3 | green | OUT (presence) |
| 6 | green | GND |
| 4 | blue | TX (UART) |
| 5 | blue | GND |
| 7 | brown | RX (spare) |
| 8 | brown | GND |

### ESP32-S3 SuperMini pin usage

| SuperMini pin | Net |
|---------------|-----|
| 5V / GND / 3V3 | power |
| GPIO 2 | status LED |
| GPIO 4 | radar BOTTOM OUT |
| GPIO 6 | radar TOP OUT |
| GPIO 5 | MOSFET gate (10 kΩ pulldown) |
| GPIO 7 | radar BOTTOM TX |
| GPIO 8 | radar TOP TX |
| GPIO 12 / 13 | BH1750 SDA / SCL |

## Files

- `lights_control.kicad_pro` / `.kicad_sch` / `.kicad_pcb` — project, schematic, PCB
- `lights_control.kicad_sym` — custom symbols (SuperMini, MP1584EN, BH1750, RJ45, passives)
- `lights_control.pretty/` — custom footprints (sockets, RJ45, MP1584 ESOP-8, THT passives)
- `lights_control_schematic.pdf` — rendered schematic
- `gerbers/` — full Gerber set + drill (JLCPCB-ready)
- `gen_kicad.py` — generator (regenerates all files)

## Regenerate

```bash
python3 gen_kicad.py
kicad-cli sch export pdf -o lights_control_schematic.pdf lights_control.kicad_sch
kicad-cli pcb export gerbers -o gerbers/ lights_control.kicad_pcb
kicad-cli pcb export drill -o gerbers/ lights_control.kicad_pcb
```

## Notes & assumptions

- **SuperMini pin order**: the 2×9 socket follows the standard 18 side-header
  pins (5V/GND/3V3 + GPIO1–13 + GPIO43/44). **Verify against your specific
  board's silkscreen** — SuperMini is a generic third-party format and ordering
  varies between manufacturers.
- **MP1584EN pinout** (ESOP-8): 1=BST 2=VIN 3=SW 4=GND 5=FB 6=COMP 7=EN 8=FREQ.
  Confirm against the datasheet before fabbing.
- **Unused pins** (SuperMini GPIO1/3/9–14/43/44, MP1584 FREQ, RJ45 shields) are
  left **no-connect** — the DRC's "unconnected items" are expected.
- **Routing is a first-pass** (auto chain, GND on B.Cu, power 0.5 mm / signals
  0.25 mm). Review and tidy in the KiCad PCB editor before manufacturing.
- The buck feedback divider sets 5 V via R3=52.3 kΩ / R4=10 kΩ (Vout ≈ 0.8 V ×
  (1 + R3/R4)). Fuse the 12 V feed (3 A) externally.
