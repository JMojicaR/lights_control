#!/usr/bin/env python3
"""
VL53L0X ToF sensor bottom housing for lights_control staircase light controller.
Generates sensor_bottom.FCStd with:
  - Small box: 30mm x 18mm x 10mm, wall thickness 1.5mm
  - Cutout window on front face for the VL53L0X ToF sensor lens
  - Mounting holes (2.5mm dia) for wall mounting
  - Cable exit hole (5mm dia) on the back
"""

import FreeCAD as App
import Part
import sys
from FreeCAD import Base

OUTPUT = "/home/hermesbot/lights_control/enclosures/sensor_bottom.FCStd"

# Dimensions (mm)
BOX_L = 30.0    # length (X)
BOX_W = 18.0    # width (Y)
BOX_H = 10.0    # height (Z)
WALL = 1.5      # wall thickness
CABLE_R = 2.5   # cable exit hole radius (5mm dia)
MOUNT_R = 1.25  # mounting hole radius (2.5mm dia)
TAB_W = 6.0     # mounting tab width
TAB_L = 8.0     # mounting tab extension
TAB_H = WALL    # tab thickness = wall

# VL53L0X sensor window dimensions (the emitter/receiver apertures)
# The VL53L0X has two small rectangular apertures, we model one combined window
WINDOW_W = 8.0   # window width
WINDOW_H = 4.0   # window height
WINDOW_OFFSET_Y = BOX_W / 2 - WINDOW_W / 2  # center window vertically
WINDOW_OFFSET_Z = BOX_H / 2 - WINDOW_H / 2  # center window in Z

print("=== Generating sensor_bottom.FCStd ===")
sys.stdout.flush()

doc = App.newDocument("sensor_bottom")

# --- Outer box ---
outer = Part.makeBox(BOX_L, BOX_W, BOX_H)

# --- Inner cavity (shell) ---
inner = Part.makeBox(BOX_L - 2*WALL, BOX_W - 2*WALL, BOX_H - WALL)
inner.translate(Base.Vector(WALL, WALL, WALL))
shell = outer.cut(inner)

# --- VL53L0X sensor window on front face (X=0) ---
# A rectangular pocket through the front wall
window = Part.makeBox(WALL + 0.2, WINDOW_W, WINDOW_H)
window.translate(Base.Vector(-0.1, WINDOW_OFFSET_Y, WINDOW_OFFSET_Z))
shell = shell.cut(window)

# --- Cable exit hole on back face (X=BOX_L) ---
cable = Part.makeCylinder(CABLE_R, WALL + 0.2)
cable.translate(Base.Vector(BOX_L - WALL - 0.1, BOX_W / 2, BOX_H / 2))
cable.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(cable)

# --- Mounting tabs on left side (Y=0) ---
# Front tab
mtab1 = Part.makeBox(TAB_L, TAB_W, TAB_H)
mtab1.translate(Base.Vector(WALL + 2, -TAB_W, 0))
shell = shell.fuse(mtab1)
mtab1_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
mtab1_hole.translate(Base.Vector(WALL + 2 + TAB_L/2, -TAB_W/2, -0.1))
shell = shell.cut(mtab1_hole)

# Rear tab
mtab2 = Part.makeBox(TAB_L, TAB_W, TAB_H)
mtab2.translate(Base.Vector(BOX_L - WALL - TAB_L - 2, -TAB_W, 0))
shell = shell.fuse(mtab2)
mtab2_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
mtab2_hole.translate(Base.Vector(BOX_L - WALL - TAB_L/2 - 2, -TAB_W/2, -0.1))
shell = shell.cut(mtab2_hole)

# --- Mounting tabs on right side (Y=BOX_W) ---
# Front tab
mtab3 = Part.makeBox(TAB_L, TAB_W, TAB_H)
mtab3.translate(Base.Vector(WALL + 2, BOX_W, 0))
shell = shell.fuse(mtab3)
mtab3_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
mtab3_hole.translate(Base.Vector(WALL + 2 + TAB_L/2, BOX_W + TAB_W/2, -0.1))
shell = shell.cut(mtab3_hole)

# Rear tab
mtab4 = Part.makeBox(TAB_L, TAB_W, TAB_H)
mtab4.translate(Base.Vector(BOX_L - WALL - TAB_L - 2, BOX_W, 0))
shell = shell.fuse(mtab4)
mtab4_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
mtab4_hole.translate(Base.Vector(BOX_L - WALL - TAB_L/2 - 2, BOX_W + TAB_W/2, -0.1))
shell = shell.cut(mtab4_hole)

# Add to document
body_obj = doc.addObject("Part::Feature", "SensorBottomHousing")
body_obj.Shape = shell

doc.recompute()
doc.saveAs(OUTPUT)

print(f"Saved: {OUTPUT}")
print(f"  Vertices: {len(shell.Vertexes)}")
sys.stdout.flush()
