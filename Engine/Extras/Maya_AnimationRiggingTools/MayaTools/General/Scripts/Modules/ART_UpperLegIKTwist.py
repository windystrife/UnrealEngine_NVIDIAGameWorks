import maya.cmds as cmds
import Modules.ART_rigUtils as utils
reload(utils)

#----------------------------------------------------------------------------------------
# Currently this script is written to work with multiple twists, but if you have more than one the last one is the only one that will work.
def upperIKTwist(color, suffix, name, prefix, upperJnt, lowerJnt, sysGroup):
    print "START Building the "+suffix+" "+upperJnt+" UPPER IK twists---------------------------------------"

    #create a nice name
    name = prefix + name + suffix
    if name.find("_") == 0:
        name = name.partition("_")[2]
        
        # define the joints for this rig.
        upperjnt = prefix + upperJnt + suffix
        lowerjnt = prefix + lowerJnt + suffix
        #OLD - upperjnt_parent = "driver_" + prefix + "clavicle" + suffix
        upperjnt_parent = cmds.listRelatives(upperjnt, p=True)[0]
        driver_upperjnt = "driver_" + prefix + upperJnt + suffix
        driver_lowerjnt = "driver_" + prefix + lowerJnt + suffix

        numRolls = 0
        for joint in ["_twist_01", "_twist_02", "_twist_03", "_twist_04", "_twist_05", "_twist_06"]:
            if cmds.objExists("driver_" + prefix + upperJnt + joint + suffix):
                numRolls = numRolls + 1

        print "...There are a total of " + str(numRolls) + " to build."

        for i in range(int(numRolls)):
            print "...Building upper twist_0" + str(i + 1)

            upperjnt_twist = prefix+upperJnt+"_twist_0"+str(i+1)+suffix
            driver_upperjnt_twist = "driver_"+prefix+upperJnt+"_twist_0"+str(i+1)+suffix

            ####################################
            # Create a master group for the roll controls to be parented to.
            rollGrp = cmds.group(empty=True, name=upperjnt+"_roll_grp_0"+str(i+1))
            cmds.parent(rollGrp, sysGroup)
            cmds.parentConstraint(upperjnt_parent, rollGrp, mo=True)

            # Create the manual twist control
            if i == 0:
                twistCtrl = utils.createControl("circle", 15, upperjnt + "_twist_anim")
            else:
                twistCtrl = utils.createControl("circle", 15, upperjnt + "_twist"+str(i+1)+"_anim")
            cmds.setAttr(twistCtrl + ".ry", -90)
            cmds.setAttr(twistCtrl + ".sx", 0.8)
            cmds.setAttr(twistCtrl + ".sy", 0.8)
            cmds.setAttr(twistCtrl + ".sz", 0.8)
            cmds.makeIdentity(twistCtrl, r = 1, s = 1, apply =True)

            # move the manual control to the correct location
            constraint = cmds.parentConstraint(driver_upperjnt_twist, twistCtrl)[0]
            cmds.delete(constraint)

            # create a group for the manual control and parent the twist to it.
            twistCtrlGrp = cmds.group(empty = True, name=twistCtrl+"_grp")
            constraint = cmds.parentConstraint(driver_upperjnt_twist, twistCtrlGrp)[0]
            cmds.delete(constraint)
            cmds.parent(twistCtrl, twistCtrlGrp)
            cmds.parent(twistCtrlGrp, rollGrp)
            cmds.makeIdentity(twistCtrl, t = 1, r = 1, s = 1, apply = True)

            # set the manual controls visibility settings
            cmds.setAttr(twistCtrl + ".overrideEnabled", 1)
            cmds.setAttr(twistCtrl + ".overrideColor", color)
            for attr in [".sx", ".sy", ".sz"]:
                cmds.setAttr(twistCtrl + attr, lock = True, keyable = False)
            cmds.setAttr(twistCtrl + ".v", keyable = False)	

            # add attr on rig settings for manual twist control visibility
            cmds.select("Rig_Settings")
            if i == 0:
                cmds.addAttr(longName=(upperjnt + "_twistCtrlVisUpper"), at = 'bool', dv = 0, keyable = True)
            cmds.connectAttr("Rig_Settings." + upperjnt + "_twistCtrlVisUpper", twistCtrl + ".v")

            # add attr to rig settings for the twist ammount values
            cmds.select("Rig_Settings")
            cmds.addAttr(longName= (upperjnt+"_Twist"+str(i+1)+"Amount"), defaultValue=0.5, minValue=0, maxValue=1, keyable = True)
            for u in range(int(i+1)):
                cmds.setAttr("Rig_Settings."+upperjnt+"_Twist"+str(u+1)+"Amount", (1.0/(i+2.0)*((2.0-u)+(i-1.0))) )

            ####################################
            # Create the twist rig on the shoulder without any extra controls.  Those are added later.
            cmds.delete(upperjnt_twist+"_orientConstraint1")

            twistIKHandle = cmds.ikHandle(name="twistIKHandle_0"+str(i+1)+"_"+upperjnt, sol="ikRPsolver", sj=upperjnt, ee=upperjnt_twist)[0]
            print "TWIST IK HANDLE IS: "+twistIKHandle
            cmds.setAttr(twistIKHandle+".poleVectorX", 0)
            cmds.setAttr(twistIKHandle+".poleVectorY", 0)
            cmds.setAttr(twistIKHandle+".poleVectorZ", 0)
            cmds.parent(twistIKHandle, driver_upperjnt)

            driver_upperjnt_offset = cmds.duplicate(driver_upperjnt, po=True, name="driver_"+upperJnt+"_twistOffset_l")[0]
            cmds.parent(driver_upperjnt_offset, driver_upperjnt)

            twistOrntConst = cmds.orientConstraint(driver_upperjnt, upperjnt, driver_upperjnt_offset, mo=True)[0]
            cmds.setAttr(twistOrntConst+".interpType", 2)

            twistMDNode = cmds.shadingNode("multiplyDivide", asUtility=True, name=driver_upperjnt_twist+"_MDnode")
            cmds.setAttr(twistMDNode+".input2X", 1)

            cmds.connectAttr(driver_upperjnt_offset+".rotateX", twistMDNode+".input1X", force=True)
            cmds.connectAttr(twistMDNode+".outputX", driver_upperjnt_twist+".rx", force=True)            

            cmds.pointConstraint(driver_upperjnt_twist, twistCtrlGrp, mo=True)
            cmds.orientConstraint(driver_upperjnt_twist, twistCtrlGrp, mo=True)

            cmds.orientConstraint(twistCtrl, upperjnt_twist, mo=True)
            
            if cmds.objExists("deltoid_anim"+suffix):
                if suffix == "_l":
                    cmds.connectAttr("deltoid_anim"+suffix+".rotateX", twistIKHandle+".twist", force=True)
                else:
                    deltMDNode = cmds.shadingNode("multiplyDivide", asUtility=True, name=twistIKHandle+"_MDNode")
                    cmds.setAttr(deltMDNode+".input2X", -1)
                    cmds.connectAttr("deltoid_anim"+suffix+".rotateX", deltMDNode+".input1X", force=True)
                    cmds.connectAttr(deltMDNode+".outputX", twistIKHandle+".twist", force=True)

            reverseMDNode = cmds.shadingNode("reverse", asUtility=True, name=driver_upperjnt_offset+"_Reversenode")
            cmds.connectAttr("Rig_Settings."+upperjnt+"_Twist"+str(i+1)+"Amount", reverseMDNode+".inputX", force=True)
            cmds.connectAttr(reverseMDNode+".outputX", twistOrntConst+"."+upperjnt+"W1", force=True)            
            cmds.connectAttr("Rig_Settings."+upperjnt+"_Twist"+str(i+1)+"Amount", twistOrntConst+"."+driver_upperjnt+"W0", force=True)


    print ".END Building the "+suffix+" "+upperJnt+" UPPER twists---------------------------------------"            

#upperIKTwist(6, "_l", "", "", "upperarm", "lowerarm", "arm_sys_grp")
