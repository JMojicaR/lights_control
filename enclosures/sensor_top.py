#!/usr/bin/env python3
"""
VL53L0X ToF sensor top housing (mirror of bottom) for lights_control staircase light controller.
Generates sensor_top.FCStd — identical to sensor_bottom but represents the upper sensor.
Same dimensions: 30mm x 18mm x 10mm, wall 1.5mm
"""

import FreeCAD as App
import Part
import sys
from FreeCAD import Base

OUTPUT = "/home/hermesbot/lights_control/enclosures/sensor_top.FCStd"

# Dimensions (mm)
BOX_L = 30.0
BOX_W = 18.0
BOX_H = 10.0
WALL = 1.5
CABLE_R = 2.5   # 5mm dia
MOUNT_R = 1.25  # 2.5mm dia
TAB_W = 6.0
TAB_L = 8.0
TAB_H = WALL

WINDOW_W = 8.0
WINDOW_H = 4.0
WINDOW_OFFSET_Y = BOX_W / 2 - WINDOW_W / 2
WINDOW_OFFSET_Z = BOX_H / 2 - WINDOW_H / 2

print("=== Generating sensor_top.FCStd ===")
sys.stdout.flush()

doc = App.newDocument("sensor_top")

# --- Outer box ---
outer = Part.makeBox(BOX_L, BOX_W, BOX_H)

# --- Inner cavity ---
inner = Part.makeBox(BOX_L - 2*WALL, BOX_W - 2*WALL, BOX_H - WALL)
inner.translate(Base.Vector(WALL, WALL, WALL))
shell = outer.cut(inner)

# --- VL53L0X sensor window on front face ---
window = Part.makeBox(WALL + 0.2, WINDOW_W, WINDOW_H)
window.translate(Base.Vector(-0.1, WINDOW_OFFSET_Y, WINDOW_OFFSET_Z))
shell = shell.cut(window)

# --- Cable exit hole on back face ---
cable = Part.makeCylinder(CABLE_R, WALL + 0.2)
cable.translate(Base.Vector(BOX_L - WALL - 0.1, BOX_W / 2, BOX_H / 2))
cable.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(cable)

# --- Mounting tabs on left side (Y=0) ---
for x_inset in [WALL + 2, BOX_L - WALL - TAB_L - 2]:
    mtab = Part.makeBox(TAB_L, TAB_W, TAB_H)
    mtab.translate(Base.Vector(x_inset, -TAB_W, 0))
    shell = shell.fuse(mtab)
    mtab_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
    mtab_hole.translate(Base.Vector(x_inset + TAB_L/2, -TAB_W/2, -0.1))
    shell = shell.cut(mtab_hole)

# --- Mounting tabs on right side (Y=BOX_W) ---
for x_inset in [WALL + 2, BOX_L - WALL - TAB_L - 2]:
    mtab = Part.makeBox(TAB_L, TAB_W, TAB_H)
    mtab.translate(Base.Vector(x_inset, BOX_W, 0))
    shell = shell.fuse(mtab)
    mtab_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
    mtab_hole.translate(Base.Vector(x_inset + TAB_L/2, BOX_W + TAB_W/2, -0.1))
    shell = shell.cut(mtab_hole)

# Add to document
body_obj = doc.addObject("Part::Feature", "SensorTopHousing")
body_obj.Shape = shell

doc.recompute()
doc.saveAs(OUTPUT)

print(f"Saved: {OUTPUT}")
print(f"  Vertices: {len(shell.Vertexes)}")
sys.stdout.flush()
