import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os

class Fortnite_IkBonesBatcher():
    
    def __init__(self):
        
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()
            
            
        #check to see if window exists, if so, delete
        if cmds.window("fortniteIkBonesBatcher_UI", exists = True):
            cmds.deleteUI("fortniteIkBonesBatcher_UI")
            
        self.path = self.mayaToolsDir
        self.widgets = {}
        
        #create the window
        self.widgets["window"] = cmds.window("fortniteIkBonesBatcher_UI", w = 400, h = 170, mnb = False, mxb = False, title = "IK Bones Batcher", sizeable = False)
        
        
        #main layout
        self.widgets["main"] = cmds.formLayout(w = 400, h = 400)
        
        
        #widgets needed [textField for path, symbolButton for browse, button for confirm, button for cancel]
        self.widgets["pathField"] = cmds.textField(w = 320, h = 50, parent = self.widgets["main"])
        self.widgets["browse"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/browse.bmp", parent = self.widgets["main"], c = self.browse)
        self.widgets["accept"] = cmds.button(w = 180, h = 50, label = "PROCESS", parent = self.widgets["main"], c = self.process)
        self.widgets["cancel"] = cmds.button(w = 180, h = 50, label = "CANCEL", parent = self.widgets["main"], c = self.cancel)
        self.widgets["info"] = cmds.text(label = "")
        
        #place widgets
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["pathField"], "left", 10), (self.widgets["pathField"], "top", 10)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["browse"], "right", 10), (self.widgets["browse"], "top", 10)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["accept"], "left", 10), (self.widgets["accept"], "bottom", 10)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["cancel"], "right", 10), (self.widgets["cancel"], "bottom", 10)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["info"], "left", 10), (self.widgets["info"], "bottom", 80)])
        
        #show the window
        cmds.showWindow(self.widgets["window"])
        
        
    
    def cancel(self, *args):
        
        cmds.deleteUI(self.widgets["window"])
        
    
    def browse(self, *args):
        
        #get directory of fbx files
        self.motionDirectory = cmds.fileDialog2(dialogStyle=2, fm = 3, startingDirectory = self.path)[0]
        
        #edit text field to show directory chosen
        cmds.textField(self.widgets["pathField"], edit = True, text = self.motionDirectory)
        
        #find the fbx files
        files = os.listdir(self.motionDirectory)
        
        fbxFiles = []
        for file in files:
            if file.rpartition(".")[2].lower() == "fbx":
                fbxFiles.append(file)
                
        #display on the UI how many FBX files will be processed
        cmds.text(self.widgets["info"], edit = True, label = "FBX Files to process : " + str(len(fbxFiles)))
        
        #set starting path to be last accessed path
        self.path = self.motionDirectory
        
    
    def process(self, *args):
        
        
        files = os.listdir(self.motionDirectory)
        
        fbxFiles = []
        for file in files:
            if file.rpartition(".")[2].lower() == "fbx":
                fbxFiles.append(file)
                
        
        for file in fbxFiles:
            #open a new scene
            cmds.file(new = True, force = True)
            
            #import the fbx
	    string = "FBXImportMode -v \"add\";"
	    mel.eval(string)
	    cmds.file(self.motionDirectory + "/" + file, i = True, prompt = False, force = True)
	    
	    #ensure that the scene is in 30fps
	    cmds.currentUnit(time = 'ntsc')
	    cmds.playbackOptions(min = 0, max = 400, animationStartTime = 0, animationEndTime = 400)
	    cmds.currentTime(0)	    
	    
	    #set timerange
	    firstFrame = cmds.findKeyframe("pelvis", which = 'first')
	    lastFrame = cmds.findKeyframe("pelvis", which = 'last')
	    if lastFrame == firstFrame:
		lastFrame = lastFrame + 1
		
	    cmds.playbackOptions(min = firstFrame, max = lastFrame, animationStartTime = firstFrame, animationEndTime = lastFrame)


	    
	    if cmds.objExists("ik_foot_root") == False:
		#create the joints
		cmds.select(clear = True)
		ikFootRoot = cmds.joint(name = "ik_foot_root")
		cmds.select(clear = True)
		
	    
		cmds.select(clear = True)
		ikFootLeft = cmds.joint(name = "ik_foot_l")
		cmds.select(clear = True)
		
		cmds.select(clear = True)
		ikFootRight = cmds.joint(name = "ik_foot_r")
		cmds.select(clear = True)
		
		cmds.select(clear = True)
		ikHandRoot = cmds.joint(name = "ik_hand_root")
		cmds.select(clear = True)
		
		cmds.select(clear = True)
		ikHandGun = cmds.joint(name = "ik_hand_gun")
		cmds.select(clear = True)
		
		cmds.select(clear = True)
		ikHandLeft = cmds.joint(name = "ik_hand_l")
		cmds.select(clear = True)
		
		cmds.select(clear = True)
		ikHandRight = cmds.joint(name = "ik_hand_r")
		cmds.select(clear = True)
	    
	    
		#create hierarhcy
		cmds.parent(ikFootRoot, "root")
		cmds.parent(ikHandRoot, "root")
		cmds.parent(ikFootLeft, ikFootRoot)
		cmds.parent(ikFootRight, ikFootRoot)
		cmds.parent(ikHandGun, ikHandRoot)
		cmds.parent(ikHandLeft, ikHandGun)
		cmds.parent(ikHandRight, ikHandGun)
	    
	    
	    
            #constrain the joints
	    leftHandConstraint = cmds.parentConstraint("hand_l", "ik_hand_l")[0]
	    rightHandGunConstraint = cmds.parentConstraint("hand_r", "ik_hand_gun")[0]
	    rightHandConstraint = cmds.parentConstraint("hand_r", "ik_hand_r")[0]
	    leftFootConstraint = cmds.parentConstraint("foot_l", "ik_foot_l")[0]
	    rightFootConstraint = cmds.parentConstraint("foot_r", "ik_foot_r")[0]
            
            #bake
	    cmds.select(clear = True)
	    for bone in ["ik_hand_l", "ik_hand_gun", "ik_hand_r", "ik_foot_l", "ik_foot_r"]:
		cmds.select(bone, add = True)
		
	    cmds.bakeResults(simulation = True, time = (firstFrame, lastFrame))
            
            
            #re-export with same name
	    cmds.select("root", hi = True)
	    cmds.file(self.motionDirectory + "/" + file, es = True, force = True, prompt = False, type = "FBX export") 
	    
	    
	#operation complete
	cmds.confirmDialog(title = "Operation Complete!", message = "The batcher has finished processing all of the files.", button = ["High Five!"])
        
        
            
        
        
    
    