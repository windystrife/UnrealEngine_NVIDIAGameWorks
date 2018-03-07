import maya.cmds as cmds

# Creates a new space switching object for the mainObj using the spaceMasterObj.  
# If you want this new space to be in control, set the control arg to "True".  False will leave everything as it is and just add the new space without it being turned on.

def addSpaceSwitching(mainObj, spaceMasterObj, control, *args):
    # find all of the objects we need to do the space switching
    space_switcher_follow = mainObj+"_space_switcher_follow"
    space_switcher = mainObj+"_space_switcher"
    cmds.select(space_switcher)
    
    spaceMasterObjClean = spaceMasterObj.replace(":", "_")
    mainObjClean = mainObj.replace(":", "_")
     
    # add attr to the space switcher node
    cmds.addAttr(space_switcher, ln = "space_"+spaceMasterObjClean, minValue = 0, maxValue = 1, dv = 0, keyable = True)
    
    # add constraint to the new object on the follow node
    constraint = cmds.parentConstraint(spaceMasterObj, space_switcher_follow, mo = True)[0]

    # find the entire list of targets on the constraint node
    targets = cmds.parentConstraint(constraint, q = True, targetList = True)

    # using the targets list, find the weight value for the node we are working with.
    weight = 0
    for i in range(int(len(targets))):
        if targets[i].find(spaceMasterObj) != -1:
            weight = i
            
    # Connect the attr we added earlier to the weight value in the constraint
    tempName = spaceMasterObj
    #print spaceMasterObj.find(":")
    if spaceMasterObj.find(":") != -1:
        tempName = spaceMasterObj.partition(":")[2]
        #print "TEST"
    
    #print "tempname = ",tempName
    cmds.connectAttr(space_switcher+".space_"+spaceMasterObjClean, constraint+"."+tempName+"W"+str(weight))

    # lockNode on space object so it cannot be deleted by the user (if node is not a referenced node)
    if spaceMasterObj.find(":") == -1:
        cmds.lockNode(spaceMasterObj)

    # set the values of the space switcher based on user input.
    if control == True:
        attrs = cmds.listAttr(space_switcher, k=True)
        for attr in attrs:
            if attr.find("space") != -1:
                cmds.setAttr(space_switcher+"."+attr, 0)
                
        cmds.setAttr(space_switcher+"."+"space_"+spaceMasterObjClean, 1)


    cmds.select(space_switcher)


# Example calls to this script.
'''
addSpaceSwitching("Rider:ik_foot_anim_l", "cockpit_pedal_l_anim", True)
'''

#addSpaceSwitching("Rider:ik_foot_anim_l", "cockpit_foot_pedal_l_anim", True)
#addSpaceSwitching("cockpit_foot_pedal_l_anim", "Rider:ik_foot_anim_l", True)
