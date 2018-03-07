import maya.cmds as cmds
import maya.mel as mel
import os

#rigging utilities


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
def createControl(controlType, size, name):
    
    scale = getScaleFactor()
            
    if controlType == "circle":
        control = cmds.circle(c = (0,0,0), sw = 360, r = size * scale, d = 3, name = name)[0]
        
    if controlType == "circleSpecial":
        control = cmds.circle(c = (0,0,0), sw = 360, r = 1, d = 3, name = name)[0]
        
        side = name.rpartition("_")[2]
        
        if side == "l":
            cmds.xform(control, piv = (0, -1, 0))
            
        else:
            cmds.xform(control, piv = (0, 1, 0))
            
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)

    if controlType == "square":
        control = cmds.circle(c = (0,0,0), s = 4, sw = 360, r = size * scale, d = 1, name = name)[0]
        cmds.setAttr(control + ".rz", 45)
        
    if controlType == "foot":
        control = cmds.curve(name = name, d = 3, p = [(0, 40, 0), (-3.42, 39, 0), (-10.2, 37, 0), (-13, 22, 0), (-15.7, 13.2, 0), (-20, -14, 0), (-18.1, -25.6, 0), (-15, -44.8, 0), (1.1, -41.2, 0), (4.8, -41.7, 0), (15.5, -31.9, 0), (16.9, -22.7, 0), (18.6, -15.2, 0), (16.5, -.5, 0), (11.2, 29.2, 0), (10.7, 39.7, 0), (3.6, 39.9, 0), (0, 40, 0)])
        
        footLoc = cmds.spaceLocator(name = (name + "_end_loc"))[0]
        cmds.parent(footLoc, control)
        cmds.setAttr(footLoc + ".ty", -40)
        cmds.setAttr(footLoc + ".v", 0)
                     
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        
    
    if controlType == "arrow":
        control = cmds.curve(name = name, d = 1, p = [(0, -45, 0), (5, -45, 0), (5, -62, 0), (10, -62, 0), (0, -72, 0), (-10, -62, 0), (-5, -62, 0), (-5, -45, 0), (0, -45, 0)])
        cmds.xform(control, cp = True)
        cmds.setAttr(control + ".ty", 58.5)
        cmds.makeIdentity(control, t = 1, apply = True)
        cmds.xform(control, piv = (0, 13.5, 0))
        
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        
    if controlType == "arrowOnBall":
        
        control = cmds.curve(name = name, d = 1, p = [(0.80718, 0.830576, 8.022739), (0.80718, 4.219206, 7.146586 ), (0.80718, 6.317059, 5.70073), (2.830981, 6.317059, 5.70073), (0, 8.422749, 2.94335), (-2.830981, 6.317059, 5.70073), (-0.80718, 6.317059, 5.70073), (-0.80718, 4.219352, 7.146486), (-0.80718, 0.830576, 8.022739), (-4.187851, 0.830576, 7.158003), (-6.310271, 0.830576, 5.705409), (-6.317059, 2.830981, 5.7007), (-8.422749, 0, 2.94335), (-6.317059, -2.830981, 5.70073), (-6.317059, -0.830576, 5.70073), (-4.225134, -0.830576, 7.142501), (-0.827872, -0.830576, 8.017446), (-0.80718, -4.176512, 7.160965), (-0.80718, -6.317059, 5.70073), (-2.830981, -6.317059, 5.70073), (0, -8.422749, 2.94335), (2.830981, -6.317059, 5.70073), (0.80718, -6.317059, 5.70073), (0.80718, -4.21137, 7.151987), (0.80718, -0.830576, 8.022739), (4.183345, -0.830576, 7.159155), (6.317059, -0.830576, 5.70073), (6.317059, -2.830981, 5.70073), (8.422749, 0, 2.94335), (6.317059, 2.830981, 5.70073), (6.317059, 0.830576, 5.70073), (4.263245, 0.830576, 7.116234), (0.80718, 0.830576, 8.022739)])
        
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        
        
    if controlType == "semiCircle":
        
        control = cmds.curve(name = name, d = 3, p = [(0,0,0), (7, 0, 0), (8, 0, 0), (5, 4, 0), (0, 5, 0), (-5, 4, 0), (-8, 0, 0), (-7, 0, 0), (0,0,0)])
        cmds.xform(control, ws = True, t = (0, 5, 0))
        cmds.xform(control, ws = True, piv = (0,0,0))
        cmds.makeIdentity(control, t = 1, apply = True)
        
        
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        
        
    if controlType == "pin":
        control = cmds.curve(name = name, d = 1, p = [(12,0,0), (0, 0, 0), (-12, -12, 0), (-12, 12, 0), (0, 0, 0)])
        cmds.xform(control, ws = True, piv = [12,0,0])
        cmds.setAttr(control + ".scaleY", .5)
        cmds.makeIdentity(control, t = 1, apply = True)
        
        
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        
    
    if controlType == "sphere":

        points = [(0, 0, 1), (0, 0.5, 0.866), (0, 0.866025, 0.5), (0, 1, 0), (0, 0.866025, -0.5), (0, 0.5, -0.866025), (0, 0, -1), (0, -0.5, -0.866025), (0, -0.866025, -0.5), (0, -1, 0), (0, -0.866025, 0.5), (0, -0.5, 0.866025), (0, 0, 1), (0.707107, 0, 0.707107), (1, 0, 0), (0.707107, 0, -0.707107), (0, 0, -1), (-0.707107, 0, -0.707107), (-1, 0, 0), (-0.866025, 0.5, 0), (-0.5, 0.866025, 0), (0, 1, 0), (0.5, 0.866025, 0), (0.866025, 0.5, 0), (1, 0, 0), (0.866025, -0.5, 0), (0.5, -0.866025, 0), (0, -1, 0), (-0.5, -0.866025, 0), (-0.866025, -0.5, 0), (-1, 0, 0), (-0.707107, 0, 0.707107), (0, 0, 1)]
        control = cmds.curve(name = name, d = 1, p = points)
        
        cmds.setAttr(control + ".scaleX", size * scale)
        cmds.setAttr(control + ".scaleY", size * scale)
        cmds.setAttr(control + ".scaleZ", size * scale)
        

    return control    


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
def getScaleFactor():
    
    headLoc = cmds.spaceLocator(name = "headLoc")[0]
    cmds.parentConstraint("driver_head", headLoc)
    
    height = cmds.getAttr(headLoc + ".tz")
    
    defaultHeight = 400
    scaleFactor = height/defaultHeight
    cmds.delete(headLoc)
    
    return scaleFactor    

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
def getUpAxis(obj):
    
    cmds.xform(obj, ws = True, relative = True, t = [0, 0, 10])
    translate = cmds.getAttr(obj + ".translate")[0]
    newTuple = (abs(translate[0]), abs(translate[1]), abs(translate[2]))
    cmds.xform(obj, ws = True, relative = True, t = [0, 0, -10])
    
    highestVal = max(newTuple)
    axis = newTuple.index(highestVal)
    upAxis = None
    
    if axis == 0:
        upAxis = "X"
        
    if axis == 1:
        upAxis = "Y"
        
    if axis == 2:
        upAxis = "Z"
        
    return upAxis
    