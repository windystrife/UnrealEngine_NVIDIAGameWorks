import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os

class Fortnite_CoreMotionMaker():
    def __init__(self):	
    
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()
 
        #check to see if window exists, if so, delete
        if cmds.window("fortniteCoreMotionMakerUI", exists = True):
	    cmds.deleteUI("fortniteCoreMotionMakerUI")
        self.widgets = {}	self.motionDirectory = self.mayaToolsDir	self.corePoseFile = None		
        #build window
        self.widgets["window"] = cmds.window("fortniteCoreMotionMakerUI", w = 600, h = 380, title = "Fortnite Core Motion Maker", sizeable = False)
        
        #create the main layout
	self.widgets["topLevelLayout"] = cmds.columnLayout(w = 600)
        
        #create the widget layout
        self.widgets["widgetLayout"] = cmds.formLayout(w = 600, h = 320, parent =  self.widgets["topLevelLayout"])
        
        #widgets needed: textscrolllist for motion FBXs, load motion directory button, core pose textField, browse button next to that, processs button.
        self.widgets["motionList"] = cmds.textScrollList(w = 300, h = 220, ams = True)
        self.widgets["loadMotionButton"] = cmds.button(w = 300, h = 50, label = "LOAD MOTION DIRECTORY", c = self.loadMotionDirectory)
        self.widgets["corePose"] = cmds.textField(w = 220, h = 30, text = "")
        self.widgets["browseButton"] = cmds.symbolButton(w = 30, h = 30, image = self.mayaToolsDir + "/General/Icons/ART/browse.bmp", c = self.loadCorePose)
        self.widgets["processButton"] = cmds.button(w = 250, h = 50, label = "PROCESS", c = self.process)		self.widgets["labelDirectory"] = cmds.text(label = "Animation Files", font = "boldLabelFont", parent = self.widgets["widgetLayout"])		self.widgets["labelPose"] = cmds.text(label = "Core Pose", font = "boldLabelFont", parent = self.widgets["widgetLayout"])	    	    	 
        
        #place widgets		cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["motionList"], "top", 35),(self.widgets["motionList"], "left", 10)])
        cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["loadMotionButton"], "bottom", 10),(self.widgets["loadMotionButton"], "left", 10)])
        cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["corePose"], "top", 35),(self.widgets["corePose"], "left", 340)])
        cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["browseButton"], "top", 35),(self.widgets["browseButton"], "right", 10)])
        cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["processButton"], "bottom", 10),(self.widgets["processButton"], "right", 10)])		cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["labelDirectory"], "top", 10), (self.widgets["labelDirectory"], "left", 10)])		cmds.formLayout(self.widgets["widgetLayout"], edit = True, af = [(self.widgets["labelPose"], "top", 10), (self.widgets["labelPose"], "right", 200)])	    
        #show the window
        cmds.showWindow(self.widgets["window"])
    def loadMotionDirectory(self, *args):        # if the user has not defined a path, use the given tools location
	path = self.motionDirectory	    
	try:
	    self.motionDirectory = cmds.fileDialog2(dialogStyle=2, fm = 3, startingDirectory = path)[0]
	    self.inputDirectory = self.motionDirectory
	    filesInDir = os.listdir(self.motionDirectory)
	
	    for file in filesInDir:
		if file.partition(".")[2] == "fbx":
		    cmds.textScrollList(self.widgets["motionList"], edit = True, append = file.partition(".")[0])
	except:
	    cmds.warning("Operation Cancelled.")
	    return
	
    def loadCorePose(self, *args):
        if(self.corePoseFile == None):	    path = self.motionDirectory	else:	    path = self.corePoseFile.rpartition("/")[0] + "/"	    self.motionDirectory = path
	try:
	    self.corePoseFile = cmds.fileDialog2(fileFilter="*.fbx", dialogStyle=2, fm = 1, startingDirectory = path)[0]
	    cmds.textField(self.widgets["corePose"], edit = True, text = self.corePoseFile)	    self.motionDirectory = self.corePoseFile.rpartition("/")[0]
	except:
	    cmds.warning("Operation Cancelled.")
	    return
	
    
    def process(self, *args):
	#get selected motion files
	selected = cmds.textScrollList(self.widgets["motionList"], q = True, si = True)
	selectedIndex = cmds.textScrollList(self.widgets["motionList"], q = True, sii = True)	
	if selected == None:
	    cmds.warning("No items in list selected")
	    return
	    
	else:
	    for each in selected:
		#build the full path
		path = self.inputDirectory + "/" + each		print path
		#open a new file
		cmds.file(new = True, force = True)		
		#import FBX motion file
		string = "FBXImportMode -v \"add\";"
		mel.eval(string)
		print path + ".fbx"
		cmds.file(path + ".fbx", i = True, prompt = False, force = True)
	    
		#ensure that the scene is in 30fps
		cmds.currentUnit(time = 'ntsc')
		cmds.playbackOptions(min = 0, max = 100, animationStartTime = 0, animationEndTime = 100)
		cmds.currentTime(0)
	    
		#set timerange
		firstFrame = cmds.findKeyframe("pelvis", which = 'first')
		lastFrame = cmds.findKeyframe("pelvis", which = 'last')
		if lastFrame == firstFrame:
		    lastFrame = lastFrame + 1
		cmds.playbackOptions(min = firstFrame, max = lastFrame, animationStartTime = firstFrame, animationEndTime = lastFrame)
		#rename skeleton
		cmds.select("root", hi = True)
		joints = cmds.ls(sl = True, type = "joint")
		for joint in joints:
		    cmds.rename(joint, "motionSkel__" + joint)

		#import in core pose file
		poseFile = cmds.textField(self.widgets["corePose"], q = True, text = True)
		string = "FBXImportMode -v \"add\";"
		mel.eval(string)
		cmds.file(poseFile, i = True, prompt = False, force = True)
		#pointConstrain the spine1 to the motionSkel_pelvis
		const = cmds.pointConstraint("motionSkel__spine_01", "spine_01")
		cmds.select("spine_01", hi = True)
		cmds.bakeResults(simulation = True, time = (firstFrame, lastFrame))
		cmds.delete(const)
		#constrain original skeleton from spine 1 up to core pose file
		cmds.select("motionSkel__spine_01", hi = True)
		constrainJoints = cmds.ls(sl = True)
		
		for joint in constrainJoints:
		    cmds.orientConstraint(joint.partition("motionSkel__")[2], joint)		    cmds.pointConstraint(joint.partition("motionSkel__")[2], joint)
		#account for IK bones		cmds.parentConstraint("motionSkel__hand_l", "motionSkel__ik_hand_l")		cmds.parentConstraint("motionSkel__hand_r", "motionSkel__ik_hand_gun")		cmds.parentConstraint("motionSkel__hand_r", "motionSkel__ik_hand_r")		cmds.parentConstraint("motionSkel__foot_l", "motionSkel__ik_foot_l")		cmds.parentConstraint("motionSkel__foot_r", "motionSkel__ik_foot_r")
		#bake down
		cmds.select("motionSkel__root", hi = True)
		cmds.bakeResults(simulation = True, time = (firstFrame, lastFrame))
		cmds.delete("root")				cmds.select("motionSkel__root", hi = True)		cmds.delete(constraints = True)			
		
		#rename skeleton
		cmds.select("motionSkel__root", hi = True)
		joints = cmds.ls(sl = True, type = "joint")
		for joint in joints:
		    cmds.rename(joint, joint.partition("motionSkel__")[2])
		
		
		
		#export new fbx
		cmds.select("root", hi = True)		tempName = each + "_Core.fbx"
		fileName = self.inputDirectory + "/" + tempName + ".fbx"		
		cmds.file(fileName, es = True, force = True, prompt = False, type = "FBX export")
		
	
	#tell user complete
	cmds.confirmDialog(title = "Core Motion Maker", message= "Operation Complete!")