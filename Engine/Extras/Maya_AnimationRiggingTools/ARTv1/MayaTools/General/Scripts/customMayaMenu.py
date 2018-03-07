import maya.cmds as cmds
import maya.mel as mel
import os, cPickle
from functools import partial
import Tools.ART_CreateHydraulicRigs as hyd
import Tools.ART_ReverseAttrHookup as rev
import Tools.ART_OrientJointWithUp as ornt
import Tools.ART_MeshSpliter as split
import Tools.ART_RenameSuffix as suffix
import Tools.ART_ImportMultipleWeights as mulw
import Tools.ART_CopyWeightsTool as copy
import Tools.ART_SurfaceVertexSelector as surf
reload(mulw)
reload(surf)
reload(ornt)
reload(split)


# Use this snipped of code to update the UI without having to close Maya.
'''
cmds.deleteUI("epicMenu")
import customMayaMenu as menu
reload(menu)
menu.customMayaMenu()
'''
#

def customMayaMenu():
    gMainWindow = mel.eval('$temp1=$gMainWindow')


    menus = cmds.window(gMainWindow, q = True, menuArray = True)
    found = False
    for menu in menus:
        label = cmds.menu(menu, q = True, label = True)
        if label == "Epic Games":
            found = True

    if found == False:
        customMenu = cmds.menu("epicMenu", parent=gMainWindow, label = 'Epic Games', to=True)

        #tools path
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            mayaToolsDir = f.readline()
            f.close()	

        #ART
        cmds.menuItem(parent = customMenu, label = "Animation Rigging Toolset", bld = True, enable = False)
        cmds.menuItem(parent = customMenu, divider = True)
        cmds.menuItem(parent = customMenu, label = "Character Rig Creator", c = launchSkeletonBuilder)
        cmds.menuItem(parent = customMenu, label = "Edit Existing Character", c = editCharacter)
        cmds.menuItem(parent = customMenu, label = "Add Character For Animation", c = launchAddCharacter)
        cmds.menuItem(parent = customMenu, label = "Animation Interface", c = launchAnimUI)

        cmds.menuItem(parent = customMenu, label = "Settings", c = launchARTSettings)
        artHelp = cmds.menuItem(parent = customMenu, label = "Help", subMenu=True, to=True)
        cmds.menuItem(parent = artHelp, label = "Learning Videos", c = launchLearningVideos)
        cmds.menuItem(parent = artHelp, label = "Help Documentation", c = launchRigHelp)
        cmds.menuItem(parent = artHelp, label = "About", c = aboutARTTools)
        cmds.menuItem(parent = customMenu, divider = True)
        cmds.menuItem(parent = customMenu, label = "Misc. Tools", bld = True, enable = False)
        cmds.menuItem(parent = customMenu, divider = True)



        #PERFORCE
        p4Menu = cmds.menuItem(parent = customMenu, label = "Perforce", subMenu=True, to = True)
        cmds.menuItem("perforceSubmitMenuItem", parent = p4Menu, label = "Submit Current File", enable = False, c = p4Submit)
        cmds.menuItem("perforceAddAndSubmitMenuItem", parent = p4Menu, label = "Add and Submit Current File", enable = False, c = p4AddSubmit)
        cmds.menuItem("perforceCheckOutMenuItem", parent = p4Menu, label = "Check Out Current File", enable = False, c = p4CheckOut)
        cmds.menuItem("perforceFileHistoryMenuItem", parent = p4Menu, label = "Current File History", enable = False, c = p4GetHistory)
        cmds.menuItem("perforceGetLatestMenuItem", parent = p4Menu, label = "Get Latest Revision of Current File", enable = False, c = p4GetLatest)
        cmds.menuItem("perforceProjectList", parent = p4Menu, label = "Set Project", enable = False, subMenu = True, to=True)
        cmds.menuItem("perforceProject_New", parent = "perforceProjectList", label = "New Project", c = createNewProj)
        cmds.radioMenuItemCollection("perforceProjectRadioMenuCollection", parent = "perforceProjectList")
        cmds.menuItem(parent = customMenu, divider = True)

        #PROJECTS
        projectMenu = cmds.menuItem(parent = customMenu, label = "Projects", subMenu = True, to=True)

        #PROJECTS\FORTNITE    
        fortniteMenu = cmds.menuItem(parent = projectMenu, label = "Fortnite", subMenu = True, to = True)
        cmds.menuItem(parent = fortniteMenu, label = "Fortnite Core Motion Maker", c = fortniteCoreMotionMaker)
        cmds.menuItem(parent = fortniteMenu, label = "IK Bones Batcher", c = fortniteikBonesBatch)
        cmds.menuItem(parent = fortniteMenu, label = "Fortnite Character Editor", c = fortniteCharacterEditor)
        cmds.menuItem(parent = fortniteMenu, label = "Fortnite FK Spine", c = threeCtrlSpineFix)

        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(parent = fortniteMenu, label = "Husk Export Script", c = fortnite_huskExport)
        cmds.menuItem(parent = fortniteMenu, label = "HUSK IK Bones Batcher", c = fortniteHuskIkBonesBatch)
        cmds.menuItem(parent = fortniteMenu, label = "HUSK Animation UI", c = fortniteHuskUI)
        cmds.menuItem(parent = fortniteMenu, label = "HUSK Character Editor", c = fortniteHuskChooser)
        cmds.menuItem(parent = fortniteMenu, label = "HUSK FBX Batch Exporter", c = fortniteHuskFbxBatch)


        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(divider = True, parent = fortniteMenu)
        cmds.menuItem(parent = fortniteMenu, label = "Rigging: Add IK Bones to Export File", c = addFortniteIKBones)
        cmds.menuItem(parent = fortniteMenu, label = "Fortnite 3 Ctrl Spine", c = threeCtrlSpine)
        cmds.menuItem(parent = fortniteMenu, label = "Create Cine Placement Skeleton", c = cineExportSkeleton, ann = "To use, select the root control of anything you want exported, as well as cameras. Then export the created placement_root and the cine_placement_export_geo.")
        cmds.menuItem(parent = fortniteMenu, label = "Shot Reference Tool", c = shotRefTool)
        cmds.menuItem(parent = fortniteMenu, label = "Shot Reference Video Settings", c = shotRefSettings)	
        cmds.menuItem(parent = fortniteMenu, label = "Shot Splitter", c = shotSplitter)

        #PROJECTS\SIDEBAR (OLD)
        legacy = cmds.menuItem(parent = projectMenu, label = "Legacy", subMenu = True, to=True)
        cmds.menuItem(parent=legacy,label='Launch Sidebar', c = launchSidebar)

        #PROJECTS\MISC TOOLS
        mscMenu = cmds.menuItem(parent = projectMenu, label = "Misc. Tools", subMenu = True, to=True)
        cmds.menuItem(parent=mscMenu,label='FBX Cinematic Export Batcher...', c = fortniteHuskFbxBatch, ann="This is a batcher used for exporting cinematics (according to Jeremy)")
        cmds.menuItem(parent=mscMenu,label='FBX Export Motion Batcher...', c = exportMotionFbxBatch, ann="Used for exporting animations from a scene using the Sequence Info set up in the Export Motion Window from the Animation UI.")
        cmds.menuItem(parent=mscMenu,label='Export Skin Weights...', c = exportSkinWeights, ann="Export a skin weights file to the \"SkinWeights\" folder for the selected meshe(s).  You will get a dialog prompt for each one to allow you to change the name.")
        cmds.menuItem(parent=mscMenu,label='Export Selected Skin Weights', c = partial(exportSelectedSkinWeights, False), ann="Export a skin weights file to the \"SkinWeights\" folder for the selected meshe(s).  It will not give you a dialog prompt unless the named skin weights file already exists on disk.   Otherwise it will just use the name of the mesh to create the .txt file.")
        cmds.menuItem(parent=mscMenu,label='Import Skin Weights...', c = importSkinWeights, ann="Import from a skin weights file from the \"SkinWeights\" folder onto the selected mesh.")
        cmds.menuItem(parent=mscMenu,label='Import Skin Weights to Selected...', c = partial(mulw.importMultipleWeightsUI), ann="Import weights on multiple selected meshes based on names. Assumes that there is a skin weights file in the \"SkinWeights\" folder for each selected mesh.")
        cmds.menuItem(parent=mscMenu,label='Copy Multiple Weights Tool...', c = copy.createUI, ann="UI that allows you to copy weights from one mesh to multiple meshes that contains the name of the source.")
        cmds.menuItem(parent=mscMenu,label='Suffix Renamer...', c = suffix.suffixHeirarchyNames, ann="Allows you to rename the selected objects by adding a user input suffix to the end.")
        cmds.menuItem(parent=mscMenu,label='Surface Vertex Selector...', c = surf.surfaceVertSelectorUI, ann="Tool that allows you to select verts based on the location of a single component along the surface of the mesh.")

        #PROJECTS\RIGGING TOOLS
        rigToolsMenu = cmds.menuItem(parent = projectMenu, label = "Rigging Tools", subMenu = True, to=True)
        cmds.menuItem(parent=rigToolsMenu,label='Create Hydraulic Rig', c = partial(hyd.CreateHydraulicRig, 1), ann="This tool only works on \"leaf\" joints created with A.R.T.  To use, select 3 objects, the start of your hydraulic system, the end, and the upvector.  It is assumed that you are selecting the \"_anim\" controls for the first 2.  It is assumed that the upvector is 1 by default unless passed in otherwise.  Once set, you can always change this by simply manually reversing the value in the aimConstraint.  It is also assumed that you are using X as the axis that points at each other and that the first control points at the 2nd, but the 2nd points away from the first.   It also assuemes Z as the up-vector axis.")
        cmds.menuItem(parent=rigToolsMenu,label='Reverse Attr Blend...', c = rev.ReverseAttrHookupUI, ann="Select 2 objects, allows you to drive 2 attrs in opposite as a blend on the 2nd object from a driver attr on the first object.")
        cmds.menuItem(parent=rigToolsMenu,label='Orient Joint with Up Vector...', c = ornt.orientJointOB, ann="Select a joint and an object to be its up vector.  This tool will orient the joint or the joints heirarchy using the up vector object to define the up axis.")
        cmds.menuItem(parent=rigToolsMenu,label='Mesh Splitter', c = split.splitMeshByInfluencesUI, ann="Select all of the meshes you would like to split.  Including meshes with only one influence.")

        #check settings to see if use source control is turned on
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            mayaToolsDir = f.readline()
            f.close()	

            settingsLocation = mayaToolsDir + "/General/Scripts/projectSettings.txt"

            if os.path.exists(settingsLocation):
                f = open(settingsLocation, 'r')
                settings = cPickle.load(f)
                f.close()	

                #find use source control value
                sourceControl = settings.get("UseSourceControl")

                if sourceControl:
                    cmds.menuItem("perforceSubmitMenuItem", edit = True, enable = True)
                    cmds.menuItem("perforceAddAndSubmitMenuItem", edit = True, enable = True)
                    cmds.menuItem("perforceCheckOutMenuItem", edit = True, enable = True)
                    cmds.menuItem("perforceFileHistoryMenuItem", edit = True, enable = True)
                    cmds.menuItem("perforceGetLatestMenuItem", edit = True, enable = True)
                    cmds.menuItem("perforceProjectList", edit = True, enable = True)

                    #launch script job for checking Maya Tools
                    cmds.scriptJob(event = ["NewSceneOpened", autoUpdateTools])





#############################################################################################
#############################################################################################
#############################################################################################
def p4ProjectMenu(*args):

    #clear any projects that are in the collection first
    items = cmds.lsUI(menuItems = True)
    for i in items:
        data = cmds.menuItem(i, q = True, docTag = True)
        if data == "P4Proj":
            cmds.deleteUI(i)

    #find projects
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    projects = os.listdir(mayaToolsDir + "/General/Scripts/")
    p4Projects = []
    #Test_Project_Settings
    for proj in projects:
        if proj.rpartition(".")[2] == "txt":
            if proj.partition("_")[2].partition("_")[0] == "Project":
                p4Projects.append(proj)


    #set the current project
    try:
        f = open(mayaToolsDir + "/General/Scripts/projectSettings.txt", 'r')
        settings = cPickle.load(f)
        f.close()
        currentProj = settings.get("CurrentProject")
    except:
        pass



    #add the projects to the menu	
    for proj in p4Projects:
        projectName = proj.partition("_")[0]

        if projectName == currentProj:
            val = True
        else:
            val = False

        menuItem = cmds.menuItem(label = projectName, parent = "perforceProjectList", cl = "perforceProjectRadioMenuCollection", rb = val, docTag = "P4Proj", c = partial(setProj, projectName))
        cmds.menuItem(parent = "perforceProjectList", optionBox = True, c = partial(editProj, projectName))	

#############################################################################################
#############################################################################################
#############################################################################################
def setProj(projectName, *args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.setCurrentProject(projectName)

#############################################################################################
#############################################################################################
#############################################################################################
def editProj(projectName, *args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.editProject(projectName)

#############################################################################################
#############################################################################################
#############################################################################################
def createNewProj(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.createNewProject()

#############################################################################################
#############################################################################################
#############################################################################################
def autoUpdateTools(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_checkForUpdates()


#############################################################################################
#############################################################################################
#############################################################################################
def launchARTSettings(*args):
    import ART_Settings
    reload(ART_Settings)
    ART_Settings.ART_Settings()

#############################################################################################
#############################################################################################
#############################################################################################
def aboutARTTools(*args):

    cmds.confirmDialog(title = "About", message = "(c) Epic Games, Inc. 2013-2017\nCreated by: Jeremy Ernst\njeremy.ernst@epicgames.com\nVisit www.epicgames.com", icon = "information")

#############################################################################################
#############################################################################################
#############################################################################################
def editCharacter(*args):

    if cmds.window("artEditCharacterUI", exists = True):
        cmds.deleteUI("artEditCharacterUI")

    window = cmds.window("artEditCharacterUI", w = 300, h = 400, title = "Edit Character", mxb = False, mnb = False, sizeable = True)
    mainLayout = cmds.columnLayout(w = 300, h = 400, rs = 5, co = ["both", 5])

    #banner image
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    cmds.image(w = 300, h = 50, image = mayaToolsDir + "/General/Icons/ART/artBanner300px.bmp", parent = mainLayout)

    cmds.text(label = "", h = 1, parent = mainLayout)
    optionMenu = cmds.optionMenu("artProjOptionMenu", label = "Project:", w =290, h = 40, cc = getProjCharacters, parent = mainLayout)
    textScrollList = cmds.textScrollList("artProjCharacterList", w = 290, h = 300, parent = mainLayout)
    button = cmds.button(w = 290, h = 40, label = "Edit Export File", c = editSelectedCharacter, ann = "Edit the character's skeleton settings, joint positions, or skin weights.", parent = mainLayout)
    button2 = cmds.button(w = 290, h = 40, label = "Edit Rig File", c = editSelectedCharacterRig, ann = "Edit the character's control rig that will be referenced in by animation.", parent = mainLayout)

    cmds.text(label = "", h = 1)

    cmds.showWindow(window)
    getProjects()
    
    # CRA NEW CODE - Adding the code snippet so that it starts in your favorite folder.
    #set favorite project if it exists
    settingsLocation = mayaToolsDir + "/General/Scripts/projectSettings.txt"
    if os.path.exists(settingsLocation):
        f = open(settingsLocation, 'r')
        settings = cPickle.load(f)
        favoriteProject = settings.get("FavoriteProject")

        try:
            cmds.optionMenu("artProjOptionMenu", edit = True, v = favoriteProject)
        except:
            pass
    # CRA END NEW CODE

    getProjCharacters()
    
#############################################################################################
#############################################################################################
#############################################################################################
def getProjects(*args):

    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    projects = os.listdir(mayaToolsDir + "/General/ART/Projects/")
    for project in projects:
        cmds.menuItem(label = project, parent = "artProjOptionMenu")

#############################################################################################
#############################################################################################
#############################################################################################        
def getProjCharacters(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)

    cmds.textScrollList("artProjCharacterList", edit = True, removeAll = True)
    characters = os.listdir(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/")

    for character in characters:
        if os.path.isfile(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/" + character):
            if character.rpartition(".")[2] == "mb":
                niceName = character.rpartition(".")[0]
                niceName = niceName.partition("_Export")[0]
                cmds.textScrollList("artProjCharacterList", edit = True, append = niceName)

#############################################################################################
#############################################################################################
#############################################################################################
def editSelectedCharacter(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)
    character = cmds.textScrollList("artProjCharacterList", q = True, si = True)[0]

    cmds.file(mayaToolsDir + "/General/ART/Projects/" + proj + "/ExportFiles/" + character + "_Export.mb", open = True, force = True)
    cmds.deleteUI("artEditCharacterUI")
    launchSkeletonBuilder()


#############################################################################################
#############################################################################################
#############################################################################################
def editSelectedCharacterRig(*args):
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

    proj = cmds.optionMenu("artProjOptionMenu", q = True, value = True)
    character = cmds.textScrollList("artProjCharacterList", q = True, si = True)[0]

    cmds.file(mayaToolsDir + "/General/ART/Projects/" + proj + "/AnimRigs/" + character + ".mb", open = True, force = True)
    cmds.deleteUI("artEditCharacterUI")
    launchSkeletonBuilder()



#############################################################################################
#############################################################################################
#############################################################################################    
def changeMayaToolsLoc(*args):
    path = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(path):
        os.remove(path)
        cmds.confirmDialog(title = "Change Location", message = "Once you have chosen your new tools location, it is recommended that you restart Maya.", button = "OK")
        cmds.file(new = True, force = True)

#############################################################################################
#############################################################################################
#############################################################################################
def launchSkeletonBuilder(*args):

    import ART_skeletonBuilder_UI
    reload(ART_skeletonBuilder_UI)
    UI = ART_skeletonBuilder_UI.SkeletonBuilder_UI()


#############################################################################################
#############################################################################################
#############################################################################################
def launchAddCharacter(*args):

    import ART_addCharacter_UI
    reload(ART_addCharacter_UI)
    UI = ART_addCharacter_UI.AddCharacter_UI()


#############################################################################################
#############################################################################################
#############################################################################################
def launchAnimUI(*args):

    import ART_animationUI
    reload(ART_animationUI)
    UI = ART_animationUI.AnimationUI()

#############################################################################################
#############################################################################################
#############################################################################################
def launchEpic(*args):

    cmds.launch(web="http://www.epicgames.com")

#############################################################################################
#############################################################################################
#############################################################################################
def launchUnreal(*args):

    cmds.launch(web="http://www.unrealengine.com")

#############################################################################################
#############################################################################################
#############################################################################################
def launchAnimHelp(*args):

    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()


    if os.path.exists(mayaToolsDir + "/General/ART/Help/ART_AnimHelp.pdf"):
        cmds.launch(pdfFile = mayaToolsDir + "/General/ART/Help/ART_AnimHelp.pdf")

#############################################################################################
#############################################################################################
#############################################################################################
def launchRigHelp(self, *args):

    cmds.launch(web = "https://docs.unrealengine.com/latest/INT/Engine/Content/Tools/MayaRiggingTool/index.html")


#############################################################################################
#############################################################################################
#############################################################################################
def launchLearningVideos(self, *args):

    import ART_Help
    reload(ART_Help)
    ART_Help.ART_LearningVideos()


#############################################################################################
#############################################################################################
#############################################################################################    
def setupScene(*args):

    cmds.currentUnit(time = 'ntsc')
    cmds.playbackOptions(min = 0, max = 100, animationStartTime = 0, animationEndTime = 100)
    cmds.currentTime(0)


    #check for skeleton builder or animation UIs
    if cmds.dockControl("skeletonBuilder_dock", exists = True):
        print "Custom Maya Menu: SetupScene"
        channelBox = cmds.formLayout("SkelBuilder_channelBoxFormLayout", q = True, childArray = True)
        if channelBox != None:
            channelBox = channelBox[0]

            #reparent the channelBox Layout back to maya's window
            cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
            channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
            channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')

            #edit the channel box pane's attachment to the formLayout
            cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))


        #print "deleting dock and window and shit"
        cmds.deleteUI("skeletonBuilder_dock")
        if cmds.window("SkelBuilder_window", exists = True):
            cmds.deleteUI("SkelBuilder_window")	





    if cmds.dockControl("artAnimUIDock", exists = True):

        channelBox = cmds.formLayout("ART_cbFormLayout", q = True, childArray = True)
        if channelBox != None:
            channelBox = channelBox[0]

            #reparent the channelBox Layout back to maya's window
            cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
            channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
            channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')

            #edit the channel box pane's attachment to the formLayout
            cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))


        #print "deleting dock and window and shit"
        cmds.deleteUI("artAnimUIDock")
        if cmds.window("artAnimUI", exists = True):
            cmds.deleteUI("artAnimUI")


#############################################################################################
#############################################################################################
#############################################################################################
def autoOpenAnimUI():
    if cmds.objExists("*:master_anim_space_switcher_follow"):
        launchAnimUI()


#############################################################################################
#############################################################################################
#############################################################################################
def p4GetLatest(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_getLatestRevision(None)


#############################################################################################
#############################################################################################
#############################################################################################    
def p4CheckOut(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_checkOutCurrentFile(None)

#############################################################################################
#############################################################################################
#############################################################################################
def p4GetHistory(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_getRevisionHistory()


#############################################################################################
#############################################################################################
#############################################################################################
def p4Submit(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_submitCurrentFile(None, None)


#############################################################################################
#############################################################################################
#############################################################################################
def p4AddSubmit(*args):
    import perforceUtils
    reload(perforceUtils)
    perforceUtils.p4_addAndSubmitCurrentFile(None, None)





# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # / P R O J E C T S / # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

def fortniteHuskUI(*args):
    import autoRig_CharacterUI as arUI
    reload(arUI)
    arUI.UI()



#############################################################################################
#############################################################################################
#############################################################################################
def cineExportSkeleton(*args):

    if cmds.objExists("cine_placement_export_geo"):
        cmds.delete("cine_placement_export_geo")
    if cmds.objExists("placement_root"):
        cmds.delete("placement_root")


    selected = cmds.ls(sl = True)
    if len(selected) >0:
        joints = []

        for each in selected:
            cmds.select(clear = True)
            niceName = each
            if each.find(":") != -1:
                niceName = each.partition(":")[0] + "_Root"
            joint = cmds.joint(name = niceName)
            cmds.select(clear = True)
            joints.append(joint)
            constraint = cmds.parentConstraint(each, joint)[0]
            cmds.delete(constraint)

        #create root joint
        cmds.select(clear = True)
        cmds.joint(name = "placement_root")
        cmds.select(clear = True)

        for joint in joints:
            cmds.parent(joint, "placement_root")

        #create tiny poly plane for exporting
        plane = cmds.polyPlane(name = "cine_placement_export_geo", w = .01, h = .01, sx = 1, sy = 1)[0]
        cmds.select([plane, "placement_root"])
        cmds.skinCluster()

    else:
        cmds.warning("No Objects Selected")


#############################################################################################
#############################################################################################
#############################################################################################
def launchSidebar(*args):

    if cmds.dockControl("je_tools_dockControl", exists = True):
        cmds.deleteUI("je_tools_dockControl")

    import je_Tools
    je_Tools.toolsUI()




#############################################################################################
#############################################################################################
#############################################################################################
def threeCtrlSpineFix(*args):

    try:
        character = cmds.ls(sl = True)[0].partition(":")[0] + ":"

    except:
        cmds.confirmDialog(title = "Error", icon = "critical", message = "Please select a valid character control")
        return



    #Remove existing driven attrs
    cmds.aliasAttr(character + "spine_04_anim.driven", rm=True )
    attrs = cmds.listAttr(character + "spine_04_anim", keyable = True)
    for attr in attrs:
        if attr.find("blend") == 0:
            cmds.deleteAttr(character + "spine_04_anim", at = attr)


    cmds.aliasAttr(character + "spine_02_anim.driven", rm=True )
    attrs = cmds.listAttr(character + "spine_02_anim", keyable = True)
    for attr in attrs:
        if attr.find("blend") == 0:
            cmds.deleteAttr(character + "spine_02_anim", at = attr)



    #Spine 4 : Rotate X
    spine4MultXA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_xa_mult")
    cmds.connectAttr(character + "spine_05_anim.rx", spine4MultXA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_5_Influence", spine4MultXA + ".input2X", force = True)

    spine4MultXB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_xb_mult")
    cmds.connectAttr(character + "spine_03_anim.rx", spine4MultXB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_3_Influence", spine4MultXB + ".input2X", force = True)

    spine4MultX = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine04_drivenX_avg")
    cmds.setAttr(spine4MultX + ".operation", 3)
    cmds.connectAttr(spine4MultXA + ".outputX", spine4MultX + ".input1D[0]", force = True)
    cmds.connectAttr(spine4MultXB + ".outputX", spine4MultX + ".input1D[1]", force = True)
    cmds.connectAttr(spine4MultX + ".output1D", character + "spine_04_anim.rotateX", force = True)

    #Spine 4 : Rotate Y
    spine4MultYA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_ya_mult")
    cmds.connectAttr(character + "spine_05_anim.ry", spine4MultYA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_5_Influence", spine4MultYA + ".input2X", force = True)

    spine4MultYB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_yb_mult")
    cmds.connectAttr(character + "spine_03_anim.ry", spine4MultYB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_3_Influence", spine4MultYB + ".input2X", force = True)

    spine4MultY = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine04_drivenY_avg")
    cmds.setAttr(spine4MultY + ".operation", 3)
    cmds.connectAttr(spine4MultYA + ".outputX", spine4MultY + ".input1D[0]", force = True)
    cmds.connectAttr(spine4MultYB + ".outputX", spine4MultY + ".input1D[1]", force = True)
    cmds.connectAttr(spine4MultY + ".output1D", character + "spine_04_anim.rotateY", force = True)


    #Spine 4 : Rotate Z
    spine4MultZA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_za_mult")
    cmds.connectAttr(character + "spine_05_anim.rz", spine4MultZA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_5_Influence", spine4MultZA + ".input2X", force = True)

    spine4MultZB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_04_zb_mult")
    cmds.connectAttr(character + "spine_03_anim.rz", spine4MultZB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_04_anim.spine_3_Influence", spine4MultZB + ".input2X", force = True)

    spine4MultZ = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine04_drivenZ_avg")
    cmds.setAttr(spine4MultZ + ".operation", 3)
    cmds.connectAttr(spine4MultZA + ".outputX", spine4MultZ + ".input1D[0]", force = True)
    cmds.connectAttr(spine4MultZB + ".outputX", spine4MultZ + ".input1D[1]", force = True)
    cmds.connectAttr(spine4MultZ + ".output1D", character + "spine_04_anim.rotateZ", force = True)

    # # # # 

    #Spine 2 : Rotate X
    spine2MultXA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_xa_mult")
    cmds.connectAttr(character + "spine_03_anim.rx", spine2MultXA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_3_Influence", spine2MultXA + ".input2X", force = True)

    spine2MultXB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_xb_mult")
    cmds.connectAttr(character + "spine_01_anim.rx", spine2MultXB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_1_Influence", spine2MultXB + ".input2X", force = True)

    spine2MultX = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine02_drivenX_avg")
    cmds.setAttr(spine2MultX + ".operation", 3)
    cmds.connectAttr(spine2MultXA + ".outputX", spine2MultX + ".input1D[0]", force = True)
    cmds.connectAttr(spine2MultXB + ".outputX", spine2MultX + ".input1D[1]", force = True)
    cmds.connectAttr(spine2MultX + ".output1D", character + "spine_02_anim.rotateX", force = True)

    #Spine 2 : Rotate Y
    spine2MultYA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_ya_mult")
    cmds.connectAttr(character + "spine_03_anim.ry", spine2MultYA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_3_Influence", spine2MultYA + ".input2X", force = True)

    spine2MultYB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_yb_mult")
    cmds.connectAttr(character + "spine_01_anim.ry", spine2MultYB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_1_Influence", spine2MultYB + ".input2X", force = True)

    spine2MultY = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine02_drivenY_avg")
    cmds.setAttr(spine2MultY + ".operation", 3)
    cmds.connectAttr(spine2MultYA + ".outputX", spine2MultY + ".input1D[0]", force = True)
    cmds.connectAttr(spine2MultYB + ".outputX", spine2MultY + ".input1D[1]", force = True)
    cmds.connectAttr(spine2MultY + ".output1D", character + "spine_02_anim.rotateY", force = True)



    #Spine 2 : Rotate Z
    spine2MultZA = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_za_mult")
    cmds.connectAttr(character + "spine_03_anim.rz", spine2MultZA + ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_3_Influence", spine2MultZA + ".input2X", force = True)

    spine2MultZB = cmds.shadingNode("multiplyDivide", asUtility = True, name = character + "spine_02_zb_mult")
    cmds.connectAttr(character + "spine_01_anim.rz", spine2MultZB+ ".input1X", force = True)
    cmds.connectAttr(character + "spine_02_anim.spine_1_Influence", spine2MultZB + ".input2X", force = True)

    spine2MultZ = cmds.shadingNode("plusMinusAverage", asUtility = True, name = character + "spine02_drivenZ_avg")
    cmds.setAttr(spine2MultZ + ".operation", 3)
    cmds.connectAttr(spine2MultZA + ".outputX", spine2MultZ + ".input1D[0]", force = True)
    cmds.connectAttr(spine2MultZB + ".outputX", spine2MultZ + ".input1D[1]", force = True)
    cmds.connectAttr(spine2MultZ + ".output1D", character + "spine_02_anim.rotateZ", force = True)





    cmds.setKeyframe(character + "spine_02_anim.rotate")
    cmds.setKeyframe(character + "spine_04_anim.rotate")

    cmds.select(character + "spine_02_anim.cv[*]")
    cmds.scale(0, 0, 0,  relative = True)

    cmds.select(character + "spine_04_anim.cv[*]")
    cmds.scale(0, 0, 0,  relative = True)


    #check for new blendUnitConversion attr and alias attr it
    spine4Connections = cmds.listConnections(character + "spine_04_anim", source = True, type = "pairBlend")
    for each in spine4Connections:

        conversions = cmds.listConnections(each, type = "unitConversion")
        if conversions != None:
            attr = conversions[0].partition("Conversion")[2]
            attr = "blendUnitConversion" + attr

            try:
                cmds.aliasAttr("driven", character + "spine_04_anim." + attr)
                cmds.setAttr(character + "spine_04_anim.driven", 1)
            except:
                pass


    spine2Connections = cmds.listConnections(character + "spine_02_anim", source = True, type = "pairBlend")
    for each in spine2Connections:

        conversions = cmds.listConnections(each, type = "unitConversion")
        if conversions != None:
            attr = conversions[0].partition("Conversion")[2]
            attr = "blendUnitConversion" + attr

            try:
                cmds.aliasAttr("driven", character + "spine_02_anim." + attr)
                cmds.setAttr(character + "spine_02_anim.driven", 1)
            except:
                pass



#############################################################################################
#############################################################################################
#############################################################################################
def threeCtrlSpine(*args):


    #Spine 4 : Rotate X
    cmds.select("spine_04_anim")
    cmds.addAttr(longName='spine_5_Influence', defaultValue=1, minValue=-1, maxValue=5, keyable = True)
    cmds.addAttr(longName='spine_3_Influence', defaultValue=1, minValue=-1, maxValue=5, keyable = True)

    spine4MultXA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_xa_mult")
    cmds.connectAttr("spine_05_anim.rx", spine4MultXA + ".input1X")
    cmds.connectAttr("spine_04_anim.spine_5_Influence", spine4MultXA + ".input2X")

    spine4MultXB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_xb_mult")
    cmds.connectAttr("spine_03_anim.rx", spine4MultXB+ ".input1X")
    cmds.connectAttr("spine_04_anim.spine_3_Influence", spine4MultXB + ".input2X")

    spine4MultX = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine04_drivenX_avg")
    cmds.setAttr(spine4MultX + ".operation", 3)
    cmds.connectAttr(spine4MultXA + ".outputX", spine4MultX + ".input1D[0]")
    cmds.connectAttr(spine4MultXB + ".outputX", spine4MultX + ".input1D[1]")
    cmds.connectAttr(spine4MultX + ".output1D", "spine_04_anim.rotateX")

    #Spine 4 : Rotate Y
    spine4MultYA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_ya_mult")
    cmds.connectAttr("spine_05_anim.ry", spine4MultYA + ".input1X")
    cmds.connectAttr("spine_04_anim.spine_5_Influence", spine4MultYA + ".input2X")

    spine4MultYB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_yb_mult")
    cmds.connectAttr("spine_03_anim.ry", spine4MultYB+ ".input1X")
    cmds.connectAttr("spine_04_anim.spine_3_Influence", spine4MultYB + ".input2X")

    spine4MultY = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine04_drivenY_avg")
    cmds.setAttr(spine4MultY + ".operation", 3)
    cmds.connectAttr(spine4MultYA + ".outputX", spine4MultY + ".input1D[0]")
    cmds.connectAttr(spine4MultYB + ".outputX", spine4MultY + ".input1D[1]")
    cmds.connectAttr(spine4MultY + ".output1D", "spine_04_anim.rotateY")


    #Spine 4 : Rotate Z
    spine4MultZA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_za_mult")
    cmds.connectAttr("spine_05_anim.rz", spine4MultZA + ".input1X")
    cmds.connectAttr("spine_04_anim.spine_5_Influence", spine4MultZA + ".input2X")

    spine4MultZB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_04_zb_mult")
    cmds.connectAttr("spine_03_anim.rz", spine4MultZB+ ".input1X")
    cmds.connectAttr("spine_04_anim.spine_3_Influence", spine4MultZB + ".input2X")

    spine4MultZ = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine04_drivenZ_avg")
    cmds.setAttr(spine4MultZ + ".operation", 3)
    cmds.connectAttr(spine4MultZA + ".outputX", spine4MultZ + ".input1D[0]")
    cmds.connectAttr(spine4MultZB + ".outputX", spine4MultZ + ".input1D[1]")
    cmds.connectAttr(spine4MultZ + ".output1D", "spine_04_anim.rotateZ")

    # # # # 

    #Spine 2 : Rotate X
    cmds.select("spine_02_anim")
    cmds.addAttr(longName='spine_3_Influence', defaultValue=1, minValue=-1, maxValue=5, keyable = True)
    cmds.addAttr(longName='spine_1_Influence', defaultValue=1, minValue=-1, maxValue=5, keyable = True)

    spine2MultXA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_xa_mult")
    cmds.connectAttr("spine_03_anim.rx", spine2MultXA + ".input1X")
    cmds.connectAttr("spine_02_anim.spine_3_Influence", spine2MultXA + ".input2X")

    spine2MultXB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_xb_mult")
    cmds.connectAttr("spine_01_anim.rx", spine2MultXB+ ".input1X")
    cmds.connectAttr("spine_02_anim.spine_1_Influence", spine2MultXB + ".input2X")

    spine2MultX = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine02_drivenX_avg")
    cmds.setAttr(spine2MultX + ".operation", 3)
    cmds.connectAttr(spine2MultXA + ".outputX", spine2MultX + ".input1D[0]")
    cmds.connectAttr(spine2MultXB + ".outputX", spine2MultX + ".input1D[1]")
    cmds.connectAttr(spine2MultX + ".output1D", "spine_02_anim.rotateX")

    #Spine 2 : Rotate Y
    spine2MultYA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_ya_mult")
    cmds.connectAttr("spine_03_anim.ry", spine2MultYA + ".input1X")
    cmds.connectAttr("spine_02_anim.spine_3_Influence", spine2MultYA + ".input2X")

    spine2MultYB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_yb_mult")
    cmds.connectAttr("spine_01_anim.ry", spine2MultYB+ ".input1X")
    cmds.connectAttr("spine_02_anim.spine_1_Influence", spine2MultYB + ".input2X")

    spine2MultY = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine02_drivenY_avg")
    cmds.setAttr(spine2MultY + ".operation", 3)
    cmds.connectAttr(spine2MultYA + ".outputX", spine2MultY + ".input1D[0]")
    cmds.connectAttr(spine2MultYB + ".outputX", spine2MultY + ".input1D[1]")
    cmds.connectAttr(spine2MultY + ".output1D", "spine_02_anim.rotateY")



    #Spine 2 : Rotate Z
    spine2MultZA = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_za_mult")
    cmds.connectAttr("spine_03_anim.rz", spine2MultZA + ".input1X")
    cmds.connectAttr("spine_02_anim.spine_3_Influence", spine2MultZA + ".input2X")

    spine2MultZB = cmds.shadingNode("multiplyDivide", asUtility = True, name = "spine_02_zb_mult")
    cmds.connectAttr("spine_01_anim.rz", spine2MultZB+ ".input1X")
    cmds.connectAttr("spine_02_anim.spine_1_Influence", spine2MultZB + ".input2X")

    spine2MultZ = cmds.shadingNode("plusMinusAverage", asUtility = True, name = "spine02_drivenZ_avg")
    cmds.setAttr(spine2MultZ + ".operation", 3)
    cmds.connectAttr(spine2MultZA + ".outputX", spine2MultZ + ".input1D[0]")
    cmds.connectAttr(spine2MultZB + ".outputX", spine2MultZ + ".input1D[1]")
    cmds.connectAttr(spine2MultZ + ".output1D", "spine_02_anim.rotateZ")





    cmds.setKeyframe("spine_02_anim.rotate")
    cmds.setKeyframe("spine_04_anim.rotate")

    cmds.select("spine_02_anim.cv[*]")
    cmds.scale(0, 0, 0,  relative = True)

    cmds.select("spine_04_anim.cv[*]")
    cmds.scale(0, 0, 0,  relative = True)


    #check for new blendUnitConversion attr and alias attr it
    spine4Connections = cmds.listConnections("spine_04_anim", source = True, type = "pairBlend")
    for each in spine4Connections:

        conversions = cmds.listConnections(each, type = "unitConversion")
        if conversions != None:
            attr = conversions[0].partition("Conversion")[2]
            attr = "blendUnitConversion" + attr

            try:
                cmds.aliasAttr("driven", "spine_04_anim." + attr)
                cmds.setAttr("spine_04_anim.driven", 1)
            except:
                pass


    spine2Connections = cmds.listConnections("spine_02_anim", source = True, type = "pairBlend")
    for each in spine2Connections:

        conversions = cmds.listConnections(each, type = "unitConversion")
        if conversions != None:
            attr = conversions[0].partition("Conversion")[2]
            attr = "blendUnitConversion" + attr

            try:
                cmds.aliasAttr("driven", "spine_02_anim." + attr)
                cmds.setAttr("spine_02_anim.driven", 1)
            except:
                pass




#############################################################################################
#############################################################################################
#############################################################################################
def fortnite_huskExport(*args):


    #find references in scene:
    refs = cmds.ls(type = "reference")
    for ref in refs:

        try:
            namespace = cmds.referenceQuery(ref, namespace = True)

            if namespace == ":husk_bodyPart":
                fileName = cmds.referenceQuery(ref, filename = True)
                cmds.file(fileName, rr = True)
        except:
            pass



    try:
        cmds.delete("husk_bodyPartRNfosterParent*")

    except:
        pass


    #show layers
    try:

        if cmds.objExists("Husk_Anim_Rig:b_MF_Root"):
            cmds.setAttr("Husk_Anim_Rig:Husk_Geo_Layer.v", 1)
            cmds.setAttr("Husk_Anim_Rig:Husk_Head.v", 1)

        else:
            cmds.setAttr("Husk:Husk_Geo_Layer.v", 1)

    except:
        pass




    #set global scale to 1
    try:
        if cmds.objExists("Husk_Anim_Rig:Master_Control"):
            cmds.cutKey("Husk_Anim_Rig:Master_Control.globalScale")
            cmds.select("Husk_Anim_Rig:Master_Control")
            cmds.setToolTo("moveSuperContext")
            cmds.select(clear = True)	    
            cmds.setAttr("Husk_Anim_Rig:Master_Control.globalScale", 1)

        if cmds.objExists("Husk:Master_Control"):
            cmds.cutKey("Husk:Master_Control.globalScale")
            cmds.select("Husk:Master_Control")
            cmds.setToolTo("moveSuperContext")
            cmds.select(clear = True)
            cmds.setAttr("Husk:Master_Control.globalScale", 1)

    except:
        pass



    character = cmds.optionMenu("selectedCharacterOptionMenu", q = True, v = True)
    dupeNodes = cmds.duplicate(character + ":b_MF_Root")

    parent = cmds.listRelatives(dupeNodes[0], parent = True)
    if parent != None:
        cmds.parent(dupeNodes[0], world = True)

    startFrame = cmds.playbackOptions(q = True, min = True)
    endFrame = cmds.playbackOptions(q = True, max = True)

    for node in dupeNodes:
        try:
            if cmds.nodeType(node) == "joint":
                cmds.parentConstraint(character + ":" + node, node)
        except:
            pass



    #ik Bones
    if cmds.objExists("b_MF_IK_Foot_Root") == False:

        #create the joints
        cmds.select(clear = True)
        ikFootRoot = cmds.joint(name = "b_MF_IK_Foot_Root")
        cmds.select(clear = True)


        cmds.select(clear = True)
        ikFootLeft = cmds.joint(name = "b_MF_IK_Foot_L")
        cmds.select(clear = True)

        cmds.select(clear = True)
        ikFootRight = cmds.joint(name = "b_MF_IK_Foot_R")
        cmds.select(clear = True)

        cmds.select(clear = True)
        ikHandRoot = cmds.joint(name = "b_MF_IK_Hand_Root")
        cmds.select(clear = True)

        cmds.select(clear = True)
        ikHandGun = cmds.joint(name = "b_MF_IK_Gun")
        cmds.select(clear = True)

        cmds.select(clear = True)
        ikHandLeft = cmds.joint(name = "b_MF_IK_Hand_L")
        cmds.select(clear = True)

        cmds.select(clear = True)
        ikHandRight = cmds.joint(name = "b_MF_IK_Hand_R")
        cmds.select(clear = True)


        #create hierarhcy
        cmds.parent(ikFootRoot, "b_MF_Root")
        cmds.parent(ikHandRoot, "b_MF_Root")
        cmds.parent(ikFootLeft, ikFootRoot)
        cmds.parent(ikFootRight, ikFootRoot)
        cmds.parent(ikHandGun, ikHandRoot)
        cmds.parent(ikHandLeft, ikHandGun)
        cmds.parent(ikHandRight, ikHandGun)    



    #constrain the joints
    leftHandConstraint = cmds.parentConstraint("b_MF_Hand_L", "b_MF_IK_Hand_L")[0]
    rightHandGunConstraint = cmds.parentConstraint("b_MF_Hand_R", "b_MF_IK_Gun")[0]
    rightHandConstraint = cmds.parentConstraint("b_MF_Hand_R", "b_MF_IK_Hand_R")[0]
    leftFootConstraint = cmds.parentConstraint("b_MF_Foot_L", "b_MF_IK_Foot_L")[0]
    rightFootConstraint = cmds.parentConstraint("b_MF_Foot_R", "b_MF_IK_Foot_R")[0]    

    #get the blendshapes to export
    blendshapes = cmds.ls(type = "blendShape")

    cmds.select("b_MF_Root", hi = True)
    if blendshapes != None:
        for shape in blendshapes:
            cmds.select(shape, add = True)


    #bake results
    cmds.bakeResults(simulation = True, time = (startFrame, endFrame))


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
                    for i in range(len(keys)):
                        cmds.setKeyframe("custom_export_shapes." + attr, t = (keys[i]), v = keyValues[i])

                i = i + 1


    #bake IK joints
    #constrain the joints
    leftHandConstraint = cmds.parentConstraint("b_MF_Hand_L", "b_MF_IK_Hand_L")[0]
    rightHandGunConstraint = cmds.parentConstraint("b_MF_Hand_R", "b_MF_IK_Gun")[0]
    rightHandConstraint = cmds.parentConstraint("b_MF_Hand_R", "b_MF_IK_Hand_R")[0]
    leftFootConstraint = cmds.parentConstraint("b_MF_Foot_L", "b_MF_IK_Foot_L")[0]
    rightFootConstraint = cmds.parentConstraint("b_MF_Foot_R", "b_MF_IK_Foot_R")[0]   

    cmds.select("b_MF_Root", hi = True)
    cmds.bakeResults(simulation = True, time = (startFrame, endFrame))


    #remove constraints
    cmds.select("b_MF_Root", hi = True)
    cmds.delete(constraints = True)
    cmds.select("b_MF_Root", hi = True)
    if cmds.objExists("custom_export"):
        cmds.select("custom_export", add = True)

    #export
    #get the name of your opened file
    openedFile = cmds.file(q = True, sceneName = True)
    fileName = openedFile.rpartition(".")[0]

    cmds.file(fileName + ".fbx", type = "FBX export", pr = True, es = True, force = True, prompt = False)

    cmds.delete("b_MF_Root")
    if cmds.objExists("custom_export"):
        cmds.delete("custom_export")


    #fix morphs
    if cmds.objExists("Morphs"):
        cmds.select(["Morphs", "Morphs1"])


        cmds.cutKey()

        cmds.connectAttr("Morph_Anim.L_Blink", "Morphs.Left_Blink")
        cmds.connectAttr("Morph_Anim.R_Blink", "Morphs.Right_Blink")

        cmds.connectAttr("Morph_Anim.Breath", "Morphs1.BreathIn")
        cmds.connectAttr("Morph_Anim.L_Toe_Splay", "Morphs1.L_Toe_Splay")
        cmds.connectAttr("Morph_Anim.R_Toe_Splay", "Morphs1.R_Toe_Splay")


    if cmds.objExists(character + ":Morphs"):
        cmds.select([character + ":Morphs", character + ":Morphs1"])


        cmds.cutKey()

        cmds.connectAttr(character + ":Morph_Anim.L_Blink", character + ":Morphs.Left_Blink")
        cmds.connectAttr(character + ":Morph_Anim.R_Blink", character + ":Morphs.Right_Blink")

        cmds.connectAttr(character + ":Morph_Anim.Breath", character + ":Morphs1.BreathIn")
        cmds.connectAttr(character + ":Morph_Anim.L_Toe_Splay", character + ":Morphs1.L_Toe_Splay")
        cmds.connectAttr(character + ":Morph_Anim.R_Toe_Splay", character + ":Morphs1.R_Toe_Splay")



#############################################################################################
#############################################################################################
#############################################################################################
def fortniteCharacterEditor(*args):

    import fortniteCharacterEditor as fnCE
    reload(fnCE)
    fnCE.Fortnite_Character_Editor()


#############################################################################################
#############################################################################################
#############################################################################################
def fortniteHuskFbxBatch(*args):
    import ART_fbxBatcher
    reload(ART_fbxBatcher)
    ART_fbxBatcher.FBX_Batcher()    


#############################################################################################
#############################################################################################
#############################################################################################
def exportMotionFbxBatch(*args):
    import ART_fbxBatch
    reload(ART_fbxBatch)
    ART_fbxBatch.FBX_Batcher() 

#############################################################################################
#############################################################################################
#############################################################################################
def shotSplitter(*args):
    import shotSplitter
    reload(shotSplitter)
    shotSplitter.UI()

#############################################################################################
#############################################################################################
#############################################################################################
def fortniteHuskChooser(*args):
    import fortnite_HuskChooser
    reload(fortnite_HuskChooser)
    fortnite_HuskChooser.Fortnite_Husk_Chooser()


#############################################################################################
#############################################################################################
#############################################################################################
def addFortniteIKBones(*args):

    try:
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
        leftHandConstraint = cmds.parentConstraint("hand_l", ikHandLeft)[0]
        rightHandGunConstraint = cmds.parentConstraint("hand_r", ikHandGun)[0]
        rightHandConstraint = cmds.parentConstraint("hand_r", ikHandRight)[0]
        leftFootConstraint = cmds.parentConstraint("foot_l", ikFootLeft)[0]
        rightFootConstraint = cmds.parentConstraint("foot_r", ikFootRight)[0]

    except:
        cmds.warning("Something went wrong")



#############################################################################################
#############################################################################################
#############################################################################################
def fortniteHuskIkBonesBatch(*args):
    import fortnite_HuskIkBonesBatcher
    reload(fortnite_HuskIkBonesBatcher)
    fortnite_HuskIkBonesBatcher.Fortnite_HuskIkBonesBatcher() 


#############################################################################################
#############################################################################################
#############################################################################################
def fortniteikBonesBatch(*args):
    import fortnite_ikBonesBatcher
    reload(fortnite_ikBonesBatcher)
    fortnite_ikBonesBatcher.Fortnite_IkBonesBatcher() 

#############################################################################################
#############################################################################################
#############################################################################################
def shotRefTool(*args):

    import je_shotReferenceTool
    reload(je_shotReferenceTool)
    je_shotReferenceTool.UI()


#############################################################################################
#############################################################################################
#############################################################################################
def shotRefSettings(*args):

    import je_shotReferenceTool
    reload(je_shotReferenceTool)
    je_shotReferenceTool.listAllReferenceMovies_UI()

#############################################################################################
#############################################################################################
#############################################################################################
def fortniteCoreMotionMaker(*args):

    import fortniteCoreMotionMaker
    reload(fortniteCoreMotionMaker)
    fortniteCoreMotionMaker.Fortnite_CoreMotionMaker()




#############################################################################################
#############################################################################################
#############################################################################################
def exportSkinWeights(*args):
    #tools path
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

        path = None

    selection = cmds.ls(sl = True)
    if selection == []:
        cmds.confirmDialog(icon = "warning", title = "Export Skin Weights", message = "Nothing Selected")
        return
    else:
        for one in selection:

            #add a dialog that warns the user that they may lose weighting information if their changes are drastic
            result = cmds.promptDialog(title = "Export Weights", text = one, message = "FileName:", button = ["Accept", "Cancel"])
            if result == "Accept":
                name = cmds.promptDialog(q = True, text = True)

                path = mayaToolsDir + "\General\ART\SkinWeights\\"
                if not os.path.exists(path):
                    os.makedirs(path)

                path = path + name + ".txt"

                if not os.path.exists(path):
                    doSkinWeightExport(one, path, False)
                else:
                    result = cmds.confirmDialog(icon = "question", title = "Export Weights", message = path+" already exists.", button = ["Overwrite", "Cancel"])
                    if result == "Overwrite":
                        doSkinWeightExport(one, path, False)
                        #print "saved skin weights for " + one + " to " + path

    print "EXPORT WEIGHTS DONE!"

def exportSelectedSkinWeights(supress, *args):
    #tools path
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()

        path = None

    selection = cmds.ls(sl = True)
    if selection == []:
        cmds.confirmDialog(icon = "warning", title = "Export Skin Weights", message = "Nothing Selected")
        return
    else:
        for one in selection:
            path = mayaToolsDir + "\General\ART\SkinWeights\\"
            if not os.path.exists(path):
                os.makedirs(path)

            path = path + one + ".txt"

            if not os.path.exists(path):
                doSkinWeightExport(one, path, False)
            else:
                if supress is False:
                    result = cmds.confirmDialog(icon = "question", title = "Export Weights", message = path+" already exists.", button = ["Overwrite", "Cancel"])
                    if result == "Overwrite":
                        doSkinWeightExport(one, path, False)
                else:
                    doSkinWeightExport(one, path, True)

    print "EXPORT WEIGHTS DONE!"
    
    
    
    
    
    
def doSkinWeightExport(one, path, supress):
    #find geo by looking at skinclusters
    skinClusters = cmds.ls(type = 'skinCluster')

    print "saved skin weights for " + one + " to " + path
    for skin in skinClusters:

        #go through each found skin cluster, and if we find a skin cluster whose geometry matches our selection, get influences
        bigList = []

        for cluster in skinClusters:
            geometry = cmds.skinCluster(cluster, q = True, g = True)[0]

            geoTransform = cmds.listRelatives(geometry, parent = True)[0]

            if geoTransform == one:
                f = open(path, 'w')

                skinCluster = cluster

                #find all vertices in the current geometry
                verts = cmds.polyEvaluate(one, vertex = True)

                for i in range(int(verts)):
                    #get weighted transforms
                    transforms = cmds.skinPercent( skinCluster, one + ".vtx[" + str(i) + "]", ib = .001, query=True, t= None)


                    #get skinPercent value per transform
                    values = cmds.skinPercent( skinCluster, one + ".vtx[" + str(i) + "]", ib = .001, query=True, value=True)
                    list = []
                    list.append(i)

                    for x in range(int(len(values))):
                        list.append([transforms[x], values[x]])

                    #add each vert info's transforms, and values to the overall huge list
                    bigList.append(list)

                #write that overall list to file
                cPickle.dump(bigList, f)

                #close the file
                f.close()

                #operation complete
                if supress is False:
                    cmds.headsUpMessage( "Skin Weights Exported!", time=3.0 )

                return



#############################################################################################
#############################################################################################
#############################################################################################
def importSkinWeights(*args):

    #tools path
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()	


    selection = cmds.ls(sl = True)
    if selection == []:
        cmds.confirmDialog(icon = "warning", title = "Import Skin Weights", message = "Nothing Selected")
        return


    if selection != []:
        if len(selection) > 1:
            cmds.confirmDialog(icon = "warning", title = "Import Skin Weights", message = "Too many objects selected. Please only selected 1 mesh.")
            return


        #find all files in the system dir
        path = mayaToolsDir + "\General\ART\SkinWeights\\"

        try:
            fileToImport = cmds.fileDialog2(fileFilter="*.txt", dialogStyle=2, fm = 1, startingDirectory = path)[0]

        except:
            cmds.warning("Operation Cancelled.")
            return



        #load the file with cPickle and parse out the information
        file = open(fileToImport, 'r')
        weightInfoList = cPickle.load(file)


        #get the number of vertices in the mesh. This will be used to update our progress window
        verts = cmds.polyEvaluate(selection[0], vertex = True)
        if verts < 20:
            verts = verts * 20

        increment = int(verts/20)
        matchNum = increment

        #find the transforms from the weightInfoList. format = [ [vertNum, [jointName, value], [jointName, value] ] ]
        transforms = []
        newList = weightInfoList[:]
        for info in newList:

            for i in info:
                if i != info[0]:
                    transform = i[0]
                    transforms.append(transform)

        #remove duplicates from the transforms list
        transforms = set(transforms)

        #clear selection. Add transforms to selection and geo
        cmds.select(clear = True)
        for t in transforms:
            if cmds.objExists(t):
                cmds.select(t, add = True)


        #check if the geometry that is selected is already skinned or not
        skinClusters = cmds.ls(type = 'skinCluster')
        skinnedGeometry = []
        for cluster in skinClusters:
            geometry = cmds.skinCluster(cluster, q = True, g = True)[0]
            geoTransform = cmds.listRelatives(geometry, parent = True)[0]
            skinnedGeometry.append(geoTransform)

            if geoTransform == selection[0]:
                skin = cluster


        #if a skin cluster does not exist already on the mesh:
        if selection[0] not in skinnedGeometry:

            #put a skin cluster on the geo normalizeWeights = 1, dr=4.5, mi = 4,
            cmds.select(selection[0], add = True)
            skin = cmds.skinCluster( tsb = True, skinMethod = 0, name = (selection[0] +"_skinCluster"))[0]


        #setup a progress window to track length of progress
        cmds.progressWindow(title='Skin Weights', progress = 0, status = "Reading skin weight information from file...")

        #get the vert number, and the weights for that vertex
        for info in weightInfoList:
            vertNum = info[0]
            info.pop(0)

            #progress bar update
            if vertNum == matchNum:
                cmds.progressWindow( edit=True, progress = ((matchNum/increment) * 5), status=  "Importing skin weights for " + selection[0] + ".\n Please be patient")
                matchNum += increment

            #set the weight for each vertex
            try:
                cmds.skinPercent(skin, selection[0] + ".vtx[" + str(vertNum) + "]", transformValue= info)
            except:
                pass


        #close the file
        file.close()

        #close progress window
        cmds.progressWindow(endProgress = 1)
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

#LAUNCH SCRIPT JOBS
scriptJobNum2 = cmds.scriptJob(event = ["PostSceneRead", autoOpenAnimUI])
scriptJobNum = cmds.scriptJob(event = ["NewSceneOpened", setupScene])
p4ScriptJob = cmds.scriptJob(event = ["NewSceneOpened", p4ProjectMenu])





