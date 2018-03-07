import maya.cmds as cmds
from functools import partial
import os, cPickle


class ART_LearningVideos():
    
    def __init__(self):
        
        #check to see if the UI exists, if so, delete
        if cmds.window("ART_LearningVideos_UI", exists = True):
            cmds.deleteUI("ART_LearningVideos_UI")
            
        self.widgets = {}
        
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()
            
            
        #create window
        self.widgets["window"] = cmds.window("ART_LearningVideos_UI", title = "Learning Videos", w = 350, h = 500, sizeable = False)
        
        #main layout
        self.widgets["mainLayout"] = cmds.columnLayout(adj = True)
        
        #banner 
        cmds.image(w = 350, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/artHelpBanner.bmp", parent = self.widgets["mainLayout"])
        
        
        #tab layout
        self.widgets["tabs"] = cmds.tabLayout(parent = self.widgets["mainLayout"])
        
        
        
        #Create Tabs
        
        #scroll layout [Tab 1]
        self.widgets["scrollLayout"] = cmds.scrollLayout(parent = self.widgets["tabs"], hst = 0, h = 500)
        
        #row column layout
        self.widgets["rowColLayout"] = cmds.rowColumnLayout(nc = 2, rs = [1, 5], cw = [(1, 100), (2, 215)], cat = [(1, "left", 5), (2, "both", 10)], parent = self.widgets["scrollLayout"])
        
        
        #scroll layout [Tab 2]
        self.widgets["scrollLayout2"] = cmds.scrollLayout(parent = self.widgets["tabs"], hst = 0, h = 500)
        
        #row column layout
        self.widgets["rowColLayout2"] = cmds.rowColumnLayout(nc = 2, rs = [1,5], cw = [(1, 100), (2, 215)], cat = [(1, "left", 5), (2, "both", 10)], parent = self.widgets["scrollLayout2"])        
        
        
        #RIGGING TAB
        
        #skeleton settings
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "skeleton_creation"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp2.bmp")
        
        #joint mover basics
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "skeleton_placement"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp3.bmp")
        
        #joint mover tools
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "joint_mover_tools"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp4.bmp")  
        
        #physique mode
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "customizing_your_mannequin"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp5.bmp")
        
        #deformation setup
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "deformation_setup"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp6.bmp")
        
        #Publish
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "publishing_your_character"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp7.bmp")
        
        #Editing
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "editing_your_character"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout"], image = self.mayaToolsDir + "/General/Icons/ART/rigHelp8.bmp")      
        
        
        
        
        
        #ANIMATION TAB
        
        #getting started
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_getting_started"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp1.bmp")      
        
        #rig overview
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_rig_overview"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp2.bmp")  
        
        #interface overview
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_ui_overview"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp3.bmp")  
        
        #import/export
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_import_export"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp4.bmp")  
        
        #space switching
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_space_switching"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp5.bmp")  
        
        #pose editor
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_pose_editor"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp6.bmp")   
        
        #matching and visibility
        cmds.symbolButton(w = 100, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/movie.bmp", c = partial(self.playMovie, "animation_matching"))
        cmds.image(w = 210, h = 100, parent = self.widgets["rowColLayout2"], image = self.mayaToolsDir + "/General/Icons/ART/animHelp7.bmp")   
        
        
        
        
        #edit tab names
        cmds.tabLayout(self.widgets["tabs"], edit = True, tabLabel = [(self.widgets["scrollLayout"], "Rigging"), (self.widgets["scrollLayout2"], "Animation")])
        #show the window
        cmds.showWindow(self.widgets["window"])
        
        
    
    def playMovie(self, movieFile, *args):
        
        moviePath = self.mayaToolsDir + "/General/ART/Help/LearningVideos/" + movieFile + ".wmv"
        cmds.launch(mov = moviePath)