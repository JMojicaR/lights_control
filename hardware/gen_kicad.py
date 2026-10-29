#!/usr/bin/env python3
"""Generate the lights_control (uart_option) KiCad schematic + PCB.

Fully self-contained: custom symbols and footprints are written as project-local
libraries, and the schematic uses global net labels for connectivity. Validate
with kicad-cli afterwards.
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
# Symbols: name -> list of (pin_name, pin_number, x, y, electrical_type)
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
    "MP1584EN": {
        "pins": [("BST","1",-7.62,8.89),("VIN","2",-7.62,6.35),("SW","3",-7.62,3.81),
                 ("GND","4",-7.62,1.27),("FB","5",-7.62,-1.27),("COMP","6",-7.62,-3.81),
                 ("EN","7",-7.62,-6.35),("FREQ","8",-7.62,-8.89)],
        "rect": (-10.16, 11.43, 10.16, -11.43), "ref": "U", "value": "MP1584EN",
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
    "Q_NMOS": {
        "pins": [("G","1",-5.08,0.0),("D","2",5.08,2.54),("S","3",5.08,-2.54)],
        "rect": (-5.08, 5.08, 5.08, -5.08), "ref": "Q", "value": "NMOS",
    },
    "D_Schottky": {
        "pins": [("A","1",-5.08,0.0),("K","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "D", "value": "Schottky",
    },
    "Inductor": {
        "pins": [("1","1",-5.08,0.0),("2","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "L", "value": "Inductor",
    },
    "Resistor": {
        "pins": [("1","1",-5.08,0.0),("2","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "R", "value": "R",
    },
    "Capacitor": {
        "pins": [("1","1",-5.08,0.0),("2","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "C", "value": "C",
    },
    "LED": {
        "pins": [("K","1",-5.08,0.0),("A","2",5.08,0.0)],
        "rect": (-5.08, 2.54, 5.08, -2.54), "ref": "D", "value": "LED",
    },
}

def gen_symbols():
    out = ['(kicad_symbol_lib', '  (version 20231120)', '  (generator "hermes")']
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
            if pn in ("5V","VCC","3V3","VIN"): etype = "power_in"
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
# Footprints: name -> list of (pad_number, x, y, type)
# =========================================================================
FOOTPRINTS = {
    "SuperMini_S3_Socket": {"tht": True, "pads": [(str(i+1), -7.62, (8-i)*2.54) for i in range(9)] + [(str(i+10), 7.62, (8-i)*2.54) for i in range(9)]},
    "MP1584EN_ESOP8": {"tht": False, "pads": [(str(i+1), -1.8, (7-2*i)*0.635) for i in range(8)] + [("9", 1.5, 0.0)]},
    "BH1750_1x04": {"tht": True, "pads": [("1", -2.54, 7.62), ("2", -2.54, 2.54), ("3", -2.54, -2.54), ("4", -2.54, -7.62)]},
    "RJ45_8P8C": {"tht": True, "pads": [(str(i+1), (i-3.5)*1.27, 2.54) for i in range(8)] + [("S1", -8.89, 0.0), ("S2", 8.89, 0.0)]},
    "Terminal_1x02": {"tht": True, "pads": [("1", 0.0, 2.54), ("2", 0.0, -2.54)]},
    "TO220_3": {"tht": True, "pads": [("1", -2.54, 3.81), ("2", 0.0, 3.81), ("3", 2.54, 3.81)]},
    "D_Axial": {"tht": True, "pads": [("1", -5.08, 0.0), ("2", 5.08, 0.0)]},
    "R_Axial": {"tht": True, "pads": [("1", -3.81, 0.0), ("2", 3.81, 0.0)]},
    "C_Radial": {"tht": True, "pads": [("1", -1.27, 0.0), ("2", 1.27, 0.0)]},
    "L_Axial": {"tht": True, "pads": [("1", -5.08, 0.0), ("2", 5.08, 0.0)]},
    "LED_5mm": {"tht": True, "pads": [("1", 0.0, -1.27), ("2", 0.0, 1.27)]},
}

def gen_footprints():
    for name, spec in FOOTPRINTS.items():
        tht = spec["tht"]
        out = [f'(footprint "{name}" (version 20240108) (generator "hermes")',
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
# Netlist: define components, positions, and each pin's net
# =========================================================================
# Each component: (ref, symbol, value, footprint, sx, sy)
# Each pin's net is listed below in PIN_NETS: (ref, pin_number) -> net
COMPS = [
    ("U1", "SuperMini_S3", "ESP32-S3 SuperMini", "SuperMini_S3_Socket", 165.1, 88.9),
    ("U2", "MP1584EN", "MP1584EN", "MP1584EN_ESOP8", 50.8, 88.9),
    ("U3", "BH1750", "BH1750", "BH1750_1x04", 165.1, 160.02),
    ("J1", "RJ45", "RJ45 BOTTOM", "RJ45_8P8C", 165.1, 35.56),
    ("J2", "RJ45", "RJ45 TOP", "RJ45_8P8C", 165.1, 15.24),
    ("J3", "Screw_1x02", "12V IN", "Terminal_1x02", 25.4, 130.0),
    ("J4", "Screw_1x02", "LED STRIP", "Terminal_1x02", 250.0, 88.9),
    ("Q1", "Q_NMOS", "IRLZ44N", "TO220_3", 250.0, 35.56),
    ("R1", "Resistor", "10k", "R_Axial", 215.0, 20.0),
    ("R2", "Resistor", "330", "R_Axial", 225.0, 130.0),
    ("D1", "LED", "STATUS", "LED_5mm", 245.0, 130.0),
    ("C1", "Capacitor", "10u", "C_Radial", 25.4, 70.0),
    ("C2", "Capacitor", "0.1u", "C_Radial", 25.4, 55.0),
    ("C3", "Capacitor", "22u", "C_Radial", 90.0, 100.0),
    ("C4", "Capacitor", "100n", "C_Radial", 80.0, 55.0),
    ("L1", "Inductor", "22uH", "L_Axial", 75.0, 80.0),
    ("D2", "D_Schottky", "SS34", "D_Axial", 60.0, 40.0),
    ("R3", "Resistor", "52.3k", "R_Axial", 90.0, 40.0),
    ("R4", "Resistor", "10k", "R_Axial", 90.0, 20.0),
]

# pin -> net assignments
PIN_NETS = {
    # U1 SuperMini
    ("U1","1"): "+5V", ("U1","2"): "GND", ("U1","3"): "+3V3",
    ("U1","5"): "LED_STATUS", ("U1","7"): "RADAR_BOTTOM_OUT", ("U1","8"): "LED_GATE",
    ("U1","9"): "RADAR_TOP_OUT", ("U1","10"): "RADAR_BOTTOM_TX", ("U1","11"): "RADAR_TOP_TX",
    ("U1","15"): "SDA", ("U1","16"): "SCL",
    # U2 MP1584
    ("U2","1"): "BST", ("U2","2"): "+12V", ("U2","3"): "SW", ("U2","4"): "GND",
    ("U2","5"): "FB", ("U2","6"): "COMP", ("U2","7"): "+12V",
    # U3 BH1750
    ("U3","1"): "+3V3", ("U3","2"): "SDA", ("U3","3"): "SCL", ("U3","4"): "GND",
    # J1 RJ45 BOTTOM  (1=5V 2=GND 3=OUT 4=TX 5=GND 6=GND 7=RX 8=GND)
    ("J1","1"): "+5V", ("J1","2"): "GND", ("J1","3"): "RADAR_BOTTOM_OUT", ("J1","4"): "RADAR_BOTTOM_TX",
    ("J1","5"): "GND", ("J1","6"): "GND", ("J1","7"): "RADAR_BOTTOM_RX", ("J1","8"): "GND",
    # J2 RJ45 TOP
    ("J2","1"): "+5V", ("J2","2"): "GND", ("J2","3"): "RADAR_TOP_OUT", ("J2","4"): "RADAR_TOP_TX",
    ("J2","5"): "GND", ("J2","6"): "GND", ("J2","7"): "RADAR_TOP_RX", ("J2","8"): "GND",
    # J3 12V IN
    ("J3","1"): "+12V", ("J3","2"): "GND",
    # J4 LED STRIP
    ("J4","1"): "+12V", ("J4","2"): "STRIP_MINUS",
    # Q1 MOSFET
    ("Q1","1"): "LED_GATE", ("Q1","2"): "STRIP_MINUS", ("Q1","3"): "GND",
    # R1 gate pulldown
    ("R1","1"): "LED_GATE", ("R1","2"): "GND",
    # R2 + D1 status LED
    ("R2","1"): "LED_STATUS", ("R2","2"): "LED_A",
    ("D1","1"): "GND", ("D1","2"): "LED_A",
    # buck passives
    ("C1","1"): "+12V", ("C1","2"): "GND",
    ("C2","1"): "SW", ("C2","2"): "BST",
    ("C3","1"): "+5V", ("C3","2"): "GND",
    ("C4","1"): "COMP", ("C4","2"): "GND",
    ("L1","1"): "SW", ("L1","2"): "+5V",
    ("D2","1"): "GND", ("D2","2"): "SW",
    ("R3","1"): "+5V", ("R3","2"): "FB",
    ("R4","1"): "FB", ("R4","2"): "GND",
}

# =========================================================================
# Schematic
# =========================================================================
def gen_schematic():
    out = ['(kicad_sch (version 20231120) (generator "hermes")',
           f'  (uuid "{u()}")',
           '  (paper "A3")',
           '  (lib_symbols)']
    for ref, sym, value, fp, sx, sy in COMPS:
        out.append(f'  (symbol (lib_id "lights_control:{sym}") (at {sx} {sy} 0) (unit 1) (uuid "{u()}")')
        out.append(f'    (property "Reference" "{ref}" (at {sx} {sy-10.16} 0) (effects (font (size 1.27 1.27))))')
        out.append(f'    (property "Value" "{value}" (at {sx} {sy+10.16} 0) (effects (font (size 1.27 1.27))))')
        out.append(f'    (property "Footprint" "lights_control:{fp}" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) hide))')
        out.append(f'    (property "Datasheet" "" (at {sx} {sy} 0) (effects (font (size 1.27 1.27)) hide))')
        # pins (instantiate with uuids)
        pins = SYMBOLS[sym]["pins"]
        for pn, pnum, px, py in pins:
            out.append(f'    (pin "{pnum}" (uuid "{u()}"))')
        out.append('  )')
    # global labels for every assigned pin
    for (ref, pnum), net in PIN_NETS.items():
        sym = dict((c[0], c[1]) for c in COMPS)[ref]
        sx = dict((c[0], c[4]) for c in COMPS)[ref]
        sy = dict((c[0], c[5]) for c in COMPS)[ref]
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
    nets = ["", "GND", "+12V", "+5V", "+3V3", "SDA", "SCL", "RADAR_BOTTOM_OUT",
            "RADAR_BOTTOM_TX", "RADAR_BOTTOM_RX", "RADAR_TOP_OUT", "RADAR_TOP_TX",
            "RADAR_TOP_RX", "LED_GATE", "STRIP_MINUS", "LED_STATUS", "LED_A",
            "BST", "SW", "FB", "COMP"]
    net_idx = {n: i for i, n in enumerate(nets)}
    out = ['(kicad_pcb (version 20240108) (generator "hermes")',
           '  (general (thickness 1.6))',
           '  (paper "A4")',
           '  (layers',
           '    (0 "F.Cu" signal)',
           '    (31 "B.Cu" signal)',
           '    (34 "B.Paste" user)',
           '    (35 "F.Paste" user)',
           '    (36 "B.SilkS" user "B.Silkscreen")',
           '    (37 "F.SilkS" user "F.Silkscreen")',
           '    (38 "B.Mask" user)',
           '    (39 "F.Mask" user)',
           '    (44 "Edge.Cuts" user)',
           '    (46 "B.CrtYd" user "B.Courtyard")',
           '    (47 "F.CrtYd" user "F.Courtyard")',
           '    (48 "B.Fab" user)',
           '    (49 "F.Fab" user)',
           '  )',
           '  (setup (pad_to_mask_clearance 0))']
    for n in nets:
        out.append(f'  (net {net_idx[n]} "{n}")')
    # board outline (100 x 80 mm)
    out.append('  (gr_line (start 0 0) (end 100 0) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 100 0) (end 100 80) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 100 80) (end 0 80) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    out.append('  (gr_line (start 0 80) (end 0 0) (stroke (width 0.1) (type default)) (layer "Edge.Cuts") (uuid "%s"))' % u())
    # footprints placed (mm coordinates; schematic is in mm too)
    fp_pos = {  # ref -> (footprint, x_mm, y_mm)
        "U1": ("SuperMini_S3_Socket", 45, 40), "U2": ("MP1584EN_ESOP8", 15, 50),
        "U3": ("BH1750_1x04", 45, 65), "J1": ("RJ45_8P8C", 20, 10), "J2": ("RJ45_8P8C", 20, 25),
        "J3": ("Terminal_1x02", 5, 70), "J4": ("Terminal_1x02", 75, 40), "Q1": ("TO220_3", 80, 10),
        "R1": ("R_Axial", 70, 15), "R2": ("R_Axial", 65, 65), "D1": ("LED_5mm", 75, 65),
        "C1": ("C_Radial", 8, 65), "C2": ("C_Radial", 8, 55), "C3": ("C_Radial", 30, 70),
        "C4": ("C_Radial", 25, 45), "L1": ("L_Axial", 25, 55), "D2": ("D_Axial", 15, 35),
        "R3": ("R_Axial", 30, 45), "R4": ("R_Axial", 30, 35),
    }
    for ref, sym, value, fp, sx, sy in COMPS:
        _, fpx, fpy = fp_pos[ref]
        out.append(f'  (footprint "lights_control:{fp}" (layer "F.Cu") (at {fpx} {fpy} 0) (uuid "{u()}")')
        out.append(f'    (property "Reference" "{ref}")')
        out.append(f'    (property "Value" "{value}")')
        # pads with net assignment (geometry inherited, absolute position + net)
        tht = FOOTPRINTS[fp]["tht"]
        for pn, px, py in FOOTPRINTS[fp]["pads"]:
            net = PIN_NETS.get((ref, pn), "")
            ni = net_idx.get(net, 0)
            netsfx = f' (net {ni} "{net}")' if net else ''
            if tht:
                out.append(f'    (pad "{pn}" thru_hole circle (at {fpx+px:.2f} {fpy+py:.2f}) (size 1.7 1.7) (drill 1.0) (layers "*.Cu" "*.Mask"){netsfx} (uuid "{u()}"))')
            else:
                out.append(f'    (pad "{pn}" smd rect (at {fpx+px:.2f} {fpy+py:.2f}) (size 1.0 0.6) (layers "F.Cu" "F.Paste" "F.Mask"){netsfx} (uuid "{u()}"))')
        out.append('  )')
    # ---- routing: chain-connect pads of each net ----
    # collect pad positions per net
    net_pads = {}
    for ref, sym, value, fp, sx, sy in COMPS:
        _, fpx, fpy = fp_pos[ref]
        for pn, px, py in FOOTPRINTS[fp]["pads"]:
            net = PIN_NETS.get((ref, pn), "")
            if not net:
                continue
            net_pads.setdefault(net, []).append((fpx + px, fpy + py))
    power_nets = {"GND", "+12V", "+5V", "+3V3"}
    for net, pads in net_pads.items():
        if len(pads) < 2:
            continue
        width = 0.5 if net in power_nets else 0.25
        layer = "B.Cu" if net == "GND" else "F.Cu"
        for i in range(len(pads) - 1):
            x1, y1 = pads[i]
            x2, y2 = pads[i + 1]
            out.append(f'  (segment (start {x1:.2f} {y1:.2f}) (end {x2:.2f} {y2:.2f}) (width {width}) (layer "{layer}") (net {net_idx[net]}) (uuid "{u()}"))')
    out.append(')')
    with open(PCB, "w") as f:
        f.write('\n'.join(out) + '\n')

# =========================================================================
# Project + lib tables
# =========================================================================
def gen_project():
    pro = ['(kicad_project', '  (version 1)', '  (date 2026-10-17)',
           '  (title "lights_control uart_option")', '  (author "Hermes")', ')']
    with open(PRO, "w") as f:
        f.write('\n'.join(pro) + '\n')
    # project-local sym-lib-table (custom lib + standard)
    symt = ['(sym_lib_table', '  (version 7)',
            f'  (lib (name "lights_control") (type "KiCad") (uri "{SYM}") (options "") (descr "custom symbols"))',
            '  (lib (name "Device") (type "KiCad") (uri "/home/hermesbot/tools/kicad-symbols/Device.kicad_symdir") (options "") (descr ""))',
            '  (lib (name "Connector") (type "KiCad") (uri "/home/hermesbot/tools/kicad-symbols/Connector.kicad_symdir") (options "") (descr ""))',
            '  (lib (name "power") (type "KiCad") (uri "/home/hermesbot/tools/kicad-symbols/power.kicad_symdir") (options "") (descr ""))',
            ')']
    with open(os.path.join(HERE, "sym-lib-table"), "w") as f:
        f.write('\n'.join(symt) + '\n')
    fpt = ['(fp_lib_table', '  (version 7)',
           f'  (lib (name "lights_control") (type "KiCad") (uri "{PRETTY}") (options "") (descr "custom footprints"))',
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
