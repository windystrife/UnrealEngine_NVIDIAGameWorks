import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os

class Fortnite_Husk_Chooser():
    
    def __init__(self):
        
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()
            
            
        #check to see if window exists, if so, delete
        if cmds.window("fortniteHuskChooser_UI", exists = True):
            cmds.deleteUI("fortniteHuskChooser_UI")
            
        
        self.widgets = {}
        #build window
        self.widgets["window"] = cmds.window("fortniteHuskChooser_UI", w = 612, h = 350, title = "Choose Your Husk!", mnb = False, mxb = False, sizeable = False)
        
        #create the main layout
        self.widgets["topLevelLayout"] = cmds.formLayout(w = 612, h = 350)
        
        #add background image
        self.widgets["background"] = cmds.image(w = 612, h = 350, parent = self.widgets["topLevelLayout"], image = self.mayaToolsDir + "/General/Icons/Fortnite/huskChooser.jpg")
        
        #add the four husk buttons
        self.widgets["husk_Normal"] = cmds.symbolButton(w = 145, h = 287, image = self.mayaToolsDir + "/General/Icons/Fortnite/husk_normal.bmp", c = partial(self.chooseHusk, None))
        self.widgets["husk_Pitcher"] = cmds.symbolButton(w = 145, h = 287, image = self.mayaToolsDir + "/General/Icons/Fortnite/husk_pitcher.bmp", c = partial(self.chooseHusk, "pitcher"))
        self.widgets["husk_Female"] = cmds.symbolButton(w = 145, h = 287, image = self.mayaToolsDir + "/General/Icons/Fortnite/husk_female.bmp", c = partial(self.chooseHusk, "female"))
        self.widgets["husk_Fat"] = cmds.symbolButton(w = 145, h = 287, image = self.mayaToolsDir + "/General/Icons/Fortnite/husk_fat.bmp", c = partial(self.chooseHusk, "fat"))
        
        #place buttons
        cmds.formLayout(self.widgets["topLevelLayout"], edit = True, af = [(self.widgets["husk_Normal"], "top", 57),(self.widgets["husk_Normal"], "left", 7)])
        cmds.formLayout(self.widgets["topLevelLayout"], edit = True, af = [(self.widgets["husk_Pitcher"], "top", 57),(self.widgets["husk_Pitcher"], "left", 159)])
        cmds.formLayout(self.widgets["topLevelLayout"], edit = True, af = [(self.widgets["husk_Female"], "top", 57),(self.widgets["husk_Female"], "left", 310)])
        cmds.formLayout(self.widgets["topLevelLayout"], edit = True, af = [(self.widgets["husk_Fat"], "top", 57),(self.widgets["husk_Fat"], "left", 461)])
        
        #show the window
        cmds.showWindow(self.widgets["window"])
        
        
        
        
    def chooseHusk(self, name, *args):
        
        #find the husk file
	if name != None:
	    filePath = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Husk/" + name + ".mb"
	    if os.path.exists(filePath):
		
		#reference in the model
		
		#find references in scene:
		refs = cmds.ls(type = "reference")
		for ref in refs:
		    namespace = cmds.referenceQuery(ref, namespace = True)
		    
		    if namespace == ":husk_bodyPart":
			fileName = cmds.referenceQuery(ref, filename = True)
			cmds.file(fileName, rr = True)
			
		
		#reference in the new body part
		cmds.file(filePath, r = True, type = "mayaBinary", loadReferenceDepth = "all", namespace = "husk_bodyPart", options = "v=0")
		
		#constrain to rig
		if cmds.objExists("Husk_Anim_Rig:b_MF_Root"):
		    cmds.select("Husk_Anim_Rig:b_MF_Root", hi = True)
		    rigJoints = cmds.ls(sl = True, type = "joint")
		    
		    for joint in rigJoints:
			newJoint = joint.partition(":")[2]
			newJoint = "husk_bodyPart:" + newJoint
			cmds.parentConstraint(joint, newJoint)
			
		    #hide original layers
		    cmds.setAttr("Husk_Anim_Rig:Husk_Geo_Layer.v", 0)
		    cmds.setAttr("Husk_Anim_Rig:Husk_Head.v", 0)
		    
		    
		elif cmds.objExists("Husk:b_MF_Root"):
		    cmds.select("Husk:b_MF_Root", hi = True)
		    rigJoints = cmds.ls(sl = True, type = "joint")
		    
		    for joint in rigJoints:
			newJoint = joint.partition(":")[2]
			newJoint = "husk_bodyPart:" + newJoint
			cmds.parentConstraint(joint, newJoint)
			
		    #hide original layers
		    try:
			cmds.setAttr("Husk:Husk_Geo_Layer.v", 0)
			
		    except:
			pass
		    
		    
		else:
		    cmds.promptDialog(title = "Error", icon = "critical", message = "Was unable to find the referenced Husk rig file.")
		    return
		
		#delete UI
		cmds.deleteUI(self.widgets["window"])
	    
	    
    
	if name == None:
	    #find references in scene:
	    refs = cmds.ls(type = "reference")
	    for ref in refs:
		namespace = cmds.referenceQuery(ref, namespace = True)
			
		if namespace == ":husk_bodyPart":
		    fileName = cmds.referenceQuery(ref, filename = True)
		    cmds.file(fileName, rr = True)
		    
	    #show layers
	    try:
		
		if cmds.objExists("Husk_Anim_Rig:b_MF_Root"):
		    cmds.setAttr("Husk_Anim_Rig:Husk_Geo_Layer.v", 1)
		    cmds.setAttr("Husk_Anim_Rig:Husk_Head.v", 1)
		
		else:
		    cmds.setAttr("Husk:Husk_Geo_Layer.v", 1)
		
	    except:
		pass
	    
		    
	    #delete the UI
	    cmds.deleteUI(self.widgets["window"])
	
        
        
        

        
