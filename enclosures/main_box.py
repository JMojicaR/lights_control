#!/usr/bin/env python3
"""
Main controller enclosure box for lights_control staircase light controller.
Generates main_box.FCStd with:
  - Outer box: 120mm x 80mm x 50mm, wall thickness 2mm
  - DC barrel jack hole (12mm dia)
  - 2x PG7 cable gland holes (12mm dia each)
  - Ventilation slots on long sides
  - Mounting tabs with 4mm screw holes
  - Lid plate with screw mounts
"""

import FreeCAD as App
import Part
import sys
from FreeCAD import Base

OUTPUT = "/home/hermesbot/lights_control/enclosures/main_box.FCStd"

# Dimensions (mm)
BOX_L = 120.0   # length (X)
BOX_W = 80.0    # width (Y)
BOX_H = 50.0    # height (Z)
WALL = 2.0      # wall thickness
TAB_L = 15.0    # mounting tab extension length
TAB_W = 12.0    # mounting tab width
TAB_H = 3.0     # mounting tab thickness
SCREW_R = 2.0   # mounting screw hole radius (4mm dia)
DC_JACK_R = 6.0  # DC barrel jack hole radius (12mm dia)
GLAND_R = 6.0   # cable gland hole radius (12mm dia)
LID_H = 2.5     # lid thickness
VENT_SLOT_W = 3.0   # ventilation slot width
VENT_SLOT_L = 25.0  # ventilation slot length
VENT_COUNT = 4

print("=== Generating main_box.FCStd ===")
sys.stdout.flush()

doc = App.newDocument("main_box")

# --- Outer box solid ---
outer = Part.makeBox(BOX_L, BOX_W, BOX_H)

# --- Inner cavity (subtract for shell) ---
inner = Part.makeBox(BOX_L - 2*WALL, BOX_W - 2*WALL, BOX_H - WALL)
inner.translate(Base.Vector(WALL, WALL, WALL))

# --- Shell = outer - inner ---
shell = outer.cut(inner)

# --- DC barrel jack hole on front face (X=0, YZ plane) ---
dc_hole = Part.makeCylinder(DC_JACK_R, WALL + 0.2)
dc_hole.translate(Base.Vector(-0.1, BOX_W/2, BOX_H/2 - 5))  # slightly above center
dc_hole.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(dc_hole)

# --- Cable gland hole 1 on rear face (X=BOX_L) ---
gland1 = Part.makeCylinder(GLAND_R, WALL + 0.2)
gland1.translate(Base.Vector(BOX_L - WALL - 0.1, BOX_W * 0.3, BOX_H/2 - 5))
gland1.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(gland1)

# --- Cable gland hole 2 on rear face ---
gland2 = Part.makeCylinder(GLAND_R, WALL + 0.2)
gland2.translate(Base.Vector(BOX_L - WALL - 0.1, BOX_W * 0.7, BOX_H/2 - 5))
gland2.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(gland2)

# --- Ventilation slots on left side (Y=0, XZ plane) ---
for i in range(VENT_COUNT):
    slot = Part.makeBox(VENT_SLOT_L, WALL + 0.2, VENT_SLOT_W)
    x_pos = 15 + i * (BOX_L - 30) / (VENT_COUNT - 1) if VENT_COUNT > 1 else BOX_L/2
    slot.translate(Base.Vector(x_pos - VENT_SLOT_L/2, -0.1, BOX_H * 0.35 + i * 0.5))
    shell = shell.cut(slot)

# --- Ventilation slots on right side (Y=BOX_W) ---
for i in range(VENT_COUNT):
    slot = Part.makeBox(VENT_SLOT_L, WALL + 0.2, VENT_SLOT_W)
    x_pos = 15 + i * (BOX_L - 30) / (VENT_COUNT - 1) if VENT_COUNT > 1 else BOX_L/2
    slot.translate(Base.Vector(x_pos - VENT_SLOT_L/2, BOX_W - WALL - 0.1, BOX_H * 0.35 + i * 0.5))
    shell = shell.cut(slot)

# --- Mounting tabs on left side (Y=0) ---
# Tab 1 (front)
tab1 = Part.makeBox(TAB_L, TAB_W, TAB_H)
tab1.translate(Base.Vector(WALL, -TAB_W, 0))
shell = shell.fuse(tab1)
# Screw hole in tab 1
tab1_hole = Part.makeCylinder(SCREW_R, TAB_H + 0.2)
tab1_hole.translate(Base.Vector(WALL + TAB_L/2, -TAB_W/2, -0.1))
shell = shell.cut(tab1_hole)

# Tab 2 (rear)
tab2 = Part.makeBox(TAB_L, TAB_W, TAB_H)
tab2.translate(Base.Vector(BOX_L - WALL - TAB_L, -TAB_W, 0))
shell = shell.fuse(tab2)
tab2_hole = Part.makeCylinder(SCREW_R, TAB_H + 0.2)
tab2_hole.translate(Base.Vector(BOX_L - WALL - TAB_L/2, -TAB_W/2, -0.1))
shell = shell.cut(tab2_hole)

# --- Mounting tabs on right side (Y=BOX_W) ---
# Tab 3 (front)
tab3 = Part.makeBox(TAB_L, TAB_W, TAB_H)
tab3.translate(Base.Vector(WALL, BOX_W, 0))
shell = shell.fuse(tab3)
tab3_hole = Part.makeCylinder(SCREW_R, TAB_H + 0.2)
tab3_hole.translate(Base.Vector(WALL + TAB_L/2, BOX_W + TAB_W/2, -0.1))
shell = shell.cut(tab3_hole)

# Tab 4 (rear)
tab4 = Part.makeBox(TAB_L, TAB_W, TAB_H)
tab4.translate(Base.Vector(BOX_L - WALL - TAB_L, BOX_W, 0))
shell = shell.fuse(tab4)
tab4_hole = Part.makeCylinder(SCREW_R, TAB_H + 0.2)
tab4_hole.translate(Base.Vector(BOX_L - WALL - TAB_L/2, BOX_W + TAB_W/2, -0.1))
shell = shell.cut(tab4_hole)

# --- Lid corner screw posts inside box ---
POST_R = 3.5    # radius of post (for M3 screw)
POST_H = BOX_H - WALL  # post height from bottom to top
SCREW_POST_R = 1.5  # pilot hole radius for M3 (3mm dia)

for cx, cy in [(8, 8), (BOX_L - 8, 8), (8, BOX_W - 8), (BOX_L - 8, BOX_W - 8)]:
    post = Part.makeCylinder(POST_R, POST_H)
    post.translate(Base.Vector(cx, cy, WALL))
    shell = shell.fuse(post)
    post_hole = Part.makeCylinder(SCREW_POST_R, POST_H + WALL)
    post_hole.translate(Base.Vector(cx, cy, WALL - 0.1))
    shell = shell.cut(post_hole)

# Add shell to document
body_obj = doc.addObject("Part::Feature", "EnclosureBody")
body_obj.Shape = shell

# --- Lid (separate part) ---
lid_plate = Part.makeBox(BOX_L, BOX_W, LID_H)
# Lid screw holes (countersunk-ish, through holes for M3)
for cx, cy in [(8, 8), (BOX_L - 8, 8), (8, BOX_W - 8), (BOX_L - 8, BOX_W - 8)]:
    lid_hole = Part.makeCylinder(SCREW_POST_R + 0.2, LID_H + 0.2)
    lid_hole.translate(Base.Vector(cx, cy, -0.1))
    lid_plate = lid_plate.cut(lid_hole)

lid_obj = doc.addObject("Part::Feature", "Lid")
lid_obj.Shape = lid_plate
# Move lid up above the box for display
lid_obj.Placement.Base = Base.Vector(0, 0, BOX_H + 3)

doc.recompute()
doc.saveAs(OUTPUT)

print(f"Saved: {OUTPUT}")
print(f"  Enclosure body vertices: {len(shell.Vertexes)}")
print(f"  Lid vertices: {len(lid_plate.Vertexes)}")
sys.stdout.flush()
