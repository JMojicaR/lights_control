#!/usr/bin/env python3
"""
BH1750 ambient light sensor housing for lights_control staircase light controller.
Generates bh1750_housing.FCStd with:
  - Small box: 25mm x 18mm x 10mm, wall thickness 1.5mm
  - Open top face for ambient light sensing
  - Mounting tabs with 2.5mm screw holes
  - Cable exit hole (5mm dia) on the back
"""

import FreeCAD as App
import Part
import sys
from FreeCAD import Base

OUTPUT = "/home/hermesbot/lights_control/enclosures/bh1750_housing.FCStd"

# Dimensions (mm)
BOX_L = 25.0
BOX_W = 18.0
BOX_H = 10.0
WALL = 1.5
CABLE_R = 2.5   # 5mm dia
MOUNT_R = 1.25  # 2.5mm dia
TAB_W = 6.0
TAB_L = 8.0
TAB_H = WALL

print("=== Generating bh1750_housing.FCStd ===")
sys.stdout.flush()

doc = App.newDocument("bh1750_housing")

# --- Outer box ---
outer = Part.makeBox(BOX_L, BOX_W, BOX_H)

# --- Inner cavity (keep bottom wall, open top) ---
inner = Part.makeBox(BOX_L - 2*WALL, BOX_W - 2*WALL, BOX_H - WALL)
inner.translate(Base.Vector(WALL, WALL, WALL))
shell = outer.cut(inner)

# --- Top face opening: remove the entire top wall so sensor sees ambient light ---
# The opening spans the top face minus a small rim
rim = 2.0  # small rim around top edge for structural integrity
top_opening = Part.makeBox(BOX_L - 2*rim, BOX_W - 2*rim, WALL + 0.2)
top_opening.translate(Base.Vector(rim, rim, BOX_H - WALL - 0.1))
shell = shell.cut(top_opening)

# --- Cable exit hole on back face (X=BOX_L) ---
cable = Part.makeCylinder(CABLE_R, WALL + 0.2)
cable.translate(Base.Vector(BOX_L - WALL - 0.1, BOX_W / 2, BOX_H / 2))
cable.rotate(Base.Vector(0,0,0), Base.Vector(0,1,0), 90)
shell = shell.cut(cable)

# --- Mounting tabs on left side (Y=0) ---
for x_inset in [WALL + 1, BOX_L - WALL - TAB_L - 1]:
    mtab = Part.makeBox(TAB_L, TAB_W, TAB_H)
    mtab.translate(Base.Vector(x_inset, -TAB_W, 0))
    shell = shell.fuse(mtab)
    mtab_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
    mtab_hole.translate(Base.Vector(x_inset + TAB_L/2, -TAB_W/2, -0.1))
    shell = shell.cut(mtab_hole)

# --- Mounting tabs on right side (Y=BOX_W) ---
for x_inset in [WALL + 1, BOX_L - WALL - TAB_L - 1]:
    mtab = Part.makeBox(TAB_L, TAB_W, TAB_H)
    mtab.translate(Base.Vector(x_inset, BOX_W, 0))
    shell = shell.fuse(mtab)
    mtab_hole = Part.makeCylinder(MOUNT_R, TAB_H + 0.2)
    mtab_hole.translate(Base.Vector(x_inset + TAB_L/2, BOX_W + TAB_W/2, -0.1))
    shell = shell.cut(mtab_hole)

# Add to document
body_obj = doc.addObject("Part::Feature", "BH1750Housing")
body_obj.Shape = shell

doc.recompute()
doc.saveAs(OUTPUT)

print(f"Saved: {OUTPUT}")
print(f"  Vertices: {len(shell.Vertexes)}")
sys.stdout.flush()
