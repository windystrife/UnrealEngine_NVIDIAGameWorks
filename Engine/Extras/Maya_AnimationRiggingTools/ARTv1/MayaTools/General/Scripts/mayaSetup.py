import maya.cmds as cmds
import os
import sys
from functools import partial
import maya.mel as mel


    
def setupTools():
    
    path = cmds.internalVar(usd = True) + "mayaTools.txt"
    

    f = open(path, 'r')
    
    mayaToolsDir = f.readline()
    
    path = mayaToolsDir + "/General/Scripts"
    pluginPath = mayaToolsDir + "/General/Plugins"
    
    #look in sys.path to see if path is in sys.path. if not, add it
    if not path in sys.path:
        sys.path.append(path)
        
    #make sure MAYA_PLUG_IN_PATH has our plugin path
    pluginPaths = os.environ["MAYA_PLUG_IN_PATH"]
    pluginPaths = pluginPaths + ";" + pluginPath
    os.environ["MAYA_PLUG_IN_PATH"] = pluginPaths
        

    
    
    #setup menu item in main window
    import customMayaMenu as cmm
    cmm.customMayaMenu()

setupTools()
