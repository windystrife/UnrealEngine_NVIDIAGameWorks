import maya.cmds as cmds
import maya.mel as mel
import os
import ART_rigUtils as utils
reload(utils)

#----------------------------------------------------------------------------------------
def forearmTwistRig(color, rollGrpParent, fkArmJoint, ikArmJoint, suffix, name, prefix, lowerArm):
    print "START Building the "+ suffix +" lower arm twists---------------------------------------"
    
    print "suffix = "+suffix
    print "name = "+name
    print "prefix = "+prefix
    print "lowerArm = "+lowerArm
    
    # create a nice name
    name = prefix + name + suffix
    if name.find("_") == 0:
        name = name.partition("_")[2]
        lowerarm = prefix + lowerArm + suffix
        driver_lowerarm = "driver_" + prefix + lowerArm + suffix
        driver_hand = "driver_hand_"+name

        numRolls = 0
        for joint in ["_twist_01", "_twist_02", "_twist_03", "_twist_04", "_twist_05", "_twist_06"]:
            if cmds.objExists("driver_" + prefix + lowerArm + joint + suffix):
                numRolls = numRolls + 1

        print "...There are a total of " + str(numRolls) + " to build."
            
        for i in range(int(numRolls)):
            print "...Building lower arm twist_0" + str(i + 1)
            driver_lowerarm_twist = "driver_" + prefix + lowerArm + "_twist_0" + str(i + 1) + suffix
            
            # create our roll group
            rollGrp = cmds.group(empty = True, name = lowerarm + "_roll_grp_0" + str(i + 1))
            cmds.parent(rollGrp, "arm_sys_grp")

            # Make sure the twist joint is exactly at the hand and aimed at the lowerarm
            #NOTE if the "side" method Jeremy is using in here(name) is intended to handle more than just "l" and "r" then the move part of this must be re-written to handle that accordingly.
            '''if i == 0:
                const = cmds.parentConstraint(driver_hand, driver_lowerarm_twist, weight=1, mo=False, sr=["x","y","z"])
                cmds.delete(const)
                cmds.select(driver_lowerarm_twist, r=True)
                if name == "l":
                    cmds.move(-0.995369, 0, 0, driver_lowerarm_twist, r = True)
                else:
                    cmds.move(0.995369, 0, 0, driver_lowerarm_twist, r = True)
                cmds.makeIdentity(driver_lowerarm_twist, r = 1, t = 1, s = 1, apply =True)'''

            # create a new heirarchy out of the driver skeleton
            driver_lowerarm_offset = cmds.duplicate(driver_lowerarm, po=True, name=driver_lowerarm+"_Offset_0" + str(i + 1))[0]
            cmds.parent(driver_lowerarm_offset, rollGrp)
            cmds.setAttr(driver_lowerarm_offset + ".rotateOrder", 1)
            cmds.setAttr(driver_lowerarm_offset + ".v", 1)

            driver_lowerarm_twist_offset = cmds.duplicate(driver_lowerarm_twist, po=True, name=driver_lowerarm_twist+"_Offset")[0]
            cmds.parent(driver_lowerarm_twist_offset, driver_lowerarm_offset)
            cmds.setAttr(driver_lowerarm_twist_offset + ".rotateOrder", 1)
            cmds.setAttr(driver_lowerarm_twist_offset + ".v", 1)

            driver_hand_offset = cmds.duplicate(driver_lowerarm_twist_offset, po=True, name=driver_hand+"_Offset_0" + str(i + 1))[0]
            const = cmds.parentConstraint(driver_hand, driver_hand_offset, weight=1, mo=False, sr=["x","y","z"])
            cmds.delete(const)
            cmds.setAttr(driver_hand_offset + ".rotateOrder", 1)
            cmds.setAttr(driver_hand_offset + ".v", 1)

            # create the manual twist control
            if i == 0:
                twistCtrl = utils.createControl("circle", 15, lowerarm + "_twist_anim")
            else:
                twistCtrl = utils.createControl("circle", 15, lowerarm + "_twist"+str(i + 1)+"_anim")
            cmds.setAttr(twistCtrl + ".ry", -90)
            cmds.setAttr(twistCtrl + ".sx", 0.8)
            cmds.setAttr(twistCtrl + ".sy", 0.8)
            cmds.setAttr(twistCtrl + ".sz", 0.8)
            cmds.makeIdentity(twistCtrl, r = 1, s = 1, apply =True)
            
            # move the manual control to the correct location
            constraint = cmds.parentConstraint(driver_lowerarm_twist_offset, twistCtrl)[0]
            cmds.delete(constraint)
            
            # create a group for the manual control and parent the twist to it.
            twistCtrlGrp = cmds.group(empty = True, name = twistCtrl + "_grp")
            constraint = cmds.parentConstraint(driver_lowerarm_twist_offset, twistCtrlGrp)[0]
            cmds.delete(constraint)
            
            cmds.parent(twistCtrl, twistCtrlGrp)
            cmds.parent(twistCtrlGrp, rollGrp)
            cmds.makeIdentity(twistCtrl, t = 1, r = 1, s = 1, apply = True)
            cmds.parentConstraint(driver_lowerarm_twist_offset, twistCtrlGrp)

            # set the manual controls visibility settings
            cmds.setAttr(twistCtrl + ".overrideEnabled", 1)
            cmds.setAttr(twistCtrl + ".overrideColor", color)
            for attr in [".sx", ".sy", ".sz"]:
                cmds.setAttr(twistCtrl + attr, lock = True, keyable = False)
            cmds.setAttr(twistCtrl + ".v", keyable = False)	
        
            # add attr on rig settings for manual twist control visibility
            cmds.select("Rig_Settings")
            if i == 0:
                cmds.addAttr(longName=(name + "twistCtrlVisLower"), at = 'bool', dv = 0, keyable = True)
            cmds.connectAttr("Rig_Settings." + name + "twistCtrlVisLower", twistCtrl + ".v")
            
            # add attr to rig settings for the twist ammount values
            cmds.select("Rig_Settings")
            cmds.addAttr(longName= ( name + "ForearmTwist"+str(i + 1)+"Amount" ), defaultValue=0.5, minValue=0, maxValue=1, keyable = True)
            for u in range(int(i+1)):
                cmds.setAttr("Rig_Settings." + name + "ForearmTwist"+str(u + 1)+"Amount", (1.0/(i+2.0)*((2.0-u)+(i-1.0))) )
            
            # Create a locator and group for the up vector of the aim for the twist.
            forearmTwistUp = cmds.spaceLocator(n="forearmTwistUp_0"+str(i + 1)+"_"+name)[0]
            constraint = cmds.parentConstraint(driver_hand_offset, forearmTwistUp)[0]
            cmds.delete(constraint)
            forearmTwistUpGrp = cmds.duplicate(driver_hand_offset, po=True, name=forearmTwistUp+"_grp")[0]
            cmds.parent(forearmTwistUpGrp, rollGrp)
            cmds.parent(forearmTwistUp, forearmTwistUpGrp)
            cmds.setAttr(forearmTwistUpGrp + ".rotateOrder", 1)
            cmds.setAttr(forearmTwistUp + ".v", 0)	

            # constrain the up group into the rig so that it takes the correct ammount of the twist
            forearmGrpPrntConst = cmds.parentConstraint(driver_lowerarm_offset, driver_hand_offset, forearmTwistUpGrp, mo=True)[0]
            cmds.setAttr(forearmGrpPrntConst+".interpType", 2)
            cmds.move(0, 0, 6, forearmTwistUp, r = True)

            twistAimConst = cmds.aimConstraint(driver_hand_offset, driver_lowerarm_twist_offset, weight=1, mo=True, aimVector=(1, 0, 0), upVector=(0, 0, 1), worldUpType="object", worldUpObject=forearmTwistUp)[0]

            # constrain the offsets back to the original driver skeleton
            cmds.parentConstraint(driver_hand, driver_hand_offset, mo=True)
            cmds.parentConstraint(driver_lowerarm, driver_lowerarm_offset, mo=True)

            # hook up the nodes to drive the twist based on the values on the rig settings node.
            reverseNode = cmds.shadingNode("reverse", asUtility=True, name=forearmGrpPrntConst+"_reverse")
            cmds.connectAttr("Rig_Settings."+name+"ForearmTwist"+str(i + 1)+"Amount", forearmGrpPrntConst+"."+driver_hand_offset+"W1", f=True)
            cmds.connectAttr("Rig_Settings."+name+"ForearmTwist"+str(i + 1)+"Amount", reverseNode+".inputX", f=True)
            cmds.connectAttr(reverseNode+".outputX", forearmGrpPrntConst+"."+driver_lowerarm_offset+"W0", f=True)
            
            #constrain driver joint to twist joint
            cmds.parentConstraint(twistCtrl, driver_lowerarm_twist,  mo = True)	
            
    print ".END Building the "+ suffix +" lower arm twists---------------------------------------"
    
#forearmTwistRig(0, "", "", "", "_l", "", "", "lowerarm")
    