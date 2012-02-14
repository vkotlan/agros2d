import os

# model
newdocument("Scripting test", "planar", "general", 1, 2, "disabled", 0, 1, 0, "steadystate", 0, 0, 0)

# variables
x = .025;
y = .05;
dx = .025;
r = .025;

# boundaries
addboundary("Dirichlet", "general_value", 0)

# materials
addmaterial("Material", 10, 10)

# edges
addedge(0, 0, x, 0, 0, "Dirichlet")
addedge(x, 0, x, y, 0, "Dirichlet")
addedge(x, y, 0, y, 0, "Dirichlet")
addedge(0, y, 0, 0, 0, "Dirichlet")

# labels
addlabel(x/2, y/2, 0, 0, "Material")

# moveselection, selectedge, selectlabel
selectedge(1)
moveselection(dx, 0, False)
selectlabel(0)
moveselection(dx/2, 0, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testMoveSelection1 = test("moveselection()", volume["S"], (x+dx)*y)

selectedge(1)
moveselection(-dx, 0, False)
selectlabel(0)
moveselection(-dx/2, 0, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testMoveSelection2 = test("moveselection()", volume["S"], x*y)

# scaleselection, selectnode
selectnode(0, 1, 2, 3)
scaleselection(0, 0, 2, False)
selectlabel(0)
scaleselection(0, 0, 2, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testScaleSelection1 = test("scaleselection()", volume["S"], (2*x)*(2*y))

selectnode(0, 1, 2, 3)
scaleselection(0, 0, 1.0/2, False)
selectlabel(0)
scaleselection(0, 0, 1.0/2, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testScaleSelection2 = test("scaleselection()", volume["S"], x*y)

# rotateselection, mode, slectall
mode("node")
selectall()
rotateselection(0, 0, 90, False)
mode("label")
selectall()
rotateselection(0, 0, 90, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testRotateSelection1 = test("rotateselection()", volume["S"], x*y)

mode("edge")
selectall()
rotateselection(0, 0, -90, False)
mode("label")
selectall()
rotateselection(0, 0, -90, False)
zoombestfit()
solve()
volume = volumeintegral(0)
testRotateSelection2 = test("rotateselection()", volume["S"], x*y)

# addrect
selectnode(0, 1, 2, 3)
deleteselection()
addrect(0, 0, x, y, "Dirichlet")
zoombestfit()
solve()
volume = volumeintegral(0)
testAddRect = test("addrect()", volume["S"], x*y)

# addcircle, deleteselection
selectnode(0, 1, 2, 3)
deleteselection()
selectlabel(0)
deleteselection()
addcircle(0, 0, r, "Dirichlet")
addlabel(0, 0, 0, 0, "Material")
zoombestfit()
solve()
volume = volumeintegral(0)
testAddCircle = test("addcircle()", volume["S"], pi*(r**2))

# addsemicircle
selectnode(0, 1, 2, 3)
deleteselection()
selectlabel(0)
deleteselection()
addsemicircle(0, 0, r, "Dirichlet")
addlabel(r/2, 0, 0, 0, "Material")
zoombestfit()
solve()
volume = volumeintegral(0)
testAddSemiCircle = test("addsemicircle()", volume["S"], (pi*(r**2))/2)

# savedocument, opendocument
import tempfile
fn = tempfile.gettempdir() + "/test.a2d"
savedocument(fn)
opendocument(fn)
solve()
volume = volumeintegral(0)
testSaveDocument = test("addsemicircle()", volume["S"], (pi*(r**2))/2)
closedocument()
import os
os.remove(fn)

print("Test: Scripting: " + str(testMoveSelection1 and testMoveSelection2 and testScaleSelection1 and testScaleSelection2 and testRotateSelection1 and testRotateSelection2 and testAddRect and testAddCircle and testAddSemiCircle and testSaveDocument))

# modifyboundary(), modifymaterial()
newdocument("unnamed", "planar", "general", 1, 2, "disabled", 1, 1, 0, "steadystate", 1.0, 1.0, 0.0)
addboundary("B", "general_value", 1)
modifyboundary("B", "general_value", 2)
modifyboundary("B", "general_derivative", 1)
addmaterial("M", 1, 1)
modifymaterial("M", 1, 2)

newdocument("unnamed", "planar", "electrostatic", 1, 2, "disabled", 1, 1, 0, "steadystate", 1.0, 1.0, 0.0)
addboundary("B", "electrostatic_potential", 1)
modifyboundary("B", "electrostatic_potential", 2)
modifyboundary("B", "electrostatic_surface_charge_density", 1)
addmaterial("M", 1, 1)
modifymaterial("M", 1, 2)

newdocument("unnamed", "planar", "magnetic", 1, 2, "disabled", 1, 1, 0, "steadystate", 1.0, 1.0, 0.0)
addboundary("B", "magnetic_vector_potential", 1)
modifyboundary("B", "magnetic_vector_potential", 2)
modifyboundary("B", "magnetic_surface_current_density", 1)
addmaterial("M", 1, 1, 1, 1, 1, 1, 1, 1, 1)
modifymaterial("M", 2, 2, 2, 2, 2, 2, 2, 2, 2)

newdocument("unnamed", "planar", "current", 1, 2, "disabled", 1, 1, 0, "steadystate", 1.0, 1.0, 0.0)
addboundary("B", "current_potential", 1)
modifyboundary("B", "current_potential", 2)
modifyboundary("B", "current_inward_current_flow", 1)
addmaterial("M", 1)
modifymaterial("M", 2)

newdocument("unnamed", "planar", "heat", 1, 2, "disabled", 1, 1, 0, "steadystate", 1.0, 1.0, 0.0)
addboundary("B", "heat_temperature", 1)
modifyboundary("B", "heat_temperature", 2)
modifyboundary("B", "heat_heat_flux", 1)
addmaterial("M", 1, 1, 1, 1)
modifymaterial("M", 2, 2, 2, 2)