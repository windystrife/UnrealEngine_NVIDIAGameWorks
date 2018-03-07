import maya.cmds as cmds
from functools import partial
import os, cPickle, math
import maya.mel as mel
import maya.utils



class Retargeter():
    
    def __init__(self):
        
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()	
        
        #create list of controls
        self.captureControls = ["Settings", "Morph_Anim", "Master_Anim", "Root_Anim", "Body_Anim", "Hip_Anim", "Spine_01_Anim", "Spine_02_Anim", "Spine_03_Anim", "Neck_Anim", "Head_Anim", "Jaw_Anim", "L_Shoulder_Anim", "L_IK_Elbow_Anim", "L_IK_Hand_Anim",
                                "FK_b_MF_UpperArm_L", "FK_b_MF_Forearm_L", "FK_b_MF_Hand_L", "R_Shoulder_Anim", "R_IK_Elbow_Anim", "R_IK_Hand_Anim", "FK_b_MF_UpperArm_R", "FK_b_MF_Forearm_R", "FK_b_MF_Hand_R", 
                                "Left_Foot_Anim", "FK_b_MF_Thigh_L", "FK_b_MF_Calf_L", "FK_b_MF_Foot_L", "FK_b_MF_Toe_L", "Right_Foot_Anim", "FK_b_MF_Thigh_R", "FK_b_MF_Calf_R", "FK_b_MF_Foot_R", "FK_b_MF_Toe_R",
                                "L_Thumb_1_Anim", "L_Thumb_2_Anim", "L_Thumb_3_Anim", "L_Index_1_Anim", "L_Index_2_Anim", "L_Index_3_Anim", "L_Middle_1_Anim", "L_Middle_2_Anim", "L_Middle_3_Anim", "L_Ring_1_Anim", "L_Ring_2_Anim", "L_Ring_3_Anim", "L_Pinky_1_Anim", "L_Pinky_2_Anim", "L_Pinky_3_Anim",
                                "R_Thumb_1_Anim", "R_Thumb_2_Anim", "R_Thumb_3_Anim", "R_Index_1_Anim", "R_Index_2_Anim", "R_Index_3_Anim", "R_Middle_1_Anim", "R_Middle_2_Anim", "R_Middle_3_Anim", "R_Ring_1_Anim", "R_Ring_2_Anim", "R_Ring_3_Anim", "R_Pinky_1_Anim", "R_Pinky_2_Anim", "R_Pinky_3_Anim"]



	
	self.widgets = {}
	
	if cmds.window("ART_RetargetTool_UI", exists = True):
	    cmds.deleteUI("ART_RetargetTool_UI")
	    
	self.widgets["window"] = cmds.window("ART_RetargetTool_UI", title = "Husk Retarget", w = 400, h = 150, sizeable = False, mnb = False, mxb = False)
	
	#main layout
	self.widgets["layout"] = cmds.formLayout(w = 400, h = 150)
	
	#textField and browse button
	self.widgets["textField"] = cmds.textField(w = 300, text = "")
	self.widgets["browse"] = cmds.button(w = 70, label = "Browse", c = self.browse)
	
	cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["textField"], "left", 10),(self.widgets["textField"], "top", 10)])
	cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["browse"], "right", 10),(self.widgets["browse"], "top", 10)])
	
	#process button
	self.widgets["process"] = cmds.button(w = 380, h = 50, label = "RETARGET", c = self.process)
	cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["process"], "left", 10),(self.widgets["process"], "top", 40)])
	
	
	#progress bar
	self.widgets["currentFileProgressBar"] = cmds.progressBar(w = 390)
	cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["currentFileProgressBar"], "left", 5),(self.widgets["currentFileProgressBar"], "bottom", 35)])
	
	self.widgets["progressBar"] = cmds.progressBar(w = 390)
	cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["progressBar"], "left", 5),(self.widgets["progressBar"], "bottom", 10)])
	
	#show window
	cmds.showWindow(self.widgets["window"])
	
	
	#TESTING
	#cmds.autoKeyframe( state=False )
	#self.zeroLocs()
	#self.retarget()
	#self.defineCaptureHierarchy()
	#self.captureData()
	#self.exportFBX(os.path.join("D:\Build\FortressArtSource\Animation\Game\Enemies\Husk\FBX", "test.fbx"), 0, 53)

        
    def browse(self, *args):
	
	try:
	    directory = cmds.fileDialog2(dialogStyle=2, fm = 3)[0]
	    cmds.textField(self.widgets["textField"], edit = True, text = directory)
	
	except:
	    pass
	
	
    def update(self, *args):
	
	time = cmds.currentTime(q = True)
	cmds.currentTime(time + 1)
	cmds.currentTime(time)
	
	
    def process(self, *args):
	
	failed = []
	succeeds = []
	

	#new file
	cmds.file(new = True, force = True)
	
	#get the files in the directory
	directory = cmds.textField(self.widgets["textField"], q = True, text = True)
	filesInDir = os.listdir(directory)
	mayaFiles = []
	
	#create output log file
	outputLogPath = os.path.join(directory, "outputLog.txt")
	f = open(outputLogPath, 'w')
	f.close()
	
	
	for each in filesInDir:
	    
	    if each.rpartition(".")[2] == "mb":
		mayaFiles.append(each)
		
	data = "start of loop"
	#go through loop
	if len(mayaFiles) > 0:
	    
	    
	    #get number of maya files
	    numFiles = len(mayaFiles)
	    amount = 100/numFiles
	    newAmount = 0
	    
	    for mayaFile in mayaFiles:
		cmds.progressBar(self.widgets["progressBar"], edit = True, progress = newAmount + amount)
		
		try:
		    
		    #get name
		    fileName = mayaFile.rpartition(".")[0]
		    
		    #open file
		    directory = os.path.normpath(directory)
		    cmds.file(os.path.join(directory, mayaFile), open = True, force = True)
			
		    #update
		    self.update()
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 10)
		    
		    
		    #capture data
		    data = "define capture hierarchy"
		    self.defineCaptureHierarchy()
		    data = "capturing data"
		    framerange = self.captureData()
		    self.update()
		    data = "data captured"
		
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 20)
		    
		    #export data
		    data = "exporting locator data"
		    cmds.select("*_transferLoc")
		    directory = os.path.normpath(directory)
		    cmds.file(os.path.join(directory, fileName + "_captureData.mb"), type = "mayaBinary", pr = True, es = True, force = True, prompt = False)
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 30)
		    
			
		    
		    #new file
		    cmds.file(new = True, force = True)
		    self.update()
			
		    #add husk
		    data = "adding husk rig to new scene"
		    huskFile = self.mayaToolsDir + "/General/ART/Projects/Fortnite/AnimRigs/Husk_Base.mb"
		    
		    #reference the rig file
		    cmds.file(huskFile, r = True, type = "mayaBinary", loadReferenceDepth = "all", namespace = "Husk_Base", options = "v=0")
		    self.update()
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 40)
		    
		    #clear selection and fit view
		    cmds.select(clear = True)
		    cmds.viewFit()
		    panels = cmds.getPanel(type = 'modelPanel')
		    
		    
		    #turn on smooth shading
		    for panel in panels:
			editor = cmds.modelPanel(panel, q = True, modelEditor = True)
			cmds.modelEditor(editor, edit = True, displayAppearance = "smoothShaded", displayTextures = True, textures = True )		    
				
		    self.update()
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 50)
		    
		    #turn on FK spine
		    cmds.setAttr("Husk_Base:Rig_Settings.spine_ik", 0)
		    cmds.setAttr("Husk_Base:Rig_Settings.spine_fk", 1)
		    self.update()
		    
		    #setup scene
		    cmds.currentUnit(time = 'ntsc')
		    cmds.playbackOptions(min = 0, max = 100, animationStartTime = 0, animationEndTime = 100)
		    cmds.currentTime(0)		
		    self.update()
		    
		    #import data
		    cmds.file(os.path.join(directory, fileName + "_captureData.mb"), i = True, prompt = False, namespace = ":", force = True)
		    self.update()
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 60)
			    
		    #set frame range
		    cmds.select("*_transferLoc")
		    start = cmds.findKeyframe(which = "first")
		    end = cmds.findKeyframe(which = "last")
		    cmds.playbackOptions(min = framerange[0], max = framerange[1], animationStartTime = framerange[0], animationEndTime = framerange[1])
		    self.update()
		    
		    #zero locs/retarget
		    #turn autokey off
		    cmds.autoKeyframe( state=False )	
		    
		    #zero out locators
		    data = "zeroing out locators"
		    self.zeroLocs()
		    data = "zeroed out locators"
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 70)
		    data = "updating progress after zeroing locs"
		    
		    #transfer data
		    data = "starting retarget"
		    self.retarget()
		    data = "finished retarget"
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 80)
		    data = "updated progress bar after retarget"
		    self.update()
		    data = "updated scene after retarget"
				
			    
		    #reference original file
		    data = "importing old scene as reference"
		    nodes = cmds.file(os.path.join(directory, mayaFile), r = True, type = "mayaBinary", loadReferenceDepth = "all", mergeNamespacesOnClash = True, namespace = ":", options = "v=0", rnn = True)
		    data = "referenced old scene"
		    self.update()
		    data = "imported reference, updated scene"
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 90)
		    
		    
		    #change materials on original file
		    try:
			data = "assign material"
			shader = cmds.shadingNode("lambert", asShader = True, name =  "originalHusk_material")
			cmds.setAttr(shader + ".color", 0, 0.327258, 1, type = 'double3')
			cmds.setAttr(shader + ".incandescence", .266011, .266011, .266011, type = 'double3')
			data = "assigned shader"
		    
		    except:
			pass
		    
		    
		    
		    for node in nodes:
			try:
			    if cmds.nodeType(node) in ["phong", "lambert"]:
				
				connections = cmds.listConnections(node)
				for connection in connections:
				    if cmds.nodeType(connection) == "shadingEngine":
					subConnections = cmds.listConnections(connection)
					geo = []
					for conn in subConnections:
					    if cmds.nodeType(conn) == "transform":
						geo.append(conn)
						
					for geo in geo:
					    cmds.select(geo)
					    cmds.hyperShade(assign = shader)
					    
				cmds.delete(node)
				
			except:
			    pass
			
			    
			    
		    
		    
    
		    cmds.select(clear = True)
		    self.update()
		    
		    #save new maya file
		    newPath = os.path.join(directory, "Retargeted")
		    if not os.path.exists(newPath):
			os.makedirs(newPath)
			
		    newFilePath = os.path.join(newPath, fileName + ".mb")
		    
		    cmds.file(rename = newFilePath)
		    cmds.file(f = True, save = True, options = "v = 0", type = "mayaBinary")
    
		    #export FBX
		    fbxPath = os.path.join(directory, "FBX")
		    if not os.path.exists(fbxPath):
			os.makedirs(fbxPath)
			
		    self.exportFBX(os.path.join(fbxPath, fileName + ".fbx"), framerange[0], framerange[1])
			
		    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 100)
		    
		    #remove capture data file
		    os.remove(os.path.join(directory, fileName + "_captureData.mb"))
		    
		    
		    #write to output log
		    f = open(outputLogPath, 'a')
		    f.write(mayaFile + " : SUCCESS\n")
		    f.close()
		    
		    data = "completed all actions"
		    
		    
		
		except:
		    f = open(outputLogPath, 'a')
		    f.write(mayaFile + " : FAILED   (" + data + ")\n")
		    f.close()
		    
		    
		#increment new amount
		newAmount = newAmount + amount		
		
	#new scene
	cmds.file(new = True, force = True)
	
	#delete UI
	cmds.deleteUI(self.widgets["window"])
	
	#confirm message
	cmds.confirmDialog(title = "Complete!", message = "Operation is complete! Please check output log for detauls")
	
	
		



	
    def captureData(self, *args):

	#get frame range
	start = cmds.playbackOptions(q = True, min = True)
	end = cmds.playbackOptions(q = True, max = True)
	
	#set layers
	try:
	    layers = cmds.ls(type = "animLayer")
	    
	    if layers != None:
		newLayers = []
		for layer in layers:
		    if layer != "BaseAnimation":
			newLayers.append(layer)
		
		if newLayers != None:
		    for layer in newLayers:
			cmds.bakeResults(removeBakedAttributeFromLayer = True, dic = False, resolveWithoutLayer = ["BaseAnimation", layer], time = (start, end))
			cmds.delete(layer)
			
	except:
	    pass
	
	    
	self.update()
	
	#create arrays
	constraints = []        

        #turn autokey off
	cmds.autoKeyframe( state=False )
	
        #zero out rig
	for control in self.captureControls:
	    
	    if cmds.objExists("*:" + control):
		#set a keyframe on first frame for safety
		cmds.setKeyframe("*:" + control)
		attrs = cmds.listAttr("*:" + control, keyable = True)
		for attr in attrs:
		    if attr in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ"]:
			cmds.setAttr("*:" + control + "." + attr, 0)
			    
			    
	    if cmds.objExists(control):
		#set a keyframe on first frame for safety
		cmds.setKeyframe(control)
		attrs = cmds.listAttr(control, keyable = True)
		for attr in attrs:
		    if attr in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ"]:
			cmds.setAttr(control + "." + attr, 0)
		    
		
	#create and constrain locators
	for control in self.captureControls:
	    if cmds.objExists("*:" + control):
		#create the locator
		locator = cmds.spaceLocator(name = control + "_transferLoc")[0]
		
		#constrain to the control
		constraint = cmds.parentConstraint("*:" + control, locator)[0]
		cmds.delete(constraint)
		
		#create hierarchy
		try:
		    parent = self.captureDict.get(control)		
		    if cmds.objExists(parent + "_transferLoc"):
			cmds.parent(locator, parent + "_transferLoc")
			
		except:
		    pass
    
		#contrain to control
		cmds.makeIdentity(locator, t = 1, r = 1, s = 1, apply = True)
		constraint = cmds.parentConstraint("*:" + control, locator, mo = True)[0]
		constraints.append(constraint)
		
	    if cmds.objExists(control):
		#create the locator
		locator = cmds.spaceLocator(name = control + "_transferLoc")[0]
		
		#constrain to the control
		constraint = cmds.parentConstraint(control, locator)[0]
		cmds.delete(constraint)
		
		#create hierarchy
		try:
		    parent = self.captureDict.get(control)		
		    if cmds.objExists(parent + "_transferLoc"):
			cmds.parent(locator, parent + "_transferLoc")
			
		except:
		    pass
    
		#contrain to control
		cmds.makeIdentity(locator, t = 1, r = 1, s = 1, apply = True)
		constraint = cmds.parentConstraint(control, locator, mo = True)[0]
		constraints.append(constraint)
		
		
	#knees
	for control in ["Result_b_MF_Calf_L", "Result_b_MF_Calf_R"]:
	    
	    if cmds.objExists("*:" + control):
		#create the locator
		locator = cmds.spaceLocator(name = control + "_transferLoc")[0]
		
		#constrain to the control
		constraint = cmds.parentConstraint("*:" + control, locator)[0]
		cmds.delete(constraint)	
		
		#contrain to control
		cmds.makeIdentity(locator, t = 1, r = 1, s = 1, apply = True)
		constraint = cmds.parentConstraint("*:" + control, locator, mo = True)[0]
		constraints.append(constraint)
		
	    if cmds.objExists(control):
		#create the locator
		locator = cmds.spaceLocator(name = control + "_transferLoc")[0]
		
		#constrain to the control
		constraint = cmds.parentConstraint(control, locator)[0]
		cmds.delete(constraint)	
		
		#contrain to control
		cmds.makeIdentity(locator, t = 1, r = 1, s = 1, apply = True)
		constraint = cmds.parentConstraint(control, locator, mo = True)[0]
		constraints.append(constraint)	    		



	#get frame range
	cmds.select(clear = True)
	for ctrl in self.captureControls:
	    if cmds.objExists("*:" + ctrl):
		cmds.select("*:" + ctrl, add = True)
	    
	startFrame = cmds.findKeyframe(which = "first")
	endFrame = cmds.findKeyframe(which = "last")
	
	#update scene
	time = cmds.currentTime(q = True)
	cmds.currentTime(time + 1)
	cmds.currentTime(time)
	
	
	#Begin Capture
	for i in range(int(startFrame), int(endFrame + 1)):
	    
	    cmds.currentTime(i)
	    
	    #capture data
	    for control in self.captureControls:
		if cmds.objExists("*:" + control):
		    locator = control + "_transferLoc"
		    
		    #find the keyframes of the given control
		    attrs = cmds.listAttr("*:" + control, keyable = True)
		    for attr in attrs:
			keys = cmds.keyframe("*:" + control, at = attr, q = True)
			
	
			if keys != None:
			    if float(i) in keys:
				
				#set a keyframe on the locator for attrs that exist on the locator
				if cmds.objExists(locator + "." + attr):
				    
				    #set blendParent back
				    if cmds.objExists(locator + ".blendParent1"):
					cmds.setAttr(locator + ".blendParent1", 1)
					cmds.setKeyframe(locator + ".blendParent1", t = (float(i)))
					
				    #if the attr is a custom attr..
				    if attr not in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ", "scaleX", "scaleY", "scaleZ", "visibility" ]:
					
					#get the key tangents
					lock = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, l = True)[0]
					inTanType = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, itt = True)[0]
					outTanType = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, ott = True)[0]				
	
					
					value = cmds.getAttr("*:" + control + "." + attr)
					cmds.setAttr(locator + "." + attr, value)
					cmds.setKeyframe(locator, time = (float(i)), itt = "linear", ott = "linear")
					cmds.keyTangent(locator, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)
					
				    else:
					
					#get the key tangents
					lock = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, l = True)[0]
					inTanType = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, itt = True)[0]
					outTanType = cmds.keyTangent("*:" + control + "." + attr, time = (float(i), float(i)),  q = True, ott = True)[0]
				
					cmds.setKeyframe(locator, time = (float(i)), itt = "linear", ott = "linear")
					cmds.keyTangent(locator, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)
				    
				    
				#IF THE ATTR DOES NOT EXIST, CREATE IT ON THE LOCATOR
				else:
				    
				    #find the attr type and add it to the locator
				    attrType = cmds.getAttr("*:" + control + "." + attr, type = True)
				    
				    if attrType == "enum":
					enumString = cmds.attributeQuery(attr, node = "*:" + control, listEnum = True)[0]
					cmds.select(locator)
					cmds.addAttr(longName= attr, at = attrType, enumName = enumString, keyable = True)				
					
				    if attrType != "enum":
					cmds.select(locator)
					cmds.addAttr(longName= attr, at = attrType, keyable = True)
				    
				    
				    #get the value of the keyframe, set the value on the locator, and key
				    value = cmds.getAttr("*:" + control + "." + attr)
				    cmds.setAttr(locator + "." + attr, value)
				    cmds.setKeyframe(locator + "." + attr, time = (float(i)))
		
		
		
		
		
		
		if cmds.objExists(control):
		    locator = control + "_transferLoc"
		    
		    #find the keyframes of the given control
		    attrs = cmds.listAttr(control, keyable = True)
		    for attr in attrs:
			keys = cmds.keyframe(control, at = attr, q = True)
			
	
			if keys != None:
			    if float(i) in keys:
				
				#set a keyframe on the locator for attrs that exist on the locator
				if cmds.objExists(locator + "." + attr):
				    
				    #set blendParent back
				    if cmds.objExists(locator + ".blendParent1"):
					cmds.setAttr(locator + ".blendParent1", 1)
					cmds.setKeyframe(locator + ".blendParent1", t = (float(i)))
					
				    #if the attr is a custom attr..
				    if attr not in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ", "scaleX", "scaleY", "scaleZ", "visibility" ]:
					
					#get the key tangents
					lock = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, l = True)[0]
					inTanType = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, itt = True)[0]
					outTanType = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, ott = True)[0]				
	
					
					value = cmds.getAttr(control + "." + attr)
					cmds.setAttr(locator + "." + attr, value)
					cmds.setKeyframe(locator + "." + attr, time = (float(i)))
					cmds.keyTangent(locator, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)
					
				    else:
					
					#get the key tangents
					lock = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, l = True)[0]
					inTanType = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, itt = True)[0]
					outTanType = cmds.keyTangent(control + "." + attr, time = (float(i), float(i)),  q = True, ott = True)[0]
				
					cmds.setKeyframe(locator + "." + attr, t = (float(i)))
					cmds.keyTangent(locator, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)
				    
				    
				#IF THE ATTR DOES NOT EXIST, CREATE IT ON THE LOCATOR
				else:
				    
				    #find the attr type and add it to the locator
				    attrType = cmds.getAttr(control + "." + attr, type = True)
				    
				    if attrType == "enum":
					enumString = cmds.attributeQuery(attr, node = control, listEnum = True)[0]
					cmds.select(locator)
					cmds.addAttr(longName= attr, at = attrType, enumName = enumString, keyable = True)				
					
				    if attrType != "enum":
					cmds.select(locator)
					cmds.addAttr(longName= attr, at = attrType, keyable = True)
				    
				    
				    #get the value of the keyframe, set the value on the locator, and key
				    value = cmds.getAttr(control + "." + attr)
				    cmds.setAttr(locator + "." + attr, value)
				    cmds.setKeyframe(locator + "." + attr, time = (float(i)))	  
			
			
			
				    
	    #knees
	    for control in ["Result_b_MF_Calf_L", "Result_b_MF_Calf_R"]:
		
		locator = control + "_transferLoc"
		
		#set blendParent back
		if cmds.objExists(locator + ".blendParent1"):
		    cmds.setAttr(locator + ".blendParent1", 1)
		    cmds.setKeyframe(locator + ".blendParent1", t = (float(i)))		
		
		if control.find("_R") != -1:
		    cmds.setKeyframe("Result_b_MF_Calf_R_transferLoc", time = float(i))
		if control.find("_L") != -1:
		    cmds.setKeyframe("Result_b_MF_Calf_L_transferLoc", time = float(i))
		    
		    
	    #master
	    for control in ["Master_Anim"]:
		locator = control + "_transferLoc"
		
		if cmds.objExists(locator + ".blendParent1"):
		    cmds.setAttr(locator + ".blendParent1", 1)
		    cmds.setKeyframe(locator + ".blendParent1", t = (float(i)))
		    
		cmds.setKeyframe(locator, t = (float(i)))
		
	    
	    #settings
	    control = "Settings"
	    locator = control + "_transferLoc"
	    
	    if cmds.objExists("*:" + control):
		attrs = cmds.listAttr("*:" + control, keyable = True)
		
		for attr in attrs:
		    if cmds.objExists(locator + "." + attr):
			pass
		    
		    else:
			#find the attr type and add it to the locator
			attrType = cmds.getAttr("*:" + control + "." + attr, type = True)
			
			if attrType == "enum":
			    enumString = cmds.attributeQuery(attr, node = "*:" + control, listEnum = True)[0]
			    cmds.select(locator)
			    cmds.addAttr(longName= attr, at = attrType, enumName = enumString, keyable = True)				
			    
			if attrType != "enum":
			    cmds.select(locator)
			    cmds.addAttr(longName= attr, at = attrType, keyable = True)
			
			
			#get the value of the keyframe, set the value on the locator, and key
			value = cmds.getAttr("*:" + control + "." + attr)
			cmds.setAttr(locator + "." + attr, value)
			cmds.setKeyframe(locator + "." + attr, time = (float(i)))
			
		
	    
	    if cmds.objExists(control):
		
		attrs = cmds.listAttr(control, keyable = True)
		
		for attr in attrs:
		    if cmds.objExists(locator + "." + attr):
			pass
		    
		    else:
			#find the attr type and add it to the locator
			attrType = cmds.getAttr(control + "." + attr, type = True)
			
			if attrType == "enum":
			    enumString = cmds.attributeQuery(attr, node = control, listEnum = True)[0]
			    cmds.select(locator)
			    cmds.addAttr(longName= attr, at = attrType, enumName = enumString, keyable = True)				
			    
			if attrType != "enum":
			    cmds.select(locator)
			    cmds.addAttr(longName= attr, at = attrType, keyable = True)
			
			
			#get the value of the keyframe, set the value on the locator, and key
			value = cmds.getAttr(control + "." + attr)
			cmds.setAttr(locator + "." + attr, value)
			cmds.setKeyframe(locator + "." + attr, time = (float(i)))				
		
		

	    
	    
	#run euler filter on the locator curves
	cmds.select("*_transferLoc")
	cmds.filterCurve()
	
	#delete the constraints on the locators
	cmds.delete(constraints)
	
	
	return [start, end]
	
	
	
    def defineCaptureHierarchy(self, *args):
	
	#create the dictionary
	self.captureDict = {}
	

	#setup the hierarchy with control : parent
	self.captureDict["Root_Anim"] = "Master_Anim"
	self.captureDict["Body_Anim"] = "Master_Anim"
	self.captureDict["Hip_Anim"] = "Body_Anim"
	self.captureDict["Spine_01_Anim"] = "Body_Anim"
	self.captureDict["Spine_02_Anim"] = "Spine_01_Anim"
	self.captureDict["Spine_03_Anim"] = "Spine_02_Anim"
	self.captureDict["Neck_Anim"] = "Spine_03_Anim"
	self.captureDict["Head_Anim"] = "Neck_Anim"
	self.captureDict["Jaw_Anim"] = "Head_Anim"
	
	self.captureDict["L_Shoulder_Anim"] = "Spine_03_Anim"
	self.captureDict["FK_b_MF_UpperArm_L"] = "L_Shoulder_Anim"
	self.captureDict["FK_b_MF_Forearm_L"] = "FK_b_MF_UpperArm_L"
	self.captureDict["FK_b_MF_Hand_L"] = "FK_b_MF_Forearm_L"
	
	self.captureDict["R_Shoulder_Anim"] = "Spine_03_Anim"
	self.captureDict["FK_b_MF_UpperArm_R"] = "R_Shoulder_Anim"
	self.captureDict["FK_b_MF_Forearm_R"] = "FK_b_MF_UpperArm_R"
	self.captureDict["FK_b_MF_Hand_R"] = "FK_b_MF_Forearm_R"
	
	self.captureDict["FK_b_MF_Thigh_L"] = "Hip_Anim"
	self.captureDict["FK_b_MF_Calf_L"] = "FK_b_MF_Thigh_L"
	self.captureDict["FK_b_MF_Foot_L"] = "FK_b_MF_Calf_L"
	self.captureDict["FK_b_MF_Toe_L"] = "FK_b_MF_Foot_L"
	
	
	self.captureDict["FK_b_MF_Thigh_R"] = "Hip_Anim"
	self.captureDict["FK_b_MF_Calf_R"] = "FK_b_MF_Thigh_R"
	self.captureDict["FK_b_MF_Foot_R"] = "FK_b_MF_Calf_R"
	self.captureDict["FK_b_MF_Toe_R"] = "FK_b_MF_Foot_R"	
	
	self.captureDict["L_Thumb_1_Anim"] = "FK_b_MF_Hand_L"
	self.captureDict["L_Thumb_2_Anim"] = "L_Thumb_1_Anim"
	self.captureDict["L_Thumb_3_Anim"] = "L_Thumb_2_Anim"
	
	self.captureDict["L_Index_1_Anim"] = "FK_b_MF_Hand_L"
	self.captureDict["L_Index_2_Anim"] = "L_Index_1_Anim"
	self.captureDict["L_Index_3_Anim"] = "L_Index_2_Anim"
	
	self.captureDict["L_Middle_1_Anim"] = "FK_b_MF_Hand_L"
	self.captureDict["L_Middle_2_Anim"] = "L_Middle_1_Anim"
	self.captureDict["L_Middle_3_Anim"] = "L_Middle_2_Anim"
	
	self.captureDict["L_Ring_1_Anim"] = "FK_b_MF_Hand_L"
	self.captureDict["L_Ring_2_Anim"] = "L_Ring_1_Anim"
	self.captureDict["L_Ring_3_Anim"] = "L_Ring_2_Anim"
	
	self.captureDict["L_Pinky_1_Anim"] = "FK_b_MF_Hand_L"
	self.captureDict["L_Pinky_2_Anim"] = "L_Pinky_1_Anim"
	self.captureDict["L_Pinky_3_Anim"] = "L_Pinky_2_Anim"
	
	
	self.captureDict["R_Thumb_1_Anim"] = "FK_b_MF_Hand_R"
	self.captureDict["R_Thumb_2_Anim"] = "R_Thumb_1_Anim"
	self.captureDict["R_Thumb_3_Anim"] = "R_Thumb_2_Anim"
	
	self.captureDict["R_Index_1_Anim"] = "FK_b_MF_Hand_R"
	self.captureDict["R_Index_2_Anim"] = "R_Index_1_Anim"
	self.captureDict["R_Index_3_Anim"] = "R_Index_2_Anim"
	
	self.captureDict["R_Middle_1_Anim"] = "FK_b_MF_Hand_R"
	self.captureDict["R_Middle_2_Anim"] = "R_Middle_1_Anim"
	self.captureDict["R_Middle_3_Anim"] = "R_Middle_2_Anim"
	
	self.captureDict["R_Ring_1_Anim"] = "FK_b_MF_Hand_R"
	self.captureDict["R_Ring_2_Anim"] = "R_Ring_1_Anim"
	self.captureDict["R_Ring_3_Anim"] = "R_Ring_2_Anim"
	
	self.captureDict["R_Pinky_1_Anim"] = "FK_b_MF_Hand_R"
	self.captureDict["R_Pinky_2_Anim"] = "R_Pinky_1_Anim"
	self.captureDict["R_Pinky_3_Anim"] = "R_Pinky_2_Anim"
	
	
	
    def retarget(self, *args):
	
	#setup list of anim controls (eventaully, this should be done through a UI
	
	self.animControls = ["Husk_Base:Body_Shapes", "Husk_Base:Head_Shapes", "Husk_Base:Rig_Settings", "Husk_Base:body_anim", "Husk_Base:hip_anim", "Husk_Base:master_anim", "Husk_Base:root_anim", "Husk_Base:spine_01_anim", "Husk_Base:spine_02_anim", "Husk_Base:spine_03_anim", "Husk_Base:neck_01_fk_anim", 
	                     "Husk_Base:head_fk_anim", "Husk_Base:jaw_anim", "Husk_Base:fk_thigh_l_anim", "Husk_Base:fk_calf_l_anim", "Husk_Base:fk_foot_l_anim", "Husk_Base:fk_ball_l_anim", "Husk_Base:ik_foot_anim_l", 
	                     "Husk_Base:fk_thigh_r_anim", "Husk_Base:fk_calf_r_anim", "Husk_Base:fk_foot_r_anim", "Husk_Base:fk_ball_r_anim", "Husk_Base:ik_foot_anim_r", "Husk_Base:clavicle_l_anim", "Husk_Base:fk_arm_l_anim",
	                     "Husk_Base:fk_elbow_l_anim", "Husk_Base:fk_wrist_l_anim", "Husk_Base:ik_elbow_l_anim", "Husk_Base:ik_wrist_l_anim", "Husk_Base:clavicle_r_anim", "Husk_Base:fk_arm_r_anim", "Husk_Base:fk_elbow_r_anim",
	                     "Husk_Base:fk_wrist_r_anim", "Husk_Base:ik_elbow_r_anim", "Husk_Base:ik_wrist_r_anim", "Husk_Base:thumb_finger_fk_ctrl_1_l", "Husk_Base:thumb_finger_fk_ctrl_2_l", "Husk_Base:thumb_finger_fk_ctrl_3_l",
	                     "Husk_Base:index_finger_fk_ctrl_1_l", "Husk_Base:index_finger_fk_ctrl_2_l", "Husk_Base:index_finger_fk_ctrl_3_l", "Husk_Base:middle_finger_fk_ctrl_1_l", "Husk_Base:middle_finger_fk_ctrl_2_l", "Husk_Base:middle_finger_fk_ctrl_3_l",
	                     "Husk_Base:ring_finger_fk_ctrl_1_l", "Husk_Base:ring_finger_fk_ctrl_2_l", "Husk_Base:ring_finger_fk_ctrl_3_l", "Husk_Base:pinky_finger_fk_ctrl_1_l", "Husk_Base:pinky_finger_fk_ctrl_2_l", "Husk_Base:pinky_finger_fk_ctrl_3_l",
	                     "Husk_Base:thumb_finger_fk_ctrl_1_r", "Husk_Base:thumb_finger_fk_ctrl_2_r", "Husk_Base:thumb_finger_fk_ctrl_3_r", "Husk_Base:index_finger_fk_ctrl_1_r", "Husk_Base:index_finger_fk_ctrl_2_r", "Husk_Base:index_finger_fk_ctrl_3_r",
	                     "Husk_Base:middle_finger_fk_ctrl_1_r", "Husk_Base:middle_finger_fk_ctrl_2_r", "Husk_Base:middle_finger_fk_ctrl_3_r", "Husk_Base:ring_finger_fk_ctrl_1_r", "Husk_Base:ring_finger_fk_ctrl_2_r", "Husk_Base:ring_finger_fk_ctrl_3_r", 
	                     "Husk_Base:pinky_finger_fk_ctrl_1_r", "Husk_Base:pinky_finger_fk_ctrl_2_r", "Husk_Base:pinky_finger_fk_ctrl_3_r"]

	#constrain the controls to the locators in the files
	
	# # core # #
	cmds.parentConstraint("Body_Anim_transferLoc", "Husk_Base:body_anim", mo = True)
	cmds.parentConstraint("Hip_Anim_transferLoc", "Husk_Base:hip_anim", mo = True)
	cmds.parentConstraint("Master_Anim_transferLoc", "Husk_Base:master_anim", mo = True)
	cmds.parentConstraint("Root_Anim_transferLoc", "Husk_Base:root_anim", mo = True)
	cmds.orientConstraint("Spine_01_Anim_transferLoc", "Husk_Base:spine_01_anim", mo = True)
	cmds.orientConstraint("Spine_02_Anim_transferLoc", "Husk_Base:spine_02_anim", mo = True)
	cmds.orientConstraint("Spine_03_Anim_transferLoc", "Husk_Base:spine_03_anim", mo = True)
	cmds.orientConstraint("Neck_Anim_transferLoc", "Husk_Base:neck_01_fk_anim", mo = True)
	cmds.orientConstraint("Head_Anim_transferLoc", "Husk_Base:head_fk_anim", mo = True)
	
	try:
	    cmds.parentConstraint("Jaw_Anim_transferLoc", "Husk_Base:jaw_anim", mo = True)
	except:
	    pass
		
	
	
	# # legs # #
	cmds.orientConstraint("FK_b_MF_Thigh_L_transferLoc", "Husk_Base:fk_thigh_l_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Calf_L_transferLoc", "Husk_Base:fk_calf_l_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Foot_L_transferLoc", "Husk_Base:fk_foot_l_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Toe_L_transferLoc", "Husk_Base:fk_ball_l_anim", mo = True)
	cmds.pointConstraint("Left_Foot_Anim_transferLoc", "Husk_Base:ik_foot_anim_l")
	cmds.orientConstraint("Left_Foot_Anim_transferLoc", "Husk_Base:ik_foot_anim_l", mo = True)
	
	
	cmds.orientConstraint("FK_b_MF_Thigh_R_transferLoc", "Husk_Base:fk_thigh_r_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Calf_R_transferLoc", "Husk_Base:fk_calf_r_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Foot_R_transferLoc", "Husk_Base:fk_foot_r_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Toe_R_transferLoc", "Husk_Base:fk_ball_r_anim", mo = True)
	cmds.pointConstraint("Right_Foot_Anim_transferLoc", "Husk_Base:ik_foot_anim_r")
	cmds.orientConstraint("Right_Foot_Anim_transferLoc", "Husk_Base:ik_foot_anim_r", mo = True)
	
	
	# # arms # #
	cmds.parentConstraint("L_Shoulder_Anim_transferLoc", "Husk_Base:clavicle_l_anim", mo = True, sr = ["x", "y", "z"])
	cmds.orientConstraint("FK_b_MF_UpperArm_L_transferLoc", "Husk_Base:fk_arm_l_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Forearm_L_transferLoc", "Husk_Base:fk_elbow_l_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Hand_L_transferLoc", "Husk_Base:fk_wrist_l_anim", mo = True)
	cmds.pointConstraint("L_IK_Elbow_Anim_transferLoc", "Husk_Base:ik_elbow_l_anim", mo = True)
	cmds.parentConstraint("L_IK_Hand_Anim_transferLoc", "Husk_Base:ik_wrist_l_anim", mo = True)
	
	cmds.parentConstraint("R_Shoulder_Anim_transferLoc", "Husk_Base:clavicle_r_anim", mo = True, sr = ["x", "y", "z"])
	cmds.orientConstraint("FK_b_MF_UpperArm_R_transferLoc", "Husk_Base:fk_arm_r_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Forearm_R_transferLoc", "Husk_Base:fk_elbow_r_anim", mo = True)
	cmds.orientConstraint("FK_b_MF_Hand_R_transferLoc", "Husk_Base:fk_wrist_r_anim", mo = True)
	cmds.pointConstraint("R_IK_Elbow_Anim_transferLoc", "Husk_Base:ik_elbow_r_anim", mo = True)
	cmds.parentConstraint("R_IK_Hand_Anim_transferLoc", "Husk_Base:ik_wrist_r_anim", mo = True)
	
	# # fingers # #
	cmds.orientConstraint("L_Thumb_1_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_1_l", mo = True)
	cmds.orientConstraint("L_Thumb_2_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_2_l", mo = True)
	cmds.orientConstraint("L_Thumb_3_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_3_l", mo = True)
	
	cmds.orientConstraint("L_Index_1_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_1_l", mo = True)
	cmds.orientConstraint("L_Index_2_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_2_l", mo = True)
	cmds.orientConstraint("L_Index_3_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_3_l", mo = True)
	
	cmds.orientConstraint("L_Middle_1_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_1_l", mo = True)
	cmds.orientConstraint("L_Middle_2_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_2_l", mo = True)
	cmds.orientConstraint("L_Middle_3_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_3_l", mo = True)
	
	cmds.orientConstraint("L_Ring_1_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_1_l", mo = True)
	cmds.orientConstraint("L_Ring_2_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_2_l", mo = True)
	cmds.orientConstraint("L_Ring_3_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_3_l", mo = True)
	
	cmds.orientConstraint("L_Pinky_1_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_1_l", mo = True)
	cmds.orientConstraint("L_Pinky_2_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_2_l", mo = True)
	cmds.orientConstraint("L_Pinky_3_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_3_l", mo = True)
	
	
	
	cmds.orientConstraint("R_Thumb_1_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_1_r", mo = True)
	cmds.orientConstraint("R_Thumb_2_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_2_r", mo = True)
	cmds.orientConstraint("R_Thumb_3_Anim_transferLoc", "Husk_Base:thumb_finger_fk_ctrl_3_r", mo = True)
	
	cmds.orientConstraint("R_Index_1_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_1_r", mo = True)
	cmds.orientConstraint("R_Index_2_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_2_r", mo = True)
	cmds.orientConstraint("R_Index_3_Anim_transferLoc", "Husk_Base:index_finger_fk_ctrl_3_r", mo = True)
	
	cmds.orientConstraint("R_Middle_1_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_1_r", mo = True)
	cmds.orientConstraint("R_Middle_2_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_2_r", mo = True)
	cmds.orientConstraint("R_Middle_3_Anim_transferLoc", "Husk_Base:middle_finger_fk_ctrl_3_r", mo = True)
	
	cmds.orientConstraint("R_Ring_1_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_1_r", mo = True)
	cmds.orientConstraint("R_Ring_2_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_2_r", mo = True)
	cmds.orientConstraint("R_Ring_3_Anim_transferLoc", "Husk_Base:ring_finger_fk_ctrl_3_r", mo = True)
	
	cmds.orientConstraint("R_Pinky_1_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_1_r", mo = True)
	cmds.orientConstraint("R_Pinky_2_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_2_r", mo = True)
	cmds.orientConstraint("R_Pinky_3_Anim_transferLoc", "Husk_Base:pinky_finger_fk_ctrl_3_r", mo = True)
	
	
    
	#update scene
	time = cmds.currentTime(q = True)
	cmds.currentTime(time + 1)
	cmds.currentTime(time)
	
    
	
	#Hide all the things
	panels = cmds.getPanel(type = 'modelPanel')
	for panel in panels:
	    editor = cmds.modelPanel(panel, q = True, modelEditor = True)
	    
	    try:
		cmds.modelEditor(editor, edit = True, interactive = False, displayTextures = False, textures = False, allObjects = False )
		
	    except:
		pass	
	
	
	#setup IK knee solve
	startPointR = cmds.xform("Result_b_MF_Calf_R_transferLoc", q = True, ws = True, t = True)
	endPointR = cmds.xform("Husk_Base:calf_r", q = True, ws = True, t = True)
	distR = cmds.distanceDimension( sp=(startPointR[0],startPointR[1],startPointR[2]), ep=(endPointR[0], endPointR[1], endPointR[2]) )
	distRParent = cmds.listRelatives(distR, parent = True)[0]
	
	locsR = cmds.listConnections(distR)
	startLocR = locsR[0]
	endLocR = locsR[1]
	
	cmds.pointConstraint("Result_b_MF_Calf_R_transferLoc", startLocR)
	cmds.pointConstraint("Husk_Base:calf_r", endLocR)
	

	
	startPointL = cmds.xform("Result_b_MF_Calf_L_transferLoc", q = True, ws = True, t = True)
	endPointL = cmds.xform("Husk_Base:calf_l", q = True, ws = True, t = True)
	distL = cmds.distanceDimension( sp=(startPointL[0],startPointL[1],startPointL[2]), ep=(endPointL[0], endPointL[1], endPointL[2]) )
	distLParent = cmds.listRelatives(distL, parent = True)[0]
	
	locsL = cmds.listConnections(distL)
	startLocL = locsL[0]
	endLocL = locsL[1]
	
	cmds.pointConstraint("Result_b_MF_Calf_L_transferLoc", startLocL)
	cmds.pointConstraint("Husk_Base:calf_l", endLocL)
	

	
	
	#get start and end frame
	cmds.select("*_transferLoc")
	startFrame = cmds.findKeyframe(which = "first")
	endFrame = cmds.findKeyframe(which = "last")
	

	
	
	#go through each frame, check controls for keys on the current frame
	for i in range(int(startFrame), int(endFrame + 1)):
	    
	    #set the current time
	    cmds.currentTime(i)
	    
	    for control in self.animControls:
		
		targetLoc = None
		
		#get the target locator
		try:
		    connections = cmds.listConnections(control, type = "constraint")
				    
		    for connection in connections:
			target = cmds.listConnections(connection + ".target")[0]
			if target.find("transferLoc") != -1:
			    targetLoc = target
			    controlPairs.append([control, targetLoc])
			    
		except:
		    pass
		
		#get attrs
		if targetLoc != None:
		    attrs = cmds.listAttr(targetLoc, keyable = True)
		    
		    #go through each attr to get keys
		    for attr in attrs:
			keys = cmds.keyframe(targetLoc, at = attr, q = True)
			
			if keys != None:
			    #if the current frame we are on is in the list of keys, transfer the data
			    if float(i) in keys:
				
				#set a keyframe on the locator for attrs that exist on the locator
				if cmds.objExists(control + "." + attr):
				    
				    #if it is keyable
				    if cmds.getAttr(control + "." + attr, keyable = True):
				    
					#set blendParent back
					if cmds.objExists(control + ".blendParent1"):
					    cmds.setAttr(control + ".blendParent1", 1)
					    cmds.setKeyframe(control + ".blendParent1", t = (float(i)))
					    
					if cmds.objExists(control + ".blendPoint1"):
					    cmds.setAttr(control + ".blendPoint1", 1)
					    cmds.setKeyframe(control + ".blendPoint1", t = (float(i)))	
					    
					if cmds.objExists(control + ".blendOrient1"):
					    cmds.setAttr(control + ".blendOrient1", 1)
					    cmds.setKeyframe(control + ".blendOrient1", t = (float(i)))				    
					

					
					#if the attr is a custom attr..
					if attr not in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ", "scaleX", "scaleY", "scaleZ", "visibility"]:
					    pass
					     

					
					else:
					    #get the key tangents
					    lock = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), l = True)[0]
					    inTanType = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), itt = True)[0]
					    outTanType = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), ott = True)[0]
			
					    
					    try:
						cmds.setKeyframe(control, t = (float(i)))
						
						if inTanType != "fixed":
						    cmds.keyTangent(control, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)						
					    except:
						print control, attr
						pass
					    
				
			
			
				    #currently only the clavicles will fall in this category
				    else:
					
					#set blendParent back
					if cmds.objExists(control + ".blendParent1"):
					    cmds.setAttr(control + ".blendParent1", 1)
					    cmds.setKeyframe(control + ".blendParent1", t = (float(i)))
					    
					if cmds.objExists(control + ".blendPoint1"):
					    cmds.setAttr(control + ".blendPoint1", 1)
					    cmds.setKeyframe(control + ".blendPoint1", t = (float(i)))	
					    
					if cmds.objExists(control + ".blendOrient1"):
					    cmds.setAttr(control + ".blendOrient1", 1)
					    cmds.setKeyframe(control + ".blendOrient1", t = (float(i)))		
				    
    
    
					#CUSTOM ATTRS
					#if the attr is a custom attr..
					if attr not in ["translateX", "translateY", "translateZ", "rotateX", "rotateY", "rotateZ", "scaleX", "scaleY", "scaleZ", "visibility"]:
					    pass
					    


					else:
					    #get the key tangents
					    lock = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), l = True)[0]
					    inTanType = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), itt = True)[0]
					    outTanType = cmds.keyTangent(targetLoc + "." + attr, q = True, time = (float(i), float(i)), ott = True)[0]
				
					    
					    try:
						cmds.setKeyframe(control, t = (float(i)))
						
						if inTanType != "fixed":
						    cmds.keyTangent(control, edit = True, time = (float(i), float(i)), attribute = attr, absolute = True, lock = lock, itt = inTanType, ott = outTanType)
					    
					    except:
						print "Failed to Key " + control
						pass

				
				#CUSTOM ATTR MAPPING
				else:
				    if targetLoc == "Left_Foot_Anim_transferLoc":
					
					if attr == "roll":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:heel_ctrl_l.rotateZ", (val * -1)/2)
					    cmds.setKeyframe("Husk_Base:heel_ctrl_l.rotateZ")
					    
					if attr == "side":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:toe_tip_ctrl_l.rotateX", val)
					    cmds.setKeyframe("Husk_Base:toe_tip_ctrl_l.rotateX")
					    
					if attr == "lean":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:heel_ctrl_l.rotateY", val)
					    cmds.setKeyframe("Husk_Base:heel_ctrl_l.rotateY")
					    
					if attr == "toe_wiggle":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:toe_wiggle_ctrl_l.rotateZ", val * -1)
					    cmds.setKeyframe("Husk_Base:toe_wiggle_ctrl_l.rotateZ")
					    
					    
					
					
				    if targetLoc == "Right_Foot_Anim_transferLoc":
					
					if attr == "roll":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:heel_ctrl_r.rotateZ", (val * -1)/2)
					    cmds.setKeyframe("Husk_Base:heel_ctrl_r.rotateZ")
					    
					if attr == "side":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:toe_tip_ctrl_r.rotateX", val)
					    cmds.setKeyframe("Husk_Base:toe_tip_ctrl_r.rotateX")
					    
					if attr == "lean":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:heel_ctrl_r.rotateY", val)
					    cmds.setKeyframe("Husk_Base:heel_ctrl_r.rotateY")
					    
					if attr == "toe_wiggle":
					    val = cmds.getAttr(targetLoc + "." + attr)
					    cmds.setAttr("Husk_Base:toe_wiggle_ctrl_r.rotateZ", val * -1)
					    cmds.setKeyframe("Husk_Base:toe_wiggle_ctrl_r.rotateZ")
					    
					    


		#if there was no target loc, than we must be seeing a control that is a settings node
		else:
		    #custom mapping
		    if control == "Husk_Base:Rig_Settings":
			targetLoc = "Settings_transferLoc"
			
			attrs = cmds.listAttr(targetLoc, keyable = True)
			
			for attr in attrs:
			    if attr == "L_Arm_Mode":
				
				#get the value
				val = cmds.getAttr(targetLoc + "." + attr)
				#set the value
				if val == 0:
				    cmds.setAttr(control + ".lArmMode", 0)
				if val == 10:
				    cmds.setAttr(control + ".lArmMode", 1)
				    
				cmds.setKeyframe(control + ".lArmMode",  t = float(i))
				
				
			    if attr == "R_Arm_Mode":
			
				#get the value
				val = cmds.getAttr(targetLoc + "." + attr)
				#set the value
				if val == 0:
				    cmds.setAttr(control + ".rArmMode", 0)
				if val == 10:
				    cmds.setAttr(control + ".rArmMode", 1)
				    
				cmds.setKeyframe(control + ".rArmMode",  t = float(i))	
				
				
			    if attr == "L_Leg_Mode":
			
				#get the value
				val = cmds.getAttr(targetLoc + "." + attr)
				#set the value
				if val == 0:
				    cmds.setAttr(control + ".lLegMode", 0)
				if val == 10:
				    cmds.setAttr(control + ".lLegMode", 1)
				    
				cmds.setKeyframe(control + ".lLegMode",  t = float(i))	
				
				
			    
			    if attr == "R_Leg_Mode":
			
				#get the value
				val = cmds.getAttr(targetLoc + "." + attr)
				#set the value
				if val == 0:
				    cmds.setAttr(control + ".rLegMode", 0)
				if val == 10:
				    cmds.setAttr(control + ".rLegMode", 1)
				    
				cmds.setKeyframe(control + ".rLegMode",  t = float(i))	
				
		    
		    if control == "Husk_Base:Body_Shapes":
			targetLoc = "Morph_Anim_transferLoc"
			
			try:
			    attrs = cmds.listAttr(targetLoc, keyable = True)
			    
			    for attr in attrs:
				if attr == "Breath":
				    val = cmds.getAttr(targetLoc + "." + attr)
				    cmds.setAttr("Husk_Base:Body_Shapes.BreathIn", val)
				    cmds.setKeyframe("Husk_Base:Body_Shapes.BreathIn", t = float(i))
				    
				if attr == "L_Toe_Splay":
				    val = cmds.getAttr(targetLoc + "." + attr)
				    cmds.setAttr("Husk_Base:Body_Shapes.L_Toe_Splay", val)
				    cmds.setKeyframe("Husk_Base:Body_Shapes.L_Toe_Splay", t = float(i))
				    
				if attr == "R_Toe_Splay":
				    val = cmds.getAttr(targetLoc + "." + attr)
				    cmds.setAttr("Husk_Base:Body_Shapes.R_Toe_Splay", val)
				    cmds.setKeyframe("Husk_Base:Body_Shapes.R_Toe_Splay", t = float(i))
				    
			except:
			    pass
			
				
				
				
				
		    if control == "Husk_Base:Head_Shapes":
			targetLoc = "Morph_Anim_transferLoc"
			
			try:
			    attrs = cmds.listAttr(targetLoc, keyable = True)
			    
			    for attr in attrs:
				
				if attr == "L_Blink":
				    val = cmds.getAttr(targetLoc + "." + attr)
				    cmds.setAttr("Husk_Base:Head_Shapes.Left_Blink", val)
				    cmds.setKeyframe("Husk_Base:Head_Shapes.Left_Blink", t = float(i))
				    
				if attr == "R_Blink":
				    val = cmds.getAttr(targetLoc + "." + attr)
				    cmds.setAttr("Husk_Base:Head_Shapes.Right_Blink", val)
				    cmds.setKeyframe("Husk_Base:Head_Shapes.Right_Blink", t = float(i))
				    
			except:
			    pass
			
				
				
		
		
	    try:
		#Settings
		control = "Husk_Base:Rig_Settings"
		targetLoc = "Settings_transferLoc"
		    
		attrs = cmds.listAttr(targetLoc, keyable = True)
		    
		for attr in attrs:
		    if attr == "L_Arm_Mode":
			
			#get the value
			val = cmds.getAttr(targetLoc + "." + attr)
			#set the value
			if val == 0:
			    cmds.setAttr(control + ".lArmMode", 0)
			if val == 10:
			    cmds.setAttr(control + ".lArmMode", 1)
			    
			cmds.setKeyframe(control + ".lArmMode",  t = float(i))
			
			
		    if attr == "R_Arm_Mode":
		
			#get the value
			val = cmds.getAttr(targetLoc + "." + attr)
			#set the value
			if val == 0:
			    cmds.setAttr(control + ".rArmMode", 0)
			if val == 10:
			    cmds.setAttr(control + ".rArmMode", 1)
			    
			cmds.setKeyframe(control + ".rArmMode",  t = float(i))	
			
			
		    if attr == "L_Leg_Mode":
		
			#get the value
			val = cmds.getAttr(targetLoc + "." + attr)
			#set the value
			if val == 0:
			    cmds.setAttr(control + ".lLegMode", 0)
			if val == 10:
			    cmds.setAttr(control + ".lLegMode", 1)
			    
			cmds.setKeyframe(control + ".lLegMode",  t = float(i))	
			
			
		    
		    if attr == "R_Leg_Mode":
		
			#get the value
			val = cmds.getAttr(targetLoc + "." + attr)
			#set the value
			if val == 0:
			    cmds.setAttr(control + ".rLegMode", 0)
			if val == 10:
			    cmds.setAttr(control + ".rLegMode", 1)
			    
			cmds.setKeyframe(control + ".rLegMode",  t = float(i))
	    except:
		print "Settings Fail"
		pass


	    try:
		#IK KNEE SOLVE
		for control in ["Result_b_MF_Calf_L_transferLoc", "Result_b_MF_Calf_R_transferLoc"]:
		    
		    #solve the IK
		    if control.find("L_transferLoc") != -1:
			
			v1 = cmds.xform(startLocL, q = True, ws = True, t = True)
			v2 = cmds.xform(endLocL, q = True, ws = True, t = True)
			distanceL = abs(cmds.angleBetween( euler=True, v1=v1, v2=v2 )[1])  
    
			self.checkDistance(distL, distanceL, distanceL, startLocL, endLocL, "l")
			
		    if control.find("R_transferLoc") != -1:
			v1 = cmds.xform(startLocR, q = True, ws = True, t = True)
			v2 = cmds.xform(endLocR, q = True, ws = True, t = True)		    
			distanceR =  abs(cmds.angleBetween( euler=True, v1=v1, v2=v2 )[1])
			
			self.checkDistance(distR, distanceR, distanceR, startLocR, endLocR, "r")			
	    except:
		print "IK KNEE SOLVE FAIL"
			
	
	try:
	    #delete locators
	    cmds.select("*transferLoc")
	    cmds.delete()
	    
	    #delete distance tools
	    for obj in [distL, distR]:
		locs = cmds.listConnections(obj)
		
		for loc in locs:
		    cmds.delete(loc)
		    
		try:
		    cmds.delete(obj)
		except:
		    pass
		
	except:
	    print "scene clean fail"
	    pass
	
	    
	    
	    
	try:
	    #euler filter curves
	    for control in self.animControls:
		curves = cmds.listConnections(control, type = "animCurve", source = True, d = False)
		
		if curves != None:
		    for curve in curves:
			try:
			    cmds.filterCurve(curve)
			except:
			    pass
			
	    #euler everything
	    cmds.select(clear = True)
	    for control in self.animControls:
		cmds.select(control, add = True)
		
	    cmds.filterCurve()
	    
	except:
	    print "clean curves fail"
	    pass
	

	#Show all the things
	panels = cmds.getPanel(type = 'modelPanel')
	for panel in panels:
	    editor = cmds.modelPanel(panel, q = True, modelEditor = True)
	    
	    try:
		cmds.modelEditor(editor, edit = True, interactive = True, displayTextures = True, textures = True, allObjects = True )
		
	    except:
		pass	
	    
	    
	    
	
	
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def zeroLocs(self, *args):
	
	cmds.select("Master_Anim_transferLoc", hi = True)
	
	for loc in ["L_IK_Elbow_Anim_transferLoc", "L_IK_Hand_Anim_transferLoc", "R_IK_Elbow_Anim_transferLoc", "R_IK_Hand_Anim_transferLoc", "Left_Foot_Anim_transferLoc", "Right_Foot_Anim_transferLoc"]:
	    cmds.select(loc, add = True)
	    
	locators = cmds.ls(sl = True, type = "transform")
	
	for loc in locators:
	    cmds.setAttr(loc + ".tx", 0)
	    cmds.setAttr(loc + ".ty", 0)
	    cmds.setAttr(loc + ".tz", 0)
	    cmds.setAttr(loc + ".rx", 0)
	    cmds.setAttr(loc + ".ry", 0)
	    cmds.setAttr(loc + ".rz", 0)
	    


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def checkDistance(self, distanceNode, distanceAttr, originalValue,  startLoc, endLoc, side):
	
	
	if distanceAttr > 1:
	    currentAttr = cmds.getAttr("Husk_Base:ik_foot_anim_" + side + ".knee_twist")
	    
	    try:
		cmds.setAttr("Husk_Base:ik_foot_anim_" + side + ".knee_twist", currentAttr + 1)
		cmds.setKeyframe("Husk_Base:ik_foot_anim_" + side + ".knee_twist")
		
		v1 = cmds.xform(startLoc, q = True, ws = True, t = True)
		v2 = cmds.xform(endLoc, q = True, ws = True, t = True)		    
		newDist =  abs(cmds.angleBetween( euler=True, v1=v1, v2=v2 )[1])
				    
		
		
		if newDist < originalValue:
		    self.checkDistance(distanceNode, newDist, newDist, side)
		    
		if newDist > originalValue:
		    cmds.setAttr("Husk_Base:ik_foot_anim_" + side + ".knee_twist", currentAttr - 2)
		    cmds.setKeyframe("Husk_Base:ik_foot_anim_" + side + ".knee_twist")
		    
		    v1 = cmds.xform(startLoc, q = True, ws = True, t = True)
		    v2 = cmds.xform(endLoc, q = True, ws = True, t = True)		    
		    newDist =  abs(cmds.angleBetween( euler=True, v1=v1, v2=v2 )[1])		    
		    
		    self.checkDistance(distanceNode, newDist, newDist, side)
		    

	    except:
		print "FAILED :" + str(cmds.currentTime(q = True))
		pass
	    
	    
	    
    def exportFBX(self, path, startFrame, endFrame, *args):
	
	
	#get rotation interp
	options = cmds.optionVar(list = True)
	for op in options:
	    if op == "rotationInterpolationDefault":
		interp = cmds.optionVar(q = op)	
		
		
	cmds.optionVar(iv = ("rotationInterpolationDefault", 3))
	
	
	#get blendshapes
	blendshapes = ["Husk_Base:Head_Shapes", "Husk_Base:Body_Shapes"]
	if blendshapes != None:
	    cube = cmds.polyCube(name = "custom_export")[0]
	    
	    for shape in blendshapes:
		attrs = cmds.listAttr(shape, m = True, string = "weight")
		
		#counter for index
		i = 1
		
		for attr in attrs:
		    keys = cmds.keyframe( shape, attribute=attrs, query=True, timeChange = True )
		    keyValues = cmds.keyframe( shape, attribute=attrs, query=True, valueChange = True )
		    
		    morph = cmds.polyCube(name = attr)[0]
		    
		    if cmds.objExists("custom_export_shapes"):
			cmds.blendShape("custom_export_shapes", edit = True, t = (cube, i, morph, 1.0))
			
		    else:
			cmds.select([morph, cube], r = True)
			cmds.blendShape(name = "custom_export_shapes")
			cmds.select(clear = True)
			
		    cmds.delete(morph)
		    
		    #transfer keys from original to new morph
		    if keys != None:
			for i in range(int(len(keys))):
			    cmds.setKeyframe("custom_export_shapes." + attr, t = (keys[i]), v = keyValues[i])
			
		    i = i + 1	



	#duplicate the skeleton
	newSkeleton = cmds.duplicate("Husk_Base" + ":" + "root", un = False, ic = False)
	character = "Husk_Base"
	
	joints = []
	for each in newSkeleton:
	    if cmds.nodeType(each) != "joint":
		cmds.delete(each)
		
	    else:
		joints.append(each)
				
	
	constraints = []
	for joint in joints:
	    #do some checks to make sure that this is valid
	    parent = cmds.listRelatives(joint, parent = True)

	    if parent != None:
		if parent[0] in joints:
		    constraint = cmds.parentConstraint(character + ":" + parent[0] + "|" + character + ":" + joint, joint)[0]
		    constraints.append(constraint)
		    
	    else:
		#root bone?
		if joint == "root":
		    constraint = cmds.parentConstraint(character + ":" + joint, joint)[0]
		    constraints.append(constraint)			    


	
	cmds.select("root", hi = True)
	cmds.bakeResults(simulation = True, t = (startFrame, endFrame))
	cmds.delete(constraints)
			
	#run an euler filter
	cmds.select("root", hi = True)
	cmds.filterCurve()
		    
	#export selected
	#first change some fbx properties
	string = "FBXExportConstraints -v 1;"
	string += "FBXExportCacheFile -v 0;"
	mel.eval(string)
			
	cmds.select("root", hi = True)
	if cmds.objExists("custom_export"):
	    cmds.select("custom_export", add = True)

	    
	cmds.file(path, es = True, force = True, prompt = False, type = "FBX export")
	
	#clean scene
	cmds.delete("root")
	if cmds.objExists("custom_export"):
	    cmds.delete("custom_export")
	    
	#reset rotation interp
	cmds.optionVar(iv = ("rotationInterpolationDefault", interp))	