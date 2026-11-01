# Hardware — Controller PCB (uart_option, 5V USB)

KiCad 10 schematic + PCB for the **lights_control `uart_option`** controller board:
ESP32-S3 SuperMini + 2× HLK-LD2410C mmWave radar (remote, via RJ45) + BH1750.

Everything runs from a single **5 V / 2 A USB phone charger** — no 12 V, no buck.

## Design

- **Modular / socketed**: the **ESP32-S3 SuperMini** sits in a **2×9 female
  header socket**, the **BH1750** in a **1×4 socket** — no direct solder.
- **2× RJ45 (8P8C)** jacks, one per remote LD2410C radar (Cat6 run carries
  5 V + GND + OUT + TX + RX over the four twisted pairs).
- **Power in**: a 2-pin terminal (**5V IN**) fed from the USB charger's 5 V rail
  (cut a USB cable, or use a USB-C breakout).
- **LED strip out**: a **USB-A female** receptacle — the strip's USB-A plug
  plugs straight in. The **IRLZ44N** MOSFET switches the strip **low-side**
  (its GND return), so the strip's VBUS stays tied to +5 V.
- **Status LED** on GPIO 2; screw terminals for power/strip.

### Power budget (5 V / 2 A)

| Load | Current |
|------|---------|
| LED strip | 1–1.5 A |
| 2× radar | ≈0.2 A |
| ESP32-S3 + BH1750 | ≈0.15 A |
| **Total (max)** | **≈1.85 A** (within 2 A) |

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
| GPIO 9 / 10 | radar BOTTOM/TOP RX (config) |
| GPIO 12 / 13 | BH1750 SDA / SCL |

## Files

- `lights_control.kicad_pro` / `.kicad_sch` / `.kicad_pcb` — project, schematic, PCB
- `lights_control.kicad_sym` — custom symbols (SuperMini, BH1750, RJ45, USB-A, passives)
- `lights_control.pretty/` — custom footprints (sockets, RJ45, USB-A, THT passives)
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
  board's silkscreen** — ordering varies between manufacturers.
- **USB-A female** (strip): pin 1 = VBUS (+5 V), pin 4 = GND (switched to the
  MOSFET drain); pins 2/3 (D−/D+) unused. The strip is switched on its **GND
  return** (low-side), which is fine for a plain white strip with no data lines.
- **Unused pins** (SuperMini GPIO1/3/11–14/43/44, RJ45 shields, USB-A D±) are
  no-connect — the DRC "unconnected items" are expected.
- **Routing is a first-pass** (auto chain, GND on B.Cu, power 0.5 mm / signals
  0.25 mm). Review and tidy in the KiCad PCB editor before manufacturing.
- The **5V IN** terminal expects a clean 5 V; the USB charger's over-current
  protection is the only "fuse" — don't run a strip that pushes the total past
  2 A.
