def addIKBones():
    
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



#open export file
sceneName = cmds.file(q = True, sceneName = True)
exportFile = sceneName.partition("AnimRigs")[0] + "ExportFiles" +  sceneName.partition("AnimRigs")[2].partition(".mb")[0] + "_Export.mb"
cmds.file(exportFile, open = True, force = True)

#add ik bones
addIKBones()

#save
cmds.file(save = True, type = "mayaBinary", force = True)

