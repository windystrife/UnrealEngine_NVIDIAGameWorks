import maya.cmds as cmds
from functools import partial
import os, cPickle, math
import maya.mel as mel
import maya.utils



class AnimationUI():

    def __init__(self):

        #check to see if there are any rigs in the scene, if not return
        characters = self.getCharacters()
        print characters
        if len(characters) == 0:
            result = cmds.confirmDialog(title = "Error", message = "No Characters found in scene. Would you like to add a character now?", button = ["Yes", "No"], defaultButton = "Yes", cancelButton = "No", dismissString = "No")
            if result == "Yes":
                import ART_addCharacter_UI
                reload(ART_addCharacter_UI)
                UI = ART_addCharacter_UI.AddCharacter_UI()

                return

            else:
                return

        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()


        #figure out which project the rigs are from
        references = cmds.ls(type = "reference")
        self.project = ""

        for ref in references:
            try:
                projects = os.listdir(self.mayaToolsDir + "/General/Icons/ART/Thumbnails/")
                proj = cmds.referenceQuery(ref, filename = True, unresolvedName = True).rpartition("Projects/")[2].partition("/")[0]

                if proj in projects:
                    self.project = proj


                resolved = cmds.referenceQuery(ref, filename = True).rpartition("Projects/")[2].partition("/")[0]
                if resolved in projects:
                    if resolved != self.project:
                        refPath = cmds.referenceQuery(ref, filename = True)
                        cmds.confirmDialog(title = "Reference", icon = "warning", message = "This file is currently referencing a rig file that is not located in the MayaTools directory.\nCurrent Reference path: " + refPath + ".")



            except:
                pass


        path = self.mayaToolsDir + "/General/ART/Projects/" + self.project + "/banner.jpg"
        if os.path.exists(path):
            projectBanner = path
        else:
            projectBanner = self.mayaToolsDir + "/General/Icons/ART/banner.jpg"

        #create class vars
        self.widgets = {}
        self.formsToHide = []
        self.assetEntries = []
        self.mats = []


        #check to see if the skeleton builder UI exists with channel box
        #check to see if window exists. if so, delete
        if cmds.dockControl("skeletonBuilder_dock", exists = True):

            channelBox = cmds.formLayout("SkelBuilder_channelBoxFormLayout", q = True, childArray = True)
            if channelBox != None:
                channelBox = channelBox[0]

                #reparent the channelBox Layout back to maya's window
                cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
                channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
                channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')

                #edit the channel box pane's attachment to the formLayout
                cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))

            cmds.deleteUI("skeletonBuilder_dock")



        #check to see if window exists. if so, delete
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

            cmds.deleteUI("artAnimUIDock")



        if cmds.window("artAnimUI", exists = True):
            cmds.deleteUI("artAnimUI")


        #build window
        self.widgets["window"] = cmds.window("artAnimUI", w = 400, h = 700, title = "Animation UI", sizeable = True)

        #create the main layout
        self.widgets["topLevelLayout"] = cmds.columnLayout()

        #create the menu bar
        self.widgets["menuBarLayout"] = cmds.menuBarLayout(w = 400, h =20, parent = self.widgets["topLevelLayout"] )
        self.widgets["menuBar_settings"] = cmds.menu(label = "Settings", parent = self.widgets["menuBarLayout"])
        self.widgets["menuBar_settings_channelBox"] = cmds.menuItem(label = "Show Channel Box", checkBox = False, parent = self.widgets["menuBar_settings"], c = self.showChannelBox)
        self.widgets["menuBar_settings_matching"] = cmds.menuItem(label = "Match On Switch", checkBox = True, parent = self.widgets["menuBar_settings"])

        #add match options
        cmds.menuItem(parent = self.widgets["menuBar_settings"], divider = True)
        cmds.menuItem(parent = self.widgets["menuBar_settings"], label = "Space Switch Settings", enable = False)
        cmds.menuItem(parent = self.widgets["menuBar_settings"], divider = True)

        self.widgets["spaceSwitch_MatchToggleCB"] = cmds.menuItem(parent = self.widgets["menuBar_settings"], label = "Match?", cb = True, c = self.saveUISettings)
        self.widgets["spaceSwitch_MatchMethodCB"] = cmds.menuItem(parent = self.widgets["menuBar_settings"], label = "Match To Control?", cb = True, c = self.saveUISettings)		

        #Add animation menu
        self.widgets["menuBar_animation"] = cmds.menu(label = "Animation", parent = self.widgets["menuBarLayout"])
        self.widgets["menuBar_animation_eulerAll"] = cmds.menuItem(label = "Run Euler Filter On All",  c = self.eulerFilterAll, parent = self.widgets["menuBar_animation"])
        self.widgets["menuBar_animation_eulerSel"] = cmds.menuItem(label = "Run Euler Filter On Selected",  c = self.eulerFilterSelected, parent = self.widgets["menuBar_animation"])
        self.widgets["menuBar_animation_bakeDyn"] = cmds.menuItem(label = "Bake Dynamics to FK",  c = self.bakeDynToFK, parent = self.widgets["menuBar_animation"])



        #create the area for the active character controls
        self.widgets["activeCharacterLayout"] = cmds.formLayout(w = 400, h = 60, parent = self.widgets["topLevelLayout"])
        self.widgets["projectBanner"] = cmds.image(w = 400, h = 60, bgc = [.5, .5, .5],  parent = self.widgets["activeCharacterLayout"], image = projectBanner )
        self.widgets["activeCharacterThumb"] = cmds.symbolButton('activeCharacterThumb', w = 50, h = 50,  parent = self.widgets["activeCharacterLayout"])

        cmds.formLayout(self.widgets["activeCharacterLayout"], edit = True, af = [(self.widgets["activeCharacterThumb"], "right", 5), (self.widgets["activeCharacterThumb"], "top", 6)])

        #create the character list pop-up menu
        self.widgets["characterRigList"] = cmds.popupMenu(parent = self.widgets["activeCharacterThumb"], b = 1)
        self.populateCharacterRigList()

        #create the row column layout where the left column will contain pretty much everything, and the right column is optional for channel box display
        self.widgets["rowColLayout"] = cmds.rowColumnLayout(nc = 3, cw = [(1, 400), (2, 50), (3, 1)], parent = self.widgets["topLevelLayout"])


        #create the main tab Layout
        self.widgets["mainLayout"] = cmds.tabLayout(w = 400, h = 700, parent = self.widgets["rowColLayout"])

        #create the formLayout that will contain each character's picker
        self.widgets["pickerLayout"] = cmds.formLayout(w = 400, h = 700, parent = self.widgets["mainLayout"])
        self.widgets["pickerScroll"] = cmds.scrollLayout(w = 400, h = 700, hst = 0, parent = self.widgets["pickerLayout"])




        #create the tools layout(50 pixel column on the screen right)
        self.widgets["pickerTools"] = cmds.columnLayout(w = 50, h = 700, parent = self.widgets["rowColLayout"], rowSpacing = 10)
        cmds.text(label = "")
        self.widgets["pickerSelectTool"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/pickerSelect.bmp", parent = self.widgets["pickerTools"], ann = "Selection Tools")
        self.widgets["pickerResetTool"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/zero.bmp", parent = self.widgets["pickerTools"], ann = "Reset Rig to the Defaults")
        self.widgets["pickerImportMotionTool"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/importMotion.bmp", parent = self.widgets["pickerTools"], c = self.importMotion, ann = "Import Motion")
        self.widgets["pickerExportMotionTool"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/exportMotion.bmp", parent = self.widgets["pickerTools"], c = self.exportMotion, ann = "Export Motion")
        self.widgets["pickerSpaceSwitchTool"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/picker_spaceSwitch.bmp", parent = self.widgets["pickerTools"], c = self.spaceSwitcher, ann = "Space Switching")
        self.widgets["pickerPoseTools"] = cmds.symbolButton(c = self.poseEditor, w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/poseTools.bmp", parent = self.widgets["pickerTools"], ann = "Pose Tools and Utilities")
        self.widgets["pickerMatchOverRange"] = cmds.symbolButton(c = self.match_frameRange_UI, w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/spaceSwitchMatch_on.bmp", parent = self.widgets["pickerTools"], ann = "Match Over Frame Range")
        self.widgets["pickerControlScaleTool"] = cmds.symbolButton(c = self.control_scale_init, w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/controlScale.bmp", parent = self.widgets["pickerTools"], ann = "Scale selected controls' size. Use to make controls larger or smaller for selecting.")
        self.widgets["pickerControlVisibility"] = cmds.iconTextCheckBox(w = 50, h = 50, style='iconOnly', value = True,  selectionImage = self.mayaToolsDir + "/General/Icons/ART/pickerVisible.bmp", image = self.mayaToolsDir + "/General/Icons/ART/pickerInvisible.bmp", parent = self.widgets["pickerTools"], onc = partial(self.toggleControlVis, True), ofc = partial(self.toggleControlVis, False), ann = "Toggle Current Rig's Control Visibility" )
        self.widgets["pickerSelectionSets"] = cmds.symbolButton(c = self.control_scale_init, w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/selectionSets.bmp", parent = self.widgets["pickerTools"], ann = "Selection Sets that are created are stored here.")
        self.widgets["pickerHelpMenu"] = cmds.symbolButton(w = 50, h = 50, image = self.mayaToolsDir + "/General/Icons/ART/helpicon.bmp", parent = self.widgets["pickerTools"], c = self.animHelp, ann = "Help")

        #create popup menu for space switcher
        menu = cmds.popupMenu(parent = self.widgets["pickerSpaceSwitchTool"], b = 3)
        cmds.menuItem(label = "Create Space", parent = menu, c = self.createSpace)

        #create radial menu for pose button for copy, paste, and paste opposite
        self.widgets["pickerPoseToolsRadial"] = cmds.popupMenu(b = 3, parent = self.widgets["pickerPoseTools"], mm = True)
        self.widgets["pickerPoseToolsRadial_copy"] = cmds.menuItem(label = "Copy", parent = self.widgets["pickerPoseToolsRadial"], rp = "N", c = self.copyPose)
        self.widgets["pickerPoseToolsRadial_paste"] = cmds.menuItem(label = "Paste", parent = self.widgets["pickerPoseToolsRadial"], rp = "W", c = self.pastePose)
        self.widgets["pickerPoseToolsRadial_pasteOpposite"] = cmds.menuItem(label = "Paste Opposite", parent = self.widgets["pickerPoseToolsRadial"], rp = "S", c = self.pastePoseOpposite)
        self.widgets["pickerPoseToolsRadial_pastePreview"] = cmds.menuItem(label = "Show Paste Controls", parent = self.widgets["pickerPoseToolsRadial"], rp = "NW", c = self.pastePreview)
        self.widgets["pickerPoseToolsRadial_pastePreviewOpp"] = cmds.menuItem(label = "Show Paste Opposite Controls", parent = self.widgets["pickerPoseToolsRadial"], rp = "SW", c = self.pasteOppositePreview)

        #create radial menu for iso select tools
        self.widgets["isoSelectRadial"] = cmds.popupMenu(b = 3, parent = self.widgets["pickerControlVisibility"])
        self.widgets["isoSelect_Generate"] = cmds.menuItem(label = "Generate Iso Selection Sets", parent = self.widgets["isoSelectRadial"], c = self.getIsoSelectionPolygons, enable = True)
        cmds.menuItem(divider = True, parent = self.widgets["isoSelectRadial"] )


        self.widgets["isoSelect_Torso"] = cmds.menuItem(label = "Torso", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_LeftArm"] = cmds.menuItem(label = "Left Arm", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_RightArm"] = cmds.menuItem(label = "Right Arm", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_LeftLeg"] = cmds.menuItem(label = "Left Leg", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_RightLeg"] = cmds.menuItem(label = "Right Leg", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_Head"] = cmds.menuItem(label = "Head", parent = self.widgets["isoSelectRadial"], c = self.isoSelect, cb = True, enable = False)
        self.widgets["isoSelect_ShowAll"] = cmds.menuItem(label = "Show All", parent = self.widgets["isoSelectRadial"], c = self.exitIso, enable = False)


        cmds.menuItem(divider = True, parent = self.widgets["isoSelectRadial"] )
        cmds.menuItem(label = "Isolation Method:", parent = self.widgets["isoSelectRadial"], enable = False)
        isoMethodCollection = cmds.radioMenuItemCollection(parent = self.widgets["isoSelectRadial"])
        self.widgets["isoMethodClassic"] = cmds.menuItem(label = "Classic", rb = True, cl  = isoMethodCollection, parent = self.widgets["isoSelectRadial"], ann = "Use Maya's isolate selection, where everything except selection is hidden", c = self.exitIso)
        self.widgets["isoMethodMaterial"] = cmds.menuItem(label = "Material", rb = False, cl  = isoMethodCollection, parent = self.widgets["isoSelectRadial"], c = self.exitIso, ann = "Use custom isolate selection function where any unselected parts are invisible, but the rest of the scene does not get hidden.")








        #selection sets menu 
        self.widgets["selectionSetMenuPopUp"] = cmds.popupMenu(b = 1, parent = self.widgets["pickerSelectionSets"])

        #create the selection popupMenu
        self.widgets["pickerSelectionToolPopup"] = cmds.popupMenu(b = 1, parent = self.widgets["pickerSelectTool"])
        self.widgets["pickerSelectionToolPopup_Controls"] = cmds.menuItem(label = "Select All Controls", parent = self.widgets["pickerSelectionToolPopup"], c = self.selectAll)
        self.widgets["pickerSelectionToolPopup_All"] = cmds.menuItem(label = "Select All (Controls + Spaces)", parent = self.widgets["pickerSelectionToolPopup"], c = self.selectEverything)
        self.widgets["pickerSelectionToolPopup_Settings"] = cmds.menuItem(label = "Select Rig Settings", parent = self.widgets["pickerSelectionToolPopup"], c = self.selectRigSettings)
        self.widgets["pickerSelectionToolPopup_Custom"] = cmds.menuItem(label = "Create Selection Set", parent = self.widgets["pickerSelectionToolPopup"], c = self.createSelectionSet)
        self.widgets["selectionSetsCustom"] = cmds.menuItem(label = "Selection Sets", parent = self.widgets["pickerSelectionToolPopup"], subMenu = True, tearOff = True)

        #create the reset popupMenu
        self.widgets["pickerResetToolPopup"] = cmds.popupMenu(b = 1, parent = self.widgets["pickerResetTool"])
        self.widgets["pickerResetToolPopup_All"] = cmds.menuItem(label = "Zero out All", parent = self.widgets["pickerResetToolPopup"], c = self.resetAll)
        self.widgets["pickerResetToolPopup_Selected"] = cmds.menuItem(label = "Zero out Selected", parent = self.widgets["pickerResetToolPopup"], c = self.resetSelection)



        #create the character picker for each character found in the scene
        characters = self.getCharacters()

        for character in characters:
            self.createCharacterPicker(character, self.widgets["pickerScroll"])



        #create the list view picker for each character found in the scene
        self.widgets["listViewLayout"] = cmds.formLayout(w = 400, h = 700, parent = self.widgets["mainLayout"])

        for character in characters:
            self.createListView(character, self.widgets["listViewLayout"])


        #create channel box layout
        self.widgets["cbFormLayout"] = cmds.formLayout("ART_cbFormLayout", w = 200, h = 700, parent = self.widgets["rowColLayout"])




        #create the rig settings tab
        self.widgets["rigSettingsLayout"] = cmds.formLayout(w = 400, h = 700, parent = self.widgets["mainLayout"])
        self.widgets["rigSettingsScroll"] = cmds.scrollLayout(w = 400, h = 700, hst = 0, parent = self.widgets["rigSettingsLayout"])

        for character in characters:
            self.createRigSettings(character, self.widgets["rigSettingsScroll"])





        #name the tabs
        cmds.tabLayout(self.widgets["mainLayout"], edit = True, tabLabel = [(self.widgets["pickerLayout"], "Picker"), (self.widgets["listViewLayout"], "List View"), (self.widgets["rigSettingsLayout"], "Rig Settings")])


        #show window
        self.widgets["dock"] = cmds.dockControl("artAnimUIDock", label = "Animation Interface", content = self.widgets["window"], area = "right", allowedArea = "right", visibleChangeCommand = self.interfaceScriptJob)


        #add attributes to controls
        self.setupButtonAttrsOnControls()

        #setup selection scriptJob
        self.scriptJob = cmds.scriptJob(parent = self.widgets["window"], event = ["SelectionChanged", self.selectionScriptJob], kws = True)

        #set the current selected character and change the thumbnail
        selected = characters[len(characters)-1]
        self.setThumbnail(selected, self.project)
        self.switchActiveCharacter(selected)


        #get all controls
        self.controls = []
        for control in ["head_fk_anim", "neck_01_fk_anim", "neck_02_fk_anim", "neck_03_fk_anim", "spine_01_anim", "spine_02_anim", "spine_03_anim", "spine_04_anim", "spine_05_anim", "mid_ik_anim", "chest_ik_anim",
                        "body_anim", "hip_anim", "clavicle_l_anim", "clavicle_r_anim", "fk_arm_l_anim", "fk_arm_r_anim", "fk_elbow_l_anim", "fk_elbow_r_anim", "fk_wrist_l_anim", "fk_wrist_r_anim",
                        "ik_elbow_l_anim", "ik_elbow_r_anim", "ik_wrist_l_anim", "ik_wrist_r_anim", "fk_thigh_l_anim", "fk_thigh_r_anim", "fk_calf_l_anim", "fk_calf_r_anim", "fk_foot_l_anim", "fk_foot_r_anim",
                        "fk_ball_l_anim", "fk_ball_r_anim", "ik_foot_anim_l", "ik_foot_anim_r", "heel_ctrl_l", "heel_ctrl_r", "toe_wiggle_ctrl_l", "toe_wiggle_ctrl_r",
                        "toe_tip_ctrl_l", "toe_tip_ctrl_r", "master_anim", "offset_anim", "root_anim", "upperarm_l_twist_anim", "upperarm_l_twist_2_anim", "upperarm_l_twist_3_anim", "upperarm_r_twist_anim", "upperarm_r_twist_2_anim", "upperarm_r_twist_3_anim", "l_thigh_twist_01_anim", "r_thigh_twist_01_anim",
                        "pinky_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_r", "pinky_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_r",
                        "ring_metacarpal_ctrl_l", "ring_metacarpal_ctrl_r", "ring_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_r",
                        "middle_metacarpal_ctrl_l", "middle_metacarpal_ctrl_r", "middle_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_r",
                        "index_metacarpal_ctrl_l", "index_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_l", "index_finger_fk_ctrl_3_r",
                        "thumb_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_3_l", "thumb_finger_fk_ctrl_3_r",
                        "index_l_ik_anim", "index_r_ik_anim", "middle_l_ik_anim", "middle_r_ik_anim", "ring_l_ik_anim", "ring_r_ik_anim", "pinky_l_ik_anim", "pinky_r_ik_anim", "thumb_l_ik_anim", "thumb_r_ik_anim",
                        "index_l_poleVector", "index_r_poleVector", "middle_l_poleVector", "middle_r_poleVector", "ring_l_poleVector", "ring_r_poleVector", "pinky_l_poleVector", "pinky_r_poleVector", "thumb_l_poleVector", "thumb_r_poleVector",
                        "l_global_ik_anim", "r_global_ik_anim", "lowerarm_l_twist_anim", "lowerarm_l_twist2_anim", "lowerarm_l_twist3_anim", "lowerarm_r_twist_anim", "lowerarm_r_twist2_anim", "lowerarm_r_twist3_anim", "calf_r_twist_anim", "calf_r_twist2_anim", "calf_r_twist3_anim",
                        "calf_l_twist_anim", "calf_l_twist2_anim", "calf_l_twist3_anim", "thigh_l_twist_2_anim", "thigh_l_twist_3_anim", "thigh_r_twist_2_anim", "thigh_r_twist_3_anim"]:
            self.controls.append(control)

        #hack
        character = selected
        for obj in ["fk_clavicle_l_anim", "fk_clavicle_r_anim"]:
            if cmds.objExists(character + ":" + obj):
                self.controls.append(obj)

        #find custom joints
        character = selected
        customJoints = []
        attrs = cmds.listAttr(character + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        for joint in customJoints:
            attribute = cmds.getAttr(character + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "leaf":
                label = label.partition(" (")[0]
                control = label + "_anim"
                self.controls.append(control)

            if jointType == "jiggle":
                control = label + "_anim"
                self.controls.append(control)

            if jointType == "chain" or jointType == "dynamic":
                numJointsInChain = label.partition("(")[2].partition(")")[0]
                label = label.partition(" (")[0]

                self.controls.append(label + "_dyn_anim")

                cmds.select("*:" + label + "_ik_*_anim")
                selection = cmds.ls(sl = True)
                for each in selection:
                    niceName = each.partition(":")[2]
                    self.controls.append(niceName)		

                for i in range(int(numJointsInChain)):
                    self.controls.append("fk_" + label + "_0" + str(i + 1) + "_anim")
                    self.controls.append(label + "_cv_" + str(i) + "_anim")		    



        #load UI settings
        self.loadUISettings()
        self.findCustomSelectionSets()

        #create script job for updating ui
        self.updateUI_scriptJob()

        #setup the scriptJob
        cmds.scriptJob(event = ["readingFile", self.killUIScriptJob], runOnce = True, kws = True)
        cmds.scriptJob(event = ["SceneSaved", self.exitIsoOnSave], parent = self.widgets["window"], kws = True, runOnce = True)




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def bakeDynToFK(self, *args):

        #find all of the dynamic controls
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        cmds.select(character + ":" + "*dyn_anim")
        dynControls = cmds.ls(sl = True)

        if len(dynControls) > 0:

            #list the controls in a UI
            if cmds.window("bakeDynToFKControls_UI", exists = True):
                cmds.deleteUI("bakeDynToFKControls_UI")

            window = cmds.window("bakeDynToFKControls_UI", title = "Bake Dynamics", w = 300, h = 400, sizeable = True, mnb = False, mxb = False)
            mainLayout = cmds.formLayout(w = 300, h = 400)

            #textScrollList
            self.widgets["bakeDynToFK_List"] = cmds.textScrollList(w = 200, h = 300, allowMultiSelection = True, parent = mainLayout)

            for control in dynControls:
                cmds.textScrollList(self.widgets["bakeDynToFK_List"], edit = True, append = control)

            cmds.formLayout(mainLayout, edit = True, af = [(self.widgets["bakeDynToFK_List"], "top", 50), (self.widgets["bakeDynToFK_List"], "left", 5)] )

            #process button
            button = cmds.button(w = 80, h = 40, label = "Bake", c = self.bakeDynToFK_Process)
            cmds.formLayout(mainLayout, edit = True, af = [(button, "bottom", 50),(button, "right", 5)])
            cmds.showWindow(window)


        else:
            cmds.warning("No Dynamic controls found on the current character.")
            return


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def bakeDynToFK_Process(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        #get the selected controls in the list
        controlsToBake = cmds.textScrollList(self.widgets["bakeDynToFK_List"], q = True, si = True)

        if controlsToBake != None:

            #find the corresponding fk controls
            for control in controlsToBake:
                name = control.partition(":")[2].partition("_dyn_anim")[0]

                masterGrp = character + ":" + name + "_master_ctrl_grp"

                cmds.select(masterGrp, hi = True)
                nodes = cmds.ls(sl = True)

                fkControls = []
                for node in nodes:
                    if node.find(":fk_") != -1:
                        if node.find("_anim") != -1:
                            if cmds.nodeType(node) == "transform":
                                fkControls.append(node)


                #constrain the fk controls to the dny joints
                #Husk_Base:fk_hood_01_anim
                constraints = []
                for control in fkControls:
                    niceName = control.partition("fk_")[2].partition("_anim")[0]
                    joint = character + ":" + "rig_dyn_" + niceName

                    constraint = cmds.orientConstraint(joint, control)[0]
                    constraints.append(constraint)

                #select the FK controls and bake
                start = cmds.playbackOptions(q = True, min = True)
                end = cmds.playbackOptions(q = True, max = True)

                cmds.select(clear = True)
                for control in fkControls:
                    cmds.select(control, add = True)

                cmds.bakeResults(simulation = True, time = (start, end))
                cmds.delete(constraints)

                #set to FK
                cmds.setAttr(character + ":" + "Rig_Settings." + name + "_fk", 1)
                cmds.setAttr(character + ":" + "Rig_Settings." + name + "_ik", 0)
                cmds.setAttr(character + ":" + "Rig_Settings." + name + "_dynamic", 0)

        #delete the UI
        cmds.deleteUI("bakeDynToFKControls_UI")



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getIsoSelectionPolygons(self, *args):

        cmds.progressWindow(title='Animation UI', progress=0, status='Building Iso Selection Sets', isInterruptable=True )
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)


        #create the list of iso selection polygons
        self.torsoFaces = []
        self.leftArmFaces = []
        self.rightArmFaces = []
        self.leftLegFaces = []
        self.rightLegFaces = []
        self.headFaces = []

        #lists of what joints belong to which "part"
        torso = ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_l", "clavicle_r"]
        leftArm = ["upperarm_l", "lowerarm_l", "hand_l", "index_metacarpal_l", "index_01_l", "index_02_l", "index_03_l", "middle_metacarpal_l", "middle_01_l", "middle_02_l", "middle_03_l", "ring_metacarpal_l", "ring_01_l", "ring_02_l", "ring_03_l", "pinky_metacarpal_l", "pinky_01_l", "pinky_02_l", "pinky_03_l", "thumb_01_l", "thumb_02_l", "thumb_03_l", "lowerarm_twist_01_l", "lowerarm_twist_02_l", "lowerarm_twist_03_l", "upperarm_twist_01_l", "upperarm_twist_02_l", "upperarm_twist_03_l" ]
        rightArm = ["upperarm_r", "lowerarm_r", "hand_r", "index_metacarpal_r", "index_01_r", "index_02_r", "index_03_r", "middle_metacarpal_r", "middle_01_r", "middle_02_r", "middle_03_r", "ring_metacarpal_r", "ring_01_r", "ring_02_r", "ring_03_r", "pinky_metacarpal_r", "pinky_01_r", "pinky_02_r", "pinky_03_r", "thumb_01_r", "thumb_02_r", "thumb_03_r", "lowerarm_twist_01_r", "lowerarm_twist_02_r", "lowerarm_twist_03_r", "upperarm_twist_01_r", "upperarm_twist_02_r", "upperarm_twist_03_r" ]
        leftLeg = ["thigh_l", "calf_l", "foot_l", "ball_l", "thigh_twist_01_l", "thigh_twist_02_l", "thigh_twist_03_l", "calf_twist_01_l", "calf_twist_02_l", "calf_twist_03_l"]
        rightLeg = ["thigh_r", "calf_r", "foot_r", "ball_r", "thigh_twist_01_r", "thigh_twist_02_r", "thigh_twist_03_r", "calf_twist_01_r", "calf_twist_02_r", "calf_twist_03_r"]
        head = ["neck_01", "neck_02", "neck_03", "head"]
        characterGeo = []


        #find all of the skin clusters. In each one, find the weighted joints and the geometry weighted to those joints
        skinClusters = cmds.ls(type = 'skinCluster')


        for skin in skinClusters:
            weightedJoints = cmds.skinCluster(skin, q = True, weightedInfluence = True)

            for joint in weightedJoints:
                #add faces to the iso selection lists
                if joint.partition(character + ":")[2] in torso:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "Torso", skin])

                if joint.partition(character + ":")[2] in leftArm:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "LeftArm", skin])

                if joint.partition(character + ":")[2] in rightArm:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "RightArm", skin])


                if joint.partition(character + ":")[2] in leftLeg:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "LeftLeg", skin])


                if joint.partition(character + ":")[2] in rightLeg:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "RightLeg", skin])


                if joint.partition(character + ":")[2] in head:
                    geometryShape = cmds.skinCluster(skin, q = True, geometry = True)
                    geometry = cmds.listRelatives(geometryShape, parent = True)[0]
                    characterGeo.append([geometry, "Head", skin])




        progress = 100/len(characterGeo)
        originalProgress = 100/len(characterGeo)

        for geo in characterGeo:
            geom = geo[0]
            part = geo[1]
            skin = geo[2]

            polys = cmds.polyEvaluate(geom, face = True)

            cmds.progressWindow(edit = True, progress = progress, status='Building Iso Selection Sets')
            progress = progress + originalProgress


            for i in range(int(polys)):
                transforms = cmds.skinPercent( skin, geom + ".f[" + str(i) + "]", ib = .25, query=True, t= None)
                if transforms != None:

                    if part == "Torso":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in torso:
                                self.torsoFaces.append(geom + ".f[" + str(i) + "]")

                    if part == "LeftArm":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in leftArm:
                                self.leftArmFaces.append(geom + ".f[" + str(i) + "]")

                    if part == "RightArm":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in rightArm:
                                self.rightArmFaces.append(geom + ".f[" + str(i) + "]")

                    if part == "LeftLeg":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in leftLeg:
                                self.leftLegFaces.append(geom + ".f[" + str(i) + "]")

                    if part == "RightLeg":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in rightLeg:
                                self.rightLegFaces.append(geom + ".f[" + str(i) + "]")

                    if part == "Head":
                        for transform in transforms:
                            if transform.partition(character + ":")[2] in head:
                                self.headFaces.append(geom + ".f[" + str(i) + "]")




        cmds.progressWindow(endProgress=1)

        #enable menu items
        cmds.menuItem(self.widgets["isoSelect_Torso"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_LeftArm"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_RightArm"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_LeftLeg"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_RightLeg"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_Head"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_ShowAll"], edit = True, enable = True)
        cmds.menuItem(self.widgets["isoSelect_Generate"], edit = True, enable = False)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def exitIsoOnSave(self, *args):
        #unIsolate
        if self.mats != []:
            for mat in self.mats:
                face = mat[0]
                sg = mat[1]
                cmds.sets(face, forceElement = sg)


        if cmds.objExists("isoSelect_M"):
            cmds.delete("isoSelect_M")

        if cmds.objExists("isoSelect_Set"):
            cmds.delete("isoSelect_Set")

        for checkbox in [[self.widgets["isoSelect_Torso"], "Torso"], [self.widgets["isoSelect_LeftArm"], "LeftArm"], [self.widgets["isoSelect_RightArm"], "RightArm"], [self.widgets["isoSelect_LeftLeg"], "LeftLeg"], [self.widgets["isoSelect_RightLeg"], "RightLeg"], [self.widgets["isoSelect_Head"], "Head"]]:
            cb = checkbox[0]
            cmds.menuItem(cb, edit = True, cb = True)


        #ReSave Scene
        filename = cmds.file(q = True, sceneName = True)
        filetype = filename.rpartition(".")[2]

        if filetype == "mb":
            filetype = "mayaBinary"

        if filetype == "ma":
            filetype = "mayaAscii"

        cmds.file(save = True, force = True, type = filetype)

        cmds.scriptJob(event = ["SceneSaved", self.exitIsoOnSave], parent = self.widgets["window"], kws = True, runOnce = True)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def exitIso(self, *args):

        #unIsolate

        isoPnl = cmds.getPanel(wf=True)
        try:
            isoCrnt = cmds.isolateSelect(isoPnl, q=True, s=True)
            if isoCrnt != False:
                mel.eval('enableIsolateSelect %s %d' % (isoPnl,not isoCrnt) )

        except:
            cmds.warning("Invalid viewport for isolate select command")


        if self.mats != []:
            for mat in self.mats:
                face = mat[0]
                sg = mat[1]
                cmds.sets(face, forceElement = sg)


        if cmds.objExists("isoSelect_M"):
            cmds.delete("isoSelect_M")

        if cmds.objExists("isoSelect_Set"):
            cmds.delete("isoSelect_Set")

        for checkbox in [[self.widgets["isoSelect_Torso"], "Torso"], [self.widgets["isoSelect_LeftArm"], "LeftArm"], [self.widgets["isoSelect_RightArm"], "RightArm"], [self.widgets["isoSelect_LeftLeg"], "LeftLeg"], [self.widgets["isoSelect_RightLeg"], "RightLeg"], [self.widgets["isoSelect_Head"], "Head"]]:
            cb = checkbox[0]
            cmds.menuItem(cb, edit = True, cb = True)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def isoSelect(self, *args):

        #unIsolate

        isoPnl = cmds.getPanel(wf=True)
        try:
            isoCrnt = cmds.isolateSelect(isoPnl, q=True, s=True)
            if isoCrnt != False:
                mel.eval('enableIsolateSelect %s %d' % (isoPnl,not isoCrnt) )

        except:
            cmds.warning("Invalid viewport for isolate select command")


        if self.mats != []:
            for mat in self.mats:
                face = mat[0]
                sg = mat[1]
                cmds.sets(face, forceElement = sg)


        if cmds.objExists("isoSelect_M"):
            cmds.delete("isoSelect_M")

        if cmds.objExists("isoSelect_Set"):
            cmds.delete("isoSelect_Set")



        #find isolation method
        classic = cmds.menuItem(self.widgets["isoMethodClassic"], q = True, rb = True)
        material = cmds.menuItem(self.widgets["isoMethodMaterial"], q = True, rb = True)


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        if material:

            #find checkbox values
            isolatedItems = []


            for checkbox in [[self.widgets["isoSelect_Torso"], "Torso"], [self.widgets["isoSelect_LeftArm"], "LeftArm"], [self.widgets["isoSelect_RightArm"], "RightArm"], [self.widgets["isoSelect_LeftLeg"], "LeftLeg"], [self.widgets["isoSelect_RightLeg"], "RightLeg"], [self.widgets["isoSelect_Head"], "Head"]]:
                cb = checkbox[0]
                part = checkbox[1]

                value = cmds.menuItem(cb, q = True, cb = True)
                if value == False:

                    if part == "Torso":
                        cmds.select(self.torsoFaces)
                        isolatedItems.append(self.torsoFaces)

                    if part == "LeftArm":
                        isolatedItems.append(self.leftArmFaces)

                    if part == "RightArm":
                        isolatedItems.append(self.rightArmFaces)

                    if part == "LeftLeg":
                        isolatedItems.append(self.leftLegFaces)

                    if part == "RightLeg":
                        isolatedItems.append(self.rightLegFaces)

                    if part == "Head":
                        isolatedItems.append(self.headFaces)



            #create your iso selection
            cmds.select(clear = True)
            for each in [self.torsoFaces, self.leftArmFaces, self.rightArmFaces, self.leftLegFaces, self.rightLegFaces, self.headFaces]:
                if each in isolatedItems:
                    cmds.select(each, add = True)



            #grab the current selection
            selection = cmds.ls(sl = True)
            self.mats = []

            #get assinged material
            faces = cmds.ls(sl = True)
            for face in faces:
                shaders = cmds.ls(type = "shadingEngine")

                for each in shaders:
                    connectedFaces = cmds.sets(each, q = True)
                    if connectedFaces != None:
                        for obj in connectedFaces:


                            if obj.find(face.rpartition(".")[0]) != -1:
                                self.mats.append([face, each])


            newMat = cmds.shadingNode("lambert", asShader = True, name = "isoSelect_M")
            cmds.setAttr(newMat + ".transparency", 1, 1, 1, type = "double3")

            shadingGroup = cmds.sets(name = "isoSelect_Set", renderable = True, noSurfaceShader = True, empty = True)
            cmds.connectAttr(newMat + ".outColor", shadingGroup + ".surfaceShader")

            if selection:
                cmds.select(selection)

                for each in selection:
                    cmds.sets(each, forceElement = shadingGroup)

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        if classic:
            #find checkbox values
            isolatedItems = []

            cmds.select(clear = True)
            for checkbox in [[self.widgets["isoSelect_Torso"], "Torso"], [self.widgets["isoSelect_LeftArm"], "LeftArm"], [self.widgets["isoSelect_RightArm"], "RightArm"], [self.widgets["isoSelect_LeftLeg"], "LeftLeg"], [self.widgets["isoSelect_RightLeg"], "RightLeg"], [self.widgets["isoSelect_Head"], "Head"]]:
                cb = checkbox[0]
                part = checkbox[1]

                value = cmds.menuItem(cb, q = True, cb = True)


                if value == True:

                    if part == "Torso":
                        cmds.select(self.torsoFaces, add = True)

                    if part == "LeftArm":
                        cmds.select(self.leftArmFaces, add = True)

                    if part == "RightArm":
                        cmds.select(self.rightArmFaces, add = True)

                    if part == "LeftLeg":
                        cmds.select(self.leftLegFaces, add = True)

                    if part == "RightLeg":
                        cmds.select(self.rightLegFaces, add = True)

                    if part == "Head":
                        cmds.select(self.headFaces, add = True)



            #isolate the selection
            isoPnl = cmds.getPanel(wf=True)
            try:
                isoCrnt = cmds.isolateSelect(isoPnl, q=True, s=True)
                mel.eval('enableIsolateSelect %s %d' % (isoPnl,not isoCrnt) )

            except:
                cmds.warning("Invalid viewport for isolate select command")


        cmds.select(clear = True)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def killUIScriptJob(self):



        #delete script job
        cmds.scriptJob(kill = self.mainScriptJob)

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


            cmds.deleteUI("artAnimUIDock")
            if cmds.window("artAnimUI", exists = True):
                cmds.deleteUI("artAnimUI")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def interfaceScriptJob(self, *args):

        #unisolate any isolated parts
        self.exitIso()



        if cmds.dockControl(self.widgets["dock"], q = True, visible = True) == False:
            #re-sort out the channel box
            channelBox = cmds.formLayout("ART_cbFormLayout", q = True, childArray = True)
            if channelBox != None:
                channelBox = channelBox[0]

                #reparent the channelBox Layout back to maya's window
                cmds.control(channelBox, e = True, p = "MainChannelsLayersLayout")
                channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
                channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')

                #edit the channel box pane's attachment to the formLayout
                cmds.formLayout(channelBoxLayout, edit = True, af = [(channelBox, "left", 0),(channelBox, "right", 0), (channelBox, "bottom", 0)], attachControl = (channelBox, "top", 0, channelBoxForm))


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def loadUISettings(self, *args):
        settingsLocation = self.mayaToolsDir + "/General/Scripts/settings.txt"
        if os.path.exists(settingsLocation):
            f = open(settingsLocation, 'r')
            settings = cPickle.load(f)

            #set the UI settings based on file
            channelBox = settings.get("ChannelBox")
            if channelBox == True:
                cmds.menuItem(self.widgets["menuBar_settings_channelBox"], edit = True, checkBox = True)
                self.showChannelBox()

            if channelBox == False:
                cmds.menuItem(self.widgets["menuBar_settings_channelBox"], edit = True, checkBox = False)
                self.showChannelBox()

            #space switch settings
            match = settings.get("Match")
            if match == True:
                cmds.menuItem(self.widgets["spaceSwitch_MatchToggleCB"], edit = True, checkBox = True)

            if match == False:
                cmds.menuItem(self.widgets["spaceSwitch_MatchToggleCB"], edit = True, checkBox = False)


            f.close()




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def saveUISettings(self, *args):

        #this function will save out the user's preferences they have set in the UI to disk
        settingsLocation = self.mayaToolsDir + "/General/Scripts/settings.txt"
        f = open(settingsLocation, 'w')

        #Channel Box display settings
        value = cmds.menuItem(self.widgets["menuBar_settings_channelBox"], q = True, checkBox = True)

        #Space switch settings
        match = cmds.menuItem(self.widgets["spaceSwitch_MatchToggleCB"], q = True, cb = True)
        method = cmds.menuItem(self.widgets["spaceSwitch_MatchMethodCB"], q = True, cb = True)

        #create a dictionary with these values
        settings = {}
        settings["ChannelBox"] = value
        settings["Match"] = match
        settings["MatchMethod"] = method

        #write our dictionary to file
        cPickle.dump(settings, f)
        f.close()

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def showChannelBox(self, *args):
        #get the value of the checkbox in the menu Item
        value = cmds.menuItem(self.widgets["menuBar_settings_channelBox"], q = True, checkBox = True)
        self.channelBoxLayout = mel.eval('$temp1=$gChannelsLayersForm')
        self.channelBoxForm = mel.eval('$temp1 = $gChannelButtonForm')




        if value == True:
            self.channelBox = mel.eval('$temp1=$gChannelsLayersPane;')

            #unhide the column that will house the channel box
            cmds.rowColumnLayout(self.widgets["rowColLayout"], edit = True, cw = [(1, 400), (2, 50), (3, 220)])

            #parent the channel box to our anim UI
            cmds.control(self.channelBox, e = True, p = self.widgets["cbFormLayout"])
            cmds.formLayout(self.widgets["cbFormLayout"], edit = True, af = [(self.channelBox, "left", 0),(self.channelBox, "right", 0), (self.channelBox, "bottom", 0), (self.channelBox, "top", 0)])
            channelBox = cmds.formLayout(self.widgets["cbFormLayout"], q = True, childArray = True)[0]
            self.channelBox = channelBox

        if value == False:

            #hide the column for the channel box in our anim UI
            cmds.rowColumnLayout(self.widgets["rowColLayout"], edit = True, cw = [(1, 400), (2, 50), (3, 1)])

            try:
                #reparent the channelBox Layout back to maya's window
                cmds.control(self.channelBox, e = True, p = "MainChannelsLayersLayout")

                #edit the channel box pane's attachment to the formLayout
                cmds.formLayout(self.channelBoxLayout, edit = True, af = [(self.channelBox, "left", 0),(self.channelBox, "right", 0), (self.channelBox, "bottom", 0)], attachControl = (self.channelBox, "top", 0, self.channelBoxForm))

            except AttributeError:
                print "channel box restored"


        self.saveUISettings()


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def setThumbnail(self, characterName, project):
        projects = os.listdir(self.mayaToolsDir + "/General/Icons/ART/Thumbnails/")
        thumbnailPath = self.mayaToolsDir + "/General/Icons/ART/Thumbnails/" + self.project + "/"


        thumbs = os.listdir(thumbnailPath)


        found = False
        for thumb in thumbs:
            if thumb.find("_small") != -1:
                thumbName = thumb.rpartition(".")[0]
                thumbName = thumbName.partition("_small")[0]
                if thumbName.find(characterName[0:-1]) == 0:
                    cmds.symbolButton(self.widgets["activeCharacterThumb"], edit = True, image = thumbnailPath + thumb, ann = characterName)
                    found = True


        #if our character wasn't in the self.project, but possibly another project, check now
        if found == False:
            for project in projects:

                thumbnailPath = self.mayaToolsDir + "/General/Icons/ART/Thumbnails/" + project + "/"
                thumbs = os.listdir(thumbnailPath)
                for thumb in thumbs:
                    if thumb.find("_small") != -1:
                        thumbName = thumb.rpartition(".")[0]
                        thumbName = thumbName.partition("_small")[0]
                        if thumbName.find(characterName[0:-1]) == 0:
                            cmds.symbolButton(self.widgets["activeCharacterThumb"], edit = True, image = thumbnailPath + thumb, ann = characterName)


        #lstly, repopulate self.controls
        self.controls = []
        for control in ["head_fk_anim", "neck_01_fk_anim", "neck_02_fk_anim", "neck_03_fk_anim", "spine_01_anim", "spine_02_anim", "spine_03_anim", "spine_04_anim", "spine_05_anim", "mid_ik_anim", "chest_ik_anim",
                        "body_anim", "hip_anim", "clavicle_l_anim", "clavicle_r_anim", "fk_arm_l_anim", "fk_arm_r_anim", "fk_elbow_l_anim", "fk_elbow_r_anim", "fk_wrist_l_anim", "fk_wrist_r_anim",
                        "ik_elbow_l_anim", "ik_elbow_r_anim", "ik_wrist_l_anim", "ik_wrist_r_anim", "fk_thigh_l_anim", "fk_thigh_r_anim", "fk_calf_l_anim", "fk_calf_r_anim", "fk_foot_l_anim", "fk_foot_r_anim",
                        "fk_ball_l_anim", "fk_ball_r_anim", "ik_foot_anim_l", "ik_foot_anim_r", "heel_ctrl_l", "heel_ctrl_r", "toe_wiggle_ctrl_l", "toe_wiggle_ctrl_r",
                        "toe_tip_ctrl_l", "toe_tip_ctrl_r", "master_anim", "offset_anim", "root_anim", "upperarm_l_twist_anim", "upperarm_l_twist_2_anim", "upperarm_l_twist_3_anim", "upperarm_r_twist_anim", "upperarm_r_twist_2_anim", "upperarm_r_twist_3_anim", "l_thigh_twist_01_anim", "r_thigh_twist_01_anim",
                        "pinky_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_r", "pinky_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_r",
                        "ring_metacarpal_ctrl_l", "ring_metacarpal_ctrl_r", "ring_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_r",
                        "middle_metacarpal_ctrl_l", "middle_metacarpal_ctrl_r", "middle_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_r",
                        "index_metacarpal_ctrl_l", "index_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_l", "index_finger_fk_ctrl_3_r",
                        "thumb_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_3_l", "thumb_finger_fk_ctrl_3_r",
                        "index_l_ik_anim", "index_r_ik_anim", "middle_l_ik_anim", "middle_r_ik_anim", "ring_l_ik_anim", "ring_r_ik_anim", "pinky_l_ik_anim", "pinky_r_ik_anim", "thumb_l_ik_anim", "thumb_r_ik_anim",
                        "index_l_poleVector", "index_r_poleVector", "middle_l_poleVector", "middle_r_poleVector", "ring_l_poleVector", "ring_r_poleVector", "pinky_l_poleVector", "pinky_r_poleVector", "thumb_l_poleVector", "thumb_r_poleVector",
                        "l_global_ik_anim", "r_global_ik_anim", "lowerarm_l_twist_anim", "lowerarm_l_twist2_anim", "lowerarm_l_twist3_anim", "lowerarm_r_twist_anim", "lowerarm_r_twist2_anim", "lowerarm_r_twist3_anim", "calf_r_twist_anim", "calf_r_twist2_anim", "calf_r_twist3_anim",
                        "calf_l_twist_anim", "calf_l_twist2_anim", "calf_l_twist3_anim", "thigh_l_twist_2_anim", "thigh_l_twist_3_anim", "thigh_r_twist_2_anim", "thigh_r_twist_3_anim"]:
            self.controls.append(control)


        #hack
        character = characterName
        for obj in ["fk_clavicle_l_anim", "fk_clavicle_r_anim"]:
            if cmds.objExists(character + ":" + obj):
                self.controls.append(obj)	

        #find custom joints
        character = characterName
        customJoints = []
        attrs = cmds.listAttr(character + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        for joint in customJoints:
            attribute = cmds.getAttr(character + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "leaf":
                label = label.partition(" (")[0]
                control = label + "_anim"
                self.controls.append(control)

            if jointType == "jiggle":
                control = label + "_anim"
                self.controls.append(control)

            if jointType == "chain" or jointType == "dynamic":
                numJointsInChain = label.partition("(")[2].partition(")")[0]
                label = label.partition(" (")[0]
                self.controls.append(label + "_dyn_anim")

                cmds.select("*:" + label + "_ik_*_anim")
                selection = cmds.ls(sl = True)
                for each in selection:
                    niceName = each.partition(":")[2]
                    self.controls.append(niceName)		

                for i in range(int(numJointsInChain)):
                    self.controls.append("fk_" + label + "_0" + str(i + 1) + "_anim")
                    self.controls.append(label + "_cv_" + str(i) + "_anim")	


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def populateCharacterRigList(self, *args):

        characters = self.getCharacters()
        for character in characters:
            cmds.menuItem(label = character, parent = self.widgets["characterRigList"], c = partial(self.switchActiveCharacter, character))


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchActiveCharacter(self, character, *args):

        #change the thumbnail
        self.setThumbnail(character, self.project)

        #change the visibility of the character pickers and show the correct one
        characters = self.getCharacters()
        for char in characters:
            cmds.columnLayout(self.widgets[char + "_characterPickerLayout"], edit = True, visible = False)
            cmds.columnLayout(self.widgets[char + "_rigSettingsMainColumn"], edit = True, visible = False)
            cmds.scrollLayout(self.widgets[char + "_listViewMainLayout"], edit = True, visible = False)

        cmds.columnLayout(self.widgets[character + "_characterPickerLayout"], edit = True, visible = True)
        cmds.columnLayout(self.widgets[character + "_rigSettingsMainColumn"], edit = True, visible = True)
        cmds.scrollLayout(self.widgets[character + "_listViewMainLayout"], edit = True, visible = True)


        #set the visibility toggle iconTextCheckBox to represent the current character's control visibility
        shape = cmds.listRelatives(character + ":" + "body_anim", shapes = True)[0]
        visible = cmds.getAttr(shape + ".v")

        if visible == False:
            cmds.iconTextCheckBox(self.widgets["pickerControlVisibility"], edit = True, value = False)

        if visible == True:
            cmds.iconTextCheckBox(self.widgets["pickerControlVisibility"], edit = True, value = True)

        #check to see if space switch window is open
        if cmds.window("spaceSwitcherUI", exists = True):
            title = cmds.window("spaceSwitcherUI", q = True, title = True)
            if character != title:
                if cmds.button("spaceSwitchSyncStatusButton", q = True, exists = True):
                    cmds.button("spaceSwitchSyncStatusButton", edit = True, visible = True)

                else:
                    cmds.deleteUI("spaceSwitcherUI")


        #check to see if pose editor is open
        if cmds.window("poseEditorUI", exists = True):
            peTitle = cmds.window("poseEditorUI", q = True, title = True)
            if character != peTitle:
                if cmds.button("poseEditor_syncStatusButton", q = True, exists = True):
                    cmds.button("poseEditor_syncStatusButton", edit = True, visible = True)

                else:
                    cmds.deleteUI("poseEditorUI")

        #enable menu items
        cmds.menuItem(self.widgets["isoSelect_Torso"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_LeftArm"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_RightArm"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_LeftLeg"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_RightLeg"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_Head"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_ShowAll"], edit = True, enable = False)
        cmds.menuItem(self.widgets["isoSelect_Generate"], edit = True, enable = True)








    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getCharacters(self):

        referenceNodes = []
        references = cmds.ls(type = "reference")
        print references

        for reference in references:
            niceName = reference.rpartition("RN")[0]
            print niceName
            suffix = reference.rpartition("RN")[2]
            print suffix
            if suffix != "":
                if cmds.objExists(niceName + suffix + ":" + "Skeleton_Settings"):
                    referenceNodes.append(niceName + suffix)

            else:
                if cmds.objExists(niceName + ":" + "Skeleton_Settings"):
                    referenceNodes.append(niceName)

        print referenceNodes
        return referenceNodes


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createCharacterPicker(self, name, layout):
        #fist thing to do is create the form layout that needs to be parented to the passed in layout
        self.widgets[name + "_characterPickerLayout"] = cmds.columnLayout(w = 470, h = 700, parent = layout, visible = False)
        self.formsToHide.append(self.widgets[name + "_characterPickerLayout"])

        #setup button color variables
        self.blue = [.09, .75, .96]
        self.white = [1, 1, 1]
        self.orange = [1, .68, 0]
        self.purple = [.5, .09, .96]
        self.green = [0, 1, .16]



        #create the body frame Layout
        self.widgets[name + "_bodyFrame"] = cmds.frameLayout(label = "Body", collapse = False, collapsable = True, borderStyle = "in", w = 370, h = 470, parent = self.widgets[name + "_characterPickerLayout"], cc = partial(self.collapseCommand, name, "Body"), ec = partial(self.expandCommand, name, "Body"))
        self.widgets[name + "_bodyPickerForm"] = cmds.formLayout(w = 370, h = 470, parent = self.widgets[name + "_bodyFrame"])

        #background image for body picker
        image = self.mayaToolsDir + "/General/Icons/ART/picker.jpg"
        self.widgets[name + "_cpBackground"] =  cmds.image(w = 370, h = 450, image = image, parent = self.widgets[name + "_bodyPickerForm"])


        #create the body picker controls
        self.createBodyPicker(name, self.widgets[name + "_bodyPickerForm"])


        #create the fingers frame Layout
        self.widgets[name + "_fingersFrame"] = cmds.frameLayout(label = "Fingers", collapse = False, collapsable = True, borderStyle = "in", w = 370, h = 205, parent = self.widgets[name + "_characterPickerLayout"], cc = partial(self.collapseCommand, name, "Fingers"), ec = partial(self.expandCommand, name, "Fingers"))
        self.widgets[name + "_fingerPickerForm"] = cmds.formLayout(w = 370, h = 205, parent = self.widgets[name + "_fingersFrame"])

        #background image for finger picker
        image = self.mayaToolsDir + "/General/Icons/ART/fingerPicker.jpg"
        self.widgets[name + "_fingerPickerBackground"] =  cmds.image(w = 370, h = 205, image = image, parent = self.widgets[name + "_fingerPickerForm"])

        #create the finger picker controls
        self.createFingersPicker(name, self.widgets[name + "_fingerPickerForm"])

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createBodyPicker(self, name, layout, *args):

        #get settings off of skeleton settings node to know what it is we need to create
        numNeckBones = cmds.getAttr(name + ":" + "Skeleton_Settings.numNeckBones")
        numSpineBones = cmds.getAttr(name + ":" + "Skeleton_Settings.numSpineBones")
        leftArmTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.leftUpperArmTwist")
        leftArmForeTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.leftLowerArmTwist")
        rightArmTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.rightUpperArmTwist")
        rightArmForeTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.rightLowerArmTwist")
        leftThighTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.leftUpperLegTwist")
        leftCalfTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.leftLowerLegTwist")
        rightThighTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.rightUpperLegTwist")
        rightCalfTwist = cmds.getAttr(name + ":" + "Skeleton_Settings.rightLowerLegTwist")
        leftBall = cmds.getAttr(name + ":" + "Skeleton_Settings.leftball")
        rightBall = cmds.getAttr(name + ":" + "Skeleton_Settings.rightball")
        numLeftToes = cmds.getAttr(name + ":" + "Skeleton_Settings.numLeftToes", asString = True)
        numRightToes = cmds.getAttr(name + ":" + "Skeleton_Settings.numRightToes", asString = True)


        #create and place each body part's buttons
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #head
        self.widgets[name + "_headPickerButton"] = cmds.button(w = 50, h = 50, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "head_fk_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_headPickerButton"], "top", 23), (self.widgets[name + "_headPickerButton"], "left", 159)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #neck
        buttonHeight = int(40/numNeckBones)
        if int(numNeckBones) == 3:
            basePosition = 103
        if int(numNeckBones) == 2:
            basePosition = 94
        if int(numNeckBones) == 1:
            basePosition = 76

        for i in range(int(numNeckBones)):
            self.widgets[name + "_neck" + str(i + 1) + "_PickerButton"] = cmds.button(w = 32, h = buttonHeight, label = "", bgc = self.blue, c = partial(self.buttonSelectCommand, name, "neck_0" + str(i + 1) + "_fk_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_neck" + str(i + 1) + "_PickerButton"], "top", basePosition), (self.widgets[name + "_neck" + str(i + 1) + "_PickerButton"], "left", 170)])
            basePosition = basePosition - buttonHeight

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik spine
        if int(numSpineBones) ==  5:
            midPos = 158
            topPos = 118
            buttonHeight = 15

        if int(numSpineBones) ==  4:
            midPos = 168
            topPos = 116
            buttonHeight = 19

        if int(numSpineBones) ==  3:
            midPos = 154
            topPos = 120
            buttonHeight = 25


        if int(numSpineBones) > 2:
            self.widgets[name + "_ikSpineMidPickerButton"] = cmds.button(w = 120, h = buttonHeight, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "mid_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_ikSpineMidPickerButton"], "top", midPos), (self.widgets[name + "_ikSpineMidPickerButton"], "left", 126)])

            self.widgets[name + "_ikSpineTopPickerButton"] = cmds.button(w = 120, h = buttonHeight, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "chest_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_ikSpineTopPickerButton"], "top", topPos), (self.widgets[name + "_ikSpineTopPickerButton"], "left", 126)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk spine
        buttonHeight = int(75/numSpineBones)
        if int(numSpineBones) ==  5:
            basePosition = 198
            space = 5
        if int(numSpineBones) ==  4:
            basePosition = 194
            space = 7
        if int(numSpineBones) ==  3:
            basePosition = 188
            space = 9
        if int(numSpineBones) ==  2:
            basePosition = 175
            space = 17

        for i in range(int(numSpineBones)):
            self.widgets[name + "_spine" + str(i + 1) + "_PickerButton"] = cmds.button(w = 80, h = buttonHeight, label = "", bgc = self.blue, c = partial(self.buttonSelectCommand, name, "spine_0" + str(i + 1) + "_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_spine" + str(i + 1) + "_PickerButton"], "top", basePosition), (self.widgets[name + "_spine" + str(i + 1) + "_PickerButton"], "left", 144)])
            basePosition = (basePosition - buttonHeight) - space

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #body and pelvis
        self.widgets[name + "_bodyPickerButton"] = cmds.button(w = 100, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "body_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_bodyPickerButton"], "top", 218), (self.widgets[name + "_bodyPickerButton"], "left", 134)])

        self.widgets[name + "_pelvisPickerButton"] = cmds.button(w = 80, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "hip_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_pelvisPickerButton"], "top", 240), (self.widgets[name + "_pelvisPickerButton"], "left", 144)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #clavicles
        self.widgets[name + "_leftClavPickerButton"] = cmds.button(w = 50, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.clavSelectCommand, name, "fk_clavicle_l_anim", "clavicle_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftClavPickerButton"], "top", 94), (self.widgets[name + "_leftClavPickerButton"], "right", 116)])

        self.widgets[name + "_rightClavPickerButton"] = cmds.button(w = 50, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.clavSelectCommand, name, "fk_clavicle_r_anim", "clavicle_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightClavPickerButton"], "top", 94), (self.widgets[name + "_rightClavPickerButton"], "left", 116)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk upper arms
        self.widgets[name + "_leftShoulderPickerButton"] = cmds.button(w = 78, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_arm_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftShoulderPickerButton"], "top", 94), (self.widgets[name + "_leftShoulderPickerButton"], "right", 36)])

        self.widgets[name + "_rightShoulderPickerButton"] = cmds.button(w = 78, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_arm_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightShoulderPickerButton"], "top", 94), (self.widgets[name + "_rightShoulderPickerButton"], "left", 36)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk lower arms
        self.widgets[name + "_leftElbowPickerButton"] = cmds.button(w = 20, h = 78, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_elbow_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftElbowPickerButton"], "top", 142), (self.widgets[name + "_leftElbowPickerButton"], "right", 35)])

        self.widgets[name + "_rightElbowPickerButton"] = cmds.button(w = 20, h = 78, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_elbow_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightElbowPickerButton"], "top", 142), (self.widgets[name + "_rightElbowPickerButton"], "left", 35)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk hands
        self.widgets[name + "_leftHandPickerButton"] = cmds.button(w = 40, h = 40, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_wrist_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftHandPickerButton"], "top", 247), (self.widgets[name + "_leftHandPickerButton"], "right", 24)])

        self.widgets[name + "_rightHandPickerButton"] = cmds.button(w = 40, h = 40, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_wrist_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightHandPickerButton"], "top", 247), (self.widgets[name + "_rightHandPickerButton"], "left", 24)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik elbows
        self.widgets[name + "_leftIkElbowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_elbow_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkElbowPickerButton"], "top", 118), (self.widgets[name + "_leftIkElbowPickerButton"], "right", 35)])

        self.widgets[name + "_rightIkElbowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_elbow_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkElbowPickerButton"], "top", 118), (self.widgets[name + "_rightIkElbowPickerButton"], "left", 35)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik hands
        self.widgets[name + "_leftIkHandPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_wrist_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkHandPickerButton"], "top", 222), (self.widgets[name + "_leftIkHandPickerButton"], "right", 35)])

        self.widgets[name + "_rightIkHandPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_wrist_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkHandPickerButton"], "top", 222), (self.widgets[name + "_rightIkHandPickerButton"], "left", 35)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk thighs
        self.widgets[name + "_leftThighPickerButton"] = cmds.button(w = 20, h = 80, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_thigh_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThighPickerButton"], "top", 240), (self.widgets[name + "_leftThighPickerButton"], "right", 124)])

        self.widgets[name + "_rightThighPickerButton"] = cmds.button(w = 20, h = 80, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_thigh_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThighPickerButton"], "top", 240), (self.widgets[name + "_rightThighPickerButton"], "left", 124)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk knees
        self.widgets[name + "_leftFkKneePickerButton"] = cmds.button(w = 20, h = 80, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_calf_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftFkKneePickerButton"], "top", 343), (self.widgets[name + "_leftFkKneePickerButton"], "right", 124)])

        self.widgets[name + "_rightFkKneePickerButton"] = cmds.button(w = 20, h = 80, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_calf_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightFkKneePickerButton"], "top", 343), (self.widgets[name + "_rightFkKneePickerButton"], "left", 124)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk ankle
        self.widgets[name + "_leftFkAnklePickerButton"] = cmds.button(w = 40, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_foot_l_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftFkAnklePickerButton"], "top", 422), (self.widgets[name + "_leftFkAnklePickerButton"], "right", 82)])

        self.widgets[name + "_rightFkAnklePickerButton"] = cmds.button(w = 40, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_foot_r_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightFkAnklePickerButton"], "top", 422), (self.widgets[name + "_rightFkAnklePickerButton"], "left", 82)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #fk ball
        if leftBall:
            self.widgets[name + "_leftFkBallPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_ball_l_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftFkBallPickerButton"], "top", 422), (self.widgets[name + "_leftFkBallPickerButton"], "right", 59)])

        if rightBall:
            self.widgets[name + "_rightFkBallPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "fk_ball_r_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightFkBallPickerButton"], "top", 422), (self.widgets[name + "_rightFkBallPickerButton"], "left", 59)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik knees

        self.widgets[name + "_leftIkKneePickerButton"] = cmds.floatField(w = 40, h = 20, parent = layout, step = 1, minValue = -360, maxValue = 360, precision = 1, ann = "Ctrl + MMB to drag invisible slider")
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkKneePickerButton"], "top", 321), (self.widgets[name + "_leftIkKneePickerButton"], "right", 114)])

        self.widgets[name + "_rightIkKneePickerButton"] = cmds.floatField(w = 40, h = 20, parent = layout, step = 1, minValue = -360, maxValue = 360, precision = 1, ann = "Ctrl + MMB to drag invisible slider")
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkKneePickerButton"], "top", 321), (self.widgets[name + "_rightIkKneePickerButton"], "left", 114)])


        cmds.connectControl(self.widgets[name + "_leftIkKneePickerButton"] , name + ":ik_foot_anim_l.knee_twist")
        cmds.connectControl(self.widgets[name + "_rightIkKneePickerButton"] , name + ":ik_foot_anim_r.knee_twist")

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik feet
        self.widgets[name + "_leftIkFootPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_foot_anim_l"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkFootPickerButton"], "top", 422), (self.widgets[name + "_leftIkFootPickerButton"], "right", 124)])

        self.widgets[name + "_rightIkFootPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ik_foot_anim_r"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkFootPickerButton"], "top", 422), (self.widgets[name + "_rightIkFootPickerButton"], "left", 124)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik heels
        self.widgets[name + "_leftIkHeelPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "heel_ctrl_l"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkHeelPickerButton"], "top", 428), (self.widgets[name + "_leftIkHeelPickerButton"], "right", 149)])

        self.widgets[name + "_rightIkHeelPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "heel_ctrl_r"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkHeelPickerButton"], "top", 428), (self.widgets[name + "_rightIkHeelPickerButton"], "left", 149)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik toe wiggles
        self.widgets[name + "_leftIkToeWigglePickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "toe_wiggle_ctrl_l"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkToeWigglePickerButton"], "top", 402), (self.widgets[name + "_leftIkToeWigglePickerButton"], "right", 74)])

        self.widgets[name + "_rightIkToeWigglePickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "toe_wiggle_ctrl_r"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkToeWigglePickerButton"], "top", 402), (self.widgets[name + "_rightIkToeWigglePickerButton"], "left", 74)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #ik toes
        self.widgets[name + "_leftIkToePickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "toe_tip_ctrl_l"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkToePickerButton"], "top", 428), (self.widgets[name + "_leftIkToePickerButton"], "right", 40)])

        self.widgets[name + "_rightIkToePickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "toe_tip_ctrl_r"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkToePickerButton"], "top", 428), (self.widgets[name + "_rightIkToePickerButton"], "left", 40)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #master, offset, and root
        self.widgets[name + "_masterPickerButton"] = cmds.button(w = 20, h = 20, label = "M", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "master_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_masterPickerButton"], "top", 401), (self.widgets[name + "_masterPickerButton"], "right", 175)])

        self.widgets[name + "_offsetPickerButton"] = cmds.button(w = 20, h = 20, label = "O", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "offset_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_offsetPickerButton"], "top", 375), (self.widgets[name + "_offsetPickerButton"], "right", 175)])

        self.widgets[name + "_rootPickerButton"] = cmds.button(w = 20, h = 20, label = "R", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "root_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rootPickerButton"], "top", 350), (self.widgets[name + "_rootPickerButton"], "right", 175)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select head group
        self.widgets[name + "_headGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["head_fk_anim", "neck_01_fk_anim", "neck_02_fk_anim", "neck_03_fk_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_headGroupPickerButton"], "top", 2), (self.widgets[name + "_headGroupPickerButton"], "right", 178)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select spine group
        self.widgets[name + "_spineGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["hip_anim", "body_anim", "spine_01_anim", "spine_02_anim", "spine_03_anim", "spine_04_anim", "spine_05_anim", "mid_ik_anim", "chest_ik_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_spineGroupPickerButton"], "top", 264), (self.widgets[name + "_spineGroupPickerButton"], "right", 178)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select left arm group
        self.widgets[name + "_leftArmGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ik_wrist_l_anim", "ik_elbow_l_anim", "fk_arm_l_anim", "fk_elbow_l_anim", "fk_wrist_l_anim", "clavicle_l_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftArmGroupPickerButton"], "top", 75), (self.widgets[name + "_leftArmGroupPickerButton"], "right", 117)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select right arm group
        self.widgets[name + "_rightArmGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ik_wrist_r_anim", "ik_elbow_r_anim", "fk_arm_r_anim", "fk_elbow_r_anim", "fk_wrist_r_anim", "clavicle_r_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightArmGroupPickerButton"], "top", 75), (self.widgets[name + "_rightArmGroupPickerButton"], "left", 117)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select left leg group
        self.widgets[name + "_leftLegGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["fk_thigh_l_anim", "fk_calf_l_anim", "fk_foot_l_anim", "fk_ball_l_anim",  "ik_foot_anim_l", "heel_ctrl_l", "toe_wiggle_ctrl_l", "toe_tip_ctrl_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftLegGroupPickerButton"], "top", 222), (self.widgets[name + "_leftLegGroupPickerButton"], "right", 114)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select left leg ik ctrls
        self.widgets[name + "_leftLegIKGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ik_foot_anim_l", "heel_ctrl_l", "toe_wiggle_ctrl_l", "toe_tip_ctrl_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftLegIKGroupPickerButton"], "top", 408), (self.widgets[name + "_leftLegIKGroupPickerButton"], "right", 40)])



        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select right leg group
        self.widgets[name + "_rightLegGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["fk_thigh_r_anim", "fk_calf_r_anim", "fk_foot_r_anim", "fk_ball_r_anim",  "ik_foot_anim_r", "heel_ctrl_r", "toe_wiggle_ctrl_r", "toe_tip_ctrl_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightLegGroupPickerButton"], "top", 222), (self.widgets[name + "_rightLegGroupPickerButton"], "left", 114)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #select right leg ik ctrls
        self.widgets[name + "_rightLegIKGroupPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ik_foot_anim_r", "heel_ctrl_r", "toe_wiggle_ctrl_r", "toe_tip_ctrl_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightLegIKGroupPickerButton"], "top", 408), (self.widgets[name + "_rightLegIKGroupPickerButton"], "left", 40)])


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #arm rolls
        if leftArmTwist > 0:
            self.widgets[name + "_leftArmRollPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_l_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftArmRollPickerButton"], "top", 75), (self.widgets[name + "_leftArmRollPickerButton"], "right", 76)])

        if leftArmTwist > 1:
            self.widgets[name + "_leftArmRoll2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_l_twist_2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftArmRoll2PickerButton"], "top", 75), (self.widgets[name + "_leftArmRoll2PickerButton"], "right", 56)])

        if leftArmTwist > 2:
            self.widgets[name + "_leftArmRoll3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_l_twist_3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftArmRoll3PickerButton"], "top", 75), (self.widgets[name + "_leftArmRoll3PickerButton"], "right", 36)])


        if rightArmTwist > 0:
            self.widgets[name + "_rightArmRollPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_r_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightArmRollPickerButton"], "top", 75), (self.widgets[name + "_rightArmRollPickerButton"], "left", 76)])

        if rightArmTwist > 1:
            self.widgets[name + "_rightArmRoll2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_r_twist_2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightArmRoll2PickerButton"], "top", 75), (self.widgets[name + "_rightArmRoll2PickerButton"], "left", 56)])

        if rightArmTwist > 2:
            self.widgets[name + "_rightArmRoll3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "upperarm_r_twist_3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightArmRoll3PickerButton"], "top", 75), (self.widgets[name + "_rightArmRoll3PickerButton"], "left", 36)])



        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #forearm twists
        if leftArmForeTwist > 0:
            self.widgets[name + "_leftForeTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_l_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftForeTwistPickerButton"], "top", 190), (self.widgets[name + "_leftForeTwistPickerButton"], "right", 15)])

        if leftArmForeTwist > 1:
            self.widgets[name + "_leftForeTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_l_twist2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftForeTwist2PickerButton"], "top", 170), (self.widgets[name + "_leftForeTwist2PickerButton"], "right", 15)])

        if leftArmForeTwist > 2:
            self.widgets[name + "_leftForeTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_l_twist3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftForeTwist3PickerButton"], "top", 150), (self.widgets[name + "_leftForeTwist3PickerButton"], "right", 15)])



        if rightArmForeTwist > 0:
            self.widgets[name + "_rightForeTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_r_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightForeTwistPickerButton"], "top", 190), (self.widgets[name + "_rightForeTwistPickerButton"], "left", 15)])

        if rightArmForeTwist > 1:
            self.widgets[name + "_rightForeTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_r_twist2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightForeTwist2PickerButton"], "top", 170), (self.widgets[name + "_rightForeTwist2PickerButton"], "left", 15)])

        if rightArmForeTwist > 2:
            self.widgets[name + "_rightForeTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "lowerarm_r_twist3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightForeTwist3PickerButton"], "top", 150), (self.widgets[name + "_rightForeTwist3PickerButton"], "left", 15)])






        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #thigh twists
        if leftThighTwist > 0:
            self.widgets[name + "_leftThighTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "l_thigh_twist_01_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThighTwistPickerButton"], "top", 241), (self.widgets[name + "_leftThighTwistPickerButton"], "right", 106)])

        if leftThighTwist > 1:
            self.widgets[name + "_leftThighTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "l_thigh_twist_02_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThighTwist2PickerButton"], "top", 261), (self.widgets[name + "_leftThighTwist2PickerButton"], "right", 106)])

        if leftThighTwist > 2:
            self.widgets[name + "_leftThighTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "l_thigh_twist_03_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThighTwist3PickerButton"], "top", 281), (self.widgets[name + "_leftThighTwist3PickerButton"], "right", 106)])


        if rightThighTwist > 0:
            self.widgets[name + "_rightThighTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "r_thigh_twist_01_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThighTwistPickerButton"], "top", 241), (self.widgets[name + "_rightThighTwistPickerButton"], "left", 106)])

        if rightThighTwist > 1:
            self.widgets[name + "_rightThighTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "r_thigh_twist_02_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThighTwist2PickerButton"], "top", 261), (self.widgets[name + "_rightThighTwist2PickerButton"], "left", 106)])

        if rightThighTwist > 2:
            self.widgets[name + "_rightThighTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "r_thigh_twist_03_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThighTwist3PickerButton"], "top", 281), (self.widgets[name + "_rightThighTwist3PickerButton"], "left", 106)])



        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #calf twists
        if leftCalfTwist > 0:
            self.widgets[name + "_leftCalfTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_l_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftCalfTwistPickerButton"], "top", 400), (self.widgets[name + "_leftCalfTwistPickerButton"], "right", 106)])

        if leftCalfTwist > 1:
            self.widgets[name + "_leftCalfTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_l_twist2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftCalfTwist2PickerButton"], "top", 380), (self.widgets[name + "_leftCalfTwist2PickerButton"], "right", 106)])

        if leftCalfTwist > 2:
            self.widgets[name + "_leftCalfTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_l_twist3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftCalfTwist3PickerButton"], "top", 360), (self.widgets[name + "_leftCalfTwist3PickerButton"], "right", 106)])


        if rightCalfTwist > 0:
            self.widgets[name + "_rightCalfTwistPickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_r_twist_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightCalfTwistPickerButton"], "top", 400), (self.widgets[name + "_rightCalfTwistPickerButton"], "left", 106)])

        if rightCalfTwist > 1:
            self.widgets[name + "_rightCalfTwist2PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_r_twist2_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightCalfTwist2PickerButton"], "top", 380), (self.widgets[name + "_rightCalfTwist2PickerButton"], "left", 106)])

        if rightCalfTwist > 2:
            self.widgets[name + "_rightCalfTwist3PickerButton"] = cmds.button(w = 15, h = 15, label = "", parent = layout, bgc = self.purple, c = partial(self.buttonSelectCommand, name, "calf_r_twist3_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightCalfTwist3PickerButton"], "top", 360), (self.widgets[name + "_rightCalfTwist3PickerButton"], "left", 106)])

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
        #Setup right click menus for each of the limbs that can switch modes

        #spine
        for i in range(int(numSpineBones)):
            buttonName = name + "_spine" + str(i + 1) + "_PickerButton"
            menu = cmds.popupMenu(b = 3, parent = self.widgets[buttonName])
            cmds.menuItem(label = "Spine FK Mode", parent = menu, c = partial(self.switchSpineMode, name, "FK"))
            cmds.menuItem(label = "Spine IK Mode", parent = menu, c = partial(self.switchSpineMode, name, "IK"))

            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "IK", "FK"))

            if i == 0:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_spine1_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_spine1_RadioCollection"] ,  "spine_01_space_switcher"))


        if int(numSpineBones) > 2:
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_ikSpineMidPickerButton"])
            cmds.menuItem(label = "Spine FK Mode", parent = menu, c = partial(self.switchSpineMode, name, "FK"))
            cmds.menuItem(label = "Spine IK Mode", parent = menu, c = partial(self.switchSpineMode, name, "IK"))

            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "IK", "FK"))

            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_ikSpineTopPickerButton"])
            cmds.menuItem(label = "Spine FK Mode", parent = menu, c = partial(self.switchSpineMode, name, "FK"))
            cmds.menuItem(label = "Spine IK Mode", parent = menu, c = partial(self.switchSpineMode, name, "IK"))

            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "spine", None, "IK", "FK"))

            spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
            self.widgets[name + "_chestIkSpine_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
            cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_chestIkSpine_RadioCollection"] ,  "chest_ik_anim_space_switcher"))


        #Arms
        for button in[self.widgets[name + "_leftClavPickerButton"], self.widgets[name + "_leftShoulderPickerButton"], self.widgets[name + "_leftElbowPickerButton"], self.widgets[name + "_leftHandPickerButton"], self.widgets[name + "_leftIkElbowPickerButton"], self.widgets[name + "_leftIkHandPickerButton"]]:
            menu = cmds.popupMenu(b = 3, parent = button)
            cmds.menuItem(label = "Arm FK Mode", parent = menu, c = partial(self.switchArmMode, name, "FK", "l"))
            cmds.menuItem(label = "Arm IK Mode", parent = menu, c = partial(self.switchArmMode, name, "IK", "l"))

            #hack to get new fk clav rig matching functionality. will eventually be replaced with something more graceful when I change it over to a modular system
            if button == self.widgets[name + "_leftClavPickerButton"]:
                if cmds.objExists(name + ":fk_clavicle_l_anim"):

                    cmds.menuItem(label = "Clavicle FK Mode", parent = menu, c = partial(self.switchClavMode, name, "FK", "l"))
                    cmds.menuItem(label = "Clavicle IK Mode", parent = menu, c = partial(self.switchClavMode, name, "IK", "l"))		    



            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "arm", "l", "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "arm", "l", "IK", "FK"))


            subMenu = cmds.menuItem(label = "Arm FK Orientation Space", parent = menu, subMenu = True)

            mode = cmds.getAttr(name + ":Rig_Settings.lFkArmOrient")
            if mode == 0:
                clavVal = True
                bodyVal = False
                worldVal = False

            if mode == 1:
                clavVal = False
                bodyVal = True
                worldVal = False

            if mode == 2:
                clavVal = False
                bodyVal = False
                worldVal = True



            self.widgets[name + "_" + button + "_leftArm_RadioCollection"] = cmds.radioMenuItemCollection(parent = subMenu)
            self.widgets[name + "_" + button + "_leftArm_ClavSpace"] = cmds.menuItem(label = "Clavicle Space(default fk behavior)", parent = subMenu, cl = self.widgets[name + "_" + button + "_leftArm_RadioCollection"], rb =clavVal, c = partial(self.switchArmOrientMode, name, 0, "l"))
            self.widgets[name + "_" + button + "_leftArm_BodySpace"] = cmds.menuItem(label = "Body Space", parent = subMenu, cl = self.widgets[name + "_" + button + "_leftArm_RadioCollection"], rb =bodyVal, c = partial(self.switchArmOrientMode, name, 1, "l"))
            self.widgets[name + "_" + button + "_leftArm_WrldSpace"] = cmds.menuItem(label = "World Space", parent = subMenu, cl = self.widgets[name + "_" + button + "_leftArm_RadioCollection"], rb =worldVal, c = partial(self.switchArmOrientMode, name, 2, "l"))



            if button == self.widgets[name + "_leftIkHandPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_leftIkHnad_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_leftIkHnad_RadioCollection"] ,  "ik_wrist_l_anim_space_switcher"))


            if button == self.widgets[name + "_leftIkElbowPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_leftIkElbow_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_leftIkElbow_RadioCollection"] ,  "ik_elbow_l_anim_space_switcher"))





        for button in[self.widgets[name + "_rightClavPickerButton"], self.widgets[name + "_rightShoulderPickerButton"], self.widgets[name + "_rightElbowPickerButton"], self.widgets[name + "_rightHandPickerButton"], self.widgets[name + "_rightIkElbowPickerButton"], self.widgets[name + "_rightIkHandPickerButton"]]:
            menu = cmds.popupMenu(b = 3, parent = button)
            cmds.menuItem(label = "Arm FK Mode", parent = menu, c = partial(self.switchArmMode, name, "FK", "r"))
            cmds.menuItem(label = "Arm IK Mode", parent = menu, c = partial(self.switchArmMode, name, "IK", "r"))

            if button == self.widgets[name + "_rightClavPickerButton"]:
                if cmds.objExists(name + ":fk_clavicle_r_anim"):

                    cmds.menuItem(label = "Clavicle FK Mode", parent = menu, c = partial(self.switchClavMode, name, "FK", "r"))
                    cmds.menuItem(label = "Clavicle IK Mode", parent = menu, c = partial(self.switchClavMode, name, "IK", "r"))	


            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "arm", "r", "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "arm", "r", "IK", "FK"))

            subMenu = cmds.menuItem(label = "Arm FK Orientation Space", parent = menu, subMenu = True)

            mode = cmds.getAttr(name + ":Rig_Settings.rFkArmOrient")
            if mode == 0:
                clavVal = True
                bodyVal = False
                worldVal = False

            if mode == 1:
                clavVal = False
                bodyVal = True
                worldVal = False

            if mode == 2:
                clavVal = False
                bodyVal = False
                worldVal = True



            self.widgets[name + "_" + button + "_rightArm_RadioCollection"] = cmds.radioMenuItemCollection(parent = subMenu)
            self.widgets[name + "_" + button + "_rightArm_ClavSpace"] = cmds.menuItem(label = "Clavicle Space(default fk behavior)", parent = subMenu, cl = self.widgets[name + "_" + button + "_rightArm_RadioCollection"], rb =clavVal, c = partial(self.switchArmOrientMode, name, 0, "r"))
            self.widgets[name + "_" + button + "_rightArm_BodySpace"] = cmds.menuItem(label = "Body Space", parent = subMenu, cl = self.widgets[name + "_" + button + "_rightArm_RadioCollection"], rb =bodyVal, c = partial(self.switchArmOrientMode, name, 1, "r"))
            self.widgets[name + "_" + button + "_rightArm_WrldSpace"] = cmds.menuItem(label = "World Space", parent = subMenu, cl = self.widgets[name + "_" + button + "_rightArm_RadioCollection"], rb =worldVal, c = partial(self.switchArmOrientMode, name, 2, "r"))


            if button == self.widgets[name + "_rightIkHandPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_rightIkHnad_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_rightIkHnad_RadioCollection"] ,  "ik_wrist_r_anim_space_switcher"))

            if button == self.widgets[name + "_rightIkElbowPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_rightIkElbow_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_rightIkElbow_RadioCollection"] ,  "ik_elbow_r_anim_space_switcher"))


        #Legs
        for button in[self.widgets[name + "_leftThighPickerButton"], self.widgets[name + "_leftFkKneePickerButton"], self.widgets[name + "_leftFkAnklePickerButton"], self.widgets[name + "_leftIkFootPickerButton"]]:
            menu = cmds.popupMenu(b = 3, parent = button)
            cmds.menuItem(label = "Leg FK Mode", parent = menu, c = partial(self.switchLegMode, name, "FK", "l"))
            cmds.menuItem(label = "Leg IK Mode", parent = menu, c = partial(self.switchLegMode, name, "IK", "l"))

            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "leg", "l", "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "leg", "l", "IK", "FK"))

            if button == self.widgets[name + "_leftIkFootPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_leftIkFoot_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_leftIkFoot_RadioCollection"] ,  "ik_foot_anim_l_space_switcher"))

        for button in[self.widgets[name + "_rightThighPickerButton"], self.widgets[name + "_rightFkKneePickerButton"], self.widgets[name + "_rightFkAnklePickerButton"],  self.widgets[name + "_rightIkFootPickerButton"]]:
            menu = cmds.popupMenu(b = 3, parent = button)
            cmds.menuItem(label = "Leg FK Mode", parent = menu, c = partial(self.switchLegMode, name, "FK", "r"))
            cmds.menuItem(label = "Leg IK Mode", parent = menu, c = partial(self.switchLegMode, name, "IK", "r"))

            matchMenu = cmds.menuItem(label = "Matching", parent = menu, subMenu = True)
            cmds.menuItem(label = "Match FK Rig to current IK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "leg", "r", "FK", "IK"))
            cmds.menuItem(label = "Match IK Rig to current FK Pose", parent = matchMenu, c = partial(self.match_singleFrame, "leg", "r", "IK", "FK"))

            if button == self.widgets[name + "_rightIkFootPickerButton"]:
                spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
                self.widgets[name + "_rightIkFoot_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
                cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_rightIkFoot_RadioCollection"] ,  "ik_foot_anim_r_space_switcher"))



        #Head
        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_headPickerButton"], postMenuCommand = self.getHeadSpace)
        subMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
        collection = cmds.radioMenuItemCollection(parent = subMenu)
        self.widgets["neckSpaceRB"] = cmds.menuItem(label = "Neck", parent = subMenu, cl = collection, rb = True, c = partial(self.switchHeadOrientMode, name, 0))
        self.widgets["shoulderSpaceRB"] = cmds.menuItem(label = "Shoulder", parent = subMenu, cl = collection, rb = False, c = partial(self.switchHeadOrientMode, name, 1))
        self.widgets["bodySpaceRB"] = cmds.menuItem(label = "Body", parent = subMenu, cl = collection, rb = False, c = partial(self.switchHeadOrientMode, name, 2))
        self.widgets["worldSpaceRB"] = cmds.menuItem(label = "World", parent = subMenu, cl = collection, rb = False, c = partial(self.switchHeadOrientMode, name, 3))


        #Neck
        if cmds.objExists(name + ":neck_01_fk_anim.fkOrientation"):
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_neck1_PickerButton"], postMenuCommand = self.getNeckSpace)
            subMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
            collection = cmds.radioMenuItemCollection(parent = subMenu)
            self.widgets["neckOrientShoulderSpaceRB"] = cmds.menuItem(label = "Shoulder", parent = subMenu, cl = collection, rb = False, c = partial(self.switchNeckOrientMode, name, 0))
            self.widgets["neckOrientBodySpaceRB"] = cmds.menuItem(label = "Body", parent = subMenu, cl = collection, rb = False, c = partial(self.switchNeckOrientMode, name, 1))
            self.widgets["neckOrientWorldSpaceRB"] = cmds.menuItem(label = "World", parent = subMenu, cl = collection, rb = False, c = partial(self.switchNeckOrientMode, name, 2))


        #Core (body, master)
        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_bodyPickerButton"])
        spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
        self.widgets[name + "_bodySpaceSwitch_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
        cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_bodySpaceSwitch_RadioCollection"] ,  "body_anim_space_switcher"))


        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_masterPickerButton"])
        spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
        self.widgets[name + "_masterSpaceSwitch_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
        cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_masterSpaceSwitch_RadioCollection"] ,  "master_anim_space_switcher"))





    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createFingersPicker(self, name, layout, *args):

        #select all fingers buttons
        self.widgets[name + "_selectAllLeftFingers"] = cmds.symbolButton(image = self.mayaToolsDir + "/General/Icons/ART/lFingerAll.bmp", w = 175, h = 170, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_l", "middle_metacarpal_ctrl_l", "ring_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_l", "index_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_l", "thumb_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_selectAllLeftFingers"], "top", 5), (self.widgets[name + "_selectAllLeftFingers"], "right", 5)])
        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_selectAllLeftFingers"])
        cmds.menuItem(label = "Select all IK Finger Controls", parent = menu, c = partial(self.multiButtonSelectCommand, name, ["index_l_ik_anim", "middle_l_ik_anim", "ring_l_ik_anim", "pinky_l_ik_anim", "thumb_l_ik_anim", "index_l_poleVector", "middle_l_poleVector", "ring_l_poleVector", "pinky_l_poleVector", "thumb_l_poleVector", "l_global_ik_anim"]))
        cmds.menuItem(label = "Select FK and IK Finger Controls", parent = menu, c = partial(self.multiButtonSelectCommand, name, ["index_l_ik_anim", "middle_l_ik_anim", "ring_l_ik_anim", "pinky_l_ik_anim", "thumb_l_ik_anim", "index_l_poleVector", "middle_l_poleVector", "ring_l_poleVector", "pinky_l_poleVector", "thumb_l_poleVector", "l_global_ik_anim", "index_metacarpal_ctrl_l", "middle_metacarpal_ctrl_l", "ring_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_l", "index_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_l", "thumb_finger_fk_ctrl_3_l"]))


        self.widgets[name + "_selectAllRightFingers"] = cmds.symbolButton(image = self.mayaToolsDir + "/General/Icons/ART/rFingerAll.bmp", w = 175, h = 170, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_r", "middle_metacarpal_ctrl_r", "ring_metacarpal_ctrl_r", "pinky_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_r", "middle_finger_fk_ctrl_3_r", "ring_finger_fk_ctrl_3_r", "pinky_finger_fk_ctrl_3_r", "thumb_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_selectAllRightFingers"], "top", 5), (self.widgets[name + "_selectAllRightFingers"], "left", 5)])
        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_selectAllRightFingers"])
        cmds.menuItem(label = "Select all IK Finger Controls", parent = menu, c = partial(self.multiButtonSelectCommand, name, ["index_r_ik_anim", "middle_r_ik_anim", "ring_r_ik_anim", "pinky_r_ik_anim", "thumb_r_ik_anim", "index_r_poleVector", "middle_r_poleVector", "ring_r_poleVector", "pinky_r_poleVector", "thumb_r_poleVector", "r_global_ik_anim"]))
        cmds.menuItem(label = "Select FK and IK Finger Controls", parent = menu, c = partial(self.multiButtonSelectCommand, name, ["index_r_ik_anim", "middle_r_ik_anim", "ring_r_ik_anim", "pinky_r_ik_anim", "thumb_r_ik_anim", "index_r_poleVector", "middle_r_poleVector", "ring_r_poleVector", "pinky_r_poleVector", "thumb_r_poleVector", "r_global_ik_anim", "index_metacarpal_ctrl_r", "middle_metacarpal_ctrl_r", "ring_metacarpal_ctrl_r", "pinky_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_r", "middle_finger_fk_ctrl_3_r", "ring_finger_fk_ctrl_3_r", "pinky_finger_fk_ctrl_3_r", "thumb_finger_fk_ctrl_3_r"]))


        #Left Pinky
        #get settings off of skeleton settings node to know what it is we need to create
        leftPinkyMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.leftpinkymeta")
        leftPinky1 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftpinky1")
        leftPinky2 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftpinky2")
        leftPinky3 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftpinky3")

        if leftPinkyMeta:
            self.widgets[name + "_leftPinkyMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_metacarpal_ctrl_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinkyMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_leftPinkyMetacarpalPickerButton"], "right", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftPinkyMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "l"))

        if leftPinky1:
            self.widgets[name + "_leftPinky1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_1_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinky1PickerButton"], "top", 55), (self.widgets[name + "_leftPinky1PickerButton"], "right", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftPinky1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "l"))

        if leftPinky2:
            self.widgets[name + "_leftPinky2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_2_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinky2PickerButton"], "top", 80), (self.widgets[name + "_leftPinky2PickerButton"], "right", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftPinky2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "l"))

        if leftPinky3:
            self.widgets[name + "_leftPinky3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_3_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinky3PickerButton"], "top", 105), (self.widgets[name + "_leftPinky3PickerButton"], "right", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftPinky3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "l"))


        #Left Ring
        #get settings off of skeleton settings node to know what it is we need to create
        leftRingMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.leftringmeta")
        leftRing1 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftring1")
        leftRing2 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftring2")
        leftRing3 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftring3")

        if leftRingMeta:
            self.widgets[name + "_leftRingMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_metacarpal_ctrl_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRingMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_leftRingMetacarpalPickerButton"], "right", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftRingMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "l"))

        if leftRing1:
            self.widgets[name + "_leftRing1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_1_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRing1PickerButton"], "top", 55), (self.widgets[name + "_leftRing1PickerButton"], "right", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftRing1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "l"))

        if leftRing2:
            self.widgets[name + "_leftRing2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_2_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRing2PickerButton"], "top", 80), (self.widgets[name + "_leftRing2PickerButton"], "right", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftRing2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "l"))

        if leftRing3:
            self.widgets[name + "_leftRing3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_3_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRing3PickerButton"], "top", 105), (self.widgets[name + "_leftRing3PickerButton"], "right", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftRing3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "l"))


        #Left Middle
        #get settings off of skeleton settings node to know what it is we need to create
        leftMiddleMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.leftmiddlemeta")
        leftMiddle1 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftmiddle1")
        leftMiddle2 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftmiddle2")
        leftMiddle3 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftmiddle3")

        if leftMiddleMeta:
            self.widgets[name + "_leftMiddleMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_metacarpal_ctrl_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddleMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_leftMiddleMetacarpalPickerButton"], "right", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftMiddleMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "l"))

        if leftMiddle1:
            self.widgets[name + "_leftMiddle1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_1_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddle1PickerButton"], "top", 55), (self.widgets[name + "_leftMiddle1PickerButton"], "right", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftMiddle1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "l"))

        if leftMiddle2:
            self.widgets[name + "_leftMiddle2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_2_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddle2PickerButton"], "top", 80), (self.widgets[name + "_leftMiddle2PickerButton"], "right", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftMiddle2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "l"))

        if leftMiddle3:
            self.widgets[name + "_leftMiddle3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_3_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddle3PickerButton"], "top", 105), (self.widgets[name + "_leftMiddle3PickerButton"], "right", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftMiddle3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "l"))

        #Left Index
        #get settings off of skeleton settings node to know what it is we need to create
        leftIndexMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.leftindexmeta")
        leftIndex1 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftindex1")
        leftIndex2 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftindex2")
        leftIndex3 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftindex3")

        if leftIndexMeta:
            self.widgets[name + "_leftIndexMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_metacarpal_ctrl_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndexMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_leftIndexMetacarpalPickerButton"], "right", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIndexMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "l"))

        if leftIndex1:
            self.widgets[name + "_leftIndex1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_1_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndex1PickerButton"], "top", 55), (self.widgets[name + "_leftIndex1PickerButton"], "right", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIndex1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "l"))

        if leftIndex2:
            self.widgets[name + "_leftIndex2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_2_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndex2PickerButton"], "top", 80), (self.widgets[name + "_leftIndex2PickerButton"], "right", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIndex2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "l"))

        if leftIndex3:
            self.widgets[name + "_leftIndex3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_3_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndex3PickerButton"], "top", 105), (self.widgets[name + "_leftIndex3PickerButton"], "right", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIndex3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "l"))


        #Left Thumb
        #get settings off of skeleton settings node to know what it is we need to create
        leftThumb1 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftthumb1")
        leftThumb2 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftthumb2")
        leftThumb3 = cmds.getAttr(name + ":" + "Skeleton_Settings.leftthumb3")

        if leftThumb1:
            self.widgets[name + "_leftThumb1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_1_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumb1PickerButton"], "top", 30), (self.widgets[name + "_leftThumb1PickerButton"], "right", 132)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftThumb1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "l"))

        if leftThumb2:
            self.widgets[name + "_leftThumb2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_2_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumb2PickerButton"], "top", 55), (self.widgets[name + "_leftThumb2PickerButton"], "right", 142)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftThumb2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "l"))

        if leftThumb3:
            self.widgets[name + "_leftThumb3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_3_l"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumb3PickerButton"], "top", 80), (self.widgets[name + "_leftThumb3PickerButton"], "right", 152)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftThumb3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "l"))

        #Left finger row globals
        #get settings off of skeleton settings node to know what it is we need to create
        self.widgets[name + "_leftMetaRowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_l", "middle_metacarpal_ctrl_l", "ring_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMetaRowPickerButton"], "top", 30), (self.widgets[name + "_leftMetaRowPickerButton"], "right", 7)])

        self.widgets[name + "_leftKnuckle1RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftKnuckle1RowPickerButton"], "top", 55), (self.widgets[name + "_leftKnuckle1RowPickerButton"], "right", 7)])

        self.widgets[name + "_leftKnuckle2RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftKnuckle2RowPickerButton"], "top", 80), (self.widgets[name + "_leftKnuckle2RowPickerButton"], "right", 7)])

        self.widgets[name + "_leftKnuckle3RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftKnuckle3RowPickerButton"], "top", 105), (self.widgets[name + "_leftKnuckle3RowPickerButton"], "right", 7)])


        #Left finger column globals
        #get settings off of skeleton settings node to know what it is we need to create
        self.widgets[name + "_leftIndexColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_l", "index_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndexColumnPickerButton"], "top", 7), (self.widgets[name + "_leftIndexColumnPickerButton"], "right", 107)])

        self.widgets[name + "_leftMiddleColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["middle_metacarpal_ctrl_l", "middle_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddleColumnPickerButton"], "top", 7), (self.widgets[name + "_leftMiddleColumnPickerButton"], "right", 82)])

        self.widgets[name + "_leftRingColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ring_metacarpal_ctrl_l", "ring_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRingColumnPickerButton"], "top", 7), (self.widgets[name + "_leftRingColumnPickerButton"], "right", 57)])

        self.widgets[name + "_leftPinkyColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["pinky_metacarpal_ctrl_l", "pinky_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinkyColumnPickerButton"], "top", 7), (self.widgets[name + "_leftPinkyColumnPickerButton"], "right", 32)])

        #Left thumb global
        self.widgets[name + "_leftThumbColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["thumb_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_3_l"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumbColumnPickerButton"], "top", 7), (self.widgets[name + "_leftThumbColumnPickerButton"], "right", 132)])


        #Left Finger IK
        if cmds.objExists(name + ":index_l_ik_anim"):
            self.widgets[name + "_leftIndexFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "index_l_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndexFingerIKPickerButton"], "top", 130), (self.widgets[name + "_leftIndexFingerIKPickerButton"], "right", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIndexFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "l"))

        if cmds.objExists(name + ":middle_l_ik_anim"):
            self.widgets[name + "_leftMiddleFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "middle_l_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddleFingerIKPickerButton"], "top", 130), (self.widgets[name + "_leftMiddleFingerIKPickerButton"], "right", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftMiddleFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "l"))

        if cmds.objExists(name + ":ring_l_ik_anim"):
            self.widgets[name + "_leftRingFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ring_l_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRingFingerIKPickerButton"], "top", 130), (self.widgets[name + "_leftRingFingerIKPickerButton"], "right", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftRingFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "l"))

        if cmds.objExists(name + ":pinky_l_ik_anim"):
            self.widgets[name + "_leftPinkyFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "pinky_l_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinkyFingerIKPickerButton"], "top", 130), (self.widgets[name + "_leftPinkyFingerIKPickerButton"], "right", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftPinkyFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "l"))

        if cmds.objExists(name + ":thumb_l_ik_anim"):
            self.widgets[name + "_leftThumbFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "thumb_l_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumbFingerIKPickerButton"], "top", 107), (self.widgets[name + "_leftThumbFingerIKPickerButton"], "right", 152)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftThumbFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "l"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "l"))

        self.widgets[name + "_leftIkFingersRowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_l_ik_anim", "middle_l_ik_anim", "ring_l_ik_anim", "pinky_l_ik_anim", "thumb_l_ik_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkFingersRowPickerButton"], "top", 130), (self.widgets[name + "_leftIkFingersRowPickerButton"], "right", 7)])

        #Left Finger IK Pole Vectors
        if cmds.objExists(name + ":index_l_ik_anim"):
            self.widgets[name + "_leftIndexIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "index_l_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIndexIkPvPickerButton"], "top", 155), (self.widgets[name + "_leftIndexIkPvPickerButton"], "right", 112)])

        if cmds.objExists(name + ":middle_l_ik_anim"):
            self.widgets[name + "_leftMiddleIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "middle_l_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftMiddleIkPvPickerButton"], "top", 155), (self.widgets[name + "_leftMiddleIkPvPickerButton"], "right", 87)])

        if cmds.objExists(name + ":ring_l_ik_anim"):
            self.widgets[name + "_leftRingIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ring_l_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftRingIkPvPickerButton"], "top", 155), (self.widgets[name + "_leftRingIkPvPickerButton"], "right", 62)])

        if cmds.objExists(name + ":pinky_l_ik_anim"):
            self.widgets[name + "_leftPinkyIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "pinky_l_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftPinkyIkPvPickerButton"], "top", 155), (self.widgets[name + "_leftPinkyIkPvPickerButton"], "right", 37)])

        if cmds.objExists(name + ":thumb_l_ik_anim"):
            self.widgets[name + "_leftThumbIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "thumb_l_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftThumbIkPvPickerButton"], "top", 132), (self.widgets[name + "_leftThumbIkPvPickerButton"], "right", 157)])

        self.widgets[name + "_leftIkFingersPvsPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_l_poleVector", "middle_l_poleVector", "ring_l_poleVector", "pinky_l_poleVector", "thumb_l_poleVector"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkFingersPvsPickerButton"], "top", 155), (self.widgets[name + "_leftIkFingersPvsPickerButton"], "right", 12)])


        #Left IK Global Control
        self.widgets[name + "_leftIkGlobalCtrlPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "l_global_ik_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_leftIkGlobalCtrlPickerButton"], "top", 7), (self.widgets[name + "_leftIkGlobalCtrlPickerButton"], "right", 7)])

        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_leftIkGlobalCtrlPickerButton"])
        spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
        self.widgets[name + "_lIkGlobalCtrl_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
        cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_lIkGlobalCtrl_RadioCollection"] ,  "l_global_ik_anim_space_switcher"))



        #Right Pinky
        #get settings off of skeleton settings node to know what it is we need to create
        rightPinkyMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.rightpinkymeta")
        rightPinky1 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightpinky1")
        rightPinky2 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightpinky2")
        rightPinky3 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightpinky3")

        if rightPinkyMeta:
            self.widgets[name + "_rightPinkyMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_metacarpal_ctrl_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinkyMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_rightPinkyMetacarpalPickerButton"], "left", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightPinkyMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "r"))

        if rightPinky1:
            self.widgets[name + "_rightPinky1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_1_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinky1PickerButton"], "top", 55), (self.widgets[name + "_rightPinky1PickerButton"], "left", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightPinky1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "r"))

        if rightPinky2:
            self.widgets[name + "_rightPinky2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_2_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinky2PickerButton"], "top", 80), (self.widgets[name + "_rightPinky2PickerButton"], "left", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightPinky2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "r"))

        if rightPinky3:
            self.widgets[name + "_rightPinky3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "pinky_finger_fk_ctrl_3_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinky3PickerButton"], "top", 105), (self.widgets[name + "_rightPinky3PickerButton"], "left", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightPinky3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "r"))


        #Right Ring
        #get settings off of skeleton settings node to know what it is we need to create
        rightRingMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.rightringmeta")
        rightRing1 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightring1")
        rightRing2 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightring2")
        rightRing3 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightring3")

        if rightRingMeta:
            self.widgets[name + "_rightRingMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_metacarpal_ctrl_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRingMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_rightRingMetacarpalPickerButton"], "left", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightRingMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "r"))

        if rightRing1:
            self.widgets[name + "_rightRing1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_1_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRing1PickerButton"], "top", 55), (self.widgets[name + "_rightRing1PickerButton"], "left", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightRing1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "r"))

        if rightRing2:
            self.widgets[name + "_rightRing2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_2_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRing2PickerButton"], "top", 80), (self.widgets[name + "_rightRing2PickerButton"], "left", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightRing2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "r"))

        if rightRing3:
            self.widgets[name + "_rightRing3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "ring_finger_fk_ctrl_3_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRing3PickerButton"], "top", 105), (self.widgets[name + "_rightRing3PickerButton"], "left", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightRing3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "r"))

        #Right Middle
        #get settings off of skeleton settings node to know what it is we need to create
        rightMiddleMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.rightmiddlemeta")
        rightMiddle1 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightmiddle1")
        rightMiddle2 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightmiddle2")
        rightMiddle3 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightmiddle3")

        if rightMiddleMeta:
            self.widgets[name + "_rightMiddleMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_metacarpal_ctrl_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddleMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_rightMiddleMetacarpalPickerButton"], "left", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightMiddleMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "r"))

        if rightMiddle1:
            self.widgets[name + "_rightMiddle1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_1_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddle1PickerButton"], "top", 55), (self.widgets[name + "_rightMiddle1PickerButton"], "left", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightMiddle1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "r"))

        if rightMiddle2:
            self.widgets[name + "_rightMiddle2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_2_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddle2PickerButton"], "top", 80), (self.widgets[name + "_rightMiddle2PickerButton"], "left", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightMiddle2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "r"))

        if rightMiddle3:
            self.widgets[name + "_rightMiddle3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "middle_finger_fk_ctrl_3_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddle3PickerButton"], "top", 105), (self.widgets[name + "_rightMiddle3PickerButton"], "left", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightMiddle3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "r"))


        #Right Index
        #get settings off of skeleton settings node to know what it is we need to create
        rightIndexMeta = cmds.getAttr(name + ":" + "Skeleton_Settings.rightindexmeta")
        rightIndex1 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightindex1")
        rightIndex2 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightindex2")
        rightIndex3 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightindex3")

        if rightIndexMeta:
            self.widgets[name + "_rightIndexMetacarpalPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_metacarpal_ctrl_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndexMetacarpalPickerButton"], "top", 30), (self.widgets[name + "_rightIndexMetacarpalPickerButton"], "left", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIndexMetacarpalPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "r"))

        if rightIndex1:
            self.widgets[name + "_rightIndex1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_1_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndex1PickerButton"], "top", 55), (self.widgets[name + "_rightIndex1PickerButton"], "left", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIndex1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "r"))

        if rightIndex2:
            self.widgets[name + "_rightIndex2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_2_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndex2PickerButton"], "top", 80), (self.widgets[name + "_rightIndex2PickerButton"], "left", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIndex2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "r"))

        if rightIndex3:
            self.widgets[name + "_rightIndex3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "index_finger_fk_ctrl_3_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndex3PickerButton"], "top", 105), (self.widgets[name + "_rightIndex3PickerButton"], "left", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIndex3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "r"))


        #Right Thumb
        #get settings off of skeleton settings node to know what it is we need to create
        rightThumb1 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightthumb1")
        rightThumb2 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightthumb2")
        rightThumb3 = cmds.getAttr(name + ":" + "Skeleton_Settings.rightthumb3")

        if rightThumb1:
            self.widgets[name + "_rightThumb1PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_1_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumb1PickerButton"], "top", 30), (self.widgets[name + "_rightThumb1PickerButton"], "left", 132)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightThumb1PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "r"))

        if rightThumb2:
            self.widgets[name + "_rightThumb2PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_2_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumb2PickerButton"], "top", 55), (self.widgets[name + "_rightThumb2PickerButton"], "left", 142)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightThumb2PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "r"))

        if rightThumb3:
            self.widgets[name + "_rightThumb3PickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.blue, c = partial(self.buttonSelectCommand, name, "thumb_finger_fk_ctrl_3_r"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumb3PickerButton"], "top", 80), (self.widgets[name + "_rightThumb3PickerButton"], "left", 152)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightThumb3PickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "r"))

        #Right finger row globals
        #get settings off of skeleton settings node to know what it is we need to create
        self.widgets[name + "_rightMetaRowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_r", "middle_metacarpal_ctrl_r", "ring_metacarpal_ctrl_r", "pinky_metacarpal_ctrl_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMetaRowPickerButton"], "top", 30), (self.widgets[name + "_rightMetaRowPickerButton"], "left", 7)])

        self.widgets[name + "_rightKnuckle1RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_1_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightKnuckle1RowPickerButton"], "top", 55), (self.widgets[name + "_rightKnuckle1RowPickerButton"], "left", 7)])

        self.widgets[name + "_rightKnuckle2RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_2_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightKnuckle2RowPickerButton"], "top", 80), (self.widgets[name + "_rightKnuckle2RowPickerButton"], "left", 7)])

        self.widgets[name + "_rightKnuckle3RowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_finger_fk_ctrl_3_r", "middle_finger_fk_ctrl_3_r", "ring_finger_fk_ctrl_3_r", "pinky_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightKnuckle3RowPickerButton"], "top", 105), (self.widgets[name + "_rightKnuckle3RowPickerButton"], "left", 7)])


        #Right finger column globals
        #get settings off of skeleton settings node to know what it is we need to create
        self.widgets[name + "_rightIndexColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndexColumnPickerButton"], "top", 7), (self.widgets[name + "_rightIndexColumnPickerButton"], "left", 107)])

        self.widgets[name + "_rightMiddleColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["middle_metacarpal_ctrl_r", "middle_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddleColumnPickerButton"], "top", 7), (self.widgets[name + "_rightMiddleColumnPickerButton"], "left", 82)])

        self.widgets[name + "_rightRingColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["ring_metacarpal_ctrl_r", "ring_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRingColumnPickerButton"], "top", 7), (self.widgets[name + "_rightRingColumnPickerButton"], "left", 57)])

        self.widgets[name + "_rightPinkyColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["pinky_metacarpal_ctrl_r", "pinky_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinkyColumnPickerButton"], "top", 7), (self.widgets[name + "_rightPinkyColumnPickerButton"], "left", 32)])

        #Right thumb global
        self.widgets[name + "_rightThumbColumnPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["thumb_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_3_r"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumbColumnPickerButton"], "top", 7), (self.widgets[name + "_rightThumbColumnPickerButton"], "left", 132)])


        #Right Finger IK
        if cmds.objExists(name + ":index_r_ik_anim"):
            self.widgets[name + "_rightIndexFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "index_r_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndexFingerIKPickerButton"], "top", 130), (self.widgets[name + "_rightIndexFingerIKPickerButton"], "left", 107)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIndexFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "index", 1, "r"))

        if cmds.objExists(name + ":middle_r_ik_anim"):
            self.widgets[name + "_rightMiddleFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "middle_r_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddleFingerIKPickerButton"], "top", 130), (self.widgets[name + "_rightMiddleFingerIKPickerButton"], "left", 82)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightMiddleFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "middle", 1, "r"))

        if cmds.objExists(name + ":ring_r_ik_anim"):
            self.widgets[name + "_rightRingFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ring_r_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRingFingerIKPickerButton"], "top", 130), (self.widgets[name + "_rightRingFingerIKPickerButton"], "left", 57)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightRingFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "ring", 1, "r"))

        if cmds.objExists(name + ":pinky_r_ik_anim"):
            self.widgets[name + "_rightPinkyFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "pinky_r_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinkyFingerIKPickerButton"], "top", 130), (self.widgets[name + "_rightPinkyFingerIKPickerButton"], "left", 32)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightPinkyFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "pinky", 1, "r"))

        if cmds.objExists(name + ":thumb_r_ik_anim"):
            self.widgets[name + "_rightThumbFingerIKPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "thumb_r_ik_anim"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumbFingerIKPickerButton"], "top", 107), (self.widgets[name + "_rightThumbFingerIKPickerButton"], "left", 152)])
            menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightThumbFingerIKPickerButton"])
            cmds.menuItem(label = "Finger FK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 0, "r"))
            cmds.menuItem(label = "Finger IK Mode", parent = menu, c = partial(self.switchFingerMode, name, "thumb", 1, "r"))

        self.widgets[name + "_rightIkFingersRowPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_r_ik_anim", "middle_r_ik_anim", "ring_r_ik_anim", "pinky_r_ik_anim", "thumb_r_ik_anim"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkFingersRowPickerButton"], "top", 130), (self.widgets[name + "_rightIkFingersRowPickerButton"], "left", 7)])

        #Right Finger IK Pole Vectors
        if cmds.objExists(name + ":index_r_ik_anim"):
            self.widgets[name + "_rightIndexIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "index_r_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIndexIkPvPickerButton"], "top", 155), (self.widgets[name + "_rightIndexIkPvPickerButton"], "left", 112)])

        if cmds.objExists(name + ":middle_r_ik_anim"):
            self.widgets[name + "_rightMiddleIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "middle_r_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightMiddleIkPvPickerButton"], "top", 155), (self.widgets[name + "_rightMiddleIkPvPickerButton"], "left", 87)])

        if cmds.objExists(name + ":ring_r_ik_anim"):
            self.widgets[name + "_rightRingIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "ring_r_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightRingIkPvPickerButton"], "top", 155), (self.widgets[name + "_rightRingIkPvPickerButton"], "left", 62)])

        if cmds.objExists(name + ":pinky_r_ik_anim"):
            self.widgets[name + "_rightPinkyIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "pinky_r_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightPinkyIkPvPickerButton"], "top", 155), (self.widgets[name + "_rightPinkyIkPvPickerButton"], "left", 37)])

        if cmds.objExists(name + ":thumb_r_ik_anim"):
            self.widgets[name + "_rightThumbIkPvPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "thumb_r_poleVector"))
            cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightThumbIkPvPickerButton"], "top", 132), (self.widgets[name + "_rightThumbIkPvPickerButton"], "left", 157)])

        self.widgets[name + "_rightIkFingersPvsPickerButton"] = cmds.button(w = 10, h = 10, label = "", parent = layout, bgc = self.green, c = partial(self.multiButtonSelectCommand, name, ["index_r_poleVector", "middle_r_poleVector", "ring_r_poleVector", "pinky_r_poleVector", "thumb_r_poleVector"]))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkFingersPvsPickerButton"], "top", 155), (self.widgets[name + "_rightIkFingersPvsPickerButton"], "left", 12)])


        #Right IK Global Control
        self.widgets[name + "_rightIkGlobalCtrlPickerButton"] = cmds.button(w = 20, h = 20, label = "", parent = layout, bgc = self.orange, c = partial(self.buttonSelectCommand, name, "r_global_ik_anim"))
        cmds.formLayout(layout, edit = True, af = [(self.widgets[name + "_rightIkGlobalCtrlPickerButton"], "top", 7), (self.widgets[name + "_rightIkGlobalCtrlPickerButton"], "left", 7)])

        menu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rightIkGlobalCtrlPickerButton"])
        spaceMenu = cmds.menuItem(label = "Space Switching", parent = menu, subMenu = True)
        self.widgets[name + "_rIkGlobalCtrl_RadioCollection"] = cmds.radioMenuItemCollection(parent = spaceMenu)
        cmds.menuItem(spaceMenu, edit = True, postMenuCommand = partial(self.getControlSpaces, menu, self.widgets[name + "_rIkGlobalCtrl_RadioCollection"] ,  "r_global_ik_anim_space_switcher"))




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def buttonSelectCommand(self, name, control, *args):
        #when a picker button gets clicked, we need to select the corresponding item(taking into account selection modifiers), and color the button white
        mods = cmds.getModifiers()

        if (mods & 1) > 0:
            if cmds.objExists(name + ":" + control):
                cmds.select(name + ":" + control, tgl = True)

        if (mods & 1) == 0:
            if cmds.objExists(name + ":" + control):
                cmds.select(name + ":" + control)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def clavSelectCommand(self, name, fkControl, ikControl, *args):
        #when a picker button gets clicked, we need to select the corresponding item(taking into account selection modifiers), and color the button white

        #get the clavicle mode
        clavMode = 1
        try:
            #fk_clavicle_l_anim
            side = fkControl.partition("clavicle_")[2].partition("_")[0]
            clavMode = cmds.getAttr(name + ":Rig_Settings." + side + "ClavMode")

        except:
            pass


        mods = cmds.getModifiers()

        if clavMode == 0:
            if (mods & 1) > 0:
                if cmds.objExists(name + ":" + fkControl):
                    cmds.select(name + ":" + fkControl, tgl = True)

            if (mods & 1) == 0:
                if cmds.objExists(name + ":" + fkControl):
                    cmds.select(name + ":" + fkControl)


        if clavMode == 1:
            if (mods & 1) > 0:
                if cmds.objExists(name + ":" + ikControl):
                    cmds.select(name + ":" + ikControl, tgl = True)

            if (mods & 1) == 0:
                if cmds.objExists(name + ":" + ikControl):
                    cmds.select(name + ":" + ikControl)	    


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def multiButtonSelectCommand(self, name, controls, *args):
        #when a picker button gets clicked, we need to select the corresponding item(taking into account selection modifiers), and color the button white
        mods = cmds.getModifiers()

        if (mods & 1) > 0:
            selection = cmds.ls(sl = True)
            for control in controls:
                if cmds.objExists(name + ":" + control):
                    cmds.select(name + ":" + control, tgl = True)

        if (mods & 1) == 0:
            cmds.select(clear = True)
            for control in controls:
                if cmds.objExists(name + ":" + control):
                    cmds.select(name + ":" + control, add = True)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchSpineMode(self, name, mode, *args):

        match = cmds.menuItem(self.widgets["menuBar_settings_matching"], q = True, checkBox = True)
        if mode == "FK":

            #if match on switch is checked, match as well
            if match == True:
                self.match_singleFrame("spine", None, "FK", "IK")

            cmds.setAttr(name + ":Rig_Settings.spine_ik", 0)
            cmds.setAttr(name + ":Rig_Settings.spine_fk", 1)
            cmds.setKeyframe(name + ":Rig_Settings.spine_ik")
            cmds.setKeyframe(name + ":Rig_Settings.spine_fk")




        if mode == "IK":
            if match == True:
                self.match_singleFrame("spine", None, "IK", "FK")

            cmds.setAttr(name + ":Rig_Settings.spine_ik", 1)
            cmds.setAttr(name + ":Rig_Settings.spine_fk", 0)
            cmds.setKeyframe(name + ":Rig_Settings.spine_ik")
            cmds.setKeyframe(name + ":Rig_Settings.spine_fk")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchClavMode(self, name, mode, side, *args):
        match = cmds.menuItem(self.widgets["menuBar_settings_matching"], q = True, checkBox = True)

        if mode == "FK":
            if match == True:
                self.match_singleFrame("clav", side, "FK", "IK")

            cmds.setAttr(name + ":Rig_Settings." + side + "ClavMode", 0)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "ClavMode")

        if mode == "IK":
            if match == True:
                self.match_singleFrame("clav", side, "IK", "FK")

            cmds.setAttr(name + ":Rig_Settings." + side + "ClavMode", 1)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "ClavMode")            


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchArmMode(self, name, mode, side, *args):
        match = cmds.menuItem(self.widgets["menuBar_settings_matching"], q = True, checkBox = True)

        if mode == "FK":
            if match == True:
                self.match_singleFrame("arm", side, "FK", "IK")

            cmds.setAttr(name + ":Rig_Settings." + side + "ArmMode", 0)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "ArmMode")

        if mode == "IK":
            if match == True:
                self.match_singleFrame("arm", side, "IK", "FK")

            cmds.setAttr(name + ":Rig_Settings." + side + "ArmMode", 1)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "ArmMode")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchArmOrientMode(self, name, mode, side, *args):


        currentMode = cmds.getAttr(name + ":Rig_Settings." + side + "FkArmOrient")

        if currentMode != mode:

            if mode == 2:
                constraint = cmds.parentConstraint(name + ":fk_orient_master_loc_" + side, name + ":fk_orient_world_loc_" + side)[0]
                cmds.setKeyframe(name + ":fk_orient_world_loc_" + side)
                cmds.delete(constraint)

            if mode == 1:
                constraint = cmds.parentConstraint(name + ":fk_orient_master_loc_" + side, name + ":fk_orient_body_loc_" + side)[0]
                cmds.setKeyframe(name + ":fk_orient_body_loc_" + side)
                cmds.delete(constraint)

            cmds.setAttr(name + ":Rig_Settings." + side + "FkArmOrient", mode)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchHeadOrientMode(self, name, mode, *args):

        currentSelection = cmds.ls(sl = True)

        currentMode = cmds.getAttr(name + ":head_fk_anim.fkOrientation")
        currentFrame = cmds.currentTime(q = True)


        if currentMode != mode:

            cmds.currentTime(currentFrame - 1)
            cmds.setKeyframe(name + ":head_fk_anim")

            cmds.currentTime(currentFrame)

            #create temp locator
            tempLoc = cmds.spaceLocator(name = "headSnapTempLoc")[0]
            constraint = cmds.parentConstraint(name + ":head_fk_anim", tempLoc)[0]
            cmds.delete(constraint)

            constraint = cmds.orientConstraint(tempLoc, name + ":head_fk_anim")[0]

            cmds.setAttr(name + ":head_fk_anim.fkOrientation", mode)


            cmds.setKeyframe(name + ":head_fk_anim")
            cmds.delete(constraint)
            cmds.delete(tempLoc)

        if len(currentSelection) > 0:
            cmds.select(currentSelection)




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchNeckOrientMode(self, name, mode, *args):

        currentSelection = cmds.ls(sl = True)

        currentMode = cmds.getAttr(name + ":neck_01_fk_anim.fkOrientation")
        currentFrame = cmds.currentTime(q = True)


        if currentMode != mode:

            cmds.currentTime(currentFrame - 1)
            cmds.setKeyframe(name + ":neck_01_fk_anim")

            cmds.currentTime(currentFrame)

            #create temp locator
            tempLoc = cmds.spaceLocator(name = "neckSnapTempLoc")[0]
            constraint = cmds.parentConstraint(name + ":neck_01_fk_anim", tempLoc)[0]
            cmds.delete(constraint)

            constraint = cmds.orientConstraint(tempLoc, name + ":neck_01_fk_anim")[0]

            cmds.setAttr(name + ":neck_01_fk_anim.fkOrientation", mode)


            cmds.setKeyframe(name + ":neck_01_fk_anim")
            cmds.delete(constraint)
            cmds.delete(tempLoc)

        if len(currentSelection) > 0:
            cmds.select(currentSelection)
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchLegMode(self, name, mode, side, *args):

        match = cmds.menuItem(self.widgets["menuBar_settings_matching"], q = True, checkBox = True)

        if mode == "FK":
            if match == True:
                self.match_singleFrame("leg", side, "FK", "IK")
            cmds.setAttr(name + ":Rig_Settings." + side + "LegMode", 0)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "LegMode")

        if mode == "IK":
            if match == True:
                self.match_singleFrame("leg", side, "IK", "FK")
            cmds.setAttr(name + ":Rig_Settings." + side + "LegMode", 1)
            cmds.setKeyframe(name + ":Rig_Settings." + side + "LegMode")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchCustomChainMode(self, character, controlPrefix, mode, *args):

        if mode == "FK":
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_fk", 1)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_ik", 0)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_dynamic", 0)
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_fk")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_ik")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_dynamic")

        if mode == "IK":
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_ik", 1)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_fk", 0)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_dynamic", 0)
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_fk")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_ik")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_dynamic")

        if mode == "DYNAMIC":
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_dynamic", 1)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_fk", 0)
            cmds.setAttr(character + ":Rig_Settings." + controlPrefix + "_ik", 0)
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_fk")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_ik")
            cmds.setKeyframe(character + ":Rig_Settings." + controlPrefix + "_dynamic")




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def switchFingerMode(self, name, finger, mode, side, *args):

        cmds.setAttr(name + ":" + finger + "_finger_" + side + "_mode_anim.FK_IK", mode)
        cmds.setKeyframe(name + ":" + finger + "_finger_" + side + "_mode_anim.FK_IK")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def listView_selAllBelow(self, name, *args):

        mods = cmds.getModifiers()

        #get all controls below
        children = cmds.treeView(self.widgets[name + "_treeViewWidget"], q = True, children = args[0])

        if (mods & 1) > 0:
            for child in children:
                if cmds.objExists(name + ":" + child):
                    cmds.select(name + ":" + child, tgl = True)
                    #hilight object in listView
                    cmds.treeView(self.widgets[name + "_treeViewWidget"], edit = True, selectItem = [child, True])

        if (mods & 1) == 0:
            cmds.select(clear = True)
            for child in children:
                if cmds.objExists(name + ":" + child):
                    cmds.select(name + ":" + child, add = True)
                    #hilight object in listView
                    cmds.treeView(self.widgets[name + "_treeViewWidget"], edit = True, selectItem = [child, True])

        #self.listView_ScriptJob(name)
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def listViewSelectItem(self, name, *args):

        mods = cmds.getModifiers()

        selected = cmds.treeView(self.widgets[name + "_treeViewWidget"], q = True, selectItem = True)
        if (mods & 1) > 0:
            for item in selected:
                if cmds.objExists(name + ":" + item):
                    cmds.select(name + ":" + item, tgl = True)

        if (mods & 1) == 0:
            cmds.select(clear = True)

            for item in selected:
                if cmds.objExists(name + ":" + item):
                    cmds.select(name + ":" + item, add = True)

        #clearSelection

        #self.listView_ScriptJob(name)
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def listView_ScriptJob(self, *args):

        self.widgets["listViewScriptJob"] = cmds.scriptJob(event = ["SelectionChanged", self.listView_scriptJobCommand], parent = self.widgets["window"], runOnce = True, kws = True)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def listView_scriptJobCommand(self, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)


        selection = cmds.ls(sl = True)
        selectedItems = cmds.treeView(self.widgets[character + "_treeViewWidget"], q = True, selectItem = True)


        if selectedItems != None:
            for item in selectedItems:
                if character + ":" + item not in selection:
                    cmds.treeView(self.widgets[character + "_treeViewWidget"], edit = True, selectItem = [item, False])


        for item in selection:
            niceName = item.partition(":")[2]

            if selectedItems != None:
                if niceName not in selectedItems:
                    if cmds.treeView(self.widgets[character + "_treeViewWidget"], q = True, itemExists = niceName):
                        cmds.treeView(self.widgets[character + "_treeViewWidget"], edit = True, selectItem = [niceName, True])

            else:
                if cmds.treeView(self.widgets[character + "_treeViewWidget"], q = True, itemExists = niceName):
                    cmds.treeView(self.widgets[character + "_treeViewWidget"], edit = True, selectItem = [niceName, True])



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createListView(self, name, layout, *args):

        #create a scrollLayout that is a child of the passed in layout
        self.widgets[name + "_listViewMainLayout"] = cmds.scrollLayout(w = 400, h = 700, hst = 0, parent = layout)
        #self.formsToHide.append(self.widgets[name + "_listViewMainLayout"] )


        #create the tree view widget
        self.widgets[name + "_treeViewWidget"] = cmds.treeView(parent = self.widgets[name + "_listViewMainLayout"], numberOfButtons = 2, abr = True, w = 380, h = 3000, selectionChangedCommand = partial(self.listViewSelectItem, name), pressCommand = [[1, partial(self.listView_selAllBelow, name)], [2, partial(self.toggleVisibilityOnSelectedControlGroups, name)]])


        #TOP LEVEL CONTROLS
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("RIG CONTROLS", ""), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["RIG CONTROLS", 1], expandItem = ["RIG CONTROLS", False],tc = ["RIG CONTROLS", self.orange[0], self.orange[1], self.orange[2]], bti = [["RIG CONTROLS", 1, "S"], ["RIG CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("Rig_Settings", "RIG CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("master_anim", "RIG CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("offset_anim", "RIG CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("root_anim", "RIG CONTROLS"), hb = True)

        #TORSO
        torsoControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("TORSO CONTROLS", "RIG CONTROLS"), hb = False)
        torsoControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["TORSO CONTROLS", 1], expandItem = ["TORSO CONTROLS", False],tc = ["TORSO CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["TORSO CONTROLS", .3, .3, .3], bti = [["TORSO CONTROLS", 1, "S"], ["TORSO CONTROLS", 2, "V"]])
        bodyAnim = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("body_anim", "TORSO CONTROLS"), hb = True)
        hipAnim = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("hip_anim", "TORSO CONTROLS"), hb = True)

        fkControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("SPINE FK CONTROLS", "TORSO CONTROLS"), hb = False)
        torsoControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["SPINE FK CONTROLS", 2], tc = ["SPINE FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["SPINE FK CONTROLS", 1, "S"], ["SPINE FK CONTROLS", 2, "V"]])


        for control in ["spine_01_anim", "spine_02_anim", "spine_03_anim", "spine_04_anim", "spine_05_anim"]:
            if cmds.objExists(name + ":" + control):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (control, "SPINE FK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "chest_ik_anim"):
            ikControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("SPINE IK CONTROLS", "TORSO CONTROLS"), hb = False)
            torsoControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["SPINE IK CONTROLS", 2], tc = ["SPINE IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["SPINE IK CONTROLS", 1, "S"], ["SPINE IK CONTROLS", 2, "V"]])

            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("chest_ik_anim", "SPINE IK CONTROLS"), hb = True)
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("mid_ik_anim", "SPINE IK CONTROLS"), hb = True)

        #HEAD
        headControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("HEAD CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["HEAD CONTROLS", 1], expandItem = ["HEAD CONTROLS", False],tc = ["HEAD CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["HEAD CONTROLS", .4, .4, .4], bti = [["HEAD CONTROLS", 1, "S"], ["HEAD CONTROLS", 2, "V"]])

        for control in ["neck_01_fk_anim", "neck_02_fk_anim", "neck_03_fk_anim"]:
            if cmds.objExists(name + ":" + control):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (control, "HEAD CONTROLS"), hb = True)

        headAnim = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("head_fk_anim", "HEAD CONTROLS"), hb = True)

        #LEFT ARM
        lArmControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("LEFT ARM CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["LEFT ARM CONTROLS", 1], expandItem = ["LEFT ARM CONTROLS", False],tc = ["LEFT ARM CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["LEFT ARM CONTROLS", .3, .3, .3], bti = [["LEFT ARM CONTROLS", 1, "S"], ["LEFT ARM CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("clavicle_l_anim", "LEFT ARM CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_clavicle_l_anim", "LEFT ARM CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L ARM FK CONTROLS", "LEFT ARM CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L ARM FK CONTROLS", 2], tc = ["L ARM FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L ARM FK CONTROLS", 1, "S"], ["L ARM FK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_arm_l_anim", "L ARM FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_elbow_l_anim", "L ARM FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_wrist_l_anim", "L ARM FK CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L ARM IK CONTROLS", "LEFT ARM CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L ARM IK CONTROLS", 2], tc = ["L ARM IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L ARM IK CONTROLS", 1, "S"], ["L ARM IK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_elbow_l_anim", "L ARM IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_wrist_l_anim", "L ARM IK CONTROLS"), hb = True)


        #RIGHT ARM
        lArmControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("RIGHT ARM CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["RIGHT ARM CONTROLS", 1], expandItem = ["RIGHT ARM CONTROLS", False],tc = ["RIGHT ARM CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["RIGHT ARM CONTROLS", .4, .4, .4], bti = [["RIGHT ARM CONTROLS", 1, "S"], ["RIGHT ARM CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("clavicle_r_anim", "RIGHT ARM CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_clavicle_r_anim", "RIGHT ARM CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R ARM FK CONTROLS", "RIGHT ARM CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R ARM FK CONTROLS", 2], tc = ["R ARM FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R ARM FK CONTROLS", 1, "S"], ["R ARM FK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_arm_r_anim", "R ARM FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_elbow_r_anim", "R ARM FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_wrist_r_anim", "R ARM FK CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R ARM IK CONTROLS", "RIGHT ARM CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R ARM IK CONTROLS", 2], tc = ["R ARM IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R ARM FK CONTROLS", 1, "S"], ["R ARM FK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_elbow_r_anim", "R ARM IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_wrist_r_anim", "R ARM IK CONTROLS"), hb = True)


        #LEFT FINGERS
        lFingerControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("LEFT FINGER CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["LEFT FINGER CONTROLS", 1], expandItem = ["LEFT FINGER CONTROLS", False],tc = ["LEFT FINGER CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["LEFT FINGER CONTROLS", .3, .3, .3], bti = [["LEFT FINGER CONTROLS", 1, "S"], ["LEFT FINGER CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L FINGER FK CONTROLS", "LEFT FINGER CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L FINGER FK CONTROLS", 2], tc = ["L FINGER FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], expandItem = ["L FINGER FK CONTROLS", False], bti = [["L FINGER FK CONTROLS", 1, "S"], ["L FINGER FK CONTROLS", 2, "V"]])

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:

            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L_" + finger + "_FK", "L FINGER FK CONTROLS"), hb = False)
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L_" + finger + "_FK", 2], tc = ["L_" + finger + "_FK", self.blue[0], self.blue[1], self.blue[2]], bti = [["L_" + finger + "_FK", 1, "S"], ["L_" + finger + "_FK", 2, "V"]])

            if cmds.objExists(name + ":" + finger + "_metacarpal_ctrl_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_metacarpal_ctrl_l", "L_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_1_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_1_l", "L_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_2_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_2_l", "L_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_3_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_3_l", "L_" + finger + "_FK"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L FINGER IK CONTROLS", "LEFT FINGER CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L FINGER IK CONTROLS", 2], tc = ["L FINGER IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], expandItem = ["L FINGER IK CONTROLS", False], bti = [["L FINGER IK CONTROLS", 1, "S"], ["L FINGER IK CONTROLS", 2, "V"]])


        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_l_ik_anim"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_l_ik_anim", "L FINGER IK CONTROLS"), hb = True)

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_l_ik_anim"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_l_poleVector", "L FINGER IK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "l_global_ik_anim"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("l_global_ik_anim", "L FINGER IK CONTROLS"), hb = True)


        #RIGHT FINGERS
        lFingerControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("RIGHT FINGER CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["RIGHT FINGER CONTROLS", 1], expandItem = ["RIGHT FINGER CONTROLS", False],tc = ["RIGHT FINGER CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["RIGHT FINGER CONTROLS", .4, .4, .4], bti = [["RIGHT FINGER CONTROLS", 1, "S"], ["RIGHT FINGER CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R FINGER FK CONTROLS", "RIGHT FINGER CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R FINGER FK CONTROLS", 2], tc = ["R FINGER FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], expandItem = ["R FINGER FK CONTROLS", False], bti = [["R FINGER FK CONTROLS", 1, "S"], ["R FINGER FK CONTROLS", 2, "V"]])

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:

            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R_" + finger + "_FK", "R FINGER FK CONTROLS"), hb = False)
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R_" + finger + "_FK", 2], tc = ["R_" + finger + "_FK", self.blue[0], self.blue[1], self.blue[2]], bti = [["R_" + finger + "_FK", 1, "S"], ["R_" + finger + "_FK", 2, "V"]])

            if cmds.objExists(name + ":" + finger + "_metacarpal_ctrl_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_metacarpal_ctrl_r", "R_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_1_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_1_r", "R_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_2_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_2_r", "R_" + finger + "_FK"), hb = True)

            if cmds.objExists(name + ":" + finger + "_finger_fk_ctrl_3_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_finger_fk_ctrl_3_r", "R_" + finger + "_FK"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R FINGER IK CONTROLS", "RIGHT FINGER CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R FINGER IK CONTROLS", 2], tc = ["R FINGER IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], expandItem = ["R FINGER IK CONTROLS", False],  bti = [["R FINGER IK CONTROLS", 1, "S"], ["R FINGER IK CONTROLS", 2, "V"]])


        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_r_ik_anim"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_r_ik_anim", "R FINGER IK CONTROLS"), hb = True)

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_r_ik_anim"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (finger + "_r_poleVector", "R FINGER IK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "r_global_ik_anim"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("r_global_ik_anim", "R FINGER IK CONTROLS"), hb = True)


        #LEFT LEG
        lLegControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("LEFT LEG CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["LEFT LEG CONTROLS", 1], expandItem = ["LEFT LEG CONTROLS", False],tc = ["LEFT LEG CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["LEFT LEG CONTROLS", .3, .3, .3], bti = [["LEFT LEG CONTROLS", 1, "S"], ["LEFT LEG CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L LEG FK CONTROLS", "LEFT LEG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L LEG FK CONTROLS", 2], tc = ["L LEG FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L LEG FK CONTROLS", 1, "S"], ["L LEG FK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_thigh_l_anim", "L LEG FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_calf_l_anim", "L LEG FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_foot_l_anim", "L LEG FK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "fk_ball_l_anim"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_ball_l_anim", "L LEG FK CONTROLS"), hb = True)


        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L LEG IK CONTROLS", "LEFT LEG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L LEG IK CONTROLS", 2], tc = ["L LEG IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L LEG IK CONTROLS", 1, "S"], ["L LEG IK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_foot_anim_l", "L LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("heel_ctrl_l", "L LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("toe_wiggle_ctrl_l", "L LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("toe_tip_ctrl_l", "L LEG IK CONTROLS"), hb = True)



        #RIGHT LEG
        rLegControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("RIGHT LEG CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["RIGHT LEG CONTROLS", 1], expandItem = ["RIGHT LEG CONTROLS", False],tc = ["RIGHT LEG CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["RIGHT LEG CONTROLS", .4, .4, .4], bti = [["RIGHT LEG CONTROLS", 1, "S"], ["RIGHT LEG CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R LEG FK CONTROLS", "RIGHT LEG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R LEG FK CONTROLS", 2], tc = ["R LEG FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R LEG FK CONTROLS", 1, "S"], ["R LEG FK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_thigh_r_anim", "R LEG FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_calf_r_anim", "R LEG FK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_foot_r_anim", "R LEG FK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "fk_ball_r_anim"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_ball_r_anim", "R LEG FK CONTROLS"), hb = True)


        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R LEG IK CONTROLS", "RIGHT LEG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R LEG IK CONTROLS", 2], tc = ["R LEG IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R LEG IK CONTROLS", 1, "S"], ["R LEG IK CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_foot_anim_r", "R LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("heel_ctrl_r", "R LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("toe_wiggle_ctrl_r", "R LEG IK CONTROLS"), hb = True)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("toe_tip_ctrl_r", "R LEG IK CONTROLS"), hb = True)


        #LEFT TOES
        lToeControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("LEFT TOE CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["LEFT TOE CONTROLS", 1], expandItem = ["LEFT TOE CONTROLS", False],tc = ["LEFT TOE CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["LEFT TOE CONTROLS", .3, .3, .3], bti = [["LEFT TOE CONTROLS", 1, "S"], ["LEFT TOE CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L TOE FK CONTROLS", "LEFT TOE CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L TOE FK CONTROLS", 2], tc = ["L TOE FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L TOE FK CONTROLS", 1, "S"], ["L TOE FK CONTROLS", 2, "V"]])

        for toe in ["index", "middle", "ring", "pinky", "bigtoe"]:

            if cmds.objExists(name + ":" + toe + "_metatarsal_ctrl_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "_metatarsal_ctrl_l", "L TOE FK CONTROLS"), hb = True)

            if cmds.objExists(name + ":" + toe + "toe_fk_ctrl_1_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "toe_fk_ctrl_1_l", "L TOE FK CONTROLS"), hb = True)

            if cmds.objExists(name + ":" + toe + "toe_fk_ctrl_2_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "toe_fk_ctrl_2_l", "L TOE FK CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("L TOE IK CONTROLS", "LEFT TOE CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["L TOE IK CONTROLS", 2], tc = ["L TOE IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["L TOE IK CONTROLS", 1, "S"], ["L TOE IK CONTROLS", 2, "V"]])

        for toe in ["index", "middle", "ring", "pinky", "bigtoe"]:
            if cmds.objExists(name + ":" + toe + "_ik_ctrl_l"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "_ik_ctrl_l", "L TOE IK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "ik_global_ctrl_l"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_global_ctrl_l", "L TOE IK CONTROLS"), hb = True)


        #RIGHT TOES
        lToeControls = cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("RIGHT TOE CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["RIGHT TOE CONTROLS", 1], expandItem = ["RIGHT TOE CONTROLS", False],tc = ["RIGHT TOE CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["RIGHT TOE CONTROLS", .4, .4, .4], bti = [["RIGHT TOE CONTROLS", 1, "S"], ["RIGHT TOE CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R TOE FK CONTROLS", "RIGHT TOE CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R TOE FK CONTROLS", 2], tc = ["R TOE FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R TOE FK CONTROLS", 1, "S"], ["R TOE FK CONTROLS", 2, "V"]])

        for toe in ["index", "middle", "ring", "pinky", "bigtoe"]:

            if cmds.objExists(name + ":" + toe + "_metatarsal_ctrl_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "_metatarsal_ctrl_r", "R TOE FK CONTROLS"), hb = True)

            if cmds.objExists(name + ":" + toe + "toe_fk_ctrl_1_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "toe_fk_ctrl_1_r", "R TOE FK CONTROLS"), hb = True)

            if cmds.objExists(name + ":" + toe + "toe_fk_ctrl_2_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "toe_fk_ctrl_2_r", "R TOE FK CONTROLS"), hb = True)

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("R TOE IK CONTROLS", "RIGHT TOE CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["R TOE IK CONTROLS", 2], tc = ["R TOE IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [["R TOE IK CONTROLS", 1, "S"], ["R TOE IK CONTROLS", 2, "V"]])

        for toe in ["index", "middle", "ring", "pinky", "bigtoe"]:
            if cmds.objExists(name + ":" + toe + "_ik_ctrl_r"):
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (toe + "_ik_ctrl_r", "R TOE IK CONTROLS"), hb = True)

        if cmds.objExists(name + ":" + "ik_global_ctrl_r"):
            cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("ik_global_ctrl_r", "R TOE IK CONTROLS"), hb = True)



        #CUSTOM JOINTS
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("CUSTOM LEAF CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["CUSTOM LEAF CONTROLS", 1], expandItem = ["CUSTOM LEAF CONTROLS", False],tc = ["CUSTOM LEAF CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["CUSTOM LEAF CONTROLS", .3, .3, .3], bti = [["CUSTOM LEAF CONTROLS", 1, "S"], ["CUSTOM LEAF CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("CUSTOM JIGGLE CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["CUSTOM JIGGLE CONTROLS", 1], expandItem = ["CUSTOM JIGGLE CONTROLS", False],tc = ["CUSTOM JIGGLE CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["CUSTOM JIGGLE CONTROLS", .4, .4, .4], bti = [["CUSTOM JIGGLE CONTROLS", 1, "S"], ["CUSTOM JIGGLE CONTROLS", 2, "V"]])

        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("CUSTOM CHAIN CONTROLS", "RIG CONTROLS"), hb = False)
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = ["CUSTOM CHAIN CONTROLS", 1], expandItem = ["CUSTOM CHAIN CONTROLS", False],tc = ["CUSTOM CHAIN CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = ["CUSTOM CHAIN CONTROLS", .3, .3, .3], bti = [["CUSTOM CHAIN CONTROLS", 1, "S"], ["CUSTOM CHAIN CONTROLS", 2, "V"]])




        customJoints = []
        attrs = cmds.listAttr(name + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        customJoints = sorted(customJoints)

        for joint in customJoints:
            attribute = cmds.getAttr(name + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "leaf":
                label = label.partition(" (")[0]
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + "_anim", "CUSTOM LEAF CONTROLS"), hb = True)

            if jointType == "jiggle":
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + "_anim", "CUSTOM JIGGLE CONTROLS"), hb = True)

            if jointType == "chain" or jointType == "dynamic":
                numJointsInChain = label.partition("(")[2].partition(")")[0]
                label = label.partition(" (")[0]

                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label +  " CONTROLS", "CUSTOM CHAIN CONTROLS"), hb = False)
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = [label +  " CONTROLS", 1], expandItem = [label +  " CONTROLS", False],tc = [label +  " CONTROLS", self.orange[0], self.orange[1], self.orange[2]], labelBackgroundColor = [label +  " CONTROLS", .3, .3, .3])

                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + " FK CONTROLS", label +  " CONTROLS"), hb = False)
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = [label + " FK CONTROLS", 2], tc = [label + " FK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [[label + " FK CONTROLS", 1, "S"], [label + " FK CONTROLS", 2, "V"]])

                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + " IK CONTROLS", label +  " CONTROLS"), hb = False)
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = [label + " IK CONTROLS", 2], tc = [label + " IK CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [[label + " IK CONTROLS", 1, "S"], [label + " IK CONTROLS", 2, "V"]])

                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + " DYNAMIC CONTROLS", label +  " CONTROLS"), hb = False)
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, ff = [label + " DYNAMIC CONTROLS", 2], tc = [label + " DYNAMIC CONTROLS", self.blue[0], self.blue[1], self.blue[2]], bti = [[label + " DYNAMIC CONTROLS", 1, "S"], [label + " DYNAMIC CONTROLS", 2, "V"]])



                for i in range(int(numJointsInChain) + 1):
                    if cmds.objExists(name + ":" + "fk_" + label + "_0" + str(i) + "_anim"):
                        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = ("fk_" + label + "_0" + str(i) + "_anim", label + " FK CONTROLS"), hb = True)

                #ik controls
                cmds.select("*:" + label + "_ik_*_anim")
                selection = cmds.ls(sl = True)
                for each in selection:
                    niceName = each.partition(":")[2]
                    try:
                        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (niceName, label + " IK CONTROLS"), hb = True)
                    except:
                        pass


                for i in range(int(numJointsInChain)):
                    if cmds.objExists(name + ":" + label + "_cv_" + str(i) + "_anim"):
                        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + "_cv_" + str(i) + "_anim", label + " IK CONTROLS"), hb = True)

                #dynamic controls
                cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, addItem = (label + "_dyn_anim", label + " DYNAMIC CONTROLS"), hb = True)


        #expand 
        cmds.treeView(self.widgets[name + "_treeViewWidget"], e=True, expandItem = ["RIG CONTROLS", True])


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createRigSettings(self, name, layout, *args):

        #create a columnLayout that is a child of the passed in layout
        self.widgets[name + "_rigSettingsMainColumn"] = cmds.columnLayout(parent = layout)
        self.formsToHide.append(self.widgets[name + "_rigSettingsMainColumn"])

        #First create left and right arm frame layouts with all of the settings for the arms
        #rig mode, fk orientation space, ik stretch settings, arm roll settings


        #LEFT ARM
        self.widgets[name + "_rigSettings_leftArmFrame"] = cmds.frameLayout(label = "Left Arm", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "LeftArmSettings"), ec = partial(self.expandCommand, name, "LeftArmSettings"))
        self.widgets[name + "_rigSettings_leftArmForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_leftArmFrame"])

        #rig mode
        text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_leftArmMode_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_leftArmFkModeButton"] = cmds.iconTextRadioButton(cl = self.widgets[name + "rigSettings_leftArmMode_Collection"], image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchArmMode, name, "FK", "l"))
        self.widgets[name + "_rigSettings_leftArmIkModeButton"] = cmds.iconTextRadioButton(cl = self.widgets[name + "rigSettings_leftArmMode_Collection"], image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchArmMode, name, "IK", "l"))

        mode = cmds.getAttr(name + ":Rig_Settings.lArmMode")

        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmFkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_leftArmFkModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmIkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_leftArmIkModeButton"], "right", 20)])

        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmIkModeButton"], edit = True, select = True)


        #fk orientation space
        text2 = cmds.text(label = "FK Arm Orientation Space:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_leftArmOrient_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_leftArmFkOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsClav_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsClav_on.bmp",  w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 0, "l"))
        self.widgets[name + "_rigSettings_leftArmBodyOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsBody_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsBody_on.bmp",w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 1, "l"))
        self.widgets[name + "_rigSettings_leftArmWorldOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsWorld_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsWorld_on.bmp",w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 2, "l"))


        mode = cmds.getAttr(name + ":Rig_Settings.lFkArmOrient")

        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text2, "top", 85),(text2, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmFkOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_leftArmFkOrientModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmBodyOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_leftArmBodyOrientModeButton"], "left", 138)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmWorldOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_leftArmWorldOrientModeButton"], "right", 20)])


        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmFkOrientModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmBodyOrientModeButton"], edit = True, select = True)
        if mode == 2:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmWorldOrientModeButton"], edit = True, select = True)


        #ik stretch settings
        text3 = cmds.text(label = "IK Arm Stretch Settings", font = "boldLabelFont")
        text4 = cmds.text(label = "Stretch")
        text5 = cmds.text(label = "Squash")
        stetchVal = cmds.getAttr(name + ":ik_wrist_l_anim.stretch")

        try:
            squashVal = cmds.getAttr(name + ":ik_wrist_l_anim.squash")
        except:
            pass

        try:
            self.widgets[name + "_rigSettings_leftArmIkStretchField"] = cmds.floatField( minValue=0, maxValue=1, value=stetchVal, w = 100)
        except:
            cmds.warning("Left Arm IK stretch above or below the allowed range.")

        try:
            self.widgets[name + "_rigSettings_leftArmIkStretchBiasField"] = cmds.floatField( minValue=0, maxValue=1, value=squashVal, w = 100 )
        except:
            cmds.warning("Left Arm IK Squash above or below the allowed range.")


        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text3, "top", 145),(text3, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text4, "top", 173),(text4, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text5, "top", 173),(text5, "left", 220)])

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmIkStretchField"], "top", 170),(self.widgets[name + "_rigSettings_leftArmIkStretchField"], "left", 55)])
        except:
            pass

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmIkStretchBiasField"], "top", 170),(self.widgets[name + "_rigSettings_leftArmIkStretchBiasField"], "left", 250)])
        except:
            pass

        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmIkStretchField"], name + ":ik_wrist_l_anim.stretch" )
        except:
            pass

        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmIkStretchBiasField"], name + ":ik_wrist_l_anim.squash")
        except:
            pass

        #arm roll settings
        text6 = cmds.text(label = "Arm Roll Settings", font = "boldLabelFont")
        text7 = cmds.text(label = "Upper Arm Twist Amount:")
        text8 = cmds.text(label = "Lower Arm Twist Amount:")

        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text6, "top", 200),(text6, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text7, "top", 223),(text7, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text8, "top", 223),(text8, "left", 220)])

        if cmds.objExists(name + ":Rig_Settings.lUpperarmTwistAmount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.lUpperarmTwistAmount")
            self.widgets[name + "_rigSettings_leftArmUpArmTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmUpArmTwistField"], "top", 240),(self.widgets[name + "_rigSettings_leftArmUpArmTwistField"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmUpArmTwistField"], name + ":Rig_Settings.lUpperarmTwistAmount" )

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 242),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.lUpperarmTwist2Amount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.lUpperarmTwist2Amount")
            self.widgets[name + "_rigSettings_leftArmUpArmTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmUpArmTwist2Field"], "top", 260),(self.widgets[name + "_rigSettings_leftArmUpArmTwist2Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmUpArmTwist2Field"], name + ":Rig_Settings.lUpperarmTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 262),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.lUpperarmTwist3Amount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.lUpperarmTwist3Amount")
            self.widgets[name + "_rigSettings_leftArmUpArmTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmUpArmTwist3Field"], "top", 280),(self.widgets[name + "_rigSettings_leftArmUpArmTwist3Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmUpArmTwist3Field"], name + ":Rig_Settings.lUpperarmTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 282),(text, "left", 10)])


        #lower arm
        if cmds.objExists(name + ":Rig_Settings.lForearmTwistAmount"):  
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.lForearmTwistAmount")
            self.widgets[name + "_rigSettings_leftArmLowArmTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100 )
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmLowArmTwistField"], "top", 240),(self.widgets[name + "_rigSettings_leftArmLowArmTwistField"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmLowArmTwistField"], name + ":Rig_Settings.lForearmTwistAmount")

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 242),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.lForearmTwist2Amount"):
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.lForearmTwist2Amount")
            self.widgets[name + "_rigSettings_leftArmLowArmTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmLowArmTwist2Field"], "top", 260),(self.widgets[name + "_rigSettings_leftArmLowArmTwist2Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmLowArmTwist2Field"], name + ":Rig_Settings.lForearmTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 262),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.lForearmTwist3Amount"):
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.lForearmTwist3Amount")
            self.widgets[name + "_rigSettings_leftArmLowArmTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftArmLowArmTwist3Field"], "top", 280),(self.widgets[name + "_rigSettings_leftArmLowArmTwist3Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftArmLowArmTwist3Field"], name + ":Rig_Settings.lForearmTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftArmForm"], edit = True, af = [(text, "top", 282),(text, "left", 200)])


        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_leftArmForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Left Arm Rig Settings", c = partial(self.selectRigSettings_Specific, "leftArm"))
















        #RIGHT ARM
        self.widgets[name + "_rigSettings_rightArmFrame"] = cmds.frameLayout(label = "Right Arm", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "RightArmSettings"), ec = partial(self.expandCommand, name, "RightArmSettings"))
        self.widgets[name + "_rigSettings_rightArmForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_rightArmFrame"])

        #rig mode
        text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_rightArmMode_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_rightArmFkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchArmMode, name, "FK", "r"))
        self.widgets[name + "_rigSettings_rightArmIkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchArmMode, name, "IK", "r"))

        mode = cmds.getAttr(name + ":Rig_Settings.rArmMode")

        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmFkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_rightArmFkModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmIkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_rightArmIkModeButton"], "right", 20)])

        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmIkModeButton"], edit = True, select = True)


        #fk orientation space
        text2 = cmds.text(label = "FK Arm Orientation Space:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_rightArmOrient_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_rightArmFkOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsClav_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsClav_on.bmp",  w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 0, "r"))
        self.widgets[name + "_rigSettings_rightArmBodyOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsBody_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsBody_on.bmp",w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 1, "r"))
        self.widgets[name + "_rigSettings_rightArmWorldOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsWorld_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsWorld_on.bmp",w = 115, h = 30, onc = partial(self.switchArmOrientMode, name, 2, "r"))


        mode = cmds.getAttr(name + ":Rig_Settings.rFkArmOrient")

        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text2, "top", 85),(text2, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmFkOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_rightArmFkOrientModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmBodyOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_rightArmBodyOrientModeButton"], "left", 138)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmWorldOrientModeButton"], "top", 105),(self.widgets[name + "_rigSettings_rightArmWorldOrientModeButton"], "right", 20)])


        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmFkOrientModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmBodyOrientModeButton"], edit = True, select = True)
        if mode == 2:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmWorldOrientModeButton"], edit = True, select = True)


        #ik stretch settings
        text3 = cmds.text(label = "IK Arm Stretch Settings", font = "boldLabelFont")
        text4 = cmds.text(label = "Stretch")
        text5 = cmds.text(label = "Squash")
        stetchVal = cmds.getAttr(name + ":ik_wrist_r_anim.stretch")

        try:
            squashVal = cmds.getAttr(name + ":ik_wrist_r_anim.squash")
        except:
            pass


        try:
            self.widgets[name + "_rigSettings_rightArmIkStretchField"] = cmds.floatField( minValue=0, maxValue=1, value=stetchVal, w = 100)
        except:
            cmds.warning("Right Arm IK Stretch above or below the allowed range.")

        try:
            self.widgets[name + "_rigSettings_rightArmIkStretchBiasField"] = cmds.floatField( minValue=0, maxValue=1, value=squashVal, w = 100 )
        except:
            cmds.warning("Right Arm IK squash above or below the allowed range.")


        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text3, "top", 145),(text3, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text4, "top", 173),(text4, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text5, "top", 173),(text5, "left", 220)])

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmIkStretchField"], "top", 170),(self.widgets[name + "_rigSettings_rightArmIkStretchField"], "left", 55)])
        except:
            pass

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmIkStretchBiasField"], "top", 170),(self.widgets[name + "_rigSettings_rightArmIkStretchBiasField"], "left", 250)])
        except:
            pass

        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmIkStretchField"], name + ":ik_wrist_r_anim.stretch" )
        except:
            pass
        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmIkStretchBiasField"], name + ":ik_wrist_r_anim.squash")
        except:
            pass


        #arm roll settings
        text6 = cmds.text(label = "Arm Roll Settings", font = "boldLabelFont")
        text7 = cmds.text(label = "Upper Arm Twist Amount:")
        text8 = cmds.text(label = "Lower Arm Twist Amount:")
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text6, "top", 200),(text6, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text7, "top", 223),(text7, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text8, "top", 223),(text8, "left", 220)])

        if cmds.objExists(name + ":Rig_Settings.rUpperarmTwistAmount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.rUpperarmTwistAmount")
            self.widgets[name + "_rigSettings_rightArmUpArmTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmUpArmTwistField"], "top", 240),(self.widgets[name + "_rigSettings_rightArmUpArmTwistField"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmUpArmTwistField"], name + ":Rig_Settings.rUpperarmTwistAmount" )

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 242),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.rUpperarmTwist2Amount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.rUpperarmTwist2Amount")
            self.widgets[name + "_rigSettings_rightArmUpArmTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmUpArmTwist2Field"], "top", 260),(self.widgets[name + "_rigSettings_rightArmUpArmTwist2Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmUpArmTwist2Field"], name + ":Rig_Settings.rUpperarmTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 262),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.rUpperarmTwist3Amount"):
            upArmVal = cmds.getAttr(name + ":Rig_Settings.rUpperarmTwist3Amount")
            self.widgets[name + "_rigSettings_rightArmUpArmTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmUpArmTwist3Field"], "top", 280),(self.widgets[name + "_rigSettings_rightArmUpArmTwist3Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmUpArmTwist3Field"], name + ":Rig_Settings.rUpperarmTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 282),(text, "left", 10)])


        #lower arm
        if cmds.objExists(name + ":Rig_Settings.rForearmTwistAmount"):  
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.rForearmTwistAmount")
            self.widgets[name + "_rigSettings_rightArmLowArmTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100 )
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmLowArmTwistField"], "top", 240),(self.widgets[name + "_rigSettings_rightArmLowArmTwistField"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmLowArmTwistField"], name + ":Rig_Settings.rForearmTwistAmount")

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 242),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.rForearmTwist2Amount"):
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.rForearmTwist2Amount")
            self.widgets[name + "_rigSettings_rightArmLowArmTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmLowArmTwist2Field"], "top", 260),(self.widgets[name + "_rigSettings_rightArmLowArmTwist2Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmLowArmTwist2Field"], name + ":Rig_Settings.rForearmTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 262),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.rForearmTwist3Amount"):
            lowArmVal = cmds.getAttr(name + ":Rig_Settings.rForearmTwist3Amount")
            self.widgets[name + "_rigSettings_rightArmLowArmTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowArmVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightArmLowArmTwist3Field"], "top", 280),(self.widgets[name + "_rigSettings_rightArmLowArmTwist3Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightArmLowArmTwist3Field"], name + ":Rig_Settings.rForearmTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightArmForm"], edit = True, af = [(text, "top", 282),(text, "left", 200)])

        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_rightArmForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Right Arm Rig Settings", c = partial(self.selectRigSettings_Specific, "rightArm"))






        #LEFT LEG
        self.widgets[name + "_rigSettings_leftLegFrame"] = cmds.frameLayout(label = "Left Leg", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "LeftLegSettings"), ec = partial(self.expandCommand, name, "LeftLegSettings"))
        self.widgets[name + "_rigSettings_leftLegForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_leftLegFrame"])

        #rig mode
        text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_leftLegMode_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_leftLegFkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchLegMode, name, "FK", "l"))
        self.widgets[name + "_rigSettings_leftLegIkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchLegMode, name, "IK", "l"))

        mode = cmds.getAttr(name + ":Rig_Settings.lLegMode")

        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegFkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_leftLegFkModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegIkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_leftLegIkModeButton"], "right", 20)])

        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftLegFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftLegIkModeButton"], edit = True, select = True)


        #ik stretch settings
        text3 = cmds.text(label = "IK Leg Stretch Settings", font = "boldLabelFont")
        text4 = cmds.text(label = "Stretch")
        text5 = cmds.text(label = "Squash")
        stetchVal = cmds.getAttr(name + ":ik_foot_anim_l.stretch")

        try:
            squashVal = cmds.getAttr(name + ":ik_foot_anim_l.squash")
        except:
            pass


        try:
            self.widgets[name + "_rigSettings_leftLegIkStretchField"] = cmds.floatField( minValue=0, maxValue=1, value=stetchVal, w = 100)
        except:
            pass

        try:
            self.widgets[name + "_rigSettings_leftLegIkStretchBiasField"] = cmds.floatField( minValue=0, maxValue=1, value=squashVal, w = 100 )
        except:
            pass

        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text3, "top", 85),(text3, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text4, "top", 108),(text4, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text5, "top", 108),(text5, "left", 220)])

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegIkStretchField"], "top", 105),(self.widgets[name + "_rigSettings_leftLegIkStretchField"], "left", 55)])
        except:
            pass

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegIkStretchBiasField"], "top", 105),(self.widgets[name + "_rigSettings_leftLegIkStretchBiasField"], "left", 250)])
        except:
            pass
        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegIkStretchField"], name + ":ik_foot_anim_l.stretch" )
        except:
            pass
        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegIkStretchBiasField"], name + ":ik_foot_anim_l.squash")
        except:
            pass


        #leg roll settings
        text6 = cmds.text(label = "Leg Roll Settings", font = "boldLabelFont")
        text7 = cmds.text(label = "Upper Leg Twist Amount:")
        text8 = cmds.text(label = "Lower Leg Twist Amount:")
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text6, "top", 145),(text6, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text7, "top", 173),(text7, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text8, "top", 173),(text8, "left", 220)])

        if cmds.objExists(name + ":Rig_Settings.lThighTwistAmount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.lThighTwistAmount")
            self.widgets[name + "_rigSettings_leftLegThighTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegThighTwistField"], "top", 190),(self.widgets[name + "_rigSettings_leftLegThighTwistField"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegThighTwistField"], name + ":Rig_Settings.lThighTwistAmount" )

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 192),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.lThighTwist2Amount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.lThighTwist2Amount")
            self.widgets[name + "_rigSettings_leftLegThighTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegThighTwist2Field"], "top", 210),(self.widgets[name + "_rigSettings_leftLegThighTwist2Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegThighTwist2Field"], name + ":Rig_Settings.lThighTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 212),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.lThighTwist3Amount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.lThighTwist3Amount")
            self.widgets[name + "_rigSettings_leftLegThighTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegThighTwist3Field"], "top", 230),(self.widgets[name + "_rigSettings_leftLegThighTwist3Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegThighTwist3Field"], name + ":Rig_Settings.lThighTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 232),(text, "left", 10)])


        #lower leg
        if cmds.objExists(name + ":Rig_Settings.lCalfTwistAmount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.lCalfTwistAmount")
            self.widgets[name + "_rigSettings_leftLegCalfTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100 )
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegCalfTwistField"], "top", 190),(self.widgets[name + "_rigSettings_leftLegCalfTwistField"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegCalfTwistField"], name + ":Rig_Settings.lCalfTwistAmount")

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 192),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.lCalfTwist2Amount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.lCalfTwist2Amount")
            self.widgets[name + "_rigSettings_leftLegCalfTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegCalfTwist2Field"], "top", 210),(self.widgets[name + "_rigSettings_leftLegCalfTwist2Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegCalfTwist2Field"], name + ":Rig_Settings.lCalfTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 212),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.lCalfTwist3Amount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.lCalfTwist3Amount")
            self.widgets[name + "_rigSettings_leftLegCalfTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_leftLegCalfTwist3Field"], "top", 230),(self.widgets[name + "_rigSettings_leftLegCalfTwist3Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_leftLegCalfTwist3Field"], name + ":Rig_Settings.lCalfTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_leftLegForm"], edit = True, af = [(text, "top", 232),(text, "left", 200)])

        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_leftLegForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Left Leg Rig Settings", c = partial(self.selectRigSettings_Specific, "leftLeg"))




        #RIGHT LEG
        self.widgets[name + "_rigSettings_rightLegFrame"] = cmds.frameLayout(label = "Right Leg", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"], collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "RightLegSettings"), ec = partial(self.expandCommand, name, "RightLegSettings"))
        self.widgets[name + "_rigSettings_rightLegForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_rightLegFrame"])

        #rig mode
        text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_rightLegMode_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_rightLegFkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchLegMode, name, "FK", "r"))
        self.widgets[name + "_rigSettings_rightLegIkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchLegMode, name, "IK", "r"))

        mode = cmds.getAttr(name + ":Rig_Settings.rLegMode")

        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegFkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_rightLegFkModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegIkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_rightLegIkModeButton"], "right", 20)])

        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightLegFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightLegIkModeButton"], edit = True, select = True)


        #ik stretch settings
        text3 = cmds.text(label = "IK Leg Stretch Settings", font = "boldLabelFont")
        text4 = cmds.text(label = "Stretch")
        text5 = cmds.text(label = "Squash")
        stetchVal = cmds.getAttr(name + ":ik_foot_anim_r.stretch")

        try:
            squashVal = cmds.getAttr(name + ":ik_foot_anim_r.squash")
        except:
            pass

        try:
            self.widgets[name + "_rigSettings_rightLegIkStretchField"] = cmds.floatField( minValue=0, maxValue=1, value=stetchVal, w = 100)
        except:
            pass

        try:
            self.widgets[name + "_rigSettings_rightLegIkStretchBiasField"] = cmds.floatField( minValue=0, maxValue=1, value=squashVal, w = 100 )
        except:
            pass

        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text3, "top", 85),(text3, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text4, "top", 108),(text4, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text5, "top", 108),(text5, "left", 220)])

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegIkStretchField"], "top", 105),(self.widgets[name + "_rigSettings_rightLegIkStretchField"], "left", 55)])
        except:
            pass

        try:
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegIkStretchBiasField"], "top", 105),(self.widgets[name + "_rigSettings_rightLegIkStretchBiasField"], "left", 250)])
        except:
            pass

        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegIkStretchField"], name + ":ik_foot_anim_r.stretch" )
        except:
            pass
        try:
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegIkStretchBiasField"], name + ":ik_foot_anim_r.squash")
        except:
            pass

        #leg roll settings
        text6 = cmds.text(label = "Leg Roll Settings", font = "boldLabelFont")
        text7 = cmds.text(label = "Upper Leg Twist Amount:")
        text8 = cmds.text(label = "Lower Leg Twist Amount:")
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text6, "top", 145),(text6, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text7, "top", 173),(text7, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text8, "top", 173),(text8, "left", 220)])


        if cmds.objExists(name + ":Rig_Settings.rThighTwistAmount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.rThighTwistAmount")
            self.widgets[name + "_rigSettings_rightLegThighTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegThighTwistField"], "top", 190),(self.widgets[name + "_rigSettings_rightLegThighTwistField"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegThighTwistField"], name + ":Rig_Settings.rThighTwistAmount" )

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 192),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.rThighTwist2Amount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.rThighTwist2Amount")
            self.widgets[name + "_rigSettings_rightLegThighTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegThighTwist2Field"], "top", 210),(self.widgets[name + "_rigSettings_rightLegThighTwist2Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegThighTwist2Field"], name + ":Rig_Settings.rThighTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 212),(text, "left", 10)])

        if cmds.objExists(name + ":Rig_Settings.rThighTwist3Amount"):
            upLegVal = cmds.getAttr(name + ":Rig_Settings.rThighTwist3Amount")
            self.widgets[name + "_rigSettings_rightLegThighTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=upLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegThighTwist3Field"], "top", 230),(self.widgets[name + "_rigSettings_rightLegThighTwist3Field"], "left", 55)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegThighTwist3Field"], name + ":Rig_Settings.rThighTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 232),(text, "left", 10)])


        #lower leg
        if cmds.objExists(name + ":Rig_Settings.rCalfTwistAmount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.rCalfTwistAmount")
            self.widgets[name + "_rigSettings_rightLegCalfTwistField"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100 )
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegCalfTwistField"], "top", 190),(self.widgets[name + "_rigSettings_rightLegCalfTwistField"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegCalfTwistField"], name + ":Rig_Settings.rCalfTwistAmount")

            text = cmds.text(label = "Twist 1:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 192),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.rCalfTwist2Amount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.rCalfTwist2Amount")
            self.widgets[name + "_rigSettings_rightLegCalfTwist2Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegCalfTwist2Field"], "top", 210),(self.widgets[name + "_rigSettings_rightLegCalfTwist2Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegCalfTwist2Field"], name + ":Rig_Settings.rCalfTwist2Amount" )

            text = cmds.text(label = "Twist 2:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 212),(text, "left", 200)])

        if cmds.objExists(name + ":Rig_Settings.rCalfTwist3Amount"):
            lowLegVal = cmds.getAttr(name + ":Rig_Settings.rCalfTwist3Amount")
            self.widgets[name + "_rigSettings_rightLegCalfTwist3Field"] = cmds.floatField( minValue=-1, maxValue=2, value=lowLegVal, w = 100)
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(self.widgets[name + "_rigSettings_rightLegCalfTwist3Field"], "top", 230),(self.widgets[name + "_rigSettings_rightLegCalfTwist3Field"], "left", 250)])
            cmds.connectControl( self.widgets[name + "_rigSettings_rightLegCalfTwist3Field"], name + ":Rig_Settings.rCalfTwist3Amount" )

            text = cmds.text(label = "Twist 3:")
            cmds.formLayout(self.widgets[name + "_rigSettings_rightLegForm"], edit = True, af = [(text, "top", 232),(text, "left", 200)])


        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_rightLegForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Right Leg Rig Settings", c = partial(self.selectRigSettings_Specific, "rightLeg"))



        #SPINE SETTINGS
        if cmds.objExists(name + ":chest_ik_anim"):
            self.widgets[name + "_rigSettings_spineFrame"] = cmds.frameLayout(label = "Spine", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"], collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "SpineSettings"), ec = partial(self.expandCommand, name, "SpineSettings"))
            self.widgets[name + "_rigSettings_spineForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_spineFrame"])


            #rig mode
            text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
            self.widgets[name + "rigSettings_spine_Collection"] = cmds.iconTextRadioCollection()
            self.widgets[name + "_rigSettings_spineFkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchSpineMode, name, "FK"))
            self.widgets[name + "_rigSettings_spineIkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchSpineMode, name, "IK"))

            mode1 = cmds.getAttr(name + ":Rig_Settings.spine_fk")
            mode2 = cmds.getAttr(name + ":Rig_Settings.spine_ik")

            cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
            cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(self.widgets[name + "_rigSettings_spineFkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_spineFkModeButton"], "left", 10)])
            cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(self.widgets[name + "_rigSettings_spineIkModeButton"], "top", 25),(self.widgets[name + "_rigSettings_spineIkModeButton"], "right", 20)])

            if mode1 > mode2:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_spineFkModeButton"], edit = True, select = True)
            else:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_spineIkModeButton"], edit = True, select = True)


            if cmds.objExists(name + ":chest_ik_anim"):

                #ik stretch settings
                text3 = cmds.text(label = "IK Spine Settings", font = "boldLabelFont")
                text4 = cmds.text(label = "Stretch")
                text5 = cmds.text(label = "Twist Amount")
                stetchVal = cmds.getAttr(name + ":chest_ik_anim.stretch")
                twistVal = cmds.getAttr(name + ":chest_ik_anim.twist_amount")
                self.widgets[name + "_rigSettings_chestIkStretchField"] = cmds.floatField( minValue=-1, maxValue=2, value=stetchVal, w = 100)
                self.widgets[name + "_rigSettings_chestIkTwistField"] = cmds.floatField( minValue=-1, maxValue=5, value=twistVal, w = 100)

                cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(text3, "top", 85),(text3, "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(text4, "top", 108),(text4, "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(text5, "top", 108),(text5, "left", 210)])
                cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(self.widgets[name + "_rigSettings_chestIkStretchField"], "top", 105),(self.widgets[name + "_rigSettings_chestIkStretchField"], "left", 55)])
                cmds.formLayout(self.widgets[name + "_rigSettings_spineForm"], edit = True, af = [(self.widgets[name + "_rigSettings_chestIkTwistField"], "top", 105),(self.widgets[name + "_rigSettings_chestIkTwistField"], "left", 280)])


                cmds.connectControl( self.widgets[name + "_rigSettings_chestIkStretchField"], name + ":chest_ik_anim.stretch" )
                cmds.connectControl( self.widgets[name + "_rigSettings_chestIkTwistField"], name + ":chest_ik_anim.twist_amount" )

            #create the right click menu for selecting the settings for the left arm
            popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_spineForm"])
            menu = cmds.menuItem(parent = popupMenu, label = "Select Spine Settings", c = partial(self.selectRigSettings_Specific, "spine"))




        #HEAD SETTINGS
        self.widgets[name + "_rigSettings_headFrame"] = cmds.frameLayout(label = "Head", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "HeadSettings"), ec = partial(self.expandCommand, name, "HeadSettings"))
        self.widgets[name + "_rigSettings_headForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_headFrame"])


        #fk orientation space
        text1 = cmds.text(label = "Head Orientation Space:", font = "boldLabelFont")
        self.widgets[name + "rigSettings_headOrient_Collection"] = cmds.iconTextRadioCollection()
        self.widgets[name + "_rigSettings_headFkOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsNeck_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsNeck_on.bmp",  w = 70, h = 30, onc = partial(self.switchHeadOrientMode, name, 0))
        self.widgets[name + "_rigSettings_headShoulderOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsChest_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsChest_on.bmp",w = 70, h = 30, onc = partial(self.switchHeadOrientMode, name, 1))
        self.widgets[name + "_rigSettings_headBodyOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsBody_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsBody_on.bmp",w = 70, h = 30, onc = partial(self.switchHeadOrientMode, name, 2))
        self.widgets[name + "_rigSettings_headWorldOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsWorld_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsWorld_on.bmp",w = 70, h = 30, onc = partial(self.switchHeadOrientMode, name, 3))


        mode = cmds.getAttr(name + ":head_fk_anim.fkOrientation")

        cmds.formLayout(self.widgets[name + "_rigSettings_headForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_headForm"], edit = True, af = [(self.widgets[name + "_rigSettings_headFkOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_headFkOrientModeButton"], "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_headForm"], edit = True, af = [(self.widgets[name + "_rigSettings_headShoulderOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_headShoulderOrientModeButton"], "left", 95)])
        cmds.formLayout(self.widgets[name + "_rigSettings_headForm"], edit = True, af = [(self.widgets[name + "_rigSettings_headBodyOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_headBodyOrientModeButton"], "left", 180)])
        cmds.formLayout(self.widgets[name + "_rigSettings_headForm"], edit = True, af = [(self.widgets[name + "_rigSettings_headWorldOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_headWorldOrientModeButton"], "left", 265)])


        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headFkOrientModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headShoulderOrientModeButton"], edit = True, select = True)
        if mode == 2:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headBodyOrientModeButton"], edit = True, select = True)
        if mode == 3:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headWorldOrientModeButton"], edit = True, select = True)



        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_headForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Head Settings", c = partial(self.selectRigSettings_Specific, "head"))



        try:
            #NECK SETTINGS
            self.widgets[name + "_rigSettings_neckFrame"] = cmds.frameLayout(label = "Neck", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "NeckSettings"), ec = partial(self.expandCommand, name, "NeckSettings"))
            self.widgets[name + "_rigSettings_neckForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_neckFrame"])


            #fk orientation space
            text1 = cmds.text(label = "Neck Orientation Space:", font = "boldLabelFont")
            self.widgets[name + "rigSettings_neckOrient_Collection"] = cmds.iconTextRadioCollection()
            self.widgets[name + "_rigSettings_neckShoulderOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsChest_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsChest_on.bmp",w = 70, h = 30, onc = partial(self.switchNeckOrientMode, name, 0))
            self.widgets[name + "_rigSettings_neckBodyOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsBody_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsBody_on.bmp",w = 70, h = 30, onc = partial(self.switchNeckOrientMode, name, 1))
            self.widgets[name + "_rigSettings_neckWorldOrientModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/headSettingsWorld_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/headSettingsWorld_on.bmp",w = 70, h = 30, onc = partial(self.switchNeckOrientMode, name, 2))


            mode = cmds.getAttr(name + ":neck_01_fk_anim.fkOrientation")

            cmds.formLayout(self.widgets[name + "_rigSettings_neckForm"], edit = True, af = [(text1, "top", 5),(text1, "left", 10)])
            cmds.formLayout(self.widgets[name + "_rigSettings_neckForm"], edit = True, af = [(self.widgets[name + "_rigSettings_neckShoulderOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_neckShoulderOrientModeButton"], "left", 95)])
            cmds.formLayout(self.widgets[name + "_rigSettings_neckForm"], edit = True, af = [(self.widgets[name + "_rigSettings_neckBodyOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_neckBodyOrientModeButton"], "left", 180)])
            cmds.formLayout(self.widgets[name + "_rigSettings_neckForm"], edit = True, af = [(self.widgets[name + "_rigSettings_neckWorldOrientModeButton"], "top", 25),(self.widgets[name + "_rigSettings_neckWorldOrientModeButton"], "left", 265)])


            if mode == 0:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckShoulderOrientModeButton"], edit = True, select = True)
            if mode == 1:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckBodyOrientModeButton"], edit = True, select = True)
            if mode == 2:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckWorldOrientModeButton"], edit = True, select = True)



            #create the right click menu for selecting the settings for the left arm
            popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_neckForm"])
            menu = cmds.menuItem(parent = popupMenu, label = "Select Neck Settings", c = partial(self.selectRigSettings_Specific, "neck"))

        except:
            pass





        #AUTO CONTROL SETTINGS
        self.widgets[name + "_rigSettings_autoControlsFrame"] = cmds.frameLayout(label = "Auto Controls", w = 400, h = 30, parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in", cc = partial(self.collapseCommand, name, "AutoControlsSettings"), ec = partial(self.expandCommand, name, "AutoControlsSettings"))
        self.widgets[name + "_rigSettings_autoControlsForm"] = cmds.formLayout(w = 400, h = 300, parent = self.widgets[name + "_rigSettings_autoControlsFrame"])


        text1 = cmds.text(label ="Auto Hips:", font = "boldLabelFont")
        text2 = cmds.text(label ="Auto Spine:", font = "boldLabelFont")
        text3 = cmds.text(label ="Auto Left Clavicle:", font = "boldLabelFont")
        text4 = cmds.text(label ="Auto Right Clavicle:", font = "boldLabelFont")

        self.widgets[name + "_rigSettings_autoHipsField"] = cmds.floatField( minValue=0, maxValue=1, w = 100)
        self.widgets[name + "_rigSettings_autoSpineField"] = cmds.floatField( minValue=0, maxValue=1, w = 100)
        self.widgets[name + "_rigSettings_autoClavLeftField"] = cmds.floatField( minValue=0, maxValue=1, w = 100)
        self.widgets[name + "_rigSettings_autoClavRightField"] = cmds.floatField( minValue=0, maxValue=1, w = 100)

        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(text1, "top", 8),(text1, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(text2, "top", 38),(text2, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(text3, "top", 68),(text3, "left", 10)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(text4, "top", 98),(text4, "left", 10)])

        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(self.widgets[name + "_rigSettings_autoHipsField"], "top", 5),(self.widgets[name + "_rigSettings_autoHipsField"], "left", 150)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(self.widgets[name + "_rigSettings_autoSpineField"], "top", 35),(self.widgets[name + "_rigSettings_autoSpineField"], "left", 150)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(self.widgets[name + "_rigSettings_autoClavLeftField"], "top", 65),(self.widgets[name + "_rigSettings_autoClavLeftField"], "left", 150)])
        cmds.formLayout(self.widgets[name + "_rigSettings_autoControlsForm"], edit = True, af = [(self.widgets[name + "_rigSettings_autoClavRightField"], "top", 95),(self.widgets[name + "_rigSettings_autoClavRightField"], "left", 150)])


        cmds.connectControl( self.widgets[name + "_rigSettings_autoHipsField"], name + ":hip_anim.autoHips" )
        cmds.connectControl( self.widgets[name + "_rigSettings_autoSpineField"], name + ":chest_ik_anim.autoSpine" )
        cmds.connectControl( self.widgets[name + "_rigSettings_autoClavLeftField"], name + ":clavicle_l_anim.autoShoulders" )
        cmds.connectControl( self.widgets[name + "_rigSettings_autoClavRightField"], name + ":clavicle_r_anim.autoShoulders" )


        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_autoControlsForm"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Auto Control Settings", c = partial(self.selectRigSettings_Specific, "auto"))




        #LEFT FINGER SETTINGS
        self.widgets[name + "_rigSettings_leftFingersFrame"] = cmds.frameLayout(label = "Left Fingers", w = 400,  parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in")
        self.widgets[name + "_rigSettings_leftFingersColumn"] = cmds.columnLayout(parent = self.widgets[name + "_rigSettings_leftFingersFrame"])

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            self.widgets[name + "_rigSettings_leftFingersFrame_" + finger] = cmds.frameLayout(label = "        " + finger, w = 400, h = 60, parent = self.widgets[name + "_rigSettings_leftFingersColumn"],  collapse = False, collapsable = False, borderStyle = "in")
            self.widgets[name + "_rigSettings_leftFingersForm_" + finger] = cmds.formLayout(parent = self.widgets[name + "_rigSettings_leftFingersFrame_" + finger])

            text = cmds.text(label = "FK Sticky:", parent = self.widgets[name + "_rigSettings_leftFingersForm_" + finger])
            self.widgets[name + "_" + finger + "_fkStickyFloatFieldL"] = cmds.floatField( minValue=0, maxValue=1, w = 100)

            cmds.formLayout(self.widgets[name + "_rigSettings_leftFingersForm_" + finger], edit = True, af = [(text, "top", 8),(text, "left", 10)])
            cmds.formLayout(self.widgets[name + "_rigSettings_leftFingersForm_" + finger], edit = True, af = [(self.widgets[name + "_" + finger + "_fkStickyFloatFieldL"], "top", 5),(self.widgets[name + "_" + finger + "_fkStickyFloatFieldL"], "left", 100)])

            cmds.connectControl(self.widgets[name + "_" + finger + "_fkStickyFloatFieldL"], name + ":" + finger + "_finger_fk_ctrl_1_l.sticky" )


            if cmds.objExists(name + ":" + finger + "_l_ik_anim"):
                #rig mode
                cmds.frameLayout(self.widgets[name + "_rigSettings_leftFingersFrame_" + finger], edit = True, h = 120)
                text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
                self.widgets[name + "rigSettings_LeftFinger_" + finger + "_Mode_Collection"] = cmds.iconTextRadioCollection()
                self.widgets[name + "rigSettings_LeftFinger_" + finger + "_FkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchFingerMode, name, finger, 0, "l"))
                self.widgets[name + "rigSettings_LeftFinger_" + finger + "_IkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchFingerMode, name, finger, 1, "l"))

                mode = cmds.getAttr(name + ":" + finger + "_finger_l_mode_anim.FK_IK")

                cmds.formLayout(self.widgets[name + "_rigSettings_leftFingersForm_" + finger], edit = True, af = [(text1, "top", 30),(text1, "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_leftFingersForm_" + finger], edit = True, af = [(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_FkModeButton"], "top", 45),(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_FkModeButton"], "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_leftFingersForm_" + finger], edit = True, af = [(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_IkModeButton"], "top", 45),(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_IkModeButton"], "right", 20)])

                if mode == 0:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_FkModeButton"], edit = True, select = True)
                if mode == 1:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_IkModeButton"], edit = True, select = True)



        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_leftFingersFrame"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Left Finger Settings", c = partial(self.selectRigSettings_Specific, "leftFingers"))


        #RIGHT FINGER SETTINGS
        spacer = cmds.text(label = "", parent = self.widgets[name + "_rigSettingsMainColumn"], h = 10)
        self.widgets[name + "_rigSettings_rightFingersFrame"] = cmds.frameLayout(label = "Right Fingers", w = 400,  parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in")
        self.widgets[name + "_rigSettings_rightFingersColumn"] = cmds.columnLayout(parent = self.widgets[name + "_rigSettings_rightFingersFrame"])

        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            self.widgets[name + "_rigSettings_rightFingersFrame_" + finger] = cmds.frameLayout(label = "        " + finger, w = 400, h = 60, parent = self.widgets[name + "_rigSettings_rightFingersColumn"],  collapse = False, collapsable = False, borderStyle = "in")
            self.widgets[name + "_rigSettings_rightFingersForm_" + finger] = cmds.formLayout(parent = self.widgets[name + "_rigSettings_rightFingersFrame_" + finger])

            text = cmds.text(label = "FK Sticky:", parent = self.widgets[name + "_rigSettings_rightFingersForm_" + finger])
            self.widgets[name + "_" + finger + "_fkStickyFloatFieldR"] = cmds.floatField( minValue=0, maxValue=1, w = 100)

            cmds.formLayout(self.widgets[name + "_rigSettings_rightFingersForm_" + finger], edit = True, af = [(text, "top", 8),(text, "left", 10)])
            cmds.formLayout(self.widgets[name + "_rigSettings_rightFingersForm_" + finger], edit = True, af = [(self.widgets[name + "_" + finger + "_fkStickyFloatFieldR"], "top", 5),(self.widgets[name + "_" + finger + "_fkStickyFloatFieldR"], "left", 100)])

            cmds.connectControl(self.widgets[name + "_" + finger + "_fkStickyFloatFieldR"], name + ":" + finger + "_finger_fk_ctrl_1_r.sticky" )


            if cmds.objExists(name + ":" + finger + "_r_ik_anim"):
                #rig mode
                cmds.frameLayout(self.widgets[name + "_rigSettings_rightFingersFrame_" + finger], edit = True, h = 120)
                text1 = cmds.text(label = "Rig Mode:", font = "boldLabelFont")
                self.widgets[name + "rigSettings_RightFinger_" + finger + "_Mode_Collection"] = cmds.iconTextRadioCollection()
                self.widgets[name + "rigSettings_RightFinger_" + finger + "_FkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchFingerMode, name, finger, 0, "r"))
                self.widgets[name + "rigSettings_RightFinger_" + finger + "_IkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",w = 180, h = 50, onc = partial(self.switchFingerMode, name, finger, 1, "r"))

                mode = cmds.getAttr(name + ":" + finger + "_finger_r_mode_anim.FK_IK")

                cmds.formLayout(self.widgets[name + "_rigSettings_rightFingersForm_" + finger], edit = True, af = [(text1, "top", 30),(text1, "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_rightFingersForm_" + finger], edit = True, af = [(self.widgets[name + "rigSettings_RightFinger_" + finger + "_FkModeButton"], "top", 45),(self.widgets[name + "rigSettings_RightFinger_" + finger + "_FkModeButton"], "left", 10)])
                cmds.formLayout(self.widgets[name + "_rigSettings_rightFingersForm_" + finger], edit = True, af = [(self.widgets[name + "rigSettings_RightFinger_" + finger + "_IkModeButton"], "top", 45),(self.widgets[name + "rigSettings_RightFinger_" + finger + "_IkModeButton"], "right", 20)])

                if mode == 0:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_RightFinger_" + finger + "_FkModeButton"], edit = True, select = True)
                if mode == 1:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_RightFinger_" + finger + "_IkModeButton"], edit = True, select = True)


        #create the right click menu for selecting the settings for the left arm
        popupMenu = cmds.popupMenu(b = 3, parent = self.widgets[name + "_rigSettings_rightFingersFrame"])
        menu = cmds.menuItem(parent = popupMenu, label = "Select Right Finger Settings", c = partial(self.selectRigSettings_Specific, "rightFingers"))

        #CUSTOM JOINT CHAINS
        spacer = cmds.text(label = "", parent = self.widgets[name + "_rigSettingsMainColumn"], h = 10)
        self.widgets[name + "_rigSettings_customJointChainsFrame"] = cmds.frameLayout(label = "Custom Joint Chains", w = 400,  parent = self.widgets[name + "_rigSettingsMainColumn"],  collapse = True, collapsable = True, borderStyle = "in")
        self.widgets[name + "_rigSettings_customJointChainsColumn"] = cmds.rowColumnLayout(nc = 2, cat = [(1, "both", 5), (2, "both", 5)],parent = self.widgets[name + "_rigSettings_customJointChainsFrame"])


        customJoints = []
        attrs = cmds.listAttr(name + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        for joint in customJoints:
            attribute = cmds.getAttr(name + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "chain" or jointType == "dynamic":
                numJointsInChain = label.partition("(")[2].partition(")")[0]
                label = label.partition(" (")[0]


                #rig mode
                cmds.text(label = "")
                cmds.text(label = "")
                text = cmds.text(label = label + " rig settings:", font = "boldLabelFont")
                cmds.text(label = "")
                cmds.text(label = "")
                cmds.text(label = "")

                self.widgets[name + "rigSettings_customJoints_" + label + "_Collection"] = cmds.iconTextRadioCollection()
                self.widgets[name + "_rigSettings_customJoints_" + label + "fkModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsFkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchCustomChainMode, name, label, "FK"))
                self.widgets[name + "_rigSettings_customJoints_" + label + "ikModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsIkMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchCustomChainMode, name, label, "IK"))
                self.widgets[name + "_rigSettings_customJoints_" + label + "dynModeButton"] = cmds.iconTextRadioButton(image = self.mayaToolsDir + "/General/Icons/ART/rigSettingsDynMode_off.bmp",selectionImage = self.mayaToolsDir + "/General/Icons/ART/rigSettingsDynMode_on.bmp",  w = 180, h = 50, onc = partial(self.switchCustomChainMode, name, label, "DYNAMIC"))

                mode1 = cmds.getAttr(name + ":Rig_Settings." + label + "_fk")
                mode2 = cmds.getAttr(name + ":Rig_Settings." + label + "_ik")
                mode3 = cmds.getAttr(name + ":Rig_Settings." + label + "_dynamic")

                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "fkModeButton"], edit = True, select = mode1)
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "ikModeButton"], edit = True, select = mode2)
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "dynModeButton"], edit = True, select = mode3)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectionScriptJob(self):
        #reset all button colors
        self.resetButtonColors()


        #look at selection and get the buttonName attr value
        selection = cmds.ls(sl = True)
        for each in selection:
            if "." not in each:
                if cmds.objExists(each + ".buttonName"):
                    button = cmds.getAttr(each + ".buttonName")

                    #color those buttons white
                    cmds.button(button, edit = True, bgc = self.white)


        self.listView_ScriptJob()





    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def resetButtonColors(self):
        characters = self.getCharacters()

        #reset all button colors
        for character in characters:
            cmds.button(self.widgets[character + "_headPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":neck_01_fk_anim"):
                cmds.button(self.widgets[character + "_neck1_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":neck_02_fk_anim"):
                cmds.button(self.widgets[character + "_neck2_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":neck_03_fk_anim"):
                cmds.button(self.widgets[character + "_neck3_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":spine_01_anim"):
                cmds.button(self.widgets[character + "_spine1_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":spine_02_anim"):
                cmds.button(self.widgets[character + "_spine2_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":spine_03_anim"):
                cmds.button(self.widgets[character + "_spine3_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":spine_04_anim"):
                cmds.button(self.widgets[character + "_spine4_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":spine_05_anim"):
                cmds.button(self.widgets[character + "_spine5_PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":mid_ik_anim"):
                cmds.button(self.widgets[character + "_ikSpineMidPickerButton"], edit = True, bgc = self.orange)

            if cmds.objExists(character + ":chest_ik_anim"):
                cmds.button(self.widgets[character + "_ikSpineTopPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_bodyPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_pelvisPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftClavPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightClavPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftShoulderPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightShoulderPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftElbowPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightElbowPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftHandPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightHandPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftIkElbowPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkElbowPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_leftIkHandPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkHandPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_leftThighPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightThighPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftFkKneePickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightFkKneePickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftFkAnklePickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rightFkAnklePickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":fk_ball_l_anim"):
                cmds.button(self.widgets[character + "_leftFkBallPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":fk_ball_r_anim"):
                cmds.button(self.widgets[character + "_rightFkBallPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_leftIkFootPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkFootPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_leftIkHeelPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkHeelPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_leftIkToeWigglePickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkToeWigglePickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_leftIkToePickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_rightIkToePickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_masterPickerButton"], edit = True, bgc = self.orange)

            cmds.button(self.widgets[character + "_offsetPickerButton"], edit = True, bgc = self.blue)

            cmds.button(self.widgets[character + "_rootPickerButton"], edit = True, bgc = self.purple)

            cmds.button(self.widgets[character + "_headGroupPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_leftArmGroupPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_rightArmGroupPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_spineGroupPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_leftLegGroupPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_rightLegGroupPickerButton"], edit = True, bgc = self.green)

            if cmds.objExists(character + ":upperarm_l_twist_anim"):
                cmds.button(self.widgets[character + "_leftArmRollPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":upperarm_l_twist_2_anim"):
                cmds.button(self.widgets[character + "_leftArmRoll2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":upperarm_l_twist_3_anim"):
                cmds.button(self.widgets[character + "_leftArmRoll3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":upperarm_r_twist_anim"):
                cmds.button(self.widgets[character + "_rightArmRollPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":upperarm_r_twist_2_anim"):
                cmds.button(self.widgets[character + "_rightArmRoll2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":upperarm_r_twist_3_anim"):
                cmds.button(self.widgets[character + "_rightArmRoll3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_l_twist_anim"):
                cmds.button(self.widgets[character + "_leftForeTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_l_twist2_anim"):
                cmds.button(self.widgets[character + "_leftForeTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_l_twist3_anim"):
                cmds.button(self.widgets[character + "_leftForeTwist3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_r_twist_anim"):
                cmds.button(self.widgets[character + "_rightForeTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_r_twist2_anim"):
                cmds.button(self.widgets[character + "_rightForeTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":lowerarm_r_twist3_anim"):
                cmds.button(self.widgets[character + "_rightForeTwist3PickerButton"], edit = True, bgc = self.purple)


            if cmds.objExists(character + ":l_thigh_twist_01_anim"):
                cmds.button(self.widgets[character + "_leftThighTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":l_thigh_twist_02_anim"):
                cmds.button(self.widgets[character + "_leftThighTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":l_thigh_twist_03_anim"):
                cmds.button(self.widgets[character + "_leftThighTwist3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":r_thigh_twist_01_anim"):
                cmds.button(self.widgets[character + "_rightThighTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":r_thigh_twist_02_anim"):
                cmds.button(self.widgets[character + "_rightThighTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":r_thigh_twist_03_anim"):
                cmds.button(self.widgets[character + "_rightThighTwist3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_l_twist_anim"):
                cmds.button(self.widgets[character + "_leftCalfTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_l_twist2_anim"):
                cmds.button(self.widgets[character + "_leftCalfTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_l_twist3_anim"):
                cmds.button(self.widgets[character + "_leftCalfTwist3PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_r_twist_anim"):
                cmds.button(self.widgets[character + "_rightCalfTwistPickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_r_twist2_anim"):
                cmds.button(self.widgets[character + "_rightCalfTwist2PickerButton"], edit = True, bgc = self.purple)

            if cmds.objExists(character + ":calf_r_twist3_anim"):
                cmds.button(self.widgets[character + "_rightCalfTwist3PickerButton"], edit = True, bgc = self.purple)





            if cmds.objExists(character + ":pinky_metacarpal_ctrl_l"):
                cmds.button(self.widgets[character + "_leftPinkyMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_1_l"):
                cmds.button(self.widgets[character + "_leftPinky1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_2_l"):
                cmds.button(self.widgets[character + "_leftPinky2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_3_l"):
                cmds.button(self.widgets[character + "_leftPinky3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":ring_metacarpal_ctrl_l"):
                cmds.button(self.widgets[character + "_leftRingMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_1_l"):
                cmds.button(self.widgets[character + "_leftRing1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_2_l"):
                cmds.button(self.widgets[character + "_leftRing2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_3_l"):
                cmds.button(self.widgets[character + "_leftRing3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":middle_metacarpal_ctrl_l"):
                cmds.button(self.widgets[character + "_leftMiddleMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_1_l"):
                cmds.button(self.widgets[character + "_leftMiddle1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_2_l"):
                cmds.button(self.widgets[character + "_leftMiddle2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_3_l"):
                cmds.button(self.widgets[character + "_leftMiddle3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":index_metacarpal_ctrl_l"):
                cmds.button(self.widgets[character + "_leftIndexMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_1_l"):
                cmds.button(self.widgets[character + "_leftIndex1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_2_l"):
                cmds.button(self.widgets[character + "_leftIndex2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_3_l"):
                cmds.button(self.widgets[character + "_leftIndex3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":thumb_finger_fk_ctrl_1_l"):
                cmds.button(self.widgets[character + "_leftThumb1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":thumb_finger_fk_ctrl_2_l"):
                cmds.button(self.widgets[character + "_leftThumb2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":thumb_finger_fk_ctrl_3_l"):
                cmds.button(self.widgets[character + "_leftThumb3PickerButton"], edit = True, bgc = self.blue)


            cmds.button(self.widgets[character + "_leftMetaRowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftKnuckle1RowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftKnuckle2RowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftKnuckle3RowPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_leftIndexColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftMiddleColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftRingColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_leftPinkyColumnPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_leftThumbColumnPickerButton"], edit = True, bgc = self.green)



            createLeftIKRow = False
            if cmds.objExists(character + ":index_l_ik_anim"):
                cmds.button(self.widgets[character + "_leftIndexFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_leftIndexIkPvPickerButton"], edit = True, bgc = self.orange)
                createLeftIKRow = True

            if cmds.objExists(character + ":middle_l_ik_anim"):
                cmds.button(self.widgets[character + "_leftMiddleFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_leftMiddleIkPvPickerButton"], edit = True, bgc = self.orange)
                createLeftIKRow = True

            if cmds.objExists(character + ":ring_l_ik_anim"):
                cmds.button(self.widgets[character + "_leftRingFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_leftRingIkPvPickerButton"], edit = True, bgc = self.orange)
                createLeftIKRow = True

            if cmds.objExists(character + ":pinky_l_ik_anim"):
                cmds.button(self.widgets[character + "_leftPinkyFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_leftPinkyIkPvPickerButton"], edit = True, bgc = self.orange)
                createLeftIKRow = True

            if cmds.objExists(character + ":thumb_l_ik_anim"):
                cmds.button(self.widgets[character + "_leftThumbFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_leftThumbIkPvPickerButton"], edit = True, bgc = self.orange)
                createLeftIKRow = True

            if createLeftIKRow:
                cmds.button(self.widgets[character + "_leftIkFingersRowPickerButton"], edit = True, bgc = self.green)
                cmds.button(self.widgets[character + "_leftIkFingersPvsPickerButton"], edit = True, bgc = self.green)


            if cmds.objExists(character + ":l_global_ik_anim"):
                cmds.button(self.widgets[character + "_leftIkGlobalCtrlPickerButton"], edit = True, bgc = self.orange)



            if cmds.objExists(character + ":pinky_metacarpal_ctrl_r"):
                cmds.button(self.widgets[character + "_rightPinkyMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_1_r"):
                cmds.button(self.widgets[character + "_rightPinky1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_2_r"):
                cmds.button(self.widgets[character + "_rightPinky2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":pinky_finger_fk_ctrl_3_r"):
                cmds.button(self.widgets[character + "_rightPinky3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":ring_metacarpal_ctrl_r"):
                cmds.button(self.widgets[character + "_rightRingMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_1_r"):
                cmds.button(self.widgets[character + "_rightRing1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_2_r"):
                cmds.button(self.widgets[character + "_rightRing2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":ring_finger_fk_ctrl_3_r"):
                cmds.button(self.widgets[character + "_rightRing3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":middle_metacarpal_ctrl_r"):
                cmds.button(self.widgets[character + "_rightMiddleMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_1_r"):
                cmds.button(self.widgets[character + "_rightMiddle1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_2_r"):
                cmds.button(self.widgets[character + "_rightMiddle2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":middle_finger_fk_ctrl_3_r"):
                cmds.button(self.widgets[character + "_rightMiddle3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":index_metacarpal_ctrl_r"):
                cmds.button(self.widgets[character + "_rightIndexMetacarpalPickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_1_r"):
                cmds.button(self.widgets[character + "_rightIndex1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_2_r"):
                cmds.button(self.widgets[character + "_rightIndex2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":index_finger_fk_ctrl_3_r"):
                cmds.button(self.widgets[character + "_rightIndex3PickerButton"], edit = True, bgc = self.blue)


            if cmds.objExists(character + ":thumb_finger_fk_ctrl_1_r"):
                cmds.button(self.widgets[character + "_rightThumb1PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":thumb_finger_fk_ctrl_2_r"):
                cmds.button(self.widgets[character + "_rightThumb2PickerButton"], edit = True, bgc = self.blue)

            if cmds.objExists(character + ":thumb_finger_fk_ctrl_3_r"):
                cmds.button(self.widgets[character + "_rightThumb3PickerButton"], edit = True, bgc = self.blue)



            cmds.button(self.widgets[character + "_rightMetaRowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightKnuckle1RowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightKnuckle2RowPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightKnuckle3RowPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_rightIndexColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightMiddleColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightRingColumnPickerButton"], edit = True, bgc = self.green)
            cmds.button(self.widgets[character + "_rightPinkyColumnPickerButton"], edit = True, bgc = self.green)

            cmds.button(self.widgets[character + "_rightThumbColumnPickerButton"], edit = True, bgc = self.green)


            createRightIKRow = False
            if cmds.objExists(character + ":index_r_ik_anim"):
                cmds.button(self.widgets[character + "_rightIndexFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_rightIndexIkPvPickerButton"], edit = True, bgc = self.orange)
                createRightIKRow = True

            if cmds.objExists(character + ":middle_r_ik_anim"):
                cmds.button(self.widgets[character + "_rightMiddleFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_rightMiddleIkPvPickerButton"], edit = True, bgc = self.orange)
                createRightIKRow = True

            if cmds.objExists(character + ":ring_r_ik_anim"):
                cmds.button(self.widgets[character + "_rightRingFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_rightRingIkPvPickerButton"], edit = True, bgc = self.orange)
                createRightIKRow = True

            if cmds.objExists(character + ":pinky_r_ik_anim"):
                cmds.button(self.widgets[character + "_rightPinkyFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_rightPinkyIkPvPickerButton"], edit = True, bgc = self.orange)
                createRightIKRow = True

            if cmds.objExists(character + ":thumb_r_ik_anim"):
                cmds.button(self.widgets[character + "_rightThumbFingerIKPickerButton"], edit = True, bgc = self.orange)
                cmds.button(self.widgets[character + "_rightThumbIkPvPickerButton"], edit = True, bgc = self.orange)
                createRightIKRow = True

            if createRightIKRow:
                cmds.button(self.widgets[character + "_rightIkFingersRowPickerButton"], edit = True, bgc = self.green)
                cmds.button(self.widgets[character + "_rightIkFingersPvsPickerButton"], edit = True, bgc = self.green)


            if cmds.objExists(character + ":r_global_ik_anim"):
                cmds.button(self.widgets[character + "_rightIkGlobalCtrlPickerButton"], edit = True, bgc = self.orange)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def collapseCommand(self, name, layout, *args):

        if layout == "Body":
            cmds.frameLayout(self.widgets[name + "_bodyFrame"], edit = True, h = 30)


        if layout == "Fingers":
            cmds.frameLayout(self.widgets[name + "_fingersFrame"], edit = True, h = 30)


        if layout == "Toes":
            cmds.frameLayout(self.widgets[name + "_toesFrame"], edit = True, h = 30)


        if layout == "Layers":
            cmds.frameLayout(self.widgets[name + "_layersFrame"], edit = True, h = 30)

        if layout == "LeftArmSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_leftArmFrame"], edit = True, h = 30)

        if layout == "RightArmSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_rightArmFrame"], edit = True, h = 30)  

        if layout == "LeftLegSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_leftLegFrame"], edit = True, h = 30)

        if layout == "RightLegSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_rightLegFrame"], edit = True, h = 30) 

        if layout == "SpineSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_spineFrame"], edit = True, h = 30) 

        if layout == "HeadSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_headFrame"], edit = True, h = 30) 

        if layout == "NeckSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_neckFrame"], edit = True, h = 30) 

        if layout == "AutoControlsSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_autoControlsFrame"], edit = True, h = 30)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def expandCommand(self, name, layout, height = None,  *args):

        if layout == "Body":
            cmds.frameLayout(self.widgets[name + "_bodyFrame"], edit = True, h = 470)


        if layout == "Fingers":
            cmds.frameLayout(self.widgets[name + "_fingersFrame"], edit = True, h = 205)


        if layout == "Toes":
            cmds.frameLayout(self.widgets[name + "_toesFrame"], edit = True, h = 220)

        if layout == "Layers":
            cmds.frameLayout(self.widgets[name + "_layersFrame"], edit = True, h = 300)

        if layout == "LeftArmSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_leftArmFrame"], edit = True, h = 330)

        if layout == "RightArmSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_rightArmFrame"], edit = True, h = 330) 

        if layout == "LeftLegSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_leftLegFrame"], edit = True, h = 280)

        if layout == "RightLegSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_rightLegFrame"], edit = True, h = 280) 

        if layout == "SpineSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_spineFrame"], edit = True, h = 150) 

        if layout == "HeadSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_headFrame"], edit = True, h = 100) 

        if layout == "NeckSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_neckFrame"], edit = True, h = 100) 

        if layout == "AutoControlsSettings":
            cmds.frameLayout(self.widgets[name + "_rigSettings_autoControlsFrame"], edit = True, h = 150) 


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectRigSettings_Specific(self, limb, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)


        if limb == "leftArm":
            cmds.select(character + ":Rig_Settings")
            cmds.select(character + ":ik_wrist_l_anim", add = True)

        if limb == "rightArm":
            cmds.select(character + ":Rig_Settings")
            cmds.select(character + ":ik_wrist_r_anim", add = True)

        if limb == "leftLeg":
            cmds.select(character + ":Rig_Settings")
            cmds.select(character + ":ik_foot_anim_l", add = True)

        if limb == "rightLeg":
            cmds.select(character + ":Rig_Settings")
            cmds.select(character + ":ik_foot_anim_r", add = True)


        if limb == "spine":
            cmds.select(character + ":Rig_Settings")

            if cmds.objExists(character + ":chest_ik_anim"):
                cmds.select(character + ":chest_ik_anim", add = True)

        if limb == "head":
            cmds.select(character + ":head_fk_anim")

        if limb == "neck":
            cmds.select(character + ":neck_01_fk_anim")

        if limb == "auto":
            cmds.select(character + ":clavicle_l_anim")
            cmds.select(character + ":clavicle_r_anim", add = True)
            cmds.select(character + ":hip_anim", add = True)
            cmds.select(character + ":chest_ik_anim", add = True)

        if limb == "leftFingers":
            cmds.select(clear = True)
            for finger in ["index", "middle", "ring", "pinky", "thumb"]:
                if cmds.objExists(character + ":" + finger + "_finger_fk_ctrl_1_l"):
                    cmds.select(character + ":" + finger + "_finger_fk_ctrl_1_l", add = True)

                if cmds.objExists(character + ":" + finger + "_finger_l_mode_anim"):
                    cmds.select(character + ":" + finger + "_finger_l_mode_anim", add = True)


        if limb == "rightFingers":
            cmds.select(clear = True)
            for finger in ["index", "middle", "ring", "pinky", "thumb"]:
                if cmds.objExists(character + ":" + finger + "_finger_fk_ctrl_1_r"):
                    cmds.select(character + ":" + finger + "_finger_fk_ctrl_1_r", add = True)

                if cmds.objExists(character + ":" + finger + "_finger_r_mode_anim"):
                    cmds.select(character + ":" + finger + "_finger_r_mode_anim", add = True)






    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectRigSettings(self, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        mods = cmds.getModifiers()


        if (mods & 1) > 0:
            cmds.select(character + ":Rig_Settings", tgl = True)

        if (mods & 1) == 0:
            cmds.select(character + ":Rig_Settings")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def findCustomSelectionSets(self, *args):

        if os.path.exists(self.mayaToolsDir + "/General/ART/system/selectionSets.txt"):
            f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'r')
            sets = cPickle.load(f)
            f.close()
            numSets = len(sets)


            #make sure menuItem doesn't already exist
            children = cmds.lsUI(mi = True)
            for child in children:
                if cmds.menuItem(child, q = True, docTag = True) == "customSelectionSet":
                    cmds.deleteUI(child)

            for i in range(int(numSets)):
                label = sets[i][0]
                controls = sets[i][1:]

                #add them to both the select sub menu and the selection sets menu
                menu = cmds.menuItem(parent = self.widgets["selectionSetsCustom"], label = label, docTag = "customSelectionSet", ann = str(sets[i]), c = partial(self.selectFromCustomSet, controls))
                cmds.menuItem(optionBox = True, parent = self.widgets["selectionSetsCustom"], c = partial(self.printSelectionInfo, controls, i))

                menu = cmds.menuItem(parent = self.widgets["selectionSetMenuPopUp"], label = label, docTag = "customSelectionSet", ann = str(sets[i]), c = partial(self.selectFromCustomSet, controls))
                cmds.menuItem(optionBox = True, parent = self.widgets["selectionSetMenuPopUp"], c = partial(self.printSelectionInfo, controls, i))



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def printSelectionInfo(self, controls, removeIndex, *args):

        string = "Controls in this selection set:\n\n" 
        for control in controls:
            string += control + " | "

        result = cmds.confirmDialog(title = "Selection Details", message = string, button = ["Close", "Rename Set", "Remove This Set"], defaultButton = "Rename Set", cancelButton = "Close", dismissString = "Close", icon = "information")

        if result == "Remove This Set":

            if os.path.exists(self.mayaToolsDir + "/General/ART/system/selectionSets.txt"):
                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'r')
                sets = cPickle.load(f)
                f.close()

                #remove the desired set
                sets.pop(removeIndex)

                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'w')
                cPickle.dump(sets, f)
                f.close()


        if result == "Rename Set":

            if os.path.exists(self.mayaToolsDir + "/General/ART/system/selectionSets.txt"):
                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'r')
                sets = cPickle.load(f)
                f.close()

                #rename index 0 of the desired set
                cmds.promptDialog(title = "Rename", message = "New Name:")
                newName = cmds.promptDialog(q = True, text = True)
                sets[removeIndex][0] = newName

                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'w')
                cPickle.dump(sets, f)
                f.close()


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createSelectionSet(self, *args):

        selection = cmds.ls(sl = True)
        if len(selection) > 0:

            #strip off the namespace of each selected item(if it has one, if not, don't include that item)
            setList = []
            controls = ["Selection Set"]
            for each in selection:
                if each.find(":") != -1:
                    control = each.rpartition(":")[2]
                    controls.append(control)


            #store this data to file in ART/system
            if os.path.exists(self.mayaToolsDir + "/General/ART/system/selectionSets.txt"):
                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'r')
                existingSetList = cPickle.load(f)
                f.close()

                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'w')
                existingSetList.append(controls)
                cPickle.dump(existingSetList, f)



            else:
                f = open(self.mayaToolsDir + "/General/ART/system/selectionSets.txt", 'w')
                setList.append(controls)
                cPickle.dump(setList, f)

            f.close()


            #reload
            self.findCustomSelectionSets()




        else:
            cmds.warning("Nothing selected to create a selection set from.")



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectFromCustomSet(self, controls, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        cmds.select(clear = True)

        for each in controls:
            if cmds.objExists(character + ":" + each):
                cmds.select(character + ":" + each, add = True)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectEverything(self, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        controls = list(self.controls)
        controls.append("Rig_Settings")

        #need to find all space switch nodes for the current character
        cmds.select(character + ":*_space_switcher_follow")
        nodes = cmds.ls(sl = True)
        spaceSwitchers = []
        for node in nodes:
            if node.find("invis") == -1:
                spaceSwitchers.append(node)


        selectNodes = []
        for control in spaceSwitchers:
            spaceSwitchNode = control.rpartition("_follow")[0]
            selectNodes.append(spaceSwitchNode)

        cmds.select(clear = True)


        for control in controls:
            if cmds.objExists(character + ":" + control):
                cmds.select(character + ":" + control, add = True)

        for node in ["fk_orient_world_loc_l", "fk_orient_world_loc_r", "fk_orient_body_loc_l", "fk_orient_body_loc_r", "head_fk_orient_neck", "head_fk_orient_shoulder", "head_fk_orient_body", "head_fk_orient_world"]:
            if cmds.objExists(character + ":" + node):
                cmds.select(character + ":" + node, add = True)

        for node in selectNodes:
            cmds.select(node, add = True)




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def selectAll(self, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        controls = list(self.controls)
        controls.append("Rig_Settings")

        for control in controls:
            if cmds.objExists(character + ":" + control):
                cmds.select(character + ":" + control, add = True)

        cmds.warning("Custom Controls and Toes not implemented yet into select all")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def resetAll(self, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        avoidAttrs = ["global_scale", "scaleX", "scaleY", "scaleZ", "stretch", "twist_amount", "bias", "spine_01_twistAmount", "spine_02_twistAmount", "spine_03_twistAmount", "spine_04_twistAmount", "spine_05_twistAmount", "sticky", "fkOrientation"]

        #reset fk arm orientation
        cmds.setAttr(character + ":Rig_Settings.lFkArmOrient", 0)
        cmds.setAttr(character + ":Rig_Settings.rFkArmOrient", 0)

        for control in self.controls:


            if cmds.objExists(character + ":" + control):
                attrs = cmds.listAttr(character + ":" + control, keyable = True, unlocked = True)
                if attrs != None:
                    for attr in attrs:
                        if attr not in avoidAttrs:
                            cmds.setAttr(character + ":" + control + "." + attr, 0)

                        if attr in ["global_scale", "scaleX", "scaleY", "scaleZ"]:
                            cmds.setAttr(character + ":" + control + "." + attr, 1)			

        cmds.warning("Custom Controls and Toes not implemented yet into reset all")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def resetSelection(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        selection = cmds.ls(sl = True)


        for each in selection:
            if each.find(character + ":") == 0:
                attrs = cmds.listAttr(each, keyable = True, unlocked = True)
                avoidAttrs = ["global_scale", "scaleX", "scaleY", "scaleZ", "stretch", "twist_amount", "bias", "spine_01_twistAmount", "spine_02_twistAmount", "spine_03_twistAmount", "spine_04_twistAmount", "spine_05_twistAmount", "sticky", "fkOrientation"]


                if attrs != None:
                    for attr in attrs:
                        if attr not in avoidAttrs:
                            cmds.setAttr(each + "." + attr, 0)

                        if attr in ["global_scale", "scaleX", "scaleY", "scaleZ"]:
                            cmds.setAttr(each + "." + attr, 1)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def importMotion(self, *args):

        import ART_importMotion
        reload(ART_importMotion)
        ART_importMotion.ImportMotionUI()

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def exportMotion(self, *args):

        import ART_exportMotion
        reload(ART_exportMotion)
        ART_exportMotion.ExportMotionUI()


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def setHeadSpace(self, mode, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        if mode == 0:
            cmds.menuItem(self.widgets["neckSpaceRB"], edit = True, rb = True)
            cmds.setAttr(character + ":" + "head_fk_anim.fkOrientation", 0)

        if mode == 1:
            cmds.menuItem(self.widgets["shoulderSpaceRB"], edit = True, rb = True)
            cmds.setAttr(character + ":" + "head_fk_anim.fkOrientation", 1)

        if mode == 2:
            cmds.menuItem(self.widgets["bodySpaceRB"], edit = True, rb = True)
            cmds.setAttr(character + ":" + "head_fk_anim.fkOrientation", 2)

        if mode == 3:
            cmds.menuItem(self.widgets["worldSpaceRB"], edit = True, rb = True)
            cmds.setAttr(character + ":" + "head_fk_anim.fkOrientation", 3)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getHeadSpace(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        space = cmds.getAttr(character + ":" + "head_fk_anim.fkOrientation")

        if space == 0:
            cmds.menuItem(self.widgets["neckSpaceRB"], edit = True, rb = True)

        if space == 1:
            cmds.menuItem(self.widgets["shoulderSpaceRB"], edit = True, rb = True)

        if space == 2:
            cmds.menuItem(self.widgets["bodySpaceRB"], edit = True, rb = True)

        if space == 3:
            cmds.menuItem(self.widgets["worldSpaceRB"], edit = True, rb = True)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getNeckSpace(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        space = cmds.getAttr(character + ":" + "neck_01_fk_anim.fkOrientation")

        if space == 0:
            cmds.menuItem(self.widgets["neckOrientShoulderSpaceRB"], edit = True, rb = True)

        if space == 1:
            cmds.menuItem(self.widgets["neckOrientBodySpaceRB"], edit = True, rb = True)

        if space == 2:
            cmds.menuItem(self.widgets["neckOrientWorldSpaceRB"], edit = True, rb = True)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def setControlSpace(self, spaceSwitchNode, attr, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        #get match and match method settings
        matching = cmds.menuItem(self.widgets["spaceSwitch_MatchToggleCB"], q = True, cb = True)
        matchToControl = cmds.menuItem(self.widgets["spaceSwitch_MatchMethodCB"], q = True, cb = True)



        if attr == None:

            if matching:
                #create temp locator to snap the space switch node to
                control = character + ":" + spaceSwitchNode.rpartition("_space")[0]
                currentTime = cmds.currentTime(q = True)

                #set pre-frame key
                if matchToControl == False:
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, t = (currentTime - 1))
                    cmds.setKeyframe(control, t = (currentTime - 1))

                else:
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, t = (currentTime - 1))
                    cmds.setKeyframe(control, t = (currentTime - 1))
                    cmds.currentTime(currentTime - 1)
                    loc = cmds.spaceLocator()[0]
                    constraint = cmds.parentConstraint(control, loc)[0]
                    cmds.delete(constraint)

                    constraint = cmds.parentConstraint(loc, control)[0]
                    cmds.setKeyframe(control, t = (cmds.currentTime(q = True)))
                    cmds.delete(constraint)
                    cmds.delete(loc)


                #create our temp loc
                tempLoc = cmds.spaceLocator()
                cmds.currentTime(currentTime)

                #constrain temp loc
                if matchToControl == False:
                    constraint = cmds.parentConstraint(character + ":" + spaceSwitchNode, tempLoc[0])[0]

                else:
                    constraint = cmds.parentConstraint(control, tempLoc[0])[0]
                cmds.delete(constraint)


                #match and switch space

                attrs = []
                try:
                    attrs.extend(cmds.listAttr(character + ":" + spaceSwitchNode, string = "space_*"))
                except:
                    pass

                try:
                    attrs.extend(cmds.listAttr(control, string = "space_*"))
                except:
                    pass

                for attribute in attrs:
                    if cmds.objExists(character + ":" + spaceSwitchNode+ "." + attribute):
                        cmds.setAttr(character + ":" + spaceSwitchNode+ "." + attribute, 0)

                    if cmds.objExists(control +  "." + attribute):
                        cmds.setAttr(control + "." + attribute, 0)


                if matchToControl == False:
                    constraint = cmds.parentConstraint(tempLoc[0], character + ":" + spaceSwitchNode)[0]
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, t = currentTime)
                    cmds.setKeyframe(control, t = currentTime)
                    cmds.delete(constraint)
                    cmds.delete(tempLoc)
                    cmds.select(clear = True)

                else:
                    try:
                        constraint = cmds.parentConstraint(tempLoc[0], control)[0]
                    except:
                        constraint = cmds.pointConstraint(tempLoc[0], control)[0]


                    #zero out space node
                    for attr in [".tx", ".ty", ".tz", ".rx", ".ry", ".rz"]:
                        cmds.setAttr(character + ":" + spaceSwitchNode + attr, 0)


                    cmds.setKeyframe(control, t = currentTime)
                    cmds.delete(constraint)
                    cmds.delete(tempLoc)
                    cmds.select(clear = True)		    


            #if we are not matching, just set attrs
            else:
                currentTime = cmds.currentTime(q = True)
                cmds.setKeyframe(character + ":" + spaceSwitchNode, t = (currentTime - 1))

                attrs = []
                try:
                    attrs.extend(cmds.listAttr(character + ":" + spaceSwitchNode, string = "space_*"))
                except:
                    pass

                try:
                    attrs.extend(cmds.listAttr(control, string = "space_*"))
                except:
                    pass

                for attr in attrs:

                    if cmds.objExists(character + ":" + spaceSwitchNode+ "." + attribute):
                        cmds.setAttr(character + ":" + spaceSwitchNode+ "." + attribute, 0)

                    if cmds.objExists(control +  "." + attribute):
                        cmds.setAttr(control + "." + attribute, 0)

                cmds.setKeyframe(character + ":" + spaceSwitchNode, t = currentTime)
                cmds.setKeyframe(control, t = currentTime)




        #if switching to a space other than default
        else:

            if matching:
                #create temp locator to snap the space switch node to
                control = character + ":" + spaceSwitchNode.rpartition("_space")[0]
                currentTime = cmds.currentTime(q = True)

                #set pre-frame key
                if matchToControl == False:
                    cmds.currentTime(currentTime - 1)
                    cmds.setKeyframe(character + ":" + spaceSwitchNode)
                    cmds.setKeyframe(control)

                else:
                    cmds.currentTime(currentTime - 1)
                    cmds.setKeyframe(character + ":" + spaceSwitchNode)
                    cmds.setKeyframe(control)
                    cmds.currentTime(currentTime - 1)
                    loc = cmds.spaceLocator()[0]

                    try:
                        constraint = cmds.parentConstraint(control, loc)[0]
                    except:
                        constraint = cmds.pointConstraint(control, loc)[0]
                    cmds.delete(constraint)

                    try:
                        constraint = cmds.parentConstraint(loc, control)[0]

                    except:
                        constraint = cmds.pointConstraint(loc, control)[0]

                    cmds.setKeyframe(control, t = (cmds.currentTime(q = True)))
                    cmds.delete(constraint)
                    cmds.delete(loc)



                #create our temp loc
                tempLoc = cmds.spaceLocator()[0]
                cmds.currentTime(currentTime)

                #constrain temp loc
                if matchToControl == False:
                    constraint = cmds.parentConstraint(character + ":" + spaceSwitchNode, tempLoc)[0]
                else:
                    constraint = cmds.parentConstraint(control, tempLoc)[0]
                cmds.delete(constraint)



                #match and switch space
                attrs = []
                try:
                    attrs.extend(cmds.listAttr(character + ":" + spaceSwitchNode, string = "space_*"))
                except:
                    pass

                try:
                    attrs.extend(cmds.listAttr(control, string = "space_*"))
                except:
                    pass


                for attribute in attrs:
                    cmds.currentTime(currentTime)

                    if cmds.objExists(character + ":" + spaceSwitchNode + "." + attribute):
                        cmds.setAttr(character + ":" + spaceSwitchNode + "." + attribute, 0)
                        cmds.setKeyframe(character + ":" + spaceSwitchNode, t = currentTime)

                    if cmds.objExists(control + "." + attribute):
                        cmds.setAttr(control + "." + attribute, 0)
                        cmds.setKeyframe(control, t = currentTime)

                if cmds.objExists(character + ":" + spaceSwitchNode + "." + attr):
                    cmds.setAttr(character + ":" + spaceSwitchNode + "." + attr, 1)
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, attribute = attr, t = currentTime)

                if cmds.objExists(control + "." + attr):
                    cmds.setAttr(control + "." + attr, 1)
                    cmds.setKeyframe(control, attribute = attr, t = currentTime)


                if matchToControl == False:
                    constraint = cmds.parentConstraint(tempLoc, character + ":" + spaceSwitchNode)[0]
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, t = currentTime)
                    cmds.delete(constraint)
                    cmds.delete(tempLoc)
                    cmds.select(clear = True)

                else:
                    try:
                        constraint = cmds.parentConstraint(tempLoc, control)[0]
                    except:
                        constraint = cmds.pointConstraint(tempLoc, control)[0]

                    #zero out space node
                    for attribute in [".tx", ".ty", ".tz", ".rx", ".ry", ".rz"]:
                        cmds.setAttr(character + ":" + spaceSwitchNode + attribute, 0)		    

                    cmds.setKeyframe(control, t = currentTime)
                    cmds.delete(constraint)
                    cmds.delete(tempLoc)
                    cmds.select(clear = True)		    


            #if we are not matching, just set attrs
            else:
                currentTime = cmds.currentTime(q = True)
                cmds.setKeyframe(character + ":" + spaceSwitchNode, t = (currentTime - 1))
                cmds.setKeyframe(control, t = (currentTime - 1))

                attrs = []
                try:
                    attrs.extend(cmds.listAttr(character + ":" + spaceSwitchNode, string = "space_*"))
                except:
                    pass

                try:
                    attrs.extend(cmds.listAttr(control, string = "space_*"))
                except:
                    pass


                for attribute in attrs:
                    if cmds.objExists(character + ":" + spaceSwitchNode+ "." + attribute):
                        cmds.setAttr(character + ":" + spaceSwitchNode+ "." + attribute, 0)

                    if cmds.objExists(control + "." + attribute):
                        cmds.setAttr(control + "." + attribute, 0)

                if cmds.objExists(character + ":" + spaceSwitchNode+ "." + attr):
                    cmds.setAttr(character + ":" + spaceSwitchNode + "." + attr, 1)
                    cmds.setKeyframe(character + ":" + spaceSwitchNode, attribute = attr, t = currentTime)

                if cmds.objExists(control + "." + attribute):
                    cmds.setAttr(control + "." + attr, 1)
                    cmds.setKeyframe(control, attribute = attr, t = currentTime)


        #set the current time back
        cmds.currentTime(currentTime)






    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def getControlSpaces(self, popupMenu, radioCollection, spaceSwitchNode, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        #delete any existing children of the radioCollection
        menuItems = cmds.lsUI(mi = True)
        for item in menuItems:
            if cmds.menuItem(item,q = True, docTag = True) == str(radioCollection):
                cmds.deleteUI(item)

        #add spaces to radio menu item collection
        if cmds.objExists(character + ":" + spaceSwitchNode):
            children = cmds.popupMenu(popupMenu, q = True, itemArray = True)

            for child in children:

                label = cmds.menuItem(child, q = True, label = True)
                if label == "Space Switching":
                    parentSpace = cmds.listRelatives(character + ":" + spaceSwitchNode + "_follow", parent = True)[0]
                    parentSpace = parentSpace.partition(":")[2]

                    #add the default space
                    defMenu = cmds.menuItem(label = "default [" + parentSpace + "]", parent = child, cl = radioCollection, rb = False, docTag = str(radioCollection), c = partial(self.setControlSpace, spaceSwitchNode, None))


                    #add the rest of the spaces foud on the space switch node
                    control = spaceSwitchNode.partition("_space")[0]
                    attrs = []

                    try:
                        attrs.extend(cmds.listAttr(character + ":" + spaceSwitchNode, string = "space_*"))
                    except:
                        pass

                    try:
                        attrs.extend(cmds.listAttr(character + ":" + control, string = "space_*"))
                    except:
                        pass


                    found = False
                    for attr in attrs:
                        label = attr.partition("space_")[2]

                        if cmds.objExists(character + ":" + spaceSwitchNode + "." + attr):
                            value = cmds.getAttr(character + ":" + spaceSwitchNode + "." + attr)

                        if cmds.objExists(character + ":" + control + "." + attr):
                            value = cmds.getAttr(character + ":" + control + "." + attr)

                        if value == True:
                            found = True

                        cmds.menuItem(label = label, parent = child, cl = radioCollection, rb = value, docTag = str(radioCollection), c = partial(self.setControlSpace, spaceSwitchNode, attr))

                    if found == False:
                        cmds.menuItem(defMenu, edit = True, rb = True)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def spaceSwitcher(self, *args):
        if cmds.symbolButton(self.widgets["activeCharacterThumb"], q= True, exists = True):
            character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
            import ART_spaceSwitcher
            reload(ART_spaceSwitcher)
            ART_spaceSwitcher.SpaceSwitcher(character, self)

        else:
            cmds.deleteUI("spaceSwitcherUI")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def createSpace(self, *args):
        if cmds.symbolButton(self.widgets["activeCharacterThumb"], q= True, exists = True):
            character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)	

            import ART_spaceSwitcher
            reload(ART_spaceSwitcher)
            inst = ART_spaceSwitcher.SpaceSwitcher(character, self)
            cmds.deleteUI("spaceSwitcherUI")
            inst.createSpaceSwitcherSpace()

        else:
            cmds.warning("No Animation UI detected")




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def poseEditor(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        import ART_poseEditor
        reload(ART_poseEditor)
        ART_poseEditor.PoseEditor_UI(character, self)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def copyPose(self, *args):

        #grab selection
        selection = cmds.ls(sl = True)

        #get attributes from each object in selection
        poseData = []

        for each in selection:
            control = each.partition(":")[2]
            controlInfo = [control]
            attrs = cmds.listAttr(each, keyable = True)

            for attr in attrs:
                value = cmds.getAttr(each + "." + attr)
                controlInfo.append(value)

            poseData.append(controlInfo)


        #write pose data to file
        f = open(self.mayaToolsDir + "/poseCache.txt", 'w')
        cPickle.dump(poseData, f)
        f.close


        #change the annotation of the button to have the clipboard contents
        string = "Pose Clipboard Contents:\n\n"

        for pose in poseData:
            control = pose[0]
            string += control + "\n"

        cmds.symbolButton(self.widgets["pickerPoseTools"], edit = True, ann = string)




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def pastePose(self, *args):

        #load pose from poseCache file
        poseCacheFile = self.mayaToolsDir + "/poseCache.txt"
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        if os.path.exists(poseCacheFile):
            f  = open(poseCacheFile, 'r')
            poseData = cPickle.load(f)
            f.close()



            #sort through pose data, finding control, and values
            for data in poseData:
                control = data[0]
                newData = []
                for i in range(1, int(len(data))):
                    newData.append(data[i])

                attrs = cmds.listAttr(character + ":" + control, keyable = True, unlocked = True)

                for i in range(int(len(attrs))):
                    cmds.setAttr(character + ":" + control + "." + attrs[i], newData[i])


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def pastePreview(self, *args):

        #load pose from poseCache file
        poseCacheFile = self.mayaToolsDir + "/poseCache.txt"
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        if os.path.exists(poseCacheFile):
            f  = open(poseCacheFile, 'r')
            poseData = cPickle.load(f)
            f.close()


            #sort through pose data, finding control, and values
            cmds.select(clear = True)
            for data in poseData:
                control = data[0]
                cmds.select(character + ":" + control, add = True)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def pasteOppositePreview(self, *args):

        #load pose from poseCache file
        poseCacheFile = self.mayaToolsDir + "/poseCache.txt"
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        if os.path.exists(poseCacheFile):
            f  = open(poseCacheFile, 'r')
            poseData = cPickle.load(f)
            f.close()


            #sort through pose data, finding control, and values
            cmds.select(clear = True)
            for data in poseData:
                control = data[0]
                ctrl = control

                if control.find("_l") != -1:
                    ctrl = control.rpartition("_l")[0] + "_r"

                if control.find("_l_") != -1:
                    prefix = control.partition("_l_")[0]
                    suffix = control.partition("_l_")[2]
                    ctrl = prefix + "_r_" + suffix

                if control.find("_r") != -1:
                    ctrl = control.rpartition("_r")[0] + "_l"

                if control.find("_r_") != -1:
                    prefix = control.partition("_r_")[0]
                    suffix = control.partition("_r_")[2]
                    ctrl = prefix + "_l_" + suffix

                cmds.select(character + ":" + ctrl, add = True)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def pastePoseOpposite(self, *args):

        #load pose from poseCache file
        poseCacheFile = self.mayaToolsDir + "/poseCache.txt"
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)
        mirrorAllTransControls =  []
        mirrorxTransControls = ["ik_elbow_l_anim", "ik_elbow_r_anim", "clavicle_l_anim", "clavicle_r_anim","ik_foot_anim_l", "ik_foot_anim_r", "ik_wrist_l_anim", "ik_wrist_r_anim"]
        mirrorRotateZandY = ["ik_foot_anim_l", "ik_foot_anim_r", "ik_wrist_l_anim", "ik_wrist_r_anim"]
        #mirrorRotateY = ["ik_wrist_l_anim", "ik_wrist_r_anim"]

        if os.path.exists(poseCacheFile):
            f  = open(poseCacheFile, 'r')
            poseData = cPickle.load(f)
            f.close()


            #sort through pose data, finding control, and values
            for data in poseData:
                control = data[0]
                newData = []
                for i in range(1, int(len(data))):
                    newData.append(data[i])

                attrs = cmds.listAttr(character + ":" + control, keyable = True, unlocked = True)

                if control.find("_l") != -1:
                    if control.rpartition("_l")[2] == "":
                        ctrl = control.rpartition("_l")[0] + "_r"

                        if ctrl in mirrorAllTransControls:
                            for attr in attrs:
                                if attr.find("translateX") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("translateY") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("translateZ") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1

                        if ctrl in mirrorxTransControls:
                            for attr in attrs:
                                if attr.find("translateX") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1

                        if ctrl in mirrorRotateZandY:
                            for attr in attrs:
                                if attr.find("rotateY") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("rotateZ") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1


                        for i in range(int(len(attrs))):
                            cmds.setAttr(character + ":" + ctrl + "." + attrs[i], newData[i])




                if control.find("_l_") != -1:
                    prefix = control.partition("_l_")[0]
                    suffix = control.partition("_l_")[2]
                    ctrl = prefix + "_r_" + suffix

                    if ctrl in mirrorAllTransControls:
                        for attr in attrs:
                            if attr.find("translateX") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("translateY") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("translateZ") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1

                    if ctrl in mirrorxTransControls:
                        for attr in attrs:
                            if attr.find("translateX") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1

                    if ctrl in mirrorRotateZandY:
                        for attr in attrs:
                            if attr.find("rotateY") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("rotateZ") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1


                    for i in range(int(len(attrs))):
                        cmds.setAttr(character + ":" + ctrl + "." + attrs[i], newData[i])



                if control.find("_r") != -1:
                    if control.rpartition("_r")[2] == "":
                        ctrl = control.rpartition("_r")[0] + "_l"

                        if ctrl in mirrorAllTransControls:
                            for attr in attrs:
                                if attr.find("translateX") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("translateY") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("translateZ") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1

                        if ctrl in mirrorxTransControls:
                            for attr in attrs:
                                if attr.find("translateX") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1

                        if ctrl in mirrorRotateZandY:
                            for attr in attrs:
                                if attr.find("rotateY") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1
                                if attr.find("rotateZ") == 0:
                                    index =  attrs.index(attr)
                                    newData[index] = newData[index] * -1


                        for i in range(int(len(attrs))):
                            cmds.setAttr(character + ":" + ctrl + "." + attrs[i], newData[i])


                if control.find("_r_") != -1:
                    prefix = control.partition("_r_")[0]
                    suffix = control.partition("_r_")[2]
                    ctrl = prefix + "_l_" + suffix

                    if ctrl in mirrorAllTransControls:
                        for attr in attrs:
                            if attr.find("translateX") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("translateY") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("translateZ") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1

                    if ctrl in mirrorxTransControls:
                        for attr in attrs:
                            if attr.find("translateX") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1

                    if ctrl in mirrorRotateZandY:
                        for attr in attrs:
                            if attr.find("rotateY") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1
                            if attr.find("rotateZ") == 0:
                                index =  attrs.index(attr)
                                newData[index] = newData[index] * -1



                    for i in range(int(len(attrs))):
                        cmds.setAttr(character + ":" + ctrl + "." + attrs[i], newData[i])



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def toggleControlVis(self, visibility, *args):
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        controls = []
        for control in ["head_fk_anim", "neck_01_fk_anim", "neck_02_fk_anim", "neck_03_fk_anim", "spine_01_anim", "spine_02_anim", "spine_03_anim", "spine_04_anim", "spine_05_anim", "mid_ik_anim", "chest_ik_anim",
                        "body_anim", "hip_anim", "clavicle_l_anim", "clavicle_r_anim", "fk_arm_l_anim", "fk_arm_r_anim", "fk_elbow_l_anim", "fk_elbow_r_anim", "fk_wrist_l_anim", "fk_wrist_r_anim",
                        "ik_elbow_l_anim", "ik_elbow_r_anim", "ik_wrist_l_anim", "ik_wrist_r_anim", "fk_thigh_l_anim", "fk_thigh_r_anim", "fk_calf_l_anim", "fk_calf_r_anim", "fk_foot_l_anim", "fk_foot_r_anim",
                        "fk_ball_l_anim", "fk_ball_r_anim", "ik_foot_anim_l", "ik_foot_anim_r", "heel_ctrl_l", "heel_ctrl_r", "toe_wiggle_ctrl_l", "toe_wiggle_ctrl_r",
                        "toe_tip_ctrl_l", "toe_tip_ctrl_r", "master_anim", "offset_anim", "root_anim", "upperarm_l_twist_anim", "upperarm_l_twist_2_anim", "upperarm_l_twist_3_anim", "upperarm_r_twist_anim", "upperarm_r_twist_2_anim", "upperarm_r_twist_3_anim", "l_thigh_twist_01_anim", "r_thigh_twist_01_anim",
                        "pinky_metacarpal_ctrl_l", "pinky_metacarpal_ctrl_r", "pinky_finger_fk_ctrl_1_l", "pinky_finger_fk_ctrl_1_r", "pinky_finger_fk_ctrl_2_l", "pinky_finger_fk_ctrl_2_r", "pinky_finger_fk_ctrl_3_l", "pinky_finger_fk_ctrl_3_r",
                        "ring_metacarpal_ctrl_l", "ring_metacarpal_ctrl_r", "ring_finger_fk_ctrl_1_l", "ring_finger_fk_ctrl_1_r", "ring_finger_fk_ctrl_2_l", "ring_finger_fk_ctrl_2_r", "ring_finger_fk_ctrl_3_l", "ring_finger_fk_ctrl_3_r",
                        "middle_metacarpal_ctrl_l", "middle_metacarpal_ctrl_r", "middle_finger_fk_ctrl_1_l", "middle_finger_fk_ctrl_1_r", "middle_finger_fk_ctrl_2_l", "middle_finger_fk_ctrl_2_r", "middle_finger_fk_ctrl_3_l", "middle_finger_fk_ctrl_3_r",
                        "index_metacarpal_ctrl_l", "index_metacarpal_ctrl_r", "index_finger_fk_ctrl_1_l", "index_finger_fk_ctrl_1_r", "index_finger_fk_ctrl_2_l", "index_finger_fk_ctrl_2_r", "index_finger_fk_ctrl_3_l", "index_finger_fk_ctrl_3_r",
                        "thumb_finger_fk_ctrl_1_l", "thumb_finger_fk_ctrl_1_r", "thumb_finger_fk_ctrl_2_l", "thumb_finger_fk_ctrl_2_r", "thumb_finger_fk_ctrl_3_l", "thumb_finger_fk_ctrl_3_r",
                        "index_l_ik_anim", "index_r_ik_anim", "middle_l_ik_anim", "middle_r_ik_anim", "ring_l_ik_anim", "ring_r_ik_anim", "pinky_l_ik_anim", "pinky_r_ik_anim", "thumb_l_ik_anim", "thumb_r_ik_anim",
                        "index_l_poleVector", "index_r_poleVector", "middle_l_poleVector", "middle_r_poleVector", "ring_l_poleVector", "ring_r_poleVector", "pinky_l_poleVector", "pinky_r_poleVector", "thumb_l_poleVector", "thumb_r_poleVector",
                        "l_global_ik_anim", "r_global_ik_anim", "lowerarm_l_twist_anim", "lowerarm_l_twist2_anim", "lowerarm_l_twist3_anim", "lowerarm_r_twist_anim", "lowerarm_r_twist2_anim", "lowerarm_r_twist3_anim", "calf_r_twist_anim", "calf_r_twist2_anim", "calf_r_twist3_anim",
                        "calf_l_twist_anim", "calf_l_twist2_anim", "calf_l_twist3_anim", "thigh_l_twist_2_anim", "thigh_l_twist_3_anim", "thigh_r_twist_2_anim", "thigh_r_twist_3_anim"]:
            controls.append(control)

        #find custom joints
        customJoints = []
        attrs = cmds.listAttr(character + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        for joint in customJoints:
            attribute = cmds.getAttr(character + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "leaf":
                label = label.partition(" (")[0]
                control = label + "_anim"
                controls.append(control)

            if jointType == "jiggle":
                control = label + "_anim"
                controls.append(control)

            if jointType == "chain" or jointType == "dynamic":
                numJointsInChain = label.partition("(")[2].partition(")")[0]
                label = label.partition(" (")[0]

                for i in range(int(numJointsInChain)):
                    controls.append("fk_" + label + "_0" + str(i + 1) + "_anim")
                controls.append(label + "_cv_0_anim")
                controls.append(label + "_dyn_anim")

                cmds.select("*:" + label + "_ik_*_anim")
                selection = cmds.ls(sl = True)
                for each in selection:
                    niceName = each.partition(":")[2]
                    controls.append(niceName)



        for control in controls:
            if cmds.objExists(character + ":" + control):
                shape = cmds.listRelatives(character + ":" + control, shapes = True)[0]
                if visibility == False:
                    cmds.setAttr(shape + ".v", 0)
                if visibility == True:
                    cmds.setAttr(shape + ".v", 1)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def updateUI_scriptJob(self, *args):
        self.mainScriptJob = cmds.scriptJob(event = ["timeChanged", self.updateUI], parent = self.widgets["window"], kws = True)

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def updateUI(self, *args):
        name = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        customJoints = []
        attrs = cmds.listAttr(name + ":" + "Skeleton_Settings")
        for attr in attrs:
            if attr.find("extraJoint") == 0:
                customJoints.append(attr)

        for joint in customJoints:
            attribute = cmds.getAttr(name + ":" + "Skeleton_Settings." + joint, asString = True)
            jointType = attribute.partition("/")[2].partition("/")[0]
            label = attribute.rpartition("/")[2]

            if jointType == "chain" or jointType == "dynamic":
                label = label.partition(" (")[0]

                mode1 =cmds.getAttr(name + ":Rig_Settings." + label + "_fk")
                mode2 =cmds.getAttr(name + ":Rig_Settings." + label + "_ik")
                mode3 =cmds.getAttr(name + ":Rig_Settings." + label + "_dynamic")

                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "fkModeButton"], edit = True, select = mode1)
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "ikModeButton"], edit = True, select = mode2)
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_customJoints_" + label + "dynModeButton"], edit = True, select = mode3)





        mode = cmds.getAttr(name + ":Rig_Settings.rArmMode")
        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightArmIkModeButton"], edit = True, select = True)


        mode = cmds.getAttr(name + ":Rig_Settings.lArmMode")
        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftArmIkModeButton"], edit = True, select = True)

        mode = cmds.getAttr(name + ":Rig_Settings.lLegMode")
        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftLegFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_leftLegIkModeButton"], edit = True, select = True)


        mode = cmds.getAttr(name + ":Rig_Settings.rLegMode")
        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightLegFkModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_rightLegIkModeButton"], edit = True, select = True)


        mode1 = cmds.getAttr(name + ":Rig_Settings.spine_fk")
        mode2 = cmds.getAttr(name + ":Rig_Settings.spine_ik")
        if mode1 > mode2:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_spineFkModeButton"], edit = True, select = True)
        else:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_spineIkModeButton"], edit = True, select = True)


        mode = cmds.getAttr(name + ":head_fk_anim.fkOrientation")
        if mode == 0:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headFkOrientModeButton"], edit = True, select = True)
        if mode == 1:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headShoulderOrientModeButton"], edit = True, select = True)
        if mode == 2:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headBodyOrientModeButton"], edit = True, select = True)
        if mode == 3:
            cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_headWorldOrientModeButton"], edit = True, select = True)

        try:
            mode = cmds.getAttr(name + ":neck_01_fk_anim.fkOrientation")

            if mode == 0:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckShoulderOrientModeButton"], edit = True, select = True)
            if mode == 1:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckBodyOrientModeButton"], edit = True, select = True)
            if mode == 2:
                cmds.iconTextRadioButton(self.widgets[name + "_rigSettings_neckWorldOrientModeButton"], edit = True, select = True)

        except:
            pass


        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_l_ik_anim"):
                mode = cmds.getAttr(name + ":" + finger + "_finger_l_mode_anim.FK_IK")
                if mode == 0:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_FkModeButton"], edit = True, select = True)
                if mode == 1:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_LeftFinger_" + finger + "_IkModeButton"], edit = True, select = True)


        for finger in ["index", "middle", "ring", "pinky", "thumb"]:
            if cmds.objExists(name + ":" + finger + "_l_ik_anim"):
                mode = cmds.getAttr(name + ":" + finger + "_finger_r_mode_anim.FK_IK")
                if mode == 0:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_RightFinger_" + finger + "_FkModeButton"], edit = True, select = True)
                if mode == 1:
                    cmds.iconTextRadioButton(self.widgets[name + "rigSettings_RightFinger_" + finger + "_IkModeButton"], edit = True, select = True)


        try:
            mode = cmds.getAttr(name + ":Rig_Settings.rFkArmOrient")
            for button in[self.widgets[name + "_rightClavPickerButton"], self.widgets[name + "_rightShoulderPickerButton"], self.widgets[name + "_rightElbowPickerButton"], self.widgets[name + "_rightHandPickerButton"], self.widgets[name + "_rightIkElbowPickerButton"], self.widgets[name + "_rightIkHandPickerButton"]]:


                if mode == 0:
                    clavVal = True
                    bodyVal = False
                    worldVal = False

                if mode == 1:
                    clavVal = False
                    bodyVal = True
                    worldVal = False

                if mode == 2:
                    clavVal = False
                    bodyVal = False
                    worldVal = True


                cmds.menuItem(self.widgets[name + "_" + button + "_rightArm_ClavSpace"], edit = True, rb = clavVal)
                cmds.menuItem(self.widgets[name + "_" + button + "_rightArm_BodySpace"], edit = True, rb = bodyVal)
                cmds.menuItem(self.widgets[name + "_" + button + "_rightArm_WrldSpace"], edit = True, rb = worldVal)

        except:
            pass


        try:
            mode = cmds.getAttr(name + ":Rig_Settings.lFkArmOrient")
            for button in[self.widgets[name + "_leftClavPickerButton"], self.widgets[name + "_leftShoulderPickerButton"], self.widgets[name + "_leftElbowPickerButton"], self.widgets[name + "_leftHandPickerButton"], self.widgets[name + "_leftIkElbowPickerButton"], self.widgets[name + "_leftIkHandPickerButton"]]:

                if mode == 0:
                    clavVal = True
                    bodyVal = False
                    worldVal = False

                if mode == 1:
                    clavVal = False
                    bodyVal = True
                    worldVal = False

                if mode == 2:
                    clavVal = False
                    bodyVal = False
                    worldVal = True


                cmds.menuItem(self.widgets[name + "_" + button + "_leftArm_ClavSpace"], edit = True, rb = clavVal)
                cmds.menuItem(self.widgets[name + "_" + button + "_leftArm_BodySpace"], edit = True, rb = bodyVal)
                cmds.menuItem(self.widgets[name + "_" + button + "_leftArm_WrldSpace"], edit = True, rb = worldVal)

        except:
            pass


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def ikKneeSolve(self, character, side, *args):



        startPoint = cmds.xform(character + ":ik_leg_calf_" + side, q = True, ws = True, t = True)
        endPoint = cmds.xform("calf_" + side, q = True, ws = True, t = True)
        dist = cmds.distanceDimension( sp=(startPoint[0],startPoint[1],startPoint[2]), ep=(endPoint[0], endPoint[1], endPoint[2]) )
        distParent = cmds.listRelatives(dist, parent = True)[0]

        locs = cmds.listConnections(dist)
        startLoc = locs[0]
        endLoc = locs[1]

        cmds.pointConstraint(character + ":ik_leg_calf_" + side, startLoc)
        cmds.pointConstraint("calf_" + side, endLoc)



        #get distance between rig knees and mocap knees
        distance =  cmds.getAttr(dist + ".distance")
        self.checkDistance(character, dist, distance, distance, side)


        #clean up
        cmds.delete([locs[0], locs[1],  dist, distParent])







    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def ikHeelSolve(self, character, side, *args):


        if cmds.objExists("ball_" + side):
            value = cmds.getAttr("ball_" + side + ".rz")

            if value > 10:
                cmds.setAttr(character + ":ik_foot_anim_" + side + ".rx", 0)
                cmds.setAttr(character + ":ik_foot_anim_" + side + ".ry", 0)
                cmds.setAttr(character + ":ik_foot_anim_" + side + ".rz", 0)
                cmds.setKeyframe(character + ":ik_foot_anim_" + side + "")

                cmds.setAttr(character + ":heel_ctrl_" + side + ".rz", value * -1)
                cmds.setKeyframe(character + ":heel_ctrl_" + side + ".rz")

                footPos = cmds.xform("foot_" + side, q = True, ws = True, t = True)
                ikFootPos = cmds.xform(character + ":ik_leg_foot_" + side, q = True, ws = True, t = True)
                yDiff = footPos[1] - ikFootPos[1]
                zDiff = footPos[2] - ikFootPos[2]

                cmds.xform(character + ":ik_foot_anim_" + side, r = True, t = [0, yDiff, zDiff])
                cmds.setKeyframe(character + ":ik_foot_anim_" + side)

            else:
                cmds.setAttr(character + ":heel_ctrl_" + side + ".rz", 0)
                cmds.setKeyframe(character + ":heel_ctrl_" + side + ".rz")



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def match_singleFrame(self, limb, side, matchFrom, matchTo, *args):

        #get the passed in limb, and duplicate the skeleton for that limb's current mode
        #for example, if limb is left arm, and matchFrom is IK, then dupe the driver joints (in IK pose) for the left arm and parent to world
        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        armBones = ["upperarm_", "lowerarm_", "hand_"]
        spineBones = ["driver_spine_01", "driver_spine_02", "driver_spine_03", "driver_spine_04", "driver_spine_05"]
        legBones = ["thigh_", "calf_", "foot_", "ball_"]

        #grab current selection
        currentSelection = cmds.ls(sl = True)

        #arm matching
        if limb == "arm":
            #setup constraints based on mode info
            if matchTo == "IK":

                constraint1 = cmds.orientConstraint(character + ":ik_upperarm_fk_matcher_" + side, character + ":fk_arm_" + side + "_anim")[0]
                constraint2 = cmds.orientConstraint(character + ":ik_lowerarm_fk_matcher_" + side, character + ":fk_elbow_" + side + "_anim")[0]
                constraint3 = cmds.orientConstraint(character + ":hand_match_loc_" + side, character + ":fk_wrist_" + side + "_anim")[0]

                cmds.setKeyframe(character + ":fk_arm_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_elbow_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_wrist_" + side + "_anim")

                cmds.delete(constraint1)
                cmds.delete(constraint2)
                cmds.delete(constraint3)



            if matchTo == "FK":

                dupeNodes = cmds.duplicate(character + ":driver_" + armBones[0] + side)

                parent = cmds.listRelatives(dupeNodes[0], parent = True)
                if parent != None:
                    cmds.parent(dupeNodes[0], world = True)

                cmds.pointConstraint("driver_hand_" + side, character + ":ik_wrist_" + side + "_anim")
                constraint = cmds.orientConstraint(character + ":fk_wrist_" + side + "_anim", character + ":ik_wrist_" + side + "_anim")[0]
                
                # CRA NEW CODE - For making sure the elbow match doesnt cause any weird twisting issues.
                if cmds.objExists(character + ":elbowswitch_"+side):
                    print "NEW CODE WORKING"
                    ptCnst = cmds.pointConstraint(character + ":elbowswitch_"+side, character + ":ik_elbow_" + side + "_anim")
                else:
                    cmds.pointConstraint("driver_lowerarm_"+side, character + ":ik_elbow_" + side + "_anim")
                # CRA END NEW CODE

                if side == "l":
                    cmds.setAttr(constraint + ".offsetX", 90)

                if side == "r":
                    cmds.setAttr(constraint + ".offsetX", -90)


                cmds.setKeyframe(character + ":ik_wrist_" + side + "_anim")
                cmds.setKeyframe(character + ":ik_elbow_" + side + "_anim")

                # CRA NEW CODE
                if cmds.objExists(character + ":elbowswitch_"+side):
                    cmds.delete(ptCnst)
                # CRA END NEW CODE
                
                cmds.delete(constraint)
                #delete the original mode pose joints
                cmds.delete(dupeNodes[0])


        if limb == "clav":
            #setup constraints based on mode info
            if matchTo == "IK":

                constraint1 = cmds.orientConstraint(character + ":ik_clavicle_" + side, character + ":fk_clavicle_" + side + "_anim")[0]
                cmds.setKeyframe(character + ":fk_clavicle_" + side + "_anim")
                cmds.delete(constraint1)

            if matchTo == "FK":

                constraint1 = cmds.pointConstraint(character + ":driver_upperarm_" + side, character + ":clavicle_" + side + "_anim")[0]
                cmds.setKeyframe(character + ":clavicle_" + side + "_anim")
                cmds.delete(constraint1)	    






        #leg matching
        if limb == "leg":
            dupeNodes = cmds.duplicate(character + ":" + legBones[0] + side)

            parent = cmds.listRelatives(dupeNodes[0], parent = True)
            if parent != None:
                cmds.parent(dupeNodes[0], world = True)


            #setup constraints based on mode info
            if matchTo == "IK":

                cmds.orientConstraint("thigh_" + side, character + ":fk_thigh_" + side + "_anim")
                cmds.orientConstraint("calf_" + side, character + ":fk_calf_" + side + "_anim")
                cmds.orientConstraint("foot_" + side, character + ":fk_foot_" + side + "_anim")

                if cmds.objExists("ball_" + side):
                    cmds.orientConstraint("ball_" + side, character + ":fk_ball_" + side + "_anim")


                cmds.setKeyframe(character + ":fk_thigh_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_calf_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_foot_" + side + "_anim")
                if cmds.objExists("ball_" + side):
                    cmds.setKeyframe(character + ":fk_ball_" + side + "_anim")



            if matchTo == "FK":
                loc = cmds.spaceLocator(name = character + ":ik_foot_anim_" + side + "_locator")[0]
                constraint = cmds.parentConstraint("foot_" + side, loc)[0]
                cmds.delete(constraint)

                cmds.pointConstraint(loc, character + ":ik_foot_anim_" + side)
                constraint = cmds.orientConstraint("foot_" + side, character + ":ik_foot_anim_" + side)[0]

                if side == "l":
                    cmds.setAttr(constraint + ".offsetY", 90)

                if side == "r":
                    cmds.setAttr(constraint + ".offsetX", 180)
                    cmds.setAttr(constraint + ".offsetY", 90)

                cmds.setKeyframe(character + ":ik_foot_anim_" + side)

                #run knee solve to get angle to set
                self.ikKneeSolve(character, side)
                self.ikHeelSolve(character, side)


            #delete the original mode pose joints
            cmds.delete(dupeNodes[0])

            if cmds.objExists(character + ":ik_foot_anim_" + side + "_locator"):
                cmds.delete(character + ":ik_foot_anim_" + side + "_locator")








        #spine matching
        if limb == "spine":
            dupeNodes = cmds.duplicate(character + ":" + spineBones[0])

            parent = cmds.listRelatives(dupeNodes[0], parent = True)
            if parent != None:
                cmds.parent(dupeNodes[0], world = True)
            
            #setup constraints based on mode info
            # Switching to FK
            if matchTo == "IK":

                #check to see if user has any project specific match scripts (Fortnite custom)
                if cmds.objExists(character + ":spine_02_anim.driven"):
                    #furthermore, if the plusMinusAvg nodes exist, then import custom matching
                    if os.path.exists(self.mayaToolsDir + "/General/Scripts/fortniteRotoMatch.py"):

                        result = cmds.confirmDialog(title = "Match Options", icon = "question", message = "Which match option would you like to use?", button = ["Standard", "Rotoscope"])

                        if result == "Standard":

                            try:
                                cmds.setAttr(character + ":spine_02_anim.driven", 0)
                                cmds.setAttr(character + ":spine_04_anim.driven", 0)
                            except:
                                pass

                            if cmds.objExists(character + ":spine_01_anim"):
                                cmds.orientConstraint("driver_spine_01", character + ":spine_01_anim")
                                cmds.setKeyframe(character + ":spine_01_anim")
                            
                            if cmds.objExists(character + ":spine_02_anim"):
                                cmds.orientConstraint("driver_spine_02", character + ":spine_02_anim")
                                cmds.setKeyframe(character + ":spine_02_anim")

                            if cmds.objExists(character + ":spine_03_anim"):
                                cmds.orientConstraint("driver_spine_03", character + ":spine_03_anim")
                                cmds.setKeyframe(character + ":spine_03_anim")

                            if cmds.objExists(character + ":spine_04_anim"):
                                cmds.orientConstraint("driver_spine_04", character + ":spine_04_anim")
                                cmds.setKeyframe(character + ":spine_04_anim")

                            if cmds.objExists(character + ":spine_05_anim"):
                                cmds.orientConstraint("driver_spine_05", character + ":spine_05_anim")
                                cmds.setKeyframe(character + ":spine_05_anim")

                            #for each in spineBones:
                                #if cmds.objExists(character + ":" + each + "_anim"):
                                    #cmds.setKeyframe(character + ":" + each + "_anim")


                        if result == "Rotoscope":
                            import fortniteRotoMatch as fnRm
                            reload(fnRm)
                            fnRm.RotoSpineMatch(character)


                else:
                    if cmds.objExists(character + ":spine_01_anim"):
                        cmds.orientConstraint("driver_spine_01", character + ":spine_01_anim")
                        cmds.setKeyframe(character + ":spine_01_anim")
                    
                    if cmds.objExists(character + ":spine_02_anim"):
                        cmds.orientConstraint("driver_spine_02", character + ":spine_02_anim")
                        cmds.setKeyframe(character + ":spine_02_anim")

                    if cmds.objExists(character + ":spine_03_anim"):
                        cmds.orientConstraint("driver_spine_03", character + ":spine_03_anim")
                        cmds.setKeyframe(character + ":spine_03_anim")

                    if cmds.objExists(character + ":spine_04_anim"):
                        cmds.orientConstraint("driver_spine_04", character + ":spine_04_anim")
                        cmds.setKeyframe(character + ":spine_04_anim")

                    if cmds.objExists(character + ":spine_05_anim"):
                        cmds.orientConstraint("driver_spine_05", character + ":spine_05_anim")
                        cmds.setKeyframe(character + ":spine_05_anim")

                    #for each in spineBones:
                        #if cmds.objExists(character + ":" + each + "_anim"):
                            #cmds.setKeyframe(character + ":" + each + "_anim")

            # Switching to IK
            if matchTo == "FK":

                if cmds.objExists(character + ":chest_ik_anim"):

                    #find highest spine joint
                    numSpineBones = cmds.getAttr(character + ":Skeleton_Settings.numSpineBones")
                    if numSpineBones == 5:
                        endSpine = "driver_spine_05"
                        midSpine = ["driver_spine_03"]

                    if numSpineBones == 4:
                        endSpine = "driver_spine_04"
                        midSpine = ["driver_spine_02", "driver_spine_03"]

                    if numSpineBones == 3:
                        endSpine = "driver_spine_03"
                        midSpine = ["driver_spine_02"]

                    if cmds.objExists("chest_ik_anim_MATCH"):
                        cmds.parentConstraint("chest_ik_anim_MATCH", character + ":chest_ik_anim")
                        cmds.parentConstraint("mid_ik_anim_MATCH", character + ":mid_ik_anim")
                    else:
                        cmds.parentConstraint(endSpine, character + ":chest_ik_anim")
                        for each in midSpine:
                            cmds.parentConstraint(each, character + ":mid_ik_anim")

                cmds.setKeyframe([character + ":chest_ik_anim", character + ":mid_ik_anim"])


            #delete the original mode pose joints
            cmds.delete(dupeNodes[0])


        #reselect selection before entering process
        if len(currentSelection) > 0:
            cmds.select(currentSelection)



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def match_frameRange_bakeMotionToRefJoints(self, character, parts, start, end, *args):
        constraints = []
        bakeJoints = []

        armBones = ["upperarm_", "lowerarm_", "hand_"]
        spineBones = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]
        legBones = ["thigh_", "calf_", "foot_", "ball_"]


        #go through each part, and duplicate the appropriate part of the skeleton for constraining the rig to
        for part in parts:

            if part == character + ":" + "Left Arm":

                #duplicate the current skeleton pose for the limb
                dupeNodes = cmds.duplicate(character + ":" + armBones[0] + "l")
                for node in dupeNodes:
                    bakeJoints.append(node)
                parent = cmds.listRelatives(dupeNodes[0], parent = True)
                if parent != None:
                    cmds.parent(dupeNodes[0], world = True)

                #bake anim data onto dupe nodes
                for node in dupeNodes:

                    constraint = cmds.parentConstraint(character + ":" + node, node)[0]
                    constraints.append(constraint)



            if part == character + ":" + "Right Arm":

                #duplicate the current skeleton pose for the limb
                dupeNodes = cmds.duplicate(character + ":" + armBones[0] + "r")
                for node in dupeNodes:
                    bakeJoints.append(node)
                parent = cmds.listRelatives(dupeNodes[0], parent = True)
                if parent != None:
                    cmds.parent(dupeNodes[0], world = True)

                #bake anim data onto dupe nodes
                for node in dupeNodes:
                    constraint = cmds.parentConstraint(character + ":" + node, node)[0]
                    constraints.append(constraint)		    


            if part == character + ":" + "Left Leg":

                #duplicate the current skeleton pose for the limb
                dupeNodes = cmds.duplicate(character + ":" + legBones[0] + "l")
                for node in dupeNodes:
                    bakeJoints.append(node)
                parent = cmds.listRelatives(dupeNodes[0], parent = True)
                if parent != None:
                    cmds.parent(dupeNodes[0], world = True)

                #bake anim data onto dupe nodes
                for node in dupeNodes:
                    try:
                        constraint = cmds.parentConstraint(character + ":" + node, node)[0]
                        constraints.append(constraint)
                    except:
                        pass


            if part == character + ":" + "Right Leg":

                #duplicate the current skeleton pose for the limb
                dupeNodes = cmds.duplicate(character + ":" + legBones[0] + "r")
                for node in dupeNodes:
                    bakeJoints.append(node)
                parent = cmds.listRelatives(dupeNodes[0], parent = True)
                if parent != None:
                    cmds.parent(dupeNodes[0], world = True)

                #bake anim data onto dupe nodes
                for node in dupeNodes:
                    try:
                        constraint = cmds.parentConstraint(character + ":" + node, node)[0]
                        constraints.append(constraint)
                    except:
                        pass


            if part == character + ":" + "Spine":

                #duplicate the current skeleton pose for the limb
                dupeNodes = cmds.duplicate(character + ":" + spineBones[0])

                #delete children after last spine bone
                lastSpine = "spine_02"

                if cmds.objExists("spine_03"):
                    lastSpine = "spine_03"

                if cmds.objExists("spine_04"):
                    lastSpine = "spine_04"

                if cmds.objExists("spine_05"):
                    lastSpine = "spine_05"

                children = cmds.listRelatives(lastSpine, children = True)
                for child in children:
                    cmds.delete(child)

                cmds.select(dupeNodes[0], hi = True)
                newNodes = cmds.ls(sl = True)

                for node in newNodes:
                    bakeJoints.append(node)

                parent = cmds.listRelatives(newNodes[0], parent = True)
                if parent != None:
                    cmds.parent(newNodes[0], world = True)

                #bake anim data onto dupe nodes
                for node in newNodes:
                    constraint = cmds.parentConstraint(character + ":" + node, node)[0]
                    constraints.append(constraint)



        #bake down all bakeJoints
        if len(bakeJoints) > 0:
            cmds.select(clear = True)
            for each in bakeJoints:
                cmds.select(each, add = True)

            cmds.bakeResults(simulation = True, t= (start, end), preserveOutsideKeys = True)
            for each in constraints:
                cmds.delete(each)

        #return bake joints
        return bakeJoints


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def ikHeelSolve_frameRange(self, character, side, start, end, *args):

        values = []

        for i in range(int(start), int(end + 1)):
            cmds.currentTime(i)

            if cmds.objExists("ball_" + side):
                value = cmds.getAttr("ball_" + side + ".rz")
                values.append(value)

        x = 0
        for i in range(int(start), int(end + 1)):
            cmds.currentTime(i)
            if cmds.objExists("ball_" + side):
                if values[x] > 10:
                    cmds.setAttr(character + ":ik_foot_anim_" + side + ".rx", 0)
                    cmds.setAttr(character + ":ik_foot_anim_" + side + ".ry", 0)
                    cmds.setAttr(character + ":ik_foot_anim_" + side + ".rz", 0)
                    cmds.setKeyframe(character + ":ik_foot_anim_" + side)

                    cmds.setAttr(character + ":heel_ctrl_" + side + ".rz", values[x] * -1)
                    cmds.setKeyframe(character + ":heel_ctrl_" + side + ".rz")

                    footPos = cmds.xform("foot_" + side, q = True, ws = True, t = True)
                    ikFootPos = cmds.xform(character + ":ik_leg_foot_" + side, q = True, ws = True, t = True)
                    yDiff = footPos[1] - ikFootPos[1]
                    zDiff = footPos[2] - ikFootPos[2]

                    cmds.xform(character + ":ik_foot_anim_" + side, r = True, t = [0, yDiff, zDiff])
                    cmds.setKeyframe(character + ":ik_foot_anim_" + side)

                else:
                    cmds.setAttr(character + ":heel_ctrl_" + side + ".rz", 0)
                    cmds.setKeyframe(character + ":heel_ctrl_" + side + ".rz")

            #iterate x
            x = x + 1

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def checkDistance(self, character, distanceNode, distanceAttr, originalValue,  side):


        if distanceAttr > 1:
            currentAttr = cmds.getAttr(character + ":ik_foot_anim_" + side + ".knee_twist")

            try:
                cmds.setAttr(character + ":ik_foot_anim_" + side + ".knee_twist", currentAttr + 1)
                cmds.setKeyframe(character + ":ik_foot_anim_" + side + ".knee_twist")
                newDist = cmds.getAttr(distanceNode + ".distance")


                if newDist < originalValue:
                    self.checkDistance(character, distanceNode, newDist, newDist, side)
                    cmds.progressWindow(self.progWindow, edit=True, progress= (cmds.progressWindow(q = True, progress = True) + 3), status= "Solving IK Pole Vectors" )

                if newDist > originalValue:
                    cmds.setAttr(character + ":ik_foot_anim_" + side + ".knee_twist", currentAttr - 2)
                    cmds.setKeyframe(character + ":ik_foot_anim_" + side + ".knee_twist")
                    newDist = cmds.getAttr(distanceNode + ".distance")

                    self.checkDistance(character, distanceNode, newDist, newDist, side)
                    cmds.progressWindow(self.progWindow, edit=True, progress= (cmds.progressWindow(q = True, progress = True) + 3), status= "Solving IK Pole Vectors" )


            except:
                pass





    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def ikKneeSolve_frameRange(self, character, side, start, end, *args):

        length = abs(cmds.getAttr("calf_" + side + ".tx"))

        startPoint = cmds.xform(character + ":ik_leg_calf_" + side, q = True, ws = True, t = True)
        endPoint = cmds.xform("calf_" + side, q = True, ws = True, t = True)
        dist = cmds.distanceDimension( sp=(startPoint[0],startPoint[1],startPoint[2]), ep=(endPoint[0], endPoint[1], endPoint[2]) )
        distParent = cmds.listRelatives(dist, parent = True)[0]

        locs = cmds.listConnections(dist)
        startLoc = locs[0]
        endLoc = locs[1]

        cmds.pointConstraint(character + ":ik_leg_calf_" + side, startLoc)
        cmds.pointConstraint("calf_" + side, endLoc)

        cmds.currentTime(int(start))

        #get distance between rig knees and mocap knees
        for i in range(int(start), int(end) + 1):
            cmds.currentTime(i)

            distance =  cmds.getAttr(dist + ".distance")
            self.checkDistance(character, dist, distance, distance, side)



        #clean up
        cmds.delete([startLoc, endLoc, dist, distParent])



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

    def match_frameRange_UI_Process(self, *args):

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        #get the body parts to match from the text scroll list
        parts = cmds.textScrollList(self.widgets["matchFrameRange_RigList"] , q = True, si = True)

        if parts == None:
            cmds.warning("Nothing selected in the parts list.")
            return

        #get match method
        button = cmds.iconTextRadioCollection(self.widgets["matchFrameRange_RadioCollection"], q = True, sl = True)
        method = cmds.iconTextRadioButton(button, q = True, ann = True)

        #get frame range
        start = cmds.intFieldGrp(self.widgets["matchFrameRange_FrameRange"], q = True, value1 = True)
        end = cmds.intFieldGrp(self.widgets["matchFrameRange_FrameRange"], q = True, value2 = True)


        #for frame in frame range, for each part selected in the list, run the match function
        for i in range(start, end + 1):
            cmds.currentTime(i)	    
            for part in parts:
                if part == character + ":" + "Right Leg":
                    if method == "fk":
                        self.match_singleFrame("leg", "r", "FK", "IK")
                    if method == "ik":
                        self.match_singleFrame("leg", "r", "IK", "FK")


                if part == character + ":" + "Left Leg":
                    if method == "fk":
                        self.match_singleFrame("leg", "l", "FK", "IK")
                    if method == "ik":
                        self.match_singleFrame("leg", "l", "IK", "FK")		



                if part == character + ":" + "Left Arm":
                    if method == "fk":
                        self.match_singleFrame("arm", "l", "FK", "IK")
                    if method == "ik":
                        self.match_singleFrame("arm", "l", "IK", "FK")		


                if part == character + ":" + "Right Arm":
                    if method == "fk":
                        self.match_singleFrame("arm", "r", "FK", "IK")
                    if method == "ik":
                        self.match_singleFrame("arm", "r", "IK", "FK")			



                if part == character + ":" + "Spine":
                    if method == "fk":
                        self.match_singleFrame("spine", None, "FK", "IK")
                    if method == "ik":
                        self.match_singleFrame("spine", None, "IK", "FK")		




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def match_frameRange_UI_Cancel(self, *args):

        cmds.deleteUI(self.widgets["matchFrameRange_Window"])


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def match_frameRange_UI(self, *args):

        if cmds.window("matchOverFrameRange_UI", exists = True):
            cmds.deleteUI("matchOverFrameRange_UI")

        character = cmds.symbolButton(self.widgets["activeCharacterThumb"], q = True, ann = True)

        self.widgets["matchFrameRange_Window"] = cmds.window("matchOverFrameRange_UI", w = 500, h = 300, sizeable = True, title = "Match Over Frame Range", titleBarMenu = False)

        #main layout
        self.widgets["matchFrameRange_MainLayout"] = cmds.formLayout(w = 500, h = 300)

        #create the UI elements we need

        #rig part list
        self.widgets["matchFrameRange_RigList"] = cmds.textScrollList(w = 200, h = 250, parent = self.widgets["matchFrameRange_MainLayout"], allowMultiSelection = True)

        #populate the list with the current character's limbs that can match
        for part in ["Left Arm", "Right Arm", "Left Leg", "Right Leg", "Spine"]:
            cmds.textScrollList(self.widgets["matchFrameRange_RigList"], edit = True, append = character + ":" + part)


        #frame range
        start = cmds.playbackOptions(q = True, min = True)
        end = cmds.playbackOptions(q = True, max = True)
        self.widgets["matchFrameRange_FrameRange"] = cmds.intFieldGrp(numberOfFields=2, label='Frame Range:', value1 = start, value2 = end, cw = [(1, 100), (2,80), (3, 80)] )


        #radio buttons for match method
        self.widgets["matchFrameRange_RadioCollection"] = cmds.iconTextRadioCollection()
        self.widgets["matchFrameRange_FkToIk"] = cmds.iconTextRadioButton( ann = "fk", select = True, st='iconOnly', image = self.mayaToolsDir + "/General/Icons/ART/fktoik_off.bmp", selectionImage = self.mayaToolsDir + "/General/Icons/ART/fktoik_on.bmp", w =  125, h = 50, collection = self.widgets["matchFrameRange_RadioCollection"], parent =  self.widgets["matchFrameRange_MainLayout"])
        self.widgets["matchFrameRange_IkToFk"] = cmds.iconTextRadioButton( ann = "ik", st='iconOnly', image = self.mayaToolsDir + "/General/Icons/ART/iktofk_off.bmp", selectionImage = self.mayaToolsDir + "/General/Icons/ART/iktofk_on.bmp", w =  125, h = 50, collection = self.widgets["matchFrameRange_RadioCollection"] , parent =  self.widgets["matchFrameRange_MainLayout"])


        #process button and cancel button
        self.widgets["matchFrameRange_Process"] = cmds.button(w = 125, h = 50, label = "Process", c = self.match_frameRange_UI_Process)
        self.widgets["matchFrameRange_Cancel"] = cmds.button(w = 125, h = 50, label = "Cancel", c = self.match_frameRange_UI_Cancel)


        #ik solve options
        label = cmds.text(label = "IK Solve Options:", font = "boldLabelFont")
        self.widgets["matchFR_RollSolveCB"] = cmds.checkBox(label = "Solve Foot Roll", v = False, parent = self.widgets["matchFrameRange_MainLayout"])
        self.widgets["matchFR_KneeSolveCB"] = cmds.checkBox(label = "Solve Knee Vectors", v = True, parent = self.widgets["matchFrameRange_MainLayout"])

        #place UI widgets
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_RigList"], 'left', 10), (self.widgets["matchFrameRange_RigList"], 'top', 25)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_FrameRange"], 'left', 210), (self.widgets["matchFrameRange_FrameRange"], 'top', 25)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_FkToIk"], 'left', 230), (self.widgets["matchFrameRange_FkToIk"], 'top', 75)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_IkToFk"], 'right', 10), (self.widgets["matchFrameRange_IkToFk"], 'top', 75)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_Process"], 'left', 230), (self.widgets["matchFrameRange_Process"], 'bottom', 25)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFrameRange_Cancel"], 'right', 10), (self.widgets["matchFrameRange_Cancel"], 'bottom', 25)])

        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(label, 'left', 230), (label, 'bottom', 140)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFR_RollSolveCB"], 'left', 230), (self.widgets["matchFR_RollSolveCB"], 'bottom', 115)])
        cmds.formLayout(self.widgets["matchFrameRange_MainLayout"], edit = True, af = [(self.widgets["matchFR_KneeSolveCB"], 'right', 10), (self.widgets["matchFR_KneeSolveCB"], 'bottom', 115)])


        #show the window
        cmds.showWindow(self.widgets["matchFrameRange_Window"])




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def control_scale_init(self, *args):

        #launch a simple UI with a slider to control the scale
        if cmds.window("controlScaleWindow", exists = True):
            cmds.deleteUI("controlScaleWindow")

        self.widgets["controlScaleWindow"] = cmds.window("controlScaleWindow", title = "CV Scale", w = 150, h = 50, sizeable = True, mnb = False, mxb = False)
        mainLayout = cmds.formLayout(w = 150, h = 50)

        smallerButton = cmds.button(label = "v", w = 50, h = 30, c = partial(self.control_scale_execute, .9))
        largerButton = cmds.button(label = "^", w = 50, h = 30, c = partial(self.control_scale_execute, 1.1))


        cmds.formLayout(mainLayout, edit = True, af = [(smallerButton, 'left', 25), (smallerButton, 'top', 10)])
        cmds.formLayout(mainLayout, edit = True, af = [(largerButton, 'right', 25), (largerButton, 'top', 10)])


        cmds.showWindow(self.widgets["controlScaleWindow"])




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def control_scale_execute(self, amount, *args):

        #get the value
        selection = cmds.ls(sl = True)
        cmds.select(clear = True)


        for each in selection:
            if each.find("anim") != -1:

                #select all cvs
                cmds.select(each + ".cv[*]", add = True)

        #set scale
        cmds.scale(amount, amount, amount,  relative = True, cp = True)

        #reselect
        cmds.select(clear = True)
        for each in selection:
            cmds.select(each, add = True)




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def animHelp(self, *args):

        cmds.launch(web = "https://docs.unrealengine.com/latest/INT/Engine/Content/Tools/MayaRiggingTool/RigTool_Animation/index.html")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def setup_ik_driven_fk_rig(self, *args):

        character = cmds.symbolButton('activeCharacterThumb', q = True, ann = True)

        #check to make sure FK wrist control is selected
        selection = cmds.ls(sl = True)[0]

        if selection.find(character + ":fk_wrist_") != -1:
            side = selection.partition(":fk_wrist_")[2].partition("_")[0]

            #duplicate FK arm joints
            upArm = cmds.duplicate(character + ":fk_upperarm_" + side, po = True, name = "ik_driver_fk_upperarm_" + side)[0]
            lowArm = cmds.duplicate(character + ":fk_lowerarm_" + side, po = True, name = "ik_driver_fk_lowerarm_" + side)[0]
            wrist = cmds.duplicate(character + ":fk_hand_" + side, po = True, name = "ik_driver_fk_hand_" + side)[0]

            cmds.parent(lowArm, upArm)
            cmds.parent(wrist, lowArm)


            #set preferred angle on elbow
            cmds.setAttr(lowArm + ".preferredAngleZ", -90)
            cmds.setAttr(upArm + ".v", 0)


            #create rp ik
            rpIkHandle = cmds.ikHandle(name = "ikdriver_fk_arm_ikHandle_" + side, solver = "ikRPsolver", sj = upArm, ee = wrist)[0]
            cmds.setAttr(rpIkHandle + ".v", 0)
            #parent ik hand under fk_wrist_r_anim
            cmds.select(rpIkHandle)

            #constrain fk controls to joints
            cmds.orientConstraint(upArm, character + ":fk_arm_" + side + "_anim", mo = True)
            cmds.orientConstraint(lowArm, character + ":fk_elbow_" + side + "_anim", mo = True)
            cmds.orientConstraint(wrist, character + ":fk_wrist_" + side + "_anim", mo = True)

            cmds.setToolTo( 'moveSuperContext' )

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def destroy_ik_driven_fk_rig(self, *args):

        character = cmds.symbolButton('activeCharacterThumb', q = True, ann = True)
        selected = cmds.ls(sl = True)[0]
        selectedSide = selected.partition("ikdriver_fk_arm_ikHandle_")[2]

        for side in ["l", "r"]:

            if cmds.objExists("ikdriver_fk_arm_ikHandle_" + side):
                cmds.setKeyframe(character + ":fk_arm_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_elbow_" + side + "_anim")
                cmds.setKeyframe(character + ":fk_wrist_" + side + "_anim")

                cmds.delete(["ikdriver_fk_arm_ikHandle_" + side, "ik_driver_fk_upperarm_" + side ])

        cmds.select(character + ":fk_wrist_" + selectedSide + "_anim")
        cmds.setToolTo("RotateSuperContext")

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def eulerFilterAll(self, *args):
        self.selectAll()
        cmds.selectKey()
        cmds.filterCurve()

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def eulerFilterSelected(self, *args):
        cmds.selectKey()
        cmds.filterCurve()

    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def toggleVisibilityOnSelectedControlGroups(self, name, *args):


        #get all controls below
        children = cmds.treeView(self.widgets[name + "_treeViewWidget"], q = True, children = args[0])


        for child in children:
            if cmds.objExists(name + ":" + child):
                try:
                    shape = cmds.listRelatives(name + ":" + child, shapes = True)[0]

                    visibility = cmds.getAttr(shape + ".v")
                    if visibility == True:
                        cmds.setAttr(shape + ".v", 0)
                    if visibility == False:
                        cmds.setAttr(shape + ".v", 1)		

                except:
                    pass



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
    def setupButtonAttrsOnControls(self):

        #when the UI is launched, we need to add an attribute to each control that tells us the corresponding button name
        characters = self.getCharacters()


        #add string attrs to controls
        for character in characters:

            #head
            try:
                if cmds.objExists(character + ":" + "head_fk_anim.buttonName"):
                    cmds.setAttr(character + ":" + "head_fk_anim.buttonName",self.widgets[character + "_headPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "head_fk_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "head_fk_anim.buttonName",self.widgets[character + "_headPickerButton"], type = "string")
            except:
                pass


            #neck1
            try:
                if cmds.objExists(character + ":" + "neck_01_fk_anim"):
                    if cmds.objExists(character + ":" + "neck_01_fk_anim.buttonName"):
                        cmds.setAttr(character + ":" + "neck_01_fk_anim.buttonName",self.widgets[character + "_neck1_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "neck_01_fk_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "neck_01_fk_anim.buttonName",self.widgets[character + "_neck1_PickerButton"], type = "string")

                else:

                    if cmds.objExists(character + ":" + "neck_fk_anim.buttonName"):
                        cmds.setAttr(character + ":" + "neck_fk_anim.buttonName",self.widgets[character + "_neck1_PickerButton"], type = "string")
                    else:
                        cmds.select(character + ":" + "neck_fk_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "neck_fk_anim.buttonName",self.widgets[character + "_neck1_PickerButton"], type = "string")

            except:
                pass


            #neck2
            try:
                if cmds.objExists(character + ":" + "neck_02_fk_anim"):
                    if cmds.objExists(character + ":" + "neck_02_fk_anim.buttonName"):
                        cmds.setAttr(character + ":" + "neck_02_fk_anim.buttonName",self.widgets[character + "_neck2_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "neck_02_fk_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "neck_02_fk_anim.buttonName",self.widgets[character + "_neck2_PickerButton"], type = "string")
            except:
                pass


            #neck3
            try:
                if cmds.objExists(character + ":" + "neck_03_fk_anim"):
                    if cmds.objExists(character + ":" + "neck_03_fk_anim.buttonName"):
                        cmds.setAttr(character + ":" + "neck_03_fk_anim.buttonName",self.widgets[character + "_neck3_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "neck_03_fk_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "neck_03_fk_anim.buttonName",self.widgets[character + "_neck3_PickerButton"], type = "string")
            except:
                pass


            #fk spine 1
            try:
                if cmds.objExists(character + ":" + "spine_01_anim"):
                    if cmds.objExists(character + ":" + "spine_01_anim.buttonName"):
                        cmds.setAttr(character + ":" + "spine_01_anim.buttonName",self.widgets[character + "_spine1_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "spine_01_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "spine_01_anim.buttonName",self.widgets[character + "_spine1_PickerButton"], type = "string")
            except:
                pass


            #fk spine 2
            try:
                if cmds.objExists(character + ":" + "spine_02_anim"):
                    if cmds.objExists(character + ":" + "spine_02_anim.buttonName"):
                        cmds.setAttr(character + ":" + "spine_02_anim.buttonName",self.widgets[character + "_spine2_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "spine_02_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "spine_02_anim.buttonName",self.widgets[character + "_spine2_PickerButton"], type = "string")
            except:
                pass


            #fk spine 3
            try:
                if cmds.objExists(character + ":" + "spine_03_anim"):
                    if cmds.objExists(character + ":" + "spine_03_anim.buttonName"):
                        cmds.setAttr(character + ":" + "spine_03_anim.buttonName",self.widgets[character + "_spine3_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "spine_03_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "spine_03_anim.buttonName",self.widgets[character + "_spine3_PickerButton"], type = "string")
            except:
                pass


            #fk spine 4
            try:
                if cmds.objExists(character + ":" + "spine_04_anim"):
                    if cmds.objExists(character + ":" + "spine_04_anim.buttonName"):
                        cmds.setAttr(character + ":" + "spine_04_anim.buttonName",self.widgets[character + "_spine4_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "spine_04_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "spine_04_anim.buttonName",self.widgets[character + "_spine4_PickerButton"], type = "string")
            except:
                pass



            #fk spine 5
            try:
                if cmds.objExists(character + ":" + "spine_05_anim"):
                    if cmds.objExists(character + ":" + "spine_05_anim.buttonName"):
                        cmds.setAttr(character + ":" + "spine_05_anim.buttonName",self.widgets[character + "_spine5_PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "spine_05_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "spine_05_anim.buttonName",self.widgets[character + "_spine5_PickerButton"], type = "string")

            except:
                pass


            #ik spine 
            try:
                if cmds.objExists(character + ":" + "mid_ik_anim"):
                    if cmds.objExists(character + ":" + "mid_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "mid_ik_anim.buttonName",self.widgets[character + "_ikSpineMidPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "mid_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "mid_ik_anim.buttonName",self.widgets[character + "_ikSpineMidPickerButton"], type = "string")
            except:
                pass

            try:
                if cmds.objExists(character + ":" + "chest_ik_anim"):
                    if cmds.objExists(character + ":" + "chest_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "chest_ik_anim.buttonName",self.widgets[character + "_ikSpineTopPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "chest_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "chest_ik_anim.buttonName",self.widgets[character + "_ikSpineTopPickerButton"], type = "string")

            except:
                pass



            #body
            try:
                if cmds.objExists(character + ":" + "body_anim.buttonName"):
                    cmds.setAttr(character + ":" + "body_anim.buttonName",self.widgets[character + "_bodyPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "body_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "body_anim.buttonName",self.widgets[character + "_bodyPickerButton"], type = "string")

            except:
                pass


            try:
                if cmds.objExists(character + ":" + "hip_anim.buttonName"):
                    cmds.setAttr(character + ":" + "hip_anim.buttonName",self.widgets[character + "_pelvisPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "hip_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "hip_anim.buttonName",self.widgets[character + "_pelvisPickerButton"], type = "string")
            except:
                pass


            #clavicles
            try:
                if cmds.objExists(character + ":" + "clavicle_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "clavicle_l_anim.buttonName",self.widgets[character + "_leftClavPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "clavicle_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "clavicle_l_anim.buttonName",self.widgets[character + "_leftClavPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "clavicle_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "clavicle_r_anim.buttonName",self.widgets[character + "_rightClavPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "clavicle_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "clavicle_r_anim.buttonName",self.widgets[character + "_rightClavPickerButton"], type = "string")
            except:
                pass


            #upper arms
            try:
                if cmds.objExists(character + ":" + "fk_arm_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_arm_l_anim.buttonName",self.widgets[character + "_leftShoulderPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_arm_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_arm_l_anim.buttonName",self.widgets[character + "_leftShoulderPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "fk_arm_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_arm_r_anim.buttonName",self.widgets[character + "_rightShoulderPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_arm_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_arm_r_anim.buttonName",self.widgets[character + "_rightShoulderPickerButton"], type = "string")

            except:
                pass



            #lower arms
            try:
                if cmds.objExists(character + ":" + "fk_elbow_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_elbow_l_anim.buttonName",self.widgets[character + "_leftElbowPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_elbow_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_elbow_l_anim.buttonName",self.widgets[character + "_leftElbowPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "fk_elbow_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_elbow_r_anim.buttonName",self.widgets[character + "_rightElbowPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_elbow_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_elbow_r_anim.buttonName",self.widgets[character + "_rightElbowPickerButton"], type = "string")

            except:
                pass



            #fk hands
            try:
                if cmds.objExists(character + ":" + "fk_wrist_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_wrist_l_anim.buttonName",self.widgets[character + "_leftHandPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_wrist_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_wrist_l_anim.buttonName",self.widgets[character + "_leftHandPickerButton"], type = "string")

            except:
                pass


            try:
                if cmds.objExists(character + ":" + "fk_wrist_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_wrist_r_anim.buttonName",self.widgets[character + "_rightHandPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_wrist_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_wrist_r_anim.buttonName",self.widgets[character + "_rightHandPickerButton"], type = "string")
            except:
                pass



            #ik elbows
            try:
                if cmds.objExists(character + ":" + "ik_elbow_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "ik_elbow_l_anim.buttonName",self.widgets[character + "_leftIkElbowPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_elbow_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_elbow_l_anim.buttonName",self.widgets[character + "_leftIkElbowPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ik_elbow_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "ik_elbow_r_anim.buttonName",self.widgets[character + "_rightIkElbowPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_elbow_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_elbow_r_anim.buttonName",self.widgets[character + "_rightIkElbowPickerButton"], type = "string")
            except:
                pass



            #ik hands
            try:
                if cmds.objExists(character + ":" + "ik_wrist_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "ik_wrist_l_anim.buttonName",self.widgets[character + "_leftIkHandPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_wrist_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_wrist_l_anim.buttonName",self.widgets[character + "_leftIkHandPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ik_wrist_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "ik_wrist_r_anim.buttonName",self.widgets[character + "_rightIkHandPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_wrist_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_wrist_r_anim.buttonName",self.widgets[character + "_rightIkHandPickerButton"], type = "string")
            except:
                pass


            #fk thighs
            try:
                if cmds.objExists(character + ":" + "fk_thigh_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_thigh_l_anim.buttonName",self.widgets[character + "_leftThighPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_thigh_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_thigh_l_anim.buttonName",self.widgets[character + "_leftThighPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "fk_thigh_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_thigh_r_anim.buttonName",self.widgets[character + "_rightThighPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_thigh_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_thigh_r_anim.buttonName",self.widgets[character + "_rightThighPickerButton"], type = "string")
            except:
                pass



            #fk knees
            try:
                if cmds.objExists(character + ":" + "fk_calf_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_calf_l_anim.buttonName",self.widgets[character + "_leftFkKneePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_calf_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_calf_l_anim.buttonName",self.widgets[character + "_leftFkKneePickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "fk_calf_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_calf_r_anim.buttonName",self.widgets[character + "_rightFkKneePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_calf_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_calf_r_anim.buttonName",self.widgets[character + "_rightFkKneePickerButton"], type = "string")
            except:
                pass




            #fk ankles
            try:
                if cmds.objExists(character + ":" + "fk_foot_l_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_foot_l_anim.buttonName",self.widgets[character + "_leftFkAnklePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_foot_l_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_foot_l_anim.buttonName",self.widgets[character + "_leftFkAnklePickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "fk_foot_r_anim.buttonName"):
                    cmds.setAttr(character + ":" + "fk_foot_r_anim.buttonName",self.widgets[character + "_rightFkAnklePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "fk_foot_r_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "fk_foot_r_anim.buttonName",self.widgets[character + "_rightFkAnklePickerButton"], type = "string")

            except:
                pass



            #fk ball joints
            try:
                if cmds.objExists(character + ":" + "fk_ball_l_anim"):
                    if cmds.objExists(character + ":" + "fk_ball_l_anim.buttonName"):
                        cmds.setAttr(character + ":" + "fk_ball_l_anim.buttonName",self.widgets[character + "_leftFkBallPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "fk_ball_l_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "fk_ball_l_anim.buttonName",self.widgets[character + "_leftFkBallPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "fk_ball_r_anim"):
                    if cmds.objExists(character + ":" + "fk_ball_r_anim.buttonName"):
                        cmds.setAttr(character + ":" + "fk_ball_r_anim.buttonName",self.widgets[character + "_rightFkBallPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "fk_ball_r_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "fk_ball_r_anim.buttonName",self.widgets[character + "_rightFkBallPickerButton"], type = "string")
            except:
                pass





            #ik feet
            try:
                if cmds.objExists(character + ":" + "ik_foot_anim_l.buttonName"):
                    cmds.setAttr(character + ":" + "ik_foot_anim_l.buttonName",self.widgets[character + "_leftIkFootPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_foot_anim_l")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_foot_anim_l.buttonName",self.widgets[character + "_leftIkFootPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ik_foot_anim_r.buttonName"):
                    cmds.setAttr(character + ":" + "ik_foot_anim_r.buttonName",self.widgets[character + "_rightIkFootPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "ik_foot_anim_r")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "ik_foot_anim_r.buttonName",self.widgets[character + "_rightIkFootPickerButton"], type = "string")
            except:
                pass



            #ik heels
            try:
                if cmds.objExists(character + ":" + "heel_ctrl_l.buttonName"):
                    cmds.setAttr(character + ":" + "heel_ctrl_l.buttonName",self.widgets[character + "_leftIkHeelPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "heel_ctrl_l")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "heel_ctrl_l.buttonName",self.widgets[character + "_leftIkHeelPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "heel_ctrl_r.buttonName"):
                    cmds.setAttr(character + ":" + "heel_ctrl_r.buttonName",self.widgets[character + "_rightIkHeelPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "heel_ctrl_r")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "heel_ctrl_r.buttonName",self.widgets[character + "_rightIkHeelPickerButton"], type = "string")
            except:
                pass



            #ik toe wiggles
            try:
                if cmds.objExists(character + ":" + "toe_wiggle_ctrl_l.buttonName"):
                    cmds.setAttr(character + ":" + "toe_wiggle_ctrl_l.buttonName",self.widgets[character + "_leftIkToeWigglePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "toe_wiggle_ctrl_l")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "toe_wiggle_ctrl_l.buttonName",self.widgets[character + "_leftIkToeWigglePickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "toe_wiggle_ctrl_r.buttonName"):
                    cmds.setAttr(character + ":" + "toe_wiggle_ctrl_r.buttonName",self.widgets[character + "_rightIkToeWigglePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "toe_wiggle_ctrl_r")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "toe_wiggle_ctrl_r.buttonName",self.widgets[character + "_rightIkToeWigglePickerButton"], type = "string")
            except:
                pass



            #ik toes
            try:
                if cmds.objExists(character + ":" + "toe_tip_ctrl_l.buttonName"):
                    cmds.setAttr(character + ":" + "toe_tip_ctrl_l.buttonName",self.widgets[character + "_leftIkToePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "toe_tip_ctrl_l")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "toe_tip_ctrl_l.buttonName",self.widgets[character + "_leftIkToePickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "toe_tip_ctrl_r.buttonName"):
                    cmds.setAttr(character + ":" + "toe_tip_ctrl_r.buttonName",self.widgets[character + "_rightIkToePickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "toe_tip_ctrl_r")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "toe_tip_ctrl_r.buttonName",self.widgets[character + "_rightIkToePickerButton"], type = "string")
            except:
                pass



            #master, offset, root
            try:
                if cmds.objExists(character + ":" + "master_anim.buttonName"):
                    cmds.setAttr(character + ":" + "master_anim.buttonName",self.widgets[character + "_masterPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "master_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "master_anim.buttonName",self.widgets[character + "_masterPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "offset_anim.buttonName"):
                    cmds.setAttr(character + ":" + "offset_anim.buttonName",self.widgets[character + "_offsetPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "offset_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "offset_anim.buttonName",self.widgets[character + "_offsetPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "root_anim.buttonName"):
                    cmds.setAttr(character + ":" + "root_anim.buttonName",self.widgets[character + "_rootPickerButton"], type = "string")

                else:
                    cmds.select(character + ":" + "root_anim")
                    cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                    cmds.setAttr(character + ":" + "root_anim.buttonName",self.widgets[character + "_rootPickerButton"], type = "string")
            except:
                pass



            #upper arm rolls
            try:
                if cmds.objExists(character + ":" + "upperarm_l_twist_anim"):
                    if cmds.objExists(character + ":" + "upperarm_l_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_l_twist_anim.buttonName",self.widgets[character + "_leftArmRollPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_l_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_l_twist_anim.buttonName",self.widgets[character + "_leftArmRollPickerButton"], type = "string")

            except:
                pass


            try:
                if cmds.objExists(character + ":" + "upperarm_l_twist_2_anim"):
                    if cmds.objExists(character + ":" + "upperarm_l_twist_2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_l_twist_2_anim.buttonName",self.widgets[character + "_leftArmRoll2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_l_twist_2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_l_twist_2_anim.buttonName",self.widgets[character + "_leftArmRoll2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "upperarm_l_twist_3_anim"):
                    if cmds.objExists(character + ":" + "upperarm_l_twist_3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_l_twist_3_anim.buttonName",self.widgets[character + "_leftArmRoll3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_l_twist_3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_l_twist_3_anim.buttonName",self.widgets[character + "_leftArmRoll3PickerButton"], type = "string")
            except:
                pass





            try:
                if cmds.objExists(character + ":" + "upperarm_r_twist_anim"):
                    if cmds.objExists(character + ":" + "upperarm_r_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_r_twist_anim.buttonName",self.widgets[character + "_rightArmRollPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_r_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_r_twist_anim.buttonName",self.widgets[character + "_rightArmRollPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "upperarm_r_twist_2_anim"):
                    if cmds.objExists(character + ":" + "upperarm_r_twist_2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_r_twist_2_anim.buttonName",self.widgets[character + "_rightArmRoll2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_r_twist_2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_r_twist_2_anim.buttonName",self.widgets[character + "_rightArmRoll2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "upperarm_r_twist_3_anim"):
                    if cmds.objExists(character + ":" + "upperarm_r_twist_3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "upperarm_r_twist_3_anim.buttonName",self.widgets[character + "_rightArmRoll3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "upperarm_r_twist_3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "upperarm_r_twist_3_anim.buttonName",self.widgets[character + "_rightArmRoll3PickerButton"], type = "string")
            except:
                pass




            #lower arm rolls
            try:
                if cmds.objExists(character + ":" + "lowerarm_l_twist_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_l_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_l_twist_anim.buttonName",self.widgets[character + "_leftForeTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_l_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_l_twist_anim.buttonName",self.widgets[character + "_leftForeTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "lowerarm_l_twist2_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_l_twist2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_l_twist2_anim.buttonName",self.widgets[character + "_leftForeTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_l_twist2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_l_twist2_anim.buttonName",self.widgets[character + "_leftForeTwist2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "lowerarm_l_twist3_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_l_twist3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_l_twist3_anim.buttonName",self.widgets[character + "_leftForeTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_l_twist3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_l_twist3_anim.buttonName",self.widgets[character + "_leftForeTwist3PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "lowerarm_r_twist_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_r_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_r_twist_anim.buttonName",self.widgets[character + "_rightForeTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_r_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_r_twist_anim.buttonName",self.widgets[character + "_rightForeTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "lowerarm_r_twist2_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_r_twist2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_r_twist2_anim.buttonName",self.widgets[character + "_rightForeTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_r_twist2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_r_twist2_anim.buttonName",self.widgets[character + "_rightForeTwist2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "lowerarm_r_twist3_anim"):
                    if cmds.objExists(character + ":" + "lowerarm_r_twist3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "lowerarm_r_twist3_anim.buttonName",self.widgets[character + "_rightForeTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "lowerarm_r_twist3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "lowerarm_r_twist3_anim.buttonName",self.widgets[character + "_rightForeTwist3PickerButton"], type = "string")
            except:
                pass




            #thigh twists
            try:
                if cmds.objExists(character + ":" + "l_thigh_twist_01_anim"):
                    if cmds.objExists(character + ":" + "l_thigh_twist_01_anim.buttonName"):
                        cmds.setAttr(character + ":" + "l_thigh_twist_01_anim.buttonName",self.widgets[character + "_leftThighTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "l_thigh_twist_01_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "l_thigh_twist_01_anim.buttonName",self.widgets[character + "_leftThighTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "l_thigh_twist_02_anim"):
                    if cmds.objExists(character + ":" + "l_thigh_twist_02_anim.buttonName"):
                        cmds.setAttr(character + ":" + "l_thigh_twist_02_anim.buttonName",self.widgets[character + "_leftThighTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "l_thigh_twist_02_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "l_thigh_twist_02_anim.buttonName",self.widgets[character + "_leftThighTwist2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "l_thigh_twist_03_anim"):
                    if cmds.objExists(character + ":" + "l_thigh_twist_03_anim.buttonName"):
                        cmds.setAttr(character + ":" + "l_thigh_twist_03_anim.buttonName",self.widgets[character + "_leftThighTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "l_thigh_twist_03_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "l_thigh_twist_03_anim.buttonName",self.widgets[character + "_leftThighTwist3PickerButton"], type = "string")
            except:
                pass





            try:
                if cmds.objExists(character + ":" + "r_thigh_twist_01_anim"):
                    if cmds.objExists(character + ":" + "r_thigh_twist_01_anim.buttonName"):
                        cmds.setAttr(character + ":" + "r_thigh_twist_01_anim.buttonName",self.widgets[character + "_rightThighTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "r_thigh_twist_01_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "r_thigh_twist_01_anim.buttonName",self.widgets[character + "_rightThighTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "r_thigh_twist_02_anim"):
                    if cmds.objExists(character + ":" + "r_thigh_twist_02_anim.buttonName"):
                        cmds.setAttr(character + ":" + "r_thigh_twist_02_anim.buttonName",self.widgets[character + "_rightThighTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "r_thigh_twist_02_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "r_thigh_twist_02_anim.buttonName",self.widgets[character + "_rightThighTwist2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "r_thigh_twist_03_anim"):
                    if cmds.objExists(character + ":" + "r_thigh_twist_03_anim.buttonName"):
                        cmds.setAttr(character + ":" + "r_thigh_twist_03_anim.buttonName",self.widgets[character + "_rightThighTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "r_thigh_twist_03_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "r_thigh_twist_03_anim.buttonName",self.widgets[character + "_rightThighTwist3PickerButton"], type = "string")

            except:
                pass




            #calf twists
            try:
                if cmds.objExists(character + ":" + "calf_l_twist_anim"):
                    if cmds.objExists(character + ":" + "calf_l_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_l_twist_anim.buttonName",self.widgets[character + "_leftCalfTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_l_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_l_twist_anim.buttonName",self.widgets[character + "_leftCalfTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "calf_l_twist2_anim"):
                    if cmds.objExists(character + ":" + "calf_l_twist2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_l_twist2_anim.buttonName",self.widgets[character + "_leftCalfTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_l_twist2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_l_twist2_anim.buttonName",self.widgets[character + "_leftCalfTwist2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "calf_l_twist3_anim"):
                    if cmds.objExists(character + ":" + "calf_l_twist3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_l_twist3_anim.buttonName",self.widgets[character + "_leftCalfTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_l_twist3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_l_twist3_anim.buttonName",self.widgets[character + "_leftCalfTwist3PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "calf_r_twist_anim"):
                    if cmds.objExists(character + ":" + "calf_r_twist_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_r_twist_anim.buttonName",self.widgets[character + "_rightCalfTwistPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_r_twist_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_r_twist_anim.buttonName",self.widgets[character + "_rightCalfTwistPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "calf_r_twist2_anim"):
                    if cmds.objExists(character + ":" + "calf_r_twist2_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_r_twist2_anim.buttonName",self.widgets[character + "_rightCalfTwist2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_r_twist2_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_r_twist2_anim.buttonName",self.widgets[character + "_rightCalfTwist2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "calf_r_twist3_anim"):
                    if cmds.objExists(character + ":" + "calf_r_twist3_anim.buttonName"):
                        cmds.setAttr(character + ":" + "calf_r_twist3_anim.buttonName",self.widgets[character + "_rightCalfTwist3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "calf_r_twist3_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "calf_r_twist3_anim.buttonName",self.widgets[character + "_rightCalfTwist3PickerButton"], type = "string")

            except:
                pass





            #left pinky finger
            try:
                if cmds.objExists(character + ":" + "pinky_metacarpal_ctrl_l"):
                    if cmds.objExists(character + ":" + "pinky_metacarpal_ctrl_l.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftPinkyMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_metacarpal_ctrl_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftPinkyMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_1_l"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_1_l.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftPinky1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_1_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftPinky1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_2_l"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_2_l.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftPinky2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_2_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftPinky2PickerButton"], type = "string")

            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_3_l"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_3_l.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftPinky3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_3_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftPinky3PickerButton"], type = "string")

            except:
                pass



            #left ring finger
            try:
                if cmds.objExists(character + ":" + "ring_metacarpal_ctrl_l"):
                    if cmds.objExists(character + ":" + "ring_metacarpal_ctrl_l.buttonName"):
                        cmds.setAttr(character + ":" + "ring_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftRingMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_metacarpal_ctrl_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftRingMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_1_l"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_1_l.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftRing1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_1_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftRing1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_2_l"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_2_l.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftRing2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_2_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftRing2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_3_l"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_3_l.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftRing3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_3_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftRing3PickerButton"], type = "string")
            except:
                pass



            #left middle finger
            try:
                if cmds.objExists(character + ":" + "middle_metacarpal_ctrl_l"):
                    if cmds.objExists(character + ":" + "middle_metacarpal_ctrl_l.buttonName"):
                        cmds.setAttr(character + ":" + "middle_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftMiddleMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_metacarpal_ctrl_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftMiddleMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_1_l"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_1_l.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftMiddle1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_1_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftMiddle1PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_2_l"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_2_l.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftMiddle2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_2_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftMiddle2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_3_l"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_3_l.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftMiddle3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_3_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftMiddle3PickerButton"], type = "string")
            except:
                pass


            #left index finger
            try:
                if cmds.objExists(character + ":" + "index_metacarpal_ctrl_l"):
                    if cmds.objExists(character + ":" + "index_metacarpal_ctrl_l.buttonName"):
                        cmds.setAttr(character + ":" + "index_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftIndexMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_metacarpal_ctrl_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_metacarpal_ctrl_l.buttonName",self.widgets[character + "_leftIndexMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_1_l"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_1_l.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftIndex1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_1_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftIndex1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_2_l"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_2_l.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftIndex2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_2_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftIndex2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_3_l"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_3_l.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftIndex3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_3_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftIndex3PickerButton"], type = "string")
            except:
                pass



            #left thumb finger
            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_1_l"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_1_l.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftThumb1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_1_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_1_l.buttonName",self.widgets[character + "_leftThumb1PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_2_l"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_2_l.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftThumb2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_2_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_2_l.buttonName",self.widgets[character + "_leftThumb2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_3_l"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_3_l.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftThumb3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_3_l")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_3_l.buttonName",self.widgets[character + "_leftThumb3PickerButton"], type = "string")
            except:
                pass



            #left IK fingers
            try:
                if cmds.objExists(character + ":" + "index_l_ik_anim"):
                    if cmds.objExists(character + ":" + "index_l_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "index_l_ik_anim.buttonName",self.widgets[character + "_leftIndexFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_l_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_l_ik_anim.buttonName",self.widgets[character + "_leftIndexFingerIKPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "middle_l_ik_anim"):
                    if cmds.objExists(character + ":" + "middle_l_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "middle_l_ik_anim.buttonName",self.widgets[character + "_leftMiddleFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_l_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_l_ik_anim.buttonName",self.widgets[character + "_leftMiddleFingerIKPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_l_ik_anim"):
                    if cmds.objExists(character + ":" + "ring_l_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "ring_l_ik_anim.buttonName",self.widgets[character + "_leftRingFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_l_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_l_ik_anim.buttonName",self.widgets[character + "_leftRingFingerIKPickerButton"], type = "string")

            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_l_ik_anim"):
                    if cmds.objExists(character + ":" + "pinky_l_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_l_ik_anim.buttonName",self.widgets[character + "_leftPinkyFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_l_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_l_ik_anim.buttonName",self.widgets[character + "_leftPinkyFingerIKPickerButton"], type = "string")

            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_l_ik_anim"):
                    if cmds.objExists(character + ":" + "thumb_l_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_l_ik_anim.buttonName",self.widgets[character + "_leftThumbFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_l_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_l_ik_anim.buttonName",self.widgets[character + "_leftThumbFingerIKPickerButton"], type = "string")
            except:
                pass




            #left IK finger PVs
            try:
                if cmds.objExists(character + ":" + "index_l_ik_anim"):
                    if cmds.objExists(character + ":" + "index_l_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "index_l_poleVector.buttonName",self.widgets[character + "_leftIndexIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_l_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_l_poleVector.buttonName",self.widgets[character + "_leftIndexIkPvPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "middle_l_ik_anim"):
                    if cmds.objExists(character + ":" + "middle_l_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "middle_l_poleVector.buttonName",self.widgets[character + "_leftMiddleIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_l_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_l_poleVector.buttonName",self.widgets[character + "_leftMiddleIkPvPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_l_ik_anim"):
                    if cmds.objExists(character + ":" + "ring_l_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "ring_l_poleVector.buttonName",self.widgets[character + "_leftRingIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_l_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_l_poleVector.buttonName",self.widgets[character + "_leftRingIkPvPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_l_ik_anim"):
                    if cmds.objExists(character + ":" + "pinky_l_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_l_poleVector.buttonName",self.widgets[character + "_leftPinkyIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_l_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_l_poleVector.buttonName",self.widgets[character + "_leftPinkyIkPvPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_l_ik_anim"):
                    if cmds.objExists(character + ":" + "thumb_l_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_l_poleVector.buttonName",self.widgets[character + "_leftThumbIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_l_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_l_poleVector.buttonName",self.widgets[character + "_leftThumbIkPvPickerButton"], type = "string")

            except:
                pass



            try:
                if cmds.objExists(character + ":" + "l_global_ik_anim"):
                    if cmds.objExists(character + ":" + "l_global_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "l_global_ik_anim.buttonName",self.widgets[character + "_leftIkGlobalCtrlPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "l_global_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "l_global_ik_anim.buttonName",self.widgets[character + "_leftIkGlobalCtrlPickerButton"], type = "string")
            except:
                pass






            #right pinky finger
            try:
                if cmds.objExists(character + ":" + "pinky_metacarpal_ctrl_r"):
                    if cmds.objExists(character + ":" + "pinky_metacarpal_ctrl_r.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightPinkyMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_metacarpal_ctrl_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightPinkyMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_1_r"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_1_r.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightPinky1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_1_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightPinky1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_2_r"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_2_r.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightPinky2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_2_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightPinky2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_3_r"):
                    if cmds.objExists(character + ":" + "pinky_finger_fk_ctrl_3_r.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightPinky3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_finger_fk_ctrl_3_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightPinky3PickerButton"], type = "string")
            except:
                pass




            #right ring finger
            try:
                if cmds.objExists(character + ":" + "ring_metacarpal_ctrl_r"):
                    if cmds.objExists(character + ":" + "ring_metacarpal_ctrl_r.buttonName"):
                        cmds.setAttr(character + ":" + "ring_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightRingMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_metacarpal_ctrl_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightRingMetacarpalPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_1_r"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_1_r.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightRing1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_1_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightRing1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_2_r"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_2_r.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightRing2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_2_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightRing2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_3_r"):
                    if cmds.objExists(character + ":" + "ring_finger_fk_ctrl_3_r.buttonName"):
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightRing3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_finger_fk_ctrl_3_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightRing3PickerButton"], type = "string")
            except:
                pass


            #right middle finger
            try:
                if cmds.objExists(character + ":" + "middle_metacarpal_ctrl_r"):
                    if cmds.objExists(character + ":" + "middle_metacarpal_ctrl_r.buttonName"):
                        cmds.setAttr(character + ":" + "middle_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightMiddleMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_metacarpal_ctrl_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightMiddleMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_1_r"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_1_r.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightMiddle1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_1_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightMiddle1PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_2_r"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_2_r.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightMiddle2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_2_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightMiddle2PickerButton"], type = "string")

            except:
                pass



            try:
                if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_3_r"):
                    if cmds.objExists(character + ":" + "middle_finger_fk_ctrl_3_r.buttonName"):
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightMiddle3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_finger_fk_ctrl_3_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightMiddle3PickerButton"], type = "string")
            except:
                pass



            #right index finger
            try:
                if cmds.objExists(character + ":" + "index_metacarpal_ctrl_r"):
                    if cmds.objExists(character + ":" + "index_metacarpal_ctrl_r.buttonName"):
                        cmds.setAttr(character + ":" + "index_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightIndexMetacarpalPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_metacarpal_ctrl_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_metacarpal_ctrl_r.buttonName",self.widgets[character + "_rightIndexMetacarpalPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_1_r"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_1_r.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightIndex1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_1_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightIndex1PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_2_r"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_2_r.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightIndex2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_2_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightIndex2PickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "index_finger_fk_ctrl_3_r"):
                    if cmds.objExists(character + ":" + "index_finger_fk_ctrl_3_r.buttonName"):
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightIndex3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_finger_fk_ctrl_3_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightIndex3PickerButton"], type = "string")
            except:
                pass



            #right thumb finger
            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_1_r"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_1_r.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightThumb1PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_1_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_1_r.buttonName",self.widgets[character + "_rightThumb1PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_2_r"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_2_r.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightThumb2PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_2_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_2_r.buttonName",self.widgets[character + "_rightThumb2PickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_3_r"):
                    if cmds.objExists(character + ":" + "thumb_finger_fk_ctrl_3_r.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightThumb3PickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_finger_fk_ctrl_3_r")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_finger_fk_ctrl_3_r.buttonName",self.widgets[character + "_rightThumb3PickerButton"], type = "string")
            except:
                pass




            #right IK fingers
            try:
                if cmds.objExists(character + ":" + "index_r_ik_anim"):
                    if cmds.objExists(character + ":" + "index_r_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "index_r_ik_anim.buttonName",self.widgets[character + "_rightIndexFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_r_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_r_ik_anim.buttonName",self.widgets[character + "_rightIndexFingerIKPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "middle_r_ik_anim"):
                    if cmds.objExists(character + ":" + "middle_r_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "middle_r_ik_anim.buttonName",self.widgets[character + "_rightMiddleFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_r_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_r_ik_anim.buttonName",self.widgets[character + "_rightMiddleFingerIKPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "ring_r_ik_anim"):
                    if cmds.objExists(character + ":" + "ring_r_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "ring_r_ik_anim.buttonName",self.widgets[character + "_rightRingFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_r_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_r_ik_anim.buttonName",self.widgets[character + "_rightRingFingerIKPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "pinky_r_ik_anim"):
                    if cmds.objExists(character + ":" + "pinky_r_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_r_ik_anim.buttonName",self.widgets[character + "_rightPinkyFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_r_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_r_ik_anim.buttonName",self.widgets[character + "_rightPinkyFingerIKPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_r_ik_anim"):
                    if cmds.objExists(character + ":" + "thumb_r_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_r_ik_anim.buttonName",self.widgets[character + "_rightThumbFingerIKPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_r_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_r_ik_anim.buttonName",self.widgets[character + "_rightThumbFingerIKPickerButton"], type = "string")
            except:
                pass



            #right IK finger PVs
            try:
                if cmds.objExists(character + ":" + "index_r_ik_anim"):
                    if cmds.objExists(character + ":" + "index_r_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "index_r_poleVector.buttonName",self.widgets[character + "_rightIndexIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "index_r_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "index_r_poleVector.buttonName",self.widgets[character + "_rightIndexIkPvPickerButton"], type = "string")
            except:
                pass


            try:        
                if cmds.objExists(character + ":" + "middle_r_ik_anim"):
                    if cmds.objExists(character + ":" + "middle_r_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "middle_r_poleVector.buttonName",self.widgets[character + "_rightMiddleIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "middle_r_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "middle_r_poleVector.buttonName",self.widgets[character + "_rightMiddleIkPvPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "ring_r_ik_anim"):
                    if cmds.objExists(character + ":" + "ring_r_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "ring_r_poleVector.buttonName",self.widgets[character + "_rightRingIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "ring_r_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "ring_r_poleVector.buttonName",self.widgets[character + "_rightRingIkPvPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "pinky_r_ik_anim"):
                    if cmds.objExists(character + ":" + "pinky_r_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "pinky_r_poleVector.buttonName",self.widgets[character + "_rightPinkyIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "pinky_r_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "pinky_r_poleVector.buttonName",self.widgets[character + "_rightPinkyIkPvPickerButton"], type = "string")
            except:
                pass


            try:
                if cmds.objExists(character + ":" + "thumb_r_ik_anim"):
                    if cmds.objExists(character + ":" + "thumb_r_poleVector.buttonName"):
                        cmds.setAttr(character + ":" + "thumb_r_poleVector.buttonName",self.widgets[character + "_rightThumbIkPvPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "thumb_r_poleVector")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "thumb_r_poleVector.buttonName",self.widgets[character + "_rightThumbIkPvPickerButton"], type = "string")
            except:
                pass



            try:
                if cmds.objExists(character + ":" + "r_global_ik_anim"):
                    if cmds.objExists(character + ":" + "r_global_ik_anim.buttonName"):
                        cmds.setAttr(character + ":" + "r_global_ik_anim.buttonName",self.widgets[character + "_rightIkGlobalCtrlPickerButton"], type = "string")

                    else:
                        cmds.select(character + ":" + "r_global_ik_anim")
                        cmds.addAttr(ln = "buttonName", dt = "string", keyable = False)
                        cmds.setAttr(character + ":" + "r_global_ik_anim.buttonName",self.widgets[character + "_rightIkGlobalCtrlPickerButton"], type = "string")
            except:
                pass


