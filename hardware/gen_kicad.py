#!/usr/bin/env python3
"""Generate the lights_control (uart_option) KiCad schematic + PCB.

5V USB design: the whole system runs from a 5V/2A USB phone charger — no 12V,
no buck. ESP32-S3 SuperMini (socketed) + 2x remote LD2410C radar (RJ45) + BH1750
(socketed) + low-side MOSFET switching a 5V USB-A LED strip.
"""
import os, uuid

HERE = os.path.dirname(os.path.abspath(__file__))
SYM = os.path.join(HERE, "lights_control.kicad_sym")
PRETTY = os.path.join(HERE, "lights_control.pretty")
SCH = os.path.join(HERE, "lights_control.kicad_sch")
PCB = os.path.join(HERE, "lights_control.kicad_pcb")
PRO = os.path.join(HERE, "lights_control.kicad_pro")
os.makedirs(PRETTY, exist_ok=True)

def u(): return str(uuid.uuid4())

# =========================================================================
# Symbols
# =========================================================================
SYMBOLS = {
    "SuperMini_S3": {
        "pins": [("5V","1",-5.08, 12.7),("GND","2",-5.08,10.16),("3V3","3",-5.08,7.62),
                 ("GPIO1","4",-5.08,5.08),("GPIO2","5",-5.08,2.54),("GPIO3","6",-5.08,0.0),
                 ("GPIO4","7",-5.08,-2.54),("GPIO5","8",-5.08,-5.08),("GPIO6","9",-5.08,-7.62),
                 ("GPIO7","10",5.08,12.7),("GPIO8","11",5.08,10.16),("GPIO9","12",5.08,7.62),
                 ("GPIO10","13",5.08,5.08),("GPIO11","14",5.08,2.54),("GPIO12","15",5.08,0.0),
                 ("GPIO13","16",5.08,-2.54),("GPIO43","17",5.08,-5.08),("GPIO44","18",5.08,-7.62)],
        "rect": (-7.62, 15.24, 7.62, -15.24), "ref": "U", "value": "ESP32-S3 SuperMini",
    },
    "BH1750": {
        "pins": [("VCC","1",-2.54,7.62),("SDA","2",-2.54,2.54),("SCL","3",-2.54,-2.54),("GND","4",-2.54,-7.62)],
        "rect": (-5.08, 10.16, 2.54, -10.16), "ref": "U", "value": "BH1750",
    },
    "RJ45": {
        "pins": [("1","1",-5.08,8.89),("2","2",-5.08,6.35),("3","3",-5.08,3.81),("4","4",-5.08,1.27),
                 ("5","5",-5.08,-1.27),("6","6",-5.08,-3.81),("7","7",-5.08,-6.35),("8","8",-5.08,-8.89)],
        "rect": (-7.62, 11.43, 2.54, -11.43), "ref": "J", "value": "RJ45",
    },
    "Screw_1x02": {
        "pins": [("1","1",-2.54,2.54),("2","2",-2.54,-2.54)],
        "rect": (-5.08, 5.08, 0.0, -5.08), "ref": "J", "value": "Terminal",
    },
    "USB_A_Female": {
        "pins": [("VBUS","1",-5.08,7.62),("D-","2",-5.08,2.54),("D+","3",-5.08,-2.54),("GND","4",-5.08,-7.62)],
        "rect": (-7.62, 10.16, 2.54, -10.16), "ref": "J", "value": "USB-A",
    },
    "Q_NMOS": {
        "pins": [("G","1",-5.08,0.0),("D","2",5.08,2.54),("S","3",5.08,-2.54)],
        "rect": (-5.08, 5.08, 5.08, -5.08), "ref": "Q", "value": "NMOS",
    },
    "Resistor": {
        "pins": [("1","1",-5.08,0.0),("2","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "R", "value": "R",
    },
    "LED": {
        "pins": [("K","1",-5.08,0.0),("A","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "D", "value": "LED",
    },
}

def gen_symbols():
    out = ['(kicad_symbol_lib', '  (version 20251024)', '  (generator "eeschema")']
    for name, spec in SYMBOLS.items():
        x1, y1, x2, y2 = spec["rect"]
        out.append(f'  (symbol "{name}"')
        out.append('    (pin_numbers hide)')
        out.append('    (pin_names (offset 1.016) hide)')
        out.append('    (exclude_from_sim no)')
        out.append('    (in_bom yes)')
        out.append('    (on_board yes)')
        out.append(f'    (property "Reference" "{spec["ref"]}" (at 0 {y2-2.54} 0) (effects (font (size 1.27 1.27))))')
        out.append(f'    (property "Value" "{spec["value"]}" (at 0 {y1+2.54} 0) (effects (font (size 1.27 1.27))))')
        out.append('    (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))')
        out.append('    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))')
        out.append(f'    (symbol "{name}_1_1"')
        out.append('      (pin_names (offset 1.016) hide)')
        out.append(f'      (rectangle (start {x1} {y1}) (end {x2} {y2}) (stroke (width 0.254) (type default)) (fill (type background)))')
        for pn, pnum, px, py in spec["pins"]:
            etype = "passive"
            if pn in ("5V","VCC","3V3","VBUS"): etype = "power_in"
            if pn == "GND": etype = "power_in"
            out.append(f'      (pin {etype} line (at {px} {py} 0) (length 2.54) '
                       f'(name "{pn}" (effects (font (size 1.27 1.27)))) '
                       f'(number "{pnum}" (effects (font (size 1.27 1.27)))))')
        out.append('    )')
        out.append('  )')
    out.append(')')
    with open(SYM, "w") as f:
        f.write('\n'.join(out) + '\n')

# =========================================================================
# Footprints
# =========================================================================
FOOTPRINTS = {
    "SuperMini_S3_Socket": {"tht": True, "pads": [(str(i+1), -7.62, (8-i)*2.54) for i in range(9)] + [(str(i+10), 7.62, (8-i)*2.54) for i in range(9)]},
    "BH1750_1x04": {"tht": True, "pads": [("1", -2.54, 7.62), ("2", -2.54, 2.54), ("3", -2.54, -2.54), ("4", -2.54, -7.62)]},
    "RJ45_8P8C": {"tht": True, "pads": [(str(i+1), (i-3.5)*1.27, 2.54) for i in range(8)] + [("S1", -8.89, 0.0), ("S2", 8.89, 0.0)]},
    "Terminal_1x02": {"tht": True, "pads": [("1", 0.0, 2.54), ("2", 0.0, -2.54)]},
    "USB_A_Female": {"tht": True, "pads": [("1", -5.08, 3.81), ("2", -5.08, 1.27), ("3", -5.08, -1.27), ("4", -5.08, -3.81), ("S1", -8.89, 0.0), ("S2", 8.89, 0.0)]},
    "TO220_3": {"tht": True, "pads": [("1", -2.54, 3.81), ("2", 0.0, 3.81), ("3", 2.54, 3.81)]},
    "R_Axial": {"tht": True, "pads": [("1", -3.81, 0.0), ("2", 3.81, 0.0)]},
    "LED_5mm": {"tht": True, "pads": [("1", 0.0, -1.27), ("2", 0.0, 1.27)]},
}

def gen_footprints():
    for name, spec in FOOTPRINTS.items():
        tht = spec["tht"]
        out = [f'(footprint "{name}" (version 20260206) (generator "pcbnew")',
               '  (layer "F.Cu")',
               f'  (attr {"through_hole" if tht else "smd"})',
               '  (fp_text reference "REF**" (at 0 -3.5 0) (layer "F.Silkscreen") (effects (font (size 1 1) (thickness 0.15))))',
               '  (fp_text value "VAL**" (at 0 3.5 0) (layer "F.Fab") (effects (font (size 1 1) (thickness 0.15))))']
        for pn, px, py in spec["pads"]:
            layers = '"*.Cu" "*.Mask"' if tht else '"F.Cu" "F.Paste" "F.Mask"'
            if tht:
                out.append(f'  (pad "{pn}" thru_hole circle (at {px} {py}) (size 1.7 1.7) (drill 1.0) (layers {layers}) (remove_unused_layers) (uuid "{u()}"))')
            else:
                out.append(f'  (pad "{pn}" smd rect (at {px} {py}) (size 1.0 0.6) (layers {layers}) (remove_unused_layers) (uuid "{u()}"))')
        out.append(')')
        with open(os.path.join(PRETTY, name + ".kicad_mod"), "w") as f:
            f.write('\n'.join(out) + '\n')

# =========================================================================
# Components (schematic positions) + pin->net map
# =========================================================================
COMPS = [
    ("U1", "SuperMini_S3", "ESP32-S3 SuperMini", "SuperMini_S3_Socket", 165.1, 88.9),
    ("U3", "BH1750", "BH1750", "BH1750_1x04", 165.1, 160.02),
    ("J1", "RJ45", "RJ45 BOTTOM", "RJ45_8P8C", 165.1, 35.56),
    ("J2", "RJ45", "RJ45 TOP", "RJ45_8P8C", 165.1, 15.24),
    ("J3", "Screw_1x02", "5V IN (USB)", "Terminal_1x02", 25.4, 130.0),
    ("J4", "USB_A_Female", "LED STRIP", "USB_A_Female", 250.0, 88.9),
    ("Q1", "Q_NMOS", "IRLZ44N", "TO220_3", 250.0, 35.56),
    ("R1", "Resistor", "10k", "R_Axial", 215.0, 20.0),
    ("R2", "Resistor", "330", "R_Axial", 225.0, 130.0),
    ("D1", "LED", "STATUS", "LED_5mm", 245.0, 130.0),
]

PIN_NETS = {
    ("U1","1"): "+5V", ("U1","2"): "GND", ("U1","3"): "+3V3",
    ("U1","5"): "LED_STATUS", ("U1","7"): "RADAR_BOTTOM_OUT", ("U1","8"): "LED_GATE",
    ("U1","9"): "RADAR_TOP_OUT", ("U1","10"): "RADAR_BOTTOM_TX", ("U1","11"): "RADAR_TOP_TX",
    ("U1","15"): "SDA", ("U1","16"): "SCL",
    ("U3","1"): "+3V3", ("U3","2"): "SDA", ("U3","3"): "SCL", ("U3","4"): "GND",
    ("J1","1"): "+5V", ("J1","2"): "GND", ("J1","3"): "RADAR_BOTTOM_OUT", ("J1","4"): "RADAR_BOTTOM_TX",
    ("J1","5"): "GND", ("J1","6"): "GND", ("J1","7"): "RADAR_BOTTOM_RX", ("J1","8"): "GND",
    ("J2","1"): "+5V", ("J2","2"): "GND", ("J2","3"): "RADAR_TOP_OUT", ("J2","4"): "RADAR_TOP_TX",
    ("J2","5"): "GND", ("J2","6"): "GND", ("J2","7"): "RADAR_TOP_RX", ("J2","8"): "GND",
    ("J3","1"): "+5V", ("J3","2"): "GND",
    # USB-A female (strip): VBUS=+5V always-on, GND=switched (low-side), D-/D+ unused
    ("J4","1"): "+5V", ("J4","4"): "STRIP_MINUS",
    ("Q1","1"): "LED_GATE", ("Q1","2"): "STRIP_MINUS", ("Q1","3"): "GND",
    ("R1","1"): "LED_GATE", ("R1","2"): "GND",
    ("R2","1"): "LED_STATUS", ("R2","2"): "LED_A",
    ("D1","1"): "GND", ("D1","2"): "LED_A",
}

# =========================================================================
# Schematic
# =========================================================================
def gen_schematic():
    out = ['(kicad_sch (version 20251024) (generator "eeschema")',
           f'  (uuid "{u()}")',
           '  (paper "A3")',
           '  (lib_symbols)']
    for ref, sym, value, fp, sx, sy in COMPS:
        out.append(f'  (symbol (lib_id "lights_control:{sym}") (at {sx} {sy} 0) (unit 1) (uuid "{u()}")')
        out.append(f'    (property "Reference" "{ref}" (at {sx} {sy-10.16} 0) (effects (font (size 1.27 1.27))))')
        out.append(f'    (property "Value" "{value}" (at {sx} {sy+10.16} 0) (effects (font (size 1.27 1.27))))')
        out.append(f'    (property "Footprint" "lights_control:{fp}" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) hide))')
        out.append(f'    (property "Datasheet" "" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) hide))')
        for pn, pnum, px, py in SYMBOLS[sym]["pins"]:
            out.append(f'    (pin "{pnum}" (uuid "{u()}"))')
        out.append('  )')
    comp_by_ref = {c[0]: c for c in COMPS}
    for (ref, pnum), net in PIN_NETS.items():
        sym = comp_by_ref[ref][1]
        sx, sy = comp_by_ref[ref][4], comp_by_ref[ref][5]
        px = py = 0
        for pn, pn2, pxx, pyy in SYMBOLS[sym]["pins"]:
            if pn2 == pnum:
                px, py = pxx, pyy
                break
        angle = 0 if px > 0 else 180
        lx = sx + px + (2.54 if px > 0 else -2.54)
        out.append(f'  (global_label "{net}" (at {lx:.2f} {sy+py:.2f} {angle}) (effects (font (size 1.27 1.27)) (justify left)) (uuid "{u()}")'
                   f' (property "Intersheetrefs" "${net}" (at 0 0 0) (effects (font (size 1.27 1.27)) hide)))')
    out.append(')')
    with open(SCH, "w") as f:
        f.write('\n'.join(out) + '\n')

# =========================================================================
# PCB
# =========================================================================
def gen_pcb():
    nets = ["", "GND", "+5V", "+3V3", "SDA", "SCL", "RADAR_BOTTOM_OUT",
            "RADAR_BOTTOM_TX", "RADAR_BOTTOM_RX", "RADAR_TOP_OUT", "RADAR_TOP_TX",
            "RADAR_TOP_RX", "LED_GATE", "STRIP_MINUS", "LED_STATUS", "LED_A"]
    net_idx = {n: i for i, n in enumerate(nets)}
    out = ['(kicad_pcb (version 20260206) (generator "pcbnew")',
           '  (general (thickness 1.6))',
           '  (paper "A4")',
           '  (layers',
           '    (0 "F.Cu" signal)', '    (31 "B.Cu" signal)',
           '    (34 "B.Paste" user)', '    (35 "F.Paste" user)',
           '    (36 "B.SilkS" user "B.Silkscreen")', '    (37 "F.SilkS" user "F.Silkscreen")',
           '    (38 "B.Mask" user)', '    (39 "F.Mask" user)',
           '    (44 "Edge.Cuts" user)',
           '    (46 "B.CrtYd" user "B.Courtyard")', '    (47 "F.CrtYd" user "F.Courtyard")',
           '    (48 "B.Fab" user)', '    (49 "F.Fab" user)',
           '  )',
           '  (setup (pad_to_mask_clearance 0))']
    for n in nets:
        out.append(f'  (net {net_idx[n]} "{n}")')
    out.append('  (gr_line (start 0 0) (end 100 0) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 100 0) (end 100 80) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 100 80) (end 0 80) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 0 80) (end 0 0) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    fp_pos = {
        "U1": ("SuperMini_S3_Socket", 45, 40), "U3": ("BH1750_1x04", 45, 65),
        "J1": ("RJ45_8P8C", 20, 10), "J2": ("RJ45_8P8C", 20, 25),
        "J3": ("Terminal_1x02", 5, 70), "J4": ("USB_A_Female", 85, 40),
        "Q1": ("TO220_3", 80, 10), "R1": ("R_Axial", 70, 15), "R2": ("R_Axial", 65, 65),
        "D1": ("LED_5mm", 75, 65),
    }
    for ref, sym, value, fp, sx, sy in COMPS:
        _, fpx, fpy = fp_pos[ref]
        out.append(f'  (footprint "lights_control:{fp}" (layer "F.Cu") (at {fpx} {fpy} 0) (uuid "{u()}")')
        out.append(f'    (property "Reference" "{ref}")')
        out.append(f'    (property "Value" "{value}")')
        tht = FOOTPRINTS[fp]["tht"]
        for pn, px, py in FOOTPRINTS[fp]["pads"]:
            net = PIN_NETS.get((ref, pn), "")
            ni = net_idx.get(net, 0)
            netsfx = f' (net {ni} "{net}")' if net else ''
            if tht:
                out.append(f'    (pad "{pn}" thru_hole circle (at {px:.2f} {py:.2f}) (size 1.7 1.7) (drill 1.0) (layers "*.Cu" "*.Mask"){netsfx} (uuid "{u()}"))')
            else:
                out.append(f'    (pad "{pn}" smd rect (at {px:.2f} {py:.2f}) (size 1.0 0.6) (layers "F.Cu" "F.Paste" "F.Mask"){netsfx} (uuid "{u()}"))')
        out.append('  )')
    # routing: chain-connect pads per net
    net_pads = {}
    for ref, sym, value, fp, sx, sy in COMPS:
        _, fpx, fpy = fp_pos[ref]
        for pn, px, py in FOOTPRINTS[fp]["pads"]:
            net = PIN_NETS.get((ref, pn), "")
            if not net:
                continue
            net_pads.setdefault(net, []).append((fpx + px, fpy + py))
    power_nets = {"GND", "+5V", "+3V3"}
    for net, pads in net_pads.items():
        if len(pads) < 2:
            continue
        width = 0.5 if net in power_nets else 0.25
        layer = "B.Cu" if net == "GND" else "F.Cu"
        for i in range(len(pads) - 1):
            x1, y1 = pads[i]; x2, y2 = pads[i + 1]
            out.append(f'  (segment (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) (width {width}) (layer "{layer}") (net {net_idx[net]}) (uuid "{u()}"))')
    out.append(')')
    with open(PCB, "w") as f:
        f.write('\n'.join(out) + '\n')

# =========================================================================
# Project + lib tables
# =========================================================================
def gen_project():
    pro = ('{\n'
           '  "meta": {\n'
           '    "filename": "lights_control.kicad_pro",\n'
           '    "version": 3\n'
           '  },\n'
           '  "libraries": {\n'
           '    "pinned_symbol_libs": ["lights_control"],\n'
           '    "pinned_footprint_libs": ["lights_control"]\n'
           '  }\n'
           '}\n')
    with open(PRO, "w") as f:
        f.write(pro)
    symt = ['(sym_lib_table', '  (version 7)',
            '  (lib (name "lights_control") (type "KiCad") (uri "${KIPRJMOD}/lights_control.kicad_sym") (options "") (descr "custom symbols"))',
            ')']
    with open(os.path.join(HERE, "sym-lib-table"), "w") as f:
        f.write('\n'.join(symt) + '\n')
    fpt = ['(fp_lib_table', '  (version 7)',
           '  (lib (name "lights_control") (type "KiCad") (uri "${KIPRJMOD}/lights_control.pretty") (options "") (descr "custom footprints"))',
           ')']
    with open(os.path.join(HERE, "fp-lib-table"), "w") as f:
        f.write('\n'.join(fpt) + '\n')

if __name__ == "__main__":
    gen_symbols()
    gen_footprints()
    gen_schematic()
    gen_pcb()
    gen_project()
    print("generated:", SYM, PRETTY, SCH, PCB, PRO)
