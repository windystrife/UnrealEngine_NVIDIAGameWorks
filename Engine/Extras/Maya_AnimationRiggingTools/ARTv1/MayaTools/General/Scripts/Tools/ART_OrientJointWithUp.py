import maya.cmds as cmds
from functools import partial
import pymel.core as pm

#Select the joint you want to orient, then the child you want it to be aimed at, and then the object to use as the upVector
#CRA NOTE: OptionVars are not retaining their settings when you re-open the UI.  They always reset to the default.  Figure out how to make them remember.
#--------------------------------------------------------------------------------------------------------------------
#aimAxis        = axis to point at the child bone.
#upAxis         = axis to point at the locator bone.
#heirarchy      = Should this be applied using the heirarchy or only to the selected joints?
#customChildInt = Do we use a custom child?   When set to 1, this will tell it to use the named joint in customChild rather than popping open a window when more than 1 child joint exists.
#customChild    = The joint to use as the aim joint when customChildInt is set to 1.  If no custom child is defined it will either use the only child available or if there are multiples it will bring up a ui to let the user choose which child to use.
# IMPORTANT NOTE: When calling this script directly and not using the UI, it is reccommended to have heirarchy set to 0.  If you try to orient an entire heirarchy we can not guarantee that it will pick the correct child unless you are 100% positive that there are no parents with multiple children.
#----------------------------------------------------------------------------------------
# This def finds the joint we want to aim (jointToAim), the joint to be aimed at (aimTarget), and the up node (upNode) based on the selection.  IF heirarchy is checked, then it uses the findChild def to find the joint to be aimed at.
def setOrientWithUpVector(aimAxis, upAxis, heirarchy, customChildInt, customChild):
    # THESE ARE THE 3 NODES WE NEED TO DEFINE!!
    jointToAim = None
    aimTarget = None
    upNode = None

    #Find the selected joint and upVector nodes from the selection
    # CRA NOTE: Could add some more logic here later to check to see if the first 2 selected are joints and that the 2nd is a relative of the first.
    selected = cmds.ls(sl=True)
    if len(selected) != 2:
        cmds.confirmDialog(icon = "warning!!", title = "Orient Joint with Up Tool", message = "You must select exactly 2 objects.  Joint you want to orient and then the object to use as the upVector.")
        return
    else:
        jointToAim = selected[0]
        upNode = selected[1]

        # Find the joints that we need to apply the orient script to.
        if heirarchy == 0:
            # find only the immediate children of jointToAim
            allChildren = cmds.listRelatives(jointToAim, c=True, type="joint")
            #print "\nAll Children..."
            #print allChildren

            if customChildInt == 1:
                aimTarget = customChild
                if allChildren != None:
                    doOrientJoint(jointToAim, aimTarget, upNode, aimAxis, upAxis, allChildren)
                else:
                    cmds.joint(jointToAim, e=True, oj="none", secondaryAxisOrient="yup", ch=True, zso=True)
            elif allChildren != None:
                if len(allChildren) == 1:
                    aimTarget = allChildren[0]
                else:
                    aimTarget = cmds.layoutDialog(t="Child Joints List", ui=partial(findChild, jointToAim))
                #print "\nPRINTING JOINTS LIST---- "
                #print "jointToAim: "+jointToAim
                #print "aimTarget: "+aimTarget
                #print "upNode   : "+upNode   
                #print "----DONE PRINTING JOINTS LIST"
                doOrientJoint(jointToAim, aimTarget, upNode, aimAxis, upAxis, allChildren)
            else:
                cmds.joint(jointToAim, e=True, oj="none", secondaryAxisOrient="yup", ch=True, zso=True)


        if heirarchy == 1:
            # find all of the joints we want to use as the jointToAim
            cmds.select(jointToAim, hi=True)
            allJointsToAim = cmds.ls(sl=True, type="joint")
            #print "\nALL Joints To Aim..."
            #print allJointsToAim

            for one in allJointsToAim:
                jointToAim = one
                #print "\nCURRENT JOINT..."
                #print jointToAim
                
                # find only the immediate children of jointToAim
                allChildren = cmds.listRelatives(jointToAim, c=True, type="joint")
                #print "\nAll Children..."
                #print allChildren

                if allChildren != None:
                    if len(allChildren) == 1:
                        aimTarget = allChildren[0]
                    else:
                        aimTarget = cmds.layoutDialog(t="Child Joints List for: "+jointToAim, ui=partial(findChild, jointToAim))
                   
                    #print "\nPRINTING JOINTS LIST---- "
                    #print "jointToAim: "+jointToAim
                    #print "aimTarget: "+aimTarget
                    #print "upNode: "+upNode   
                    #print "----DONE PRINTING JOINTS LIST"
                    doOrientJoint(jointToAim, aimTarget, upNode, aimAxis, upAxis, allChildren)
                else:
                    # Set the joint orient to none so that it just takes the orientation of its parent.  This is only for the leaf joints in the heirarchy.
                    cmds.joint(jointToAim, e=True, oj="none", secondaryAxisOrient="yup", ch=True, zso=True)

#----------------------------------------------------------------------------------------
# This def is only used by the setOrientWithUpVector def IF the heirarchy check box is set.  It creates a UI that outputs the child that the user selects in a UI to be used as the aim child for joints with more than 1 child.
def findChild(jointToAim):
    #print "\nFIND CHILD START----->" 

    allChildren = cmds.listRelatives(jointToAim, c=True, type="joint")

    form = cmds.setParent(q=True)
    cmds.formLayout(form, e=True, width=200, height=300)
    
    tsl = cmds.textScrollList("childJointList", nr=10, ams=1, dcc="pm.runtime.ShowAttributeEditorOrChannelBox()")
    cmds.textScrollList(tsl, e=True, sc=partial(selectItem, tsl))
    for s in allChildren:
        cmds.textScrollList(tsl, e=True, a=s, sii=1) 
    cmds.select(cmds.textScrollList(tsl, q=True, si=True))
    txt = cmds.text("childText", l="Select the child joint that you want the aim axis to point at and press continue.")
    butt = cmds.button("continueButton", l="Continue", c=partial(getTextScrollListName, tsl))
    cmds.formLayout(form, e=True, 
        af=[(tsl, 'top', 1), (tsl, 'left', 1), (tsl, 'right', 1), (txt, 'left', 1), (txt, 'right', 1), (butt, 'left', 1), (butt, 'right', 1), (butt, 'bottom', 1)], 
        ac=[(tsl, 'bottom', 1, txt), (txt, 'bottom', 1, butt)])

def selectItem(textScrollListName):
    cmds.select(cmds.textScrollList(textScrollListName, q=True, si=True))
    
def getTextScrollListName(textScrollListName, *args):
    cmds.layoutDialog(dismiss=cmds.textScrollList(textScrollListName, q=True, si=True)[0])

#----------------------------------------------------------------------------------------
def doOrientJoint (jointToAim, aimTarget, upNode, aimAxis, upAxis, allChildren):
    
    print jointToAim
    print aimTarget
    print upNode
    print "upAxis: " + upAxis 
    print "aimAxis: " + aimAxis
    print allChildren

    Coords_up = []
    if upAxis == "x":
        Coords_up = [1, 0, 0]
    if upAxis == "-x":
        Coords_up = [-1, 0, 0]
    if upAxis == "y":
        Coords_up = [0, 1, 0]
    if upAxis == "-y":
        Coords_up = [0, -1, 0]
    if upAxis == "z":
        Coords_up = [0, 0, 1]
    if upAxis == "-z":
        Coords_up = [0, 0, -1]

    Coords_aim = [] 
    if aimAxis == "x":
        Coords_aim = [1, 0, 0]
    if aimAxis == "-x":
        Coords_aim = [-1, 0, 0]
    if aimAxis == "y":
        Coords_aim = [0, 1, 0]
    if aimAxis == "-y":
        Coords_aim = [0, -1, 0]
    if aimAxis == "z":
        Coords_aim = [0, 0, 1]
    if aimAxis == "-z":
        Coords_aim = [0, 0, -1]

    #Do the aim constraint to make the joint orient properly
    cmds.parent(allChildren, w=True)
    aimConst = cmds.aimConstraint(aimTarget, jointToAim, n=jointToAim+"_AimConst", o=[0, 0, 0], w=1, aim=[Coords_aim[0], Coords_aim[1], Coords_aim[2]], u=[Coords_up[0], Coords_up[1], Coords_up[2]], wut="object", wuo=upNode)[0]
    cmds.delete(aimConst)
    cmds.select(jointToAim)
    cmds.makeIdentity(apply=True, t=1, r=1, s=1, n=0) 
    cmds.delete(ch=True)
    cmds.parent(allChildren, jointToAim)

#----------------------------------------------------------------------------------------
def orientJointOB(*args):
    #Spine Option Variables
    if cmds.optionVar(exists="orientJointHeirarchyOV") == 0:
        cmds.optionVar(iv=("orientJointHeirarchyOV", 1)) 
    if cmds.optionVar(exists="OV_frameLayout_collapse") == 0:
        cmds.optionVar(iv=("OV_frameLayout_collapse", 0))
    if cmds.optionVar(exists="OV_upVectorType") == 0:
        cmds.optionVar(iv=("OV_upVectorType", 3))
    if cmds.optionVar(exists="OV_aimVectorType") == 0:
        cmds.optionVar(iv=("OV_aimVectorType", 1))
        
    # Create the window
    if cmds.window("orientJointsWindow", exists=True):
        cmds.deleteUI("orientJointsWindow", window=True)
    cmds.window("orientJointsWindow", t="Orient Joints with Up Vector", menuBar=True, s=0, mxb=0, rtf=1, w=300, h=175)
    
    # Create the menu
    cmds.menu("editMenu", l="Edit", p="orientJointsWindow", to=0, aob=True)
    cmds.menuItem("resetSettingsItem", l="Reset Settings", ann="Reset all of the settings in this window to their default.", echoCommand=True, p="editMenu", c=resetOrientJointVariables)
    cmds.menu("helpMenu", l="Help", p="orientJointsWindow", hm=1, to=0, aob=True)
    cmds.menuItem("helpItem", l="Help on Orient Joints with Up Vector...", ann="Help on this tool.  Currently there is none...", echoCommand=True, p="helpMenu", c="")

    # base layout for the entire UI
    cmds.columnLayout(adjustableColumn=True)
    cmds.frameLayout(label="Options", bgc=[0.1, 0.1, 0.1], cll=True, cl=cmds.optionVar(q="OV_frameLayout_collapse"), cc="cmds.optionVar(iv=(\"OV_frameLayout_collapse\", 1))", ec="cmds.optionVar(iv=(\"OV_frameLayout_collapse\", 0))", borderStyle="etchedOut")

    # Aim vector dropdown menu
    cmds.optionMenuGrp("OMG_aimVectorType", l="Aim Axis", cw=[1, 80], ann="Aim Axis is the axis of your joints that will point at the child. Default is \"x\"", cc="cmds.optionVar(iv=(\"OV_aimVectorType\", "+"cmds.optionMenuGrp(\"OMG_aimVectorType\", q=True, sl=True)"+"))")
    cmds.menuItem("xItem", l="x")
    cmds.menuItem("nxItem", l="-x")
    cmds.menuItem("yItem", l="y")
    cmds.menuItem("nyItem", l="-y")
    cmds.menuItem("zItem", l="z")
    cmds.menuItem("nzItem", l="-z")
    cmds.optionMenuGrp("OMG_aimVectorType", e=True, sl=cmds.optionVar(q="OV_aimVectorType"))

    # Up vector dropdown menu
    cmds.optionMenuGrp("OMG_upVectorType", l="Up Axis", cw=[1, 80], ann="Up Axis is the axis of your joint that will be aimed at the up object you have selected to be the up vector for your joint. Default is \"y\"", cc="cmds.optionVar(iv=(\"OV_upVectorType\", "+"cmds.optionMenuGrp(\"OMG_upVectorType\", q=True, sl=True)"+"))")
    cmds.menuItem("xItem", l="x")
    cmds.menuItem("nxItem", l="-x")
    cmds.menuItem("yItem", l="y")
    cmds.menuItem("nyItem", l="-y")
    cmds.menuItem("zItem", l="z")
    cmds.menuItem("nzItem", l="-z")
    cmds.optionMenuGrp("OMG_upVectorType", e=True, sl=cmds.optionVar(q="OV_upVectorType"))

    # Heirarchy checkbox
    cmds.checkBox("heirarchyCB", l="Heirarchy?", ann="Do you want to orient all of the children of the selected joint?", v=cmds.optionVar(q="orientJointHeirarchyOV"), onc="cmds.optionVar(iv=(\"orientJointHeirarchyOV\", 1))", ofc="cmds.optionVar(iv=(\"orientJointHeirarchyOV\", 0))")
    cmds.setParent( '..' )  
    cmds.setParent( '..' ) 
    cmds.setParent( '..' )  
    cmds.setParent( '..' ) 

    frmLyout = cmds.formLayout(nd=100) 

    # Buttons
    cmds.button("orientJointButton", l="Orient Joints", ann="Orient Joints with the current settings and close this window.", al="center", w=75, h=20, c=applyAndClose)
    cmds.button("applyGroupButton", l="Apply", ann="Orient Joints with the current settings.", al="center", w=75, h=20, c=findOrientJointValues)
    cmds.button("CloseButton", l="Close", ann="Close this window.", al="center", w=75, h=20, c="cmds.deleteUI(\"orientJointsWindow\", window=True)")
    cmds.setParent( '..' )  
    cmds.setParent( '..' ) 

    cmds.formLayout(frmLyout, e=True, af=[("orientJointButton", "top", 5), ("orientJointButton", "left", 5), ("orientJointButton", "bottom", 5), ("applyGroupButton", "top", 5), ("applyGroupButton", "bottom", 5), ("CloseButton", "top", 5), ("CloseButton", "right", 5), ("CloseButton", "bottom", 5)], ap=[("orientJointButton", "right", 1, 33), ("applyGroupButton", "right", 1, 66)], ac=[("applyGroupButton", "left", 1, "orientJointButton"), ("CloseButton", "left", 1, "applyGroupButton")])
    cmds.showWindow()

#----------------------------------------------------------------------------------------
def applyAndClose(*args):
    findOrientJointValues()
    cmds.deleteUI("orientJointsWindow", window=True)

#----------------------------------------------------------------------------------------
def resetOrientJointVariables(*args):
    #Fixup options
    cmds.optionVar(iv=("orientJointHeirarchyOV", 1))
    cmds.checkBox("heirarchyCB", e=True, v=1)

    cmds.optionVar(iv=("OV_upVectorType", 3))
    cmds.optionMenuGrp("OMG_upVectorType", e=True, sl=3)

    cmds.optionVar(iv=("OV_aimVectorType", 1))
    cmds.optionMenuGrp("OMG_aimVectorType", e=True, sl=1)

#----------------------------------------------------------------------------------------
def findOrientJointValues(*args):
    upVector = cmds.optionVar(q="OV_upVectorType")
    aimVector = cmds.optionVar(q="OV_aimVectorType")

    #Set the Up Axis string
    if upVector == 1:
        upAxis = "x" 
    if upVector == 2:
        upAxis = "-x" 
    if upVector == 3:
        upAxis = "y" 
    if upVector == 4:
        upAxis = "-y" 
    if upVector == 5:
        upAxis = "z" 
    if upVector == 6:
        upAxis = "-z" 

    #Set the Aim Axis string
    if aimVector == 1:
        aimAxis = "x" 
    if aimVector == 2:
        aimAxis = "-x" 
    if aimVector == 3:
        aimAxis = "y" 
    if aimVector == 4:
        aimAxis = "-y" 
    if aimVector == 5:
        aimAxis = "z" 
    if aimVector == 6:
        aimAxis = "-z" 

    if cmds.optionVar(q="orientJointHeirarchyOV") == 0:
        heirarchy = 0 
    if cmds.optionVar(q="orientJointHeirarchyOV") == 1:
        heirarchy = 1 

    setOrientWithUpVector(aimAxis, upAxis, heirarchy, 0, None)

# Run it through the UI
#orientJointOB()

# Run it without the UI (Values are passed in as args)
#setOrientWithUpVector("x", "y", 0, 1, "lowerarm_l")
#setOrientWithUpVector("x", "y", 1, 0, None)
