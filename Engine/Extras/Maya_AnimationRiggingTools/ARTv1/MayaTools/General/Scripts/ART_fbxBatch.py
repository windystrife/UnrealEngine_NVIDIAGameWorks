#fbx export batcher

import maya.cmds as cmds
from functools import partial
import os, cPickle, math
import maya.mel as mel
import maya.utils
    
class FBX_Batcher():
    def __init__(self):
        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):
            
            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()   

        self.widgets = {}
        
        if cmds.window("ART_FBX_BATCH", exists = True):
            cmds.deleteUI("ART_FBX_BATCH")
            
        self.widgets["window"] = cmds.window("ART_FBX_BATCH", title = "FBX Batch", w = 400, h = 150, sizeable = False, mnb = False, mxb = False)
        
        #main layout
        self.widgets["layout"] = cmds.formLayout(w = 400, h = 150)
        
        #textField and browse button
        self.widgets["textField"] = cmds.textField(w = 300, text = "")
        self.widgets["browse"] = cmds.button(w = 70, label = "Browse", c = self.browse)
        
        cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["textField"], "left", 10),(self.widgets["textField"], "top", 10)])
        cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["browse"], "right", 10),(self.widgets["browse"], "top", 10)])
        
        #process button
        self.widgets["process"] = cmds.button(w = 380, h = 50, label = "BEGIN BATCH", c = self.process)
        cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["process"], "left", 10),(self.widgets["process"], "top", 40)])
        
        #progress bar
        self.widgets["currentFileProgressBar"] = cmds.progressBar(w = 390)
        cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["currentFileProgressBar"], "left", 5),(self.widgets["currentFileProgressBar"], "bottom", 35)])
        
        self.widgets["progressBar"] = cmds.progressBar(w = 390)
        cmds.formLayout(self.widgets["layout"], edit = True, af = [(self.widgets["progressBar"], "left", 5),(self.widgets["progressBar"], "bottom", 10)])
        
        #show window
        cmds.showWindow(self.widgets["window"])

    def browse(self, *args):
        try:
            directory = cmds.fileDialog2(dialogStyle=2, fm = 3)[0]
            cmds.textField(self.widgets["textField"], edit = True, text = directory)
        except:
            pass
    
    def process(self, *args):
        failed = []
        succeeds = []
        
        #new file
        cmds.file(new = True, force = True)
        
        #get the files in the directory
        directory = cmds.textField(self.widgets["textField"], q = True, text = True)
        filesInDir = os.listdir(directory)
        mayaFiles = []
        
        for each in filesInDir:
            
            if each.rpartition(".")[2] == "mb":
                mayaFiles.append(each)
            
            #go through loop
            if len(mayaFiles) > 0:
                
                #get number of maya files
                numFiles = len(mayaFiles)
                amount = 100/numFiles
                newAmount = 0
                
                #print mayaFiles
                
                for mayaFile in mayaFiles:
                    cmds.progressBar(self.widgets["progressBar"], edit = True, progress = newAmount + amount)
                
                #open file
                directory = os.path.normpath(directory)
                cmds.file(os.path.join(directory, mayaFile), open = True, force = True)
                    
                #update
                cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 50)
                
                #get characters
                characters = []
                references = cmds.ls(type = "reference")
                
                for reference in references:
                    niceName = reference.rpartition("RN")[0]
                    suffix = reference.rpartition("RN")[2]
                    if suffix != "":
                        if cmds.objExists(niceName + suffix + ":" + "Skeleton_Settings"):
                            characters.append(niceName + suffix)
                        
                    else:
                        if cmds.objExists(niceName + ":" + "Skeleton_Settings"):
                            characters.append(niceName) 
                        
                #export FBX for each character
                for character in characters:
                    #check to see if file has cached data:
                    if cmds.objExists("ExportAnimationSettings"):
                        sequeneces = cmds.listAttr("ExportAnimationSettings", string = "sequence*")
                    
                        for seq in sequeneces:
                            #print seq
                            
                            #get data
                            data = cmds.getAttr("ExportAnimationSettings." + seq)
                            name = data.partition("::")[0]
                            data = data.partition("::")[2]
                            start = data.partition("::")[0]
                            data = data.partition("::")[2]
                            end = data.partition("::")[0]
                            data = data.partition("::")[2]
                            fps = data.partition("::")[0]
                            interp = data.partition("::")[2]
                            
                            if interp == "Independent Euler Angle":
                                interp = 1
                            
                            if interp == "Synchronized Euler Angle":
                                interp = 2
                            
                            if interp == "Quaternion Slerp":
                                interp = 3
                            
                            #print name
                            #print start
                            #print end
                            #print fps
                            #print interp
                            
                            #self.getBlendshapes(character, start, end)
                            
                            #export
                            self.exportFBX(character, os.path.join(name), start, end, fps, interp)
                    else:
                        #create the fbx path with /FBX
                        fbxPath = os.path.join(directory, "FBX")
                        if not os.path.exists(fbxPath):
                            os.makedirs(fbxPath)
                    
                        #get name
                        fileName = mayaFile.rpartition(".")[0]          
                
                        #get current frame range
                        start = cmds.playbackOptions(q = True, min = True)
                        end = cmds.playbackOptions(q = True, max = True)
                        #self.getBlendshapes(character, start, end)
                        self.exportFBX(character, os.path.join(fbxPath, fileName + ".fbx"), start, end, "30 FPS", 3)
                        
                    cmds.progressBar(self.widgets["currentFileProgressBar"], edit = True, progress = 100)
                    
                #increment new amount
                newAmount = newAmount + amount
            
        #new scene
        cmds.file(new = True, force = True)
        
        #delete UI
        cmds.deleteUI(self.widgets["window"])
        
        #confirm message
        cmds.confirmDialog(title = "Complete!", message = "Operation is complete!")
        
    def exportFBX(self, character, path, startFrame, endFrame, fps, interp, *args):
        #get the current time range values
        originalStart = cmds.playbackOptions(q = True, min = True)
        originalEnd = cmds.playbackOptions(q = True, max = True)
        currentFPS = cmds.currentUnit(q = True, time = True)
        currentInterp = 1
        
        #set rotation interp
        cmds.optionVar(iv = ("rotationInterpolationDefault", int(interp)))
        
        #duplicate the skeleton
        newSkeleton = cmds.duplicate(character + ":" + "root", un = False, ic = False)
        
        joints = []
        for each in newSkeleton:
            if cmds.nodeType(each) != "joint":
                cmds.delete(each)
            else:
                joints.append(each)
        
        constraints = []
        for joint in joints:
            #do some checks to make sure that this is valid
            parent = cmds.listRelatives(joint, parent = True)

            if parent != None:
                if parent[0] in joints:
                    constraint = cmds.parentConstraint(character + ":" + parent[0] + "|" + character + ":" + joint, joint)[0]
                    constraints.append(constraint)
            else:
                #root bone?
                if joint == "root":
                    constraint = cmds.parentConstraint(character + ":" + joint, joint)[0]
                    constraints.append(constraint)              

        #Set start and end frame
        cmds.playbackOptions(min = startFrame, animationStartTime = startFrame)
        cmds.playbackOptions(max = endFrame, animationEndTime = endFrame)
        
        #set to the new fps
        if fps == "30 FPS":
            fps = "ntsc"
            
        if fps == "60 FPS":
            fps = "ntscf"
            
        if fps == "120 FPS":
            fps  = "120fps"
            
        cmds.currentUnit(time = fps)
        
        #set the new frame range
        if fps == "ntscf":
            cmds.playbackOptions(min = startFrame * 2, animationStartTime = startFrame * 2)
            cmds.playbackOptions(max = endFrame * 2, animationEndTime = endFrame * 2)
            startFrame = startFrame * 2
            endFrame = endFrame * 2
            
        if fps == "120fps":
            cmds.playbackOptions(min = startFrame * 4, animationStartTime = startFrame * 4)
            cmds.playbackOptions(max = endFrame * 4, animationEndTime = endFrame * 4)
            startFrame = startFrame * 4
            endFrame = endFrame * 4
            
        #bake results
        cmds.select("root", hi = True)
        cmds.bakeResults(simulation = True, t = (startFrame, endFrame))
        cmds.delete(constraints)
                
        #run an euler filter
        cmds.select("root", hi = True)
        cmds.filterCurve()
                

        #print "startFrame: ",startFrame
        #print "endFrame: ",endFrame
        #print "BLENDSHAPES CODE IS RUNNING!!!!!!!!"
        
        #get the blendshapes to export
        allBlends = cmds.ls(type = "blendShape")
        jmBlends = ["calf_l_shapes",  "calf_r_shapes",  "head_shapes",  "l_elbow_shapes",  "r_elbow_shapes",  "l_lowerarm_shapes",  "r_lowerarm_shapes",  "l_shoulder_shapes",  "r_shoulder_shapes",  "l_upperarm_shapes",  "r_upperarm_shapes",
                    "neck1_shapes",  "neck2_shapes",  "neck3_shapes",  "pelvis_shapes",  "spine1_shapes",  "spine2_shapes",  "spine3_shapes",  "spine4_shapes",  "spine5_shapes",  "thigh_l_shapes",  "thigh_r_shapes"]
        blendshapes = []
        #print "allBlends list:", allBlends
        #print "jmBlends list:", jmBlends

        if allBlends != None:
            for shape in allBlends:
                if shape.partition(":")[2] not in jmBlends:
                    if shape.partition(":")[0] == character:
                        blendshapes.append(shape)
        #print "blendshapes list:", blendshapes

        if blendshapes != None:
            if cmds.objExists("custom_export"):
                cmds.delete("custom_export")

            cube = cmds.polyCube(name = "custom_export")[0]
            i = 1
            for shape in blendshapes:
                attrs = cmds.listAttr(shape, m = True, string = "weight")
                #counter for index

                for attr in attrs:
                    #print attr
                    keys = cmds.keyframe( shape, attribute=attr, query=True, timeChange = True )
                    #print keys
                    keyValues = cmds.keyframe( shape, attribute=attr, query=True, valueChange = True )
                    #print keyValues

                    morph = cmds.polyCube(name = attr)[0]
                    if cmds.objExists("custom_export_shapes"):
                        cmds.blendShape("custom_export_shapes", edit = True, t = (cube, i, morph, 1.0))
                        #print "editing blendshape to include " + cube + str(i) + morph

                    else:
                        cmds.select([morph, cube], r = True)
                        cmds.blendShape(name = "custom_export_shapes")
                        cmds.select(clear = True)

                    cmds.delete(morph)
                    #print i
                    #transfer keys from original to new morph
                    if keys != None:
                        for x in range(int(len(keys))):
                            cmds.setKeyframe("custom_export_shapes." + attr, t = (keys[x]), v = keyValues[x])

                    if keys == None:
                        for x in range(int(startFrame), (int(endFrame) + 1)):
                            value = cmds.getAttr(shape + "." + attr, time = x)
                            cmds.setKeyframe("custom_export_shapes." + attr, t = (x), v = value)

                    i = i + 1

        #export selected
        #first change some fbx properties
        string = "FBXExportConstraints -v 1;"
        string += "FBXExportCacheFile -v 0;"
        mel.eval(string)
                
        cmds.select("root", hi = True)
        if cmds.objExists("custom_export"):
            cmds.select("custom_export", add = True)

        cmds.file(path, es = True, force = True, prompt = False, type = "FBX export")
        
        #clean scene
        cmds.delete("root")
        if cmds.objExists("custom_export"):
            cmds.delete("custom_export")
            
        #reset fps to original
        cmds.currentUnit(time = currentFPS)     

        #reset timeslider to original values
        cmds.playbackOptions(min = originalStart, max = originalEnd)
        
        #reset rotation interp
        cmds.optionVar(iv = ("rotationInterpolationDefault", currentInterp))
        
    

        