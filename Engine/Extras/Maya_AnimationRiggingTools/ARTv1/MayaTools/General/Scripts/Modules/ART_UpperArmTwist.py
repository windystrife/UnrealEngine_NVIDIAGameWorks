import maya.cmds as cmds
import maya.mel as mel
import os
import ART_rigUtils as utils
reload(utils)

#----------------------------------------------------------------------------------------
# Currently this script is written to work with multiple twists, but if you have more than one the last one is the only one that will work.
def upperArmTwist(color, rollGrpParent, fkArmJoint, ikArmJoint, suffix, name, prefix, upperArm, lowerArm):
    print "START Building the "+ suffix +" UPPER arm twists---------------------------------------"

    #create a nice name
    name = prefix + name + suffix
    if name.find("_") == 0:
        name = name.partition("_")[2]
        
        # define the joints for this rig.
        upperarm = prefix + upperArm + suffix
        lowerarm = prefix + lowerArm + suffix
        driver_clavicle = "driver_" + prefix + "clavicle" + suffix
        driver_upperarm = "driver_deltoid_" + prefix + upperArm + suffix
        driver_lowerarm = "driver_" + prefix + lowerArm + suffix

        numRolls = 0
        for joint in ["_twist_01", "_twist_02", "_twist_03", "_twist_04", "_twist_05", "_twist_06"]:
            if cmds.objExists("driver_" + prefix + upperArm + joint + suffix):
                numRolls = numRolls + 1

        print "...There are a total of " + str(numRolls) + " to build."
            
        for i in range(int(numRolls)):
            print "...Building upper arm twist_0" + str(i + 1)
            driver_upperarm_twist = "driver_"+prefix+upperArm+"_twist_0"+str(i+1)+suffix

            rollGrp = cmds.group(empty=True, name=upperarm+"_roll_grp_0"+str(i+1))
            cmds.parent(rollGrp, "arm_sys_grp")

            # Move the driver_upperarm_twist joint to its correct position between the driver_upperarm and its driver_lowerarm using a point constraint.
            const = cmds.parentConstraint(driver_upperarm, driver_upperarm_twist, n=driver_upperarm+"_temp_parentConst", weight=1)
            cmds.delete(const)
            const = cmds.pointConstraint(driver_upperarm, driver_lowerarm, driver_upperarm_twist, n=driver_upperarm+"_temp_PointConst", weight=1)
            cmds.delete(const)

            # Duplicate the driver_upperarm, driver_lowerarm, and driver_upperarm_twist joints
            blendJnts=[]
            blend_driver_upperarmParent = cmds.duplicate(driver_upperarm, n="Blend_"+driver_upperarm+"_parent_ignore_0"+str(i+1), ic=True, po=True)[0]
            blendJnts.append(blend_driver_upperarmParent)

            blend_driver_lowerarm = cmds.duplicate(driver_lowerarm, n="Blend_"+driver_lowerarm+"_ignore_0"+str(i+1), ic=True, po =True)[0]
            blendJnts.append(blend_driver_lowerarm)

            blend_driver_upperarm = cmds.duplicate(driver_upperarm, n="Blend_"+driver_upperarm+"_ignore_0"+str(i+1), po=True)[0]
            blendJnts.append(blend_driver_upperarm)

            blend_driver_upperarm_twist = cmds.duplicate(driver_upperarm_twist, n="Blend_"+driver_upperarm_twist+"_ignore_0"+str(i+1), ic=True, po= True)[0]
            blendJnts.append(blend_driver_upperarm_twist)

            cmds.parent(blend_driver_lowerarm, blend_driver_upperarm_twist, blend_driver_upperarmParent)
            cmds.parent(blend_driver_upperarm, blend_driver_upperarm_twist)

            # Set the parent bone to have zyx rotation order
            cmds.setAttr(blend_driver_upperarmParent+".rotateOrder", 2)

            originalJnts = [driver_upperarm, driver_lowerarm]
            #print originalJnts
            
            # Now we are going to disconnect the constraints that were driving the original driver_upperarm 
            # and driver_lowerarm joints and re-connect them to the parent_ignore and ignore joints that were duped from them.
            # This is so that we can create new constraints on the driver_upperarm and driver_lowerarm joints for the rig.
            j = 0
            a = 0

            #while a < len(originalJnts):
            for a in range(0,len(originalJnts)):
                # Go through the inputs of the original joints and disconnect them
                origConn = cmds.listConnections(originalJnts[a], d=False, c=+True, p=True)

                for j in range(0,len(origConn),2):
                    cmds.disconnectAttr(origConn[j+1], origConn[j])
                    nameList = origConn[j+1].split(".")[0]
                    destConn = cmds.listConnections(nameList, s=False)
                    # Find out if the node that was an input to the original joint is also an output
                    if originalJnts[a] in destConn:
                        sourceConn = cmds.listConnections(nameList, d=False, c=True, p=True)
                        for k in range(0,len(sourceConn),2):
                            # Get the input connections to the node that are connected to the original joint
                            nameConnList = sourceConn[k+1].split(".")
                            if (nameConnList[0] == originalJnts[a]):
                                # Disconnect from the original joint and connect to the blend joint
                                cmds.disconnectAttr(sourceConn[k+1], sourceConn[k])
                                nameConnList[0] = blendJnts[a]
                                cmds.connectAttr(nameConnList[0] + "." + nameConnList[-1], sourceConn[k])

            # create the manual twist control
            if i == 0:
                twistCtrl = utils.createControl("circle", 15, upperarm + "_twist_anim")
            else:
                twistCtrl = utils.createControl("circle", 15, upperarm + "_twist"+str(i+1)+"_anim")
            cmds.setAttr(twistCtrl + ".ry", -90)
            cmds.setAttr(twistCtrl + ".sx", 0.8)
            cmds.setAttr(twistCtrl + ".sy", 0.8)
            cmds.setAttr(twistCtrl + ".sz", 0.8)
            cmds.makeIdentity(twistCtrl, r = 1, s = 1, apply =True)

            # move the manual control to the correct location
            constraint = cmds.parentConstraint(blend_driver_upperarm_twist, twistCtrl)[0]
            cmds.delete(constraint)

            # create a group for the manual control and parent the twist to it.
            twistCtrlGrp = cmds.group(empty = True, name = twistCtrl + "_grp")
            constraint = cmds.parentConstraint(blend_driver_upperarm_twist, twistCtrlGrp)[0]
            cmds.delete(constraint)
            cmds.parent(twistCtrl, twistCtrlGrp)
            cmds.parent(twistCtrlGrp, rollGrp)
            cmds.makeIdentity(twistCtrl, t = 1, r = 1, s = 1, apply = True)
            cmds.parentConstraint(blend_driver_upperarm_twist, twistCtrlGrp)

            # set the manual controls visibility settings
            cmds.setAttr(twistCtrl + ".overrideEnabled", 1)
            cmds.setAttr(twistCtrl + ".overrideColor", color)
            for attr in [".sx", ".sy", ".sz"]:
                cmds.setAttr(twistCtrl + attr, lock = True, keyable = False)
            cmds.setAttr(twistCtrl + ".v", keyable = False)	
            
            # add attr on rig settings for manual twist control visibility
            cmds.select("Rig_Settings")
            if i == 0:
                cmds.addAttr(longName=(name + "twistCtrlVisUpper"), at = 'bool', dv = 0, keyable = True)
            cmds.connectAttr("Rig_Settings." + name + "twistCtrlVisUpper", twistCtrl + ".v")
                        
            # add attr to rig settings for the twist ammount values
            cmds.select("Rig_Settings")
            cmds.addAttr(longName= (name+"UpperarmTwist"+str(i+1)+"Amount"), defaultValue=0.5, minValue=0, maxValue=1, keyable = True)
            for u in range(int(i+1)):
                cmds.setAttr("Rig_Settings."+name+"UpperarmTwist"+str(u+1)+"Amount", (1.0/(i+2.0)*((2.0-u)+(i-1.0))) )

            cmds.parent(blend_driver_upperarmParent, rollGrp)

            # Constrain the original joints to the blend joints
            cmds.orientConstraint(blend_driver_upperarm, driver_upperarm, n=driver_upperarm+"_0"+str(i+1)+"_OrntCnst", mo=True, w=1)
            cmds.pointConstraint(blend_driver_upperarm, driver_upperarm, n=driver_upperarm+"_0"+str(i+1)+"_PtCnst", mo=True, w=1)

            cmds.orientConstraint(blend_driver_lowerarm, driver_lowerarm, n=driver_lowerarm+"_0"+str(i+1)+"_OrntCnst", mo=True, w=1 )
            cmds.pointConstraint(blend_driver_lowerarm, driver_lowerarm, n=driver_lowerarm+"_0"+str(i+1)+"_PtCnst", mo=True, w=1)

            cmds.orientConstraint(twistCtrl, driver_upperarm_twist, n=driver_upperarm_twist+"_0"+str(i+1)+"_OrntCnst", mo=True, w=1)
            cmds.pointConstraint(twistCtrl, driver_upperarm_twist, n=driver_upperarm_twist+"_0"+str(i+1)+"_PtCnst", mo=True, w=1)

            # Create a driver_upperarm_twist multiplier to multiply the driver_upperarm_twist values by -.5 to counter rotate the driver_upperarm_twist and driver_upperarm back from the overall limb's driver_upperarm_twist amount.
            twistMult = cmds.shadingNode("multiplyDivide", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_MDnode")
            twistMultDrive = cmds.shadingNode("multiplyDivide", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_Drive_MDnode")
            twistMultDriveInv = cmds.shadingNode("multiplyDivide", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_DriveInv_MDnode")
            twistMultMultiplier = cmds.shadingNode("multiplyDivide", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_Multiplier_MDnode")
            twistMultMultiplierInv = cmds.shadingNode("multiplyDivide", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_MultiplierInv_MDnode")
            twistReverse = cmds.shadingNode("reverse", asUtility=True, name=blend_driver_upperarm_twist+"_0"+str(i+1)+"_REVnode")

            cmds.connectAttr(blend_driver_upperarmParent+".rotateX", twistMult+".input1X", force=True)
            cmds.connectAttr("Rig_Settings."+name+"UpperarmTwist"+str(i+1)+"Amount", twistReverse+".inputX")
            cmds.connectAttr("Rig_Settings."+name+"UpperarmTwist"+str(i+1)+"Amount", twistMultMultiplierInv+".input1X")
            cmds.connectAttr(twistReverse+".outputX", twistMultMultiplier+".input1X", force=True)
            cmds.connectAttr(twistMult+".outputX", twistMultDrive+".input1X", force=True)
            cmds.connectAttr(twistMultMultiplier+".outputX", twistMultDrive+".input2X", force=True)
            cmds.connectAttr(twistMult+".outputX", twistMultDriveInv+".input1X", force=True)
            cmds.connectAttr(twistMultMultiplierInv+".outputX", twistMultDriveInv+".input2X", force=True)
            cmds.connectAttr(twistMultDrive+".outputX", blend_driver_upperarm_twist+".rotateX", force=True)
            cmds.connectAttr(twistMultDriveInv+".outputX", blend_driver_upperarm+".rotateX", force=True)
            cmds.setAttr(twistMult+".input2X", -0.5)
            cmds.setAttr(twistMult+".input2X", l=True)

            cmds.setAttr(twistMultMultiplier+".input2X", 2)
            cmds.setAttr(twistMultMultiplierInv+".input2X", 2)
            
            cmds.select(blend_driver_upperarmParent)
            
            cmds.parentConstraint(driver_clavicle, rollGrp, mo=True)
    print ".END Building the "+ suffix +" UPPER arm twists---------------------------------------"