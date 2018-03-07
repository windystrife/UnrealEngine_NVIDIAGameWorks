import maya.cmds as cmds
from functools import partial
import re
import time
import maya.mel as mel

def UI():
    
    #check to see if the window exists, if so delete it
    if cmds.window("je_shotSplitter_UI", exists = True):
        cmds.deleteUI("je_shotSplitter_UI")
        
    #create the window    
    window = cmds.window("je_shotSplitter_UI", title = "Shot Splitter", w = 400, h = 200, sizeable = False, mxb= False, mnb = False)
    
    #create the mainLayout
    mainLayout = cmds.columnLayout(w = 400, h = 200)
    
    #create a row column layout
    rowColumnLayout = cmds.rowColumnLayout(nc =2, cw = [(1, 300), (2, 100)], columnOffset = [(1,"both", 5), (2, "both", 5)])
    

    #Create fields for the text file
    
    cmds.separator(h = 10, style = "none")
    cmds.separator(h = 10, style = "none")
    
    cmds.text(label = "Matinee Text File:", align = "left")
    cmds.text(label = "")
    
    cmds.textField("shotSplitterTextFile", w = 300)
    cmds.button(label = "Browse", w = 50, c = partial(shotSplitter_getTextFieldInfo, "*.txt", 1, "shotSplitterTextFile"))
    
    

    
    #Create fields for the audio file
    
    cmds.separator(h = 15, style = "none")
    cmds.separator(h = 15, style = "none")
    
    cmds.text(label = "Matinee Audio File:", align = "left")
    cmds.text(label = "")
    
    cmds.textField("shotSplitterAudioFile", w = 300)
    cmds.button(label = "Browse", w = 50, c = partial(shotSplitter_getTextFieldInfo, "*.wav", 1, "shotSplitterAudioFile"))
    
    
    #Create fields for the fbx file
    
    cmds.separator(h = 15, style = "none")
    cmds.separator(h = 15, style = "none")
    
    cmds.text(label = "Matinee FBX File:", align = "left")
    cmds.text(label = "")
    
    cmds.textField("shotSplitterFBXFile", w = 300, enable = False, text = "Uses currently made file at the moment")
    cmds.button(label = "Browse", w = 50, enable = False, c = partial(shotSplitter_getTextFieldInfo, "*.fbx", 1, "shotSplitterFBXFile"))
    
    
    #Create fields for the output directory
    
    cmds.separator(h = 15, style = "none")
    cmds.separator(h = 15, style = "none")
    
    cmds.text(label = "Output Directory:", align = "left")
    cmds.text(label = "")
    
    cmds.textField("shotSplitterOutput", w = 300)
    cmds.button(label = "Browse", w = 50, c = partial(shotSplitter_getTextFieldInfo, None, 3, "shotSplitterOutput"))
    
    
    
    #create the process button
    cmds.separator(h = 15, style = "none", parent = mainLayout)

    cmds.button(label = "Process", parent = mainLayout, w = 400, h = 50, c = shotSplitter_process)
    
    
    
    #show the window
    cmds.showWindow(window)
    

def shotSplitter_getTextFieldInfo(fileFilter, fileMode, textField, *args):
    
    filePath =  cmds.fileDialog2(fileMode= fileMode, dialogStyle=2, fileFilter = fileFilter)[0]
    filePath = str(filePath)
    
    cmds.textField(textField, edit = True, text = filePath)
    return filePath


def shotSplitter_process(*args):
    start_time = time.time()
    
    #get info
    textFile = cmds.textField("shotSplitterTextFile", q = True, text = True)
    audioFile = cmds.textField("shotSplitterAudioFile", q = True, text = True)
    #fbxFile = cmds.textField("shotSplitterFBXFile", q = True, text = True)
    output = cmds.textField("shotSplitterOutput", q = True, text = True)
    
    mel.eval('trace "Getting Data from text file..." ;')
    #get shot data from the text file
    shotInfo = shotSplitter_getShotData(textFile)
    
    
    #import in the audio file
    #if audioFile != "":
        #mel.eval('trace "Importing audio..." ;')
        #cmds.file(audioFile, i = True, type = "audio")
        
        ##offset audio 300 frames
        #firstShot = shotInfo[0]
        #offsetValue = firstShot[1]
        #audioFileName = audioFile.rpartition("/")[2].partition(".")[0]
        #cmds.setAttr(audioFileName + ".offset", offsetValue)
    
    
    #import in the fbx file and set it up
    #mel.eval('trace "Setting up matinee FBX file..." ;')
    cmds.unloadPlugin(  "fbxmaya.mll" )
    cmds.loadPlugin(  "fbxmaya.mll" )
    shotSplitter_setupFBXFile()
    

    #snap the characters to their spots and orient everything correctly
    #mel.eval('trace "Snapping Characters..." ;')
    #shotSplitter_setupCharacters()
    

    #save out individual shot files
    mel.eval('trace "Saving out master file..." ;')
    masterFile = shotSplitter_SaveMaster()
    
    i = 0
    sceneFrameEnd = shotSplitter_findSceneFrameRange()
    
    for shot in shotInfo:
        shotNum = shot[0]
        mel.eval('trace "Creating shot %s" ;' %str(shotNum))
        shotFrameStart = shot[1]
        try:
            shotFrameEnd = shotInfo[i + 1][1]
            
        except IndexError:
            print "Last shot. Using Scene Frame Range"
            shotFrameEnd = sceneFrameEnd
            
        shotCamera = shot[2]
        shotName = shot[3]
        
        
        #set frame range, set camera, save out shot
        cmds.playbackOptions(min = shotFrameStart, max = shotFrameEnd, ast = shotFrameStart, aet = shotFrameEnd)
        
        if cmds.objExists(shotCamera):
            cameraName = cmds.listRelatives(shotCamera, children = True)[0]
            
            #debug print
            print(cameraName)
            
            #if shake is in the name
            #select the non shake camera
            if 'Shake' in cameraName:
                print('changing name')
                cameraName = cameraName.partition('_Shake')[0]
                
            #debug print            
            print('changing the camera name to ' + cameraName)
            
                
            newCamName = cmds.rename(cameraName, "SHOT_CAM")
            
            #lock the camera
            cmds.setAttr(newCamName + ".tx", lock = True)
            cmds.setAttr(newCamName + ".ty", lock = True)
            cmds.setAttr(newCamName + ".tz", lock = True)
            cmds.setAttr(newCamName + ".rx", lock = True)
            cmds.setAttr(newCamName + ".ry", lock = True)
            cmds.setAttr(newCamName + ".rz", lock = True)
            cmds.setAttr(newCamName + ".sx", lock = True)
            cmds.setAttr(newCamName + ".sy", lock = True)
            cmds.setAttr(newCamName + ".sz", lock = True)
            
        
        #This will get replaced when the new text file info is in
        
        f = open(textFile, "r")
        lines=f.readlines()
        f.close()
        
        #find the cine name
        cineFullName = lines[1]
        cineName = cineFullName.partition(": ")[2]
        cineName = cineName.rstrip("\r\n")
    
        #chop off excess keys
        cmds.select(all = True)

        
        try:
            cmds.cutKey(option="keys", time = (0, shotFrameStart - 30))
            cmds.cutKey(option="keys", time = (shotFrameEnd + 30, 120000))
        except:
            pass
        

        #save out the shot
        if shotName.find("<Unknown>") == -1:
            newFileName = (output + "/" + cineName + "_" + shotName + ".mb")
            cmds.file(rename = newFileName)
            shotFile = cmds.file(save=True, type='mayaBinary', force = True)
        
            #rename the shot cam back
            if cmds.objExists(shotCamera):
                cmds.rename(newCamName, cameraName)
        
        #iterate i
        i = i + 1
        
        #open up the master file
        cmds.file(masterFile, open = True, force = True, prompt = False)
        
    
    
    elapsed_time = time.time() - start_time
    timeInMins = str((elapsed_time/60))[0:4]
    minutes = str(timeInMins).partition(".")[0]
    seconds = str(timeInMins).rpartition(".")[2]
    seconds = ("." + seconds)
    seconds = float(seconds) * 60
    seconds = str(seconds)[0:2]
    finalTime = "Total Time: " + minutes + " minutes and " + seconds + " seconds"
    
    result = cmds.confirmDialog(title = "Shot Splits Complete", message = "Yay! Your shots have been generated! Total time: " + minutes + " minutes and " + seconds + " seconds", button = ["Close"], cancelButton = "Close", dismissString = "Close")

        
    

def shotSplitter_findSceneFrameRange():
    endFrame = cmds.playbackOptions(q = True, max = True)
    endFrame = int(endFrame)
    return endFrame
    
def shotSplitter_SaveMaster():
    
    #first, save the current file in the output directory as the master file.
    
    #open the text file, grab all the lines, and close it
    textFile = cmds.textField("shotSplitterTextFile", q = True, text = True)
    output = cmds.textField("shotSplitterOutput", q = True, text = True)
    
    
    f = open(textFile, "r")
    lines=f.readlines()
    f.close()
    
    #find the cine name
    cineFullName = lines[1]
    cineName = cineFullName.partition(": ")[2]
    cineName = cineName.rstrip("\r\n")
    
    fileName = (output + "/" + str(cineName) + "_Master.mb")
    cmds.file( rename= fileName  )
    cmds.file( save=True, type='mayaBinary', force = True)
    
    return fileName
    
    
def shotSplitter_getShotData(textFile):
    
    #open the text file, grab all the lines, and close it
    f = open(textFile, "r")
    lines=f.readlines()
    f.close()
    
    #find the cine name
    cineFullName = lines[1]
    cineName = cineFullName.partition(": ")[2]
    
    #compile a list of the shots
    shotInfo = []
    for line in lines:
        if re.search("CameraGroup:", line):
            shot = line.partition(": ")[2].partition(",")[0]
            
            timeStart = line.partition("Time: ")[2].partition(",")[0]
            timeStart = float(timeStart) * 30
            timeStart = int(timeStart)
            
            camera = line.partition("CameraGroup: ")[2].partition(",")[0]
            camera = camera.rstrip("\r\n")
            
            shotName = line.partition("ShotName: ")[2]
            shotName = shotName.rstrip("\r\n")
            
            shotInfo.append([shot, timeStart, camera, shotName])
            
    
    return shotInfo


def shotSplitter_setupFBXFile():
    
    #Now let's import the fbx file from matinee
    #cmds.file(fbxFile, i = True, ignoreVersion = True, force = True)
    
    #Now let's clean up the imported fbx assets
    mel.eval('trace "Cleaning and organizing scene..." ;')
    cmds.select(all = True)
    selection = cmds.ls(sl = True, transforms = True)
    cmds.select(clear = True)
    
    
    #create some groups to put things in
    matineeGroup = cmds.group(empty = True, name = "Matinee_Group")
    interpActorGroup = cmds.group(empty = True, name = "InterpActors_Group")
    worldGeoGroup = cmds.group(empty = True, name = "WorldGeo_Group")
    fxGroup = cmds.group(empty = True, name = "FX_And_Lighting_Group")
    camGroup = cmds.group(empty = True, name = "Cam_Group")
    miscGroup = cmds.group(empty = True, name = "Misc_Group")
    
    cmds.parent(interpActorGroup, matineeGroup)
    cmds.parent(worldGeoGroup, matineeGroup)
    cmds.parent(fxGroup, matineeGroup)
    cmds.parent(camGroup, matineeGroup)
    cmds.parent(miscGroup, matineeGroup)
    
    cmds.select(clear = True)
    
    for each in selection:
        
        if re.search("(.*?)InterpActor", each):
            cmds.parent(each, interpActorGroup)
        
        if re.search("(.*?)Light", each):
            cmds.parent(each, fxGroup) 
            
        if re.search("(.*?)Emitter", each):
            cmds.parent(each, fxGroup) 
            
        if re.search("(.*?)Mesh", each):
            cmds.parent(each, worldGeoGroup) 
            
        if re.search("(.*?)Camera", each):
            cmds.parent(each, camGroup)
            
        
    #Now select all again, toggle off matinee group, and put anything left in misc group
    cmds.select(all = True)
    cmds.select("Matinee_Group", tgl = True)
    selection = cmds.ls(sl = True, transforms = True)
    cmds.select(clear = True)
    
    for each in selection:
        if each.find(":") == -1:
            cmds.parent(each, miscGroup)
        

def shotSplitter_setupCharacters():
    
    cmds.select("*:Master_Anim")
    selection = cmds.ls(sl = True)
    
    namespaces = []
    
    for each in selection:
        namespace = each.partition(":")[0]
        namespaces.append(namespace)
        
    cmds.select(clear = True)
    
    for name in namespaces:
        #find fbx file
        actorNode = cmds.textField((name + "_mocapTextField"), q = True, text = True).rpartition("/")[2].partition(".")[0]
        characterNamespace = cmds.frameLayout((name + "_frameLayout"), q = True, label = True)
        
        if cmds.objExists(actorNode):
            cmds.pointConstraint(actorNode, (characterNamespace + ":Master_Anim"))
            orientConstraint = cmds.orientConstraint(actorNode, (characterNamespace + ":Master_Anim"))[0]
            
            cmds.setAttr((orientConstraint + ".offsetZ"), 90)
            
