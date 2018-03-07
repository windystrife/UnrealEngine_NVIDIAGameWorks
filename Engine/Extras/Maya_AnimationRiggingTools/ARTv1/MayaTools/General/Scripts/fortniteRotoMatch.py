import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os


class RotoSpineMatch():
    
    def __init__(self, character):
        
        
        #before you switch, create a locator for the neck position
        neckLoc = cmds.spaceLocator(name = "roto_neck_locator")[0]
        constraintNeckLoc = cmds.parentConstraint(character + ":neck_01_fk_anim", neckLoc)[0]
        cmds.delete(constraintNeckLoc)
        
        
	constraint = cmds.orientConstraint("spine_01", character + ":spine_01_anim")[0]
	cmds.setKeyframe(character + ":spine_01_anim")
	cmds.delete(constraint)
	
	constraint = cmds.orientConstraint(character + ":twist_splineIK_spine_03", character + ":spine_03_anim", skip = ["y", "z"])[0]
	cmds.setKeyframe(character + ":spine_03_anim.rx")
	cmds.delete(constraint)
	
	constraint = cmds.orientConstraint(character + ":chest_ik_anim", character + ":spine_05_anim", skip = ["y", "z"])[0]
	cmds.setKeyframe(character + ":spine_05_anim.rx")
	cmds.delete(constraint)
	

        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        #
        #
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        
        #create (rotate) Y dist locators for the spine
        spineStartPointY = cmds.xform(character + ":twist_splineIK_spine_05", q = True, ws = True, t = True)[0]
        spineEndPointY = cmds.xform(character + ":spine_05_anim", q = True, ws = True, t = True)[0]
        
        spineDistY = cmds.distanceDimension( sp = (spineStartPointY, 0, 0), ep = (spineEndPointY, 0, 0))
        spineDistYParent = cmds.listRelatives(spineDistY, parent = True)[0]
        
        spineLocs = cmds.listConnections(spineDistY)
        spineStartLocY = spineLocs[0]
        spineEndLocY = spineLocs[1]
        
        cmds.pointConstraint(character + ":twist_splineIK_spine_05", spineStartLocY, skip = ["y", "z"], mo = True)
        cmds.pointConstraint(character + ":spine_05_anim", spineEndLocY, skip = ["y", "z"], mo = True)
        
        #get the distance. if distance is greater than .05, then modify rotations
        currentYDist = cmds.getAttr(spineDistY + ".distance")
        self.checkDistance(character, spineDistY, currentYDist, currentYDist, ".ry", "spine_03_anim")
        
        try:
            cmds.delete(spineDistYParent)
            cmds.delete(spineStartLocY)
            cmds.delete(spineEndLocY)
        except:
            pass
        
        
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        #
        #
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        
        #create (rotate) Z dist locators for the spine
        spineStartPointZ = cmds.xform(character + ":twist_splineIK_spine_05", q = True, ws = True, t = True)[1]
        spineEndPointZ = cmds.xform(character + ":spine_05_anim", q = True, ws = True, t = True)[1]
        
        spineDistZ = cmds.distanceDimension( sp= (0, spineStartPointZ,0), ep=(0, spineEndPointZ, 0) )
        spineDistZParent = cmds.listRelatives(spineDistZ, parent = True)[0]
        
        spineZLocs = cmds.listConnections(spineDistZ)
        spineStartLocZ = spineZLocs[0]
        spineEndLocZ = spineZLocs[1]
        
        cmds.pointConstraint(character + ":twist_splineIK_spine_05", spineStartLocZ, skip = ["x", "z"], mo = True)
        cmds.pointConstraint(character + ":spine_05_anim", spineEndLocZ, skip = ["x", "z"], mo = True)
        
        currentZDist = cmds.getAttr(spineDistZ + ".distance")
        self.checkDistance(character, spineDistZ, currentZDist, currentZDist, ".rz", "spine_03_anim")
        
        try:
            cmds.delete(spineDistZParent)
            cmds.delete(spineStartLocZ)
            cmds.delete(spineEndLocZ)
        except:
            pass
        
        
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        #
        #
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        cmds.setAttr(character + ":Rig_Settings.spine_ik", 0)
        cmds.setAttr(character + ":Rig_Settings.spine_fk", 1)       
        
        #create (rotate) Y dist locators for the neck
        neckStartPointY = cmds.xform(neckLoc, q = True, ws = True, t = True)[0]
        neckEndPointY = cmds.xform(character + ":neck_01_fk_anim", q = True, ws = True, t = True)[0]
        
        neckDistY = cmds.distanceDimension( sp= (neckStartPointY,0, 0), ep=(neckEndPointY, 0, 0) )
        neckDistYParent = cmds.listRelatives(neckDistY, parent = True)[0]
        
        neckYLocs = cmds.listConnections(neckDistY)
        neckStartLocY = neckYLocs[0]
        neckEndLocY = neckYLocs[1]
        
        cmds.pointConstraint(neckLoc, neckStartLocY, skip = ["y", "z"], mo = True)
        cmds.pointConstraint(character + ":neck_01_fk_anim", neckEndLocY, skip = ["y", "z"], mo = True)
        
        currentNeckYDist = cmds.getAttr(neckDistY + ".distance")
        self.checkDistance(character, neckDistY, currentNeckYDist, currentNeckYDist, ".ry", "spine_05_anim")
        
        try:
            cmds.delete(neckDistYParent)
            cmds.delete(neckStartLocY)
            cmds.delete(neckEndLocY)
        except:
            pass
        
        
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        #
        #
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        cmds.setAttr(character + ":Rig_Settings.spine_ik", 0)
        cmds.setAttr(character + ":Rig_Settings.spine_fk", 1)  
        
        #create (rotate) Z dist locators for the neck
        neckStartPointZ = cmds.xform(neckLoc, q = True, ws = True, t = True)[1]
        neckEndPointZ = cmds.xform(character + ":neck_01_fk_anim", q = True, ws = True, t = True)[1]
        
        neckDistZ = cmds.distanceDimension( sp= (0, neckStartPointZ,0), ep=(0, neckEndPointZ, 0) )
        neckDistZParent = cmds.listRelatives(neckDistZ, parent = True)[0]
        
        neckZLocs = cmds.listConnections(neckDistZ)
        neckStartLocZ = neckZLocs[0]
        neckEndLocZ = neckZLocs[1]
        
        cmds.pointConstraint(neckLoc, neckStartLocZ, skip = ["x", "z"], mo = True)
        cmds.pointConstraint(character + ":neck_01_fk_anim", neckEndLocZ, skip = ["x", "z"], mo = True)
        
        currentNeckZDist = cmds.getAttr(neckDistZ + ".distance")
        self.checkDistance(character, neckDistZ, currentNeckZDist, currentNeckZDist, ".rz", "spine_05_anim")
        
        try:
            cmds.delete(neckDistZParent)
            cmds.delete(neckStartLocZ)
            cmds.delete(neckEndLocZ)
        except:
            pass
        
        
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        #
        #
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
        cmds.delete(neckLoc)
	

	    
    #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
    #
    #
    #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#
    def checkDistance(self, character, distanceNode, distanceAttr, originalValue, attr, manipControl):
        
        print distanceAttr
        if distanceAttr > .05:
            
            currentAttr = cmds.getAttr(character + ":" + manipControl + attr)
                
            try:
                cmds.setAttr(character + ":" + manipControl + attr, currentAttr + .015)
                cmds.setKeyframe(character + ":" + manipControl + attr)
                newDist = cmds.getAttr(distanceNode + ".distance")
                
                if newDist < originalValue:
                    self.checkDistance(character, distanceNode, newDist, newDist, attr, manipControl)
                        
                if newDist > originalValue:
                    cmds.setAttr(character + ":" + manipControl + attr, currentAttr - .025)
                    cmds.setKeyframe(character + ":" + manipControl + attr)
                    newDist = cmds.getAttr(distanceNode + ".distance")
                        
                    self.checkDistance(character, distanceNode, newDist, newDist, attr, manipControl)
            
            except:
                pass