import maya.cmds as cmds
import maya.mel as mel
import os
import ART_rigUtils as utils
reload(utils)


class Spine():
    
    #pass in items we need to know about (do we build a clavicle? What is the root name of the module?(If set to None, just use upperarm, lowerarm, etc), 
    def __init__(self, includeClavicle, name, prefix, suffix, armGrpParent, color):
        
        
        print "test"
        
        

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def buildFKSpine(self):
	
	#find the number of spine bones from the skeleton settings
	spineJoints = self.getSpineJoints()
	fkControls = []
	parent = None
	
	for joint in spineJoints:
	    if joint == "spine_01":
		#add space switcher node to base of spine
		spaceSwitcherFollow = cmds.group(empty = True, name = joint + "_space_switcher_follow")
		constraint = cmds.parentConstraint(joint, spaceSwitcherFollow)[0]
		cmds.delete(constraint)
		spaceSwitcher = cmds.duplicate(spaceSwitcherFollow, name = joint + "_space_switcher")[0]
		cmds.parent(spaceSwitcher, spaceSwitcherFollow)
		
	    #create an empty group in the same space as the joint
	    group = cmds.group(empty = True, name = joint + "_anim_grp")
	    constraint = cmds.parentConstraint(joint, group)[0]
	    cmds.delete(constraint)
	    
	    #create an additional layer of group that has zeroed attrs
	    offsetGroup = cmds.group(empty = True, name = joint + "_anim_offset_grp")
	    constraint = cmds.parentConstraint(joint, offsetGroup)[0]
	    cmds.delete(constraint)
	    cmds.parent(offsetGroup, group)
	    
	    #create a control object in the same space as the joint
	    control = utils.createControl("circle", 45, joint + "_anim")
	    tempDupe = cmds.duplicate(control)[0]
	    constraint = cmds.parentConstraint(joint, control)[0]
	    cmds.delete(constraint)
	    fkControls.append(control)
	    
	    
	    #parent the control object to the group
	    cmds.parent(control, offsetGroup)
	    constraint = cmds.orientConstraint(tempDupe, control, skip = ["x", "z"])[0]
	    cmds.delete(constraint)
	    cmds.makeIdentity(control, t = 1, r = 1, s = 1, apply = True)

	    
	    #setup hierarchy
	    if parent != None:
		cmds.parent(group, parent, absolute = True)
		
	    else:
		cmds.parent(group, spaceSwitcher)
		cmds.parent(spaceSwitcherFollow, "body_anim")
		
		
	    #set the parent to be the current spine control
	    parent = control
	    
	    #clean up
	    cmds.delete(tempDupe)
	    
	    for attr in [".v"]:
		cmds.setAttr(control + attr, lock = True, keyable = False)
		
	    #set the control's color
	    cmds.setAttr(control + ".overrideEnabled", 1)
	    cmds.setAttr(control + ".overrideColor", 18)
	    

	return fkControls
    
    

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getSpineJoints(self):
        
        if cmds.objExists("Skeleton_Settings"):
	    numSpineBones = int(cmds.getAttr("Skeleton_Settings.numSpineBones"))
	    spineJoints = []
	    
	    for i in range(int(numSpineBones)):
		
		if i < 10:
		    spineJoint = "spine_0" + str(i + 1)
		    
		else:
		    spineJoint = "spine_" + (i + 1)
		    
		spineJoints.append(spineJoint)
		
	    return spineJoints
	
	else:
	    cmds.confirmDialog(title = "Error", icon = "critical", message = "Needed node, Skeleton_Settings could not be found.")
	    return
	