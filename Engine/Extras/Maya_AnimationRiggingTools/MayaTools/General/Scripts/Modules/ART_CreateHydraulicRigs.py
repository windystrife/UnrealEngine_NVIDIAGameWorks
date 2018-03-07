import maya.cmds as cmds

# This tool only works on "leaf" joints created with A.R.T 
# To use, select 3 objects, the start of your hydraulic system, the end, and the upvector.  It is assumed that you are selecting the _anim controls for the first 2
# It is assumed that the upvector is 1 by default unless passed in otherwise.  Once set, you can always change this by simply manually reversing the value in the aimConstraint
# It is also assumed that you are using X as the axis that points at each other and that the first control points at the 2nd, but the 2nd points away from the first.
# Assuming Z as the up vector axis.

def CreateHydraulicRig(upDir):

    selection = cmds.ls(sl=True)
    hyd_start_01_anim = selection[0]
    hyd_start_02_anim = selection[1]
    upObject = selection[2]
    
    hyd_start_01 = hyd_start_01_anim.rpartition("_")[0]
    hyd_start_02 = hyd_start_02_anim.rpartition("_")[0]

    # Create a rig for the lower arm extra hydraulic piston.
    cmds.delete("driver_"+hyd_start_01+"_parentConstraint1")
    robo_lowarm_pistonrod_01_anim_sub = cmds.group(em=True, name=hyd_start_01_anim+"_sub", parent=hyd_start_01_anim)
    const = cmds.parentConstraint(hyd_start_01_anim, robo_lowarm_pistonrod_01_anim_sub, weight=1, mo=False)
    cmds.delete(const)
    const = cmds.parentConstraint(robo_lowarm_pistonrod_01_anim_sub, "driver_"+hyd_start_01, weight=1, mo=True)
    
    cmds.delete("driver_"+hyd_start_02+"_parentConstraint1")
    robo_lowarm_pistonrod_02_anim_sub = cmds.group(em=True, name=hyd_start_02_anim+"_sub", parent=hyd_start_02_anim)
    const = cmds.parentConstraint(hyd_start_02_anim, robo_lowarm_pistonrod_02_anim_sub, weight=1, mo=False)
    cmds.delete(const)
    const = cmds.parentConstraint(robo_lowarm_pistonrod_02_anim_sub, "driver_"+hyd_start_02, weight=1, mo=True)

    # Hook up the hydraulics for the lowerarm piston.
    const = cmds.aimConstraint(robo_lowarm_pistonrod_01_anim_sub, robo_lowarm_pistonrod_02_anim_sub, weight=1, mo=False, aimVector=(-1, 0, 0), upVector=(0, 0, upDir), worldUpType="object", worldUpVector=(0, 0, -1), worldUpObject=upObject)
    const = cmds.aimConstraint(robo_lowarm_pistonrod_02_anim_sub, robo_lowarm_pistonrod_01_anim_sub, weight=1, mo=False, aimVector=(1, 0, 0), upVector=(0, 0, upDir), worldUpType="object", worldUpVector=(0, 0, -1), worldUpObject=upObject)


def CreateHydraulicSlideRig(*args):
    print "test"
    # Create a slide mechanism on the forearm part of the lowerarm piston.
    # CRA NOTE: this will come at a later date.
    '''
    cmds.addAttr("robo_lowarm_pistonrod_02_anim", ln = "Slide", dv = 0, min=0, max=10, keyable = True)
    cmds.setDrivenKeyframe("robo_lowarm_pistonrod_02_anim_grp.translateX", cd="robo_lowarm_pistonrod_02_anim.Slide", itt="spline", ott="spline")
    cmds.setDrivenKeyframe("robo_lowarm_pistonrod_02_anim_grp.translateZ", cd="robo_lowarm_pistonrod_02_anim.Slide", itt="spline", ott="spline")

    cmds.setAttr("robo_lowarm_pistonrod_02_anim.Slide", 10)
    cmds.setAttr("robo_lowarm_pistonrod_02_anim_grp.tx", -6.662443)
    cmds.setAttr("robo_lowarm_pistonrod_02_anim_grp.tz", -5.510393)

    cmds.setDrivenKeyframe("robo_lowarm_pistonrod_02_anim_grp.translateX", cd="robo_lowarm_pistonrod_02_anim.Slide", itt="spline", ott="spline")
    cmds.setDrivenKeyframe("robo_lowarm_pistonrod_02_anim_grp.translateZ", cd="robo_lowarm_pistonrod_02_anim.Slide", itt="spline", ott="spline")
    cmds.setAttr("robo_lowarm_pistonrod_02_anim.Slide", 0)
    '''
