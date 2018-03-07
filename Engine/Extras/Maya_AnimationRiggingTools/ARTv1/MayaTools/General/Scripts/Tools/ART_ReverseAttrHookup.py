import maya.cmds as cmds
from functools import partial

# Creates a connection between the 2 selected objects with a reverse node to drive to values in opposite directions of each other based on one attr.
# This is mostly used for having a single attr drive the 2 weights on a constraint with 2 sources in opposite of each other.
# CRA FEATURE: going forward, add the ability to have 3 or even more inputs driven by the single output.
# CRA FEATURE: Add check boxes and filters for filtering the attr lists better.
# To use, select the driver, then the driven, then run the tool.

textScrollList = None
menuDriven0 = None
menuDriven1 = None

def ReverseAttrHookupUI(*args):
    
    print "UI"

    selection = cmds.ls(sl=True)
    if len(selection) != 2:
        cmds.confirmDialog(icon = "warning!!", title = "Reverse Attr Hookup Tool", message = "You must select only 2 objects.  Driver then Driven.")
        return
    else:
        driver = selection[0]
        driven = selection[1]


        attrsListDriver = cmds.listAttr(driver, o=True, c=True, u=True, s=True, k=False)
        attrsListDriven = cmds.listAttr(driven, o=True, c=True, u=True, s=True, k=True)


        if cmds.window("ReverseAttrHookupWin", exists = True):
            cmds.deleteUI("ReverseAttrHookupWin")

        window = cmds.window("ReverseAttrHookupWin", w=4300, h=500, title="Reverse Attr Hookup Tool", mxb=True, mnb=True, sizeable=False, rtf=True)
        mainLayout = cmds.columnLayout(w = 300, h = 400, rs = 5, co = ["both", 5])
        cmds.text(l=driver)
        textScrollList = cmds.textScrollList("artProjCharacterList", w = 290, h = 300, parent = mainLayout, ra=True, ann="This is the attribute that will drive the Driven attributes listed below.")
        cmds.textScrollList(textScrollList, e=True, a=attrsListDriver)
        
        menuDriven0 = cmds.optionMenuGrp(l=driven+" 0", cw=[1, 175], ann="This attribute will be driven as a direct connection from the driver.  When the driver is 1, this input will be 1.  When the driver is 0, this input will be 0.")
        for attr in attrsListDriven:
            cmds.menuItem(l=attr)
        
        menuDriven1 = cmds.optionMenuGrp(l=driven+" 1", cw=[1, 175], ann="This attribute will be driven by the driver through a reverse node.  When the driver is 1, this input will be 0.  When the driver is 0, this input will be 1.")
        for attr in attrsListDriven:
            cmds.menuItem(l=attr)

        buttonUpdate = cmds.button(w = 290, h = 40, label = "Update", c=ReverseAttrHookupUI, ann = "Refresh the UI.", parent = mainLayout)
        buttonConnect = cmds.button(w = 290, h = 40, label = "Connect", c=partial(ReverseAttrHookup, driver, driven, textScrollList, menuDriven0, menuDriven1), ann = "Connect the Selected Attrs.", parent = mainLayout)
         
        cmds.showWindow(window)
    
# This script hooks up the attrs
def ReverseAttrHookup(driver, driven, textScrollList, menuDriven0, menuDriven1, *args):
    
    print "SCRIPT"

    driverAttr = cmds.textScrollList(textScrollList, q=True, si=True)[0]
    drivenAttr0 = cmds.optionMenuGrp(menuDriven0, q=True, v=True)
    drivenAttr1 = cmds.optionMenuGrp(menuDriven1, q=True, v=True)
    
    print driverAttr
    print drivenAttr0
    print drivenAttr1

    reverseNode = cmds.shadingNode("reverse", asUtility=True, name=driver+"_reverse")
    cmds.connectAttr(driver+"."+driverAttr, driven+"."+drivenAttr0, f=True)
    cmds.connectAttr(driver+"."+driverAttr, reverseNode+".inputX", f=True)
    cmds.connectAttr(reverseNode+".outputX", driven+"."+drivenAttr1, f=True)
