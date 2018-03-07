import maya.cmds as cmds
import os
from functools import partial


def UI():
    
    #check to see if the window exists, if so delete it
    if cmds.window("je_shotReference_UI", exists = True):
        cmds.deleteUI("je_shotReference_UI")
        
    #create the window    
    window = cmds.window("je_shotReference_UI", title = "Shot Reference", w = 400, h = 200, sizeable = False, mxb= False, mnb = False)
    
    #create the mainLayout
    mainLayout = cmds.columnLayout(w = 400, h = 200)
    
    #create a row column layout
    rowColumnLayout = cmds.rowColumnLayout(nc =2, cw = [(1, 300), (2, 100)], columnOffset = [(1,"both", 5), (2, "both", 5)])
    

    #Create fields for the text file
    
    cmds.separator(h = 10, style = "none")
    cmds.separator(h = 10, style = "none")
    
    cmds.text(label = "Reference Quicktime Movie:", align = "left")
    cmds.text(label = "")
    
    cmds.textField("refQuickTimeMovieTextField", w = 300)
    cmds.button(label = "Browse", w = 50, c = partial(shotReferenceTool_getTextFieldInfo, "*.mov", 1, "refQuickTimeMovieTextField"))

    
    cmds.separator(h = 10, style = "none")
    cmds.separator(h = 10, style = "none")
    
    
    #create a new row column layout for the frame offset values
    frameOffsetRowColumnLayout = cmds.rowColumnLayout(nc =4, cw = [(1, 100), (2, 100), (3, 100), (4, 100)], columnOffset = [(1,"both", 5), (2, "both", 5), (3, "both", 5), (4, "both", 5)],parent = mainLayout)
    
    #create the frame offset fields
    cmds.text(label = "Frame Offset: ")
    cmds.textField("frameOffsetTextField", w = 100)
    cmds.text(label = "")
    cmds.text(label = "")
    
    

    
    #create the process button
    cmds.separator(h = 15, style = "none", parent = mainLayout)

    cmds.button(label = "Import Reference Video", parent = mainLayout, w = 400, h = 50, c = importReferenceVideo_Process)
    
    
    
    #show the window
    cmds.showWindow(window)
    
    
    

def shotReferenceTool_getTextFieldInfo(fileFilter, fileMode, textField, *args):
    
    filePath =  cmds.fileDialog2(fileMode= fileMode, dialogStyle=2, fileFilter = fileFilter)[0]
    filePath = str(filePath)
    
    cmds.textField(textField, edit = True, text = filePath)
    return filePath

def importReferenceVideo_Process(*args):
    
    videoFile = cmds.textField("refQuickTimeMovieTextField", q = True, text = True)
    videoNiceName = videoFile.rpartition("/")[2].partition(".")[0]
    
    #create the polygon plane
    moviePlane = cmds.polyPlane(w = 720, h = 480, sx = 1, sy = 1, name = ( videoNiceName + "_ShotReferencePlane"))[0]
    cmds.select(moviePlane)
    cmds.addAttr(longName='transparency', defaultValue=0, minValue=0, maxValue=1, keyable = True)

    cmds.createDisplayLayer(name = ( videoNiceName + "_Layer"), nr = True)
    
    cmds.select(clear = True)

    cmds.setAttr(moviePlane + ".tz", -10000)
    cmds.setAttr(moviePlane + ".rx", -180)
    cmds.makeIdentity(moviePlane, apply = True, r = 1, t = 1, s = 1)

    
    #create the camera for the plane
    camera = cmds.camera()[0]
    cam = cmds.rename(camera, (videoNiceName + "_Reference_Cam"))
    
    #position the camera
    constraint = cmds.parentConstraint(moviePlane, cam)[0]
    cmds.delete(constraint)
    
    cmds.setAttr(cam + ".rotateX", 90)
    cmds.setAttr(cam + ".translateY", -800)
    cmds.setAttr(cam + "Shape.focalLength", 38)
    
    #constrain the camera to the plane
    cmds.parentConstraint(moviePlane, cam, mo = True)
    
    #scale up the plane a tad, and lock all attrs
    cmds.setAttr(moviePlane + ".sx", 1.055, lock = True)
    cmds.setAttr(moviePlane + ".sy", 1.055, lock = True)
    cmds.setAttr(moviePlane + ".sz", 1.055, lock = True)
    cmds.setAttr(moviePlane + ".tx", lock = True)
    cmds.setAttr(moviePlane + ".ty", lock = True)
    cmds.setAttr(moviePlane + ".tz", lock = True)
    cmds.setAttr(moviePlane + ".rx", lock = True)
    cmds.setAttr(moviePlane + ".ry", lock = True)
    cmds.setAttr(moviePlane + ".rz", lock = True)
    
    #lock down the camera attrs
    cmds.setAttr(cam + ".tx", lock = True)
    cmds.setAttr(cam + ".ty", lock = True)
    cmds.setAttr(cam + ".tz", lock = True)
    cmds.setAttr(cam + ".rx", lock = True)
    cmds.setAttr(cam + ".ry", lock = True)
    cmds.setAttr(cam + ".rz", lock = True)
    cmds.setAttr(cam + ".sx", lock = True)
    cmds.setAttr(cam + ".sy", lock = True)
    cmds.setAttr(cam + ".sz", lock = True)
    
    
    #create the movie shader/texture and attach to plane
    movieShader = cmds.shadingNode("lambert", asShader = True, name = moviePlane + "_M")
    moviePlaceNode = cmds.shadingNode("place2dTexture", asUtility = True)
    movieNode = cmds.shadingNode("movie", asTexture = True)
    
    cmds.connectAttr(moviePlaceNode + ".outUV", movieNode + ".uvCoord")
    cmds.connectAttr(moviePlaceNode + ".outUvFilterSize", movieNode + ".uvFilterSize")
    cmds.connectAttr(movieNode + ".outColor", movieShader + ".color")
    
    #connect the transparency of the shader to the plane attr
    cmds.connectAttr(moviePlane + ".transparency", movieShader + ".transparencyR")
    cmds.connectAttr(moviePlane + ".transparency", movieShader + ".transparencyG")
    cmds.connectAttr(moviePlane + ".transparency", movieShader + ".transparencyB")
    
    
    cmds.setAttr(movieNode + ".fileTextureName", videoFile, type = "string")
    
    #add the material to the movie plane
    cmds.select(moviePlane)
    cmds.hyperShade(assign = movieShader)
    cmds.select(clear = True)
    
    cmds.setAttr(movieNode + ".useFrameExtension", 1)
    
    #make sure that the playback options are updating all viewports
    cmds.playbackOptions(v = "all")
    
    
    #take the frame offset value, and create an expression
    frameOffsetValue = cmds.textField("frameOffsetTextField", q = True, text = True)
    if frameOffsetValue != "":
        expressionString = (movieNode + ".frameExtension = frame + " + str(frameOffsetValue))
        expression = cmds.expression(name = (movieNode + "_expression"), string = expressionString)
        
    else:
        expressionString = (movieNode + ".frameExtension = frame")
        expression = cmds.expression(name = (movieNode + "_expression"), string = expressionString)
        
        
def listAllReferenceMovies_UI():
    
    #check to see if the window exists, if so delete it
    if cmds.window("je_listAllReferenceMovies_UI", exists = True):
        cmds.deleteUI("je_listAllReferenceMovies_UI")
        
    #create the window    
    window = cmds.window("je_listAllReferenceMovies_UI", title = "Shot Reference Movies", w = 400, h = 200, sizeable = False, mxb= False, mnb = False)
    
    #create the mainLayout
    mainLayout = cmds.columnLayout(w = 400, h = 200)
    
    #create a dropdown menu of all the movies in the scene
    cmds.text(label = "Movies In Scene: ", align = 'left')
    
    cmds.separator(h = 10, style = "none", parent = mainLayout)
    optionMenu = cmds.optionMenu("referenceMoviesOptionMenu", w = 400)
    cmds.separator(h = 20, style = "none", parent = mainLayout)
    
    
    
    #find all the movies in the scene
    movieFiles = []
    
    try:
        cmds.select("*ShotReferencePlane_M*")
        
    except ValueError:
        print "No Reference Videos in Scene"
        return
    
    movieShaders = cmds.ls(sl = True)
    
    for each in movieShaders:
        connections = cmds.listConnections(each, connections = True)
        
        for connection in connections:
            if connection.find("movie") == 0:
                movieNode = connection
                
                file = cmds.getAttr(movieNode + ".fileTextureName")
                movieFiles.append(file)
                
    
    if len(movieFiles) > 0:
        for file in movieFiles:
            cmds.menuItem( label= file, parent =  "referenceMoviesOptionMenu")
            
    else:
        cmds.warning( "No Movies in Scene")
        return
        

    
    
    #create a new row column layout for the frame offset values
    frameOffsetRowColumnLayout = cmds.rowColumnLayout(nc =4, cw = [(1, 100), (2, 100), (3, 100), (4, 100)], columnOffset = [(1,"both", 5), (2, "both", 5), (3, "both", 5), (4, "both", 5)],parent = mainLayout)
    
    
    #create the frame offset fields
    cmds.text(label = "Frame Offset: ", align = 'left')
    cmds.textField("movieFilesFrameOffsetTextField", w = 100)
    cmds.text(label = "")
    cmds.checkBox("hideVideoCheckBox", label = "Toggle Video", v = 0, cc = hideVideo)
    
    
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")

    
    
    #create an optionMenu for attaching reference plane to another camera
    cmds.text(label = "Attach to Camera: ", align = 'left')
    cameraOptionMenu = cmds.optionMenu("cameraOptionMenu", w = 100)
    cmds.menuItem(label = "None")
    
    #get all the cameras and populate the option menu
    cameras = cmds.ls(type = 'camera')
    
    for camera in cameras:
        cam = cmds.listRelatives(camera, parent = True)[0]
        cmds.menuItem(label = str(cam), parent = cameraOptionMenu)
        
    cmds.text(label = "")
    cmds.text(label = "")
    
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")
    cmds.separator(h = 20, style = "none")
    
    
    
    #create an optionMenu for PiP location
    cmds.text(label = "PiP Location: ", align = 'left')
    pipOptionMenu = cmds.optionMenu("pipOptionMenu", w = 100)
    cmds.menuItem(label = "Bottom Left Corner")
    cmds.menuItem(label = "Bottom Right Corner")
    cmds.menuItem(label = "Top Left Corner")
    cmds.menuItem(label = "Top Right Corner")
    


    cmds.text(label = "")
    cmds.text(label = "")
    

    #create the process button
    cmds.separator(h = 15, style = "none", parent = mainLayout)

    cmds.button(label = "Apply Settings", parent = mainLayout, w = 400, h = 50, c = applyFrameOffsetToSelected)
    cmds.button(label = "Delete Selected Video", parent = mainLayout, w = 400, h = 50, c = partial(deleteSelectedMovie, file))

    
    
    #show the window
    cmds.showWindow(window)
    
    
def deleteSelectedMovie(file, *args):
    
    result = cmds.confirmDialog( title='Confirm', message='Are you sure?', button=['Yes','No'], defaultButton='Yes', cancelButton='No', dismissString='No' )
    
    if result == "Yes":
        name = file.rpartition("/")[2].partition(".")[0]
        shader = (name + "_ShotReferencePlane_M")
        plane = (name + "_ShotReferencePlane")
        layer = (name + "_Layer")
        camera = (name + "_Reference_Cam")
        cmds.delete([shader, plane, layer, camera])
        
    else:
        return
    
    
def applyFrameOffsetToSelected(*args):
    selectVideo = cmds.optionMenu("referenceMoviesOptionMenu", q = True, v = True)
    PiPLoc = cmds.optionMenu("pipOptionMenu", q = True, v = True)
    videoNiceName = selectVideo.rpartition("/")[2].partition(".")[0]
    
    if selectVideo == "":
        return
    
    #find the movie node that has the matching video file
    cmds.select("*ShotReferencePlane_M*")
    movieShaders = cmds.ls(sl = True)
    
    for each in movieShaders:
        connections = cmds.listConnections(each, connections = True)
        
        for connection in connections:
            if connection.find("movie") == 0:
                movieNode = connection
                
                file = cmds.getAttr(movieNode + ".fileTextureName")
                if file == selectVideo:

                    
                    #find offset value
                    frameOffsetValue = cmds.textField("movieFilesFrameOffsetTextField", q = True, text = True)
                    
                    if frameOffsetValue != "":
                    
                        #delete the existing expression
                        cmds.delete(movieNode + "_expression")
                        
                    
                        #create the new expression
                        expressionString = (movieNode + ".frameExtension = frame + " + str(frameOffsetValue))
                        expression = cmds.expression(name = (movieNode + "_expression"), string = expressionString)
                    
                    
    
    #If the attach to camera is not None, then let's set that up now
    attachToCamera = cmds.optionMenu("cameraOptionMenu", q = True, v = True)
    
    if attachToCamera != "None":
        
        #duplicate our plane, and position it to the selected camera
        
        if cmds.objExists(("ShotReferencePlane_" + attachToCamera + "_Grp")):
            cmds.delete(("ShotReferencePlane_" + attachToCamera + "_Grp"))
            
            
        dupePlane = cmds.duplicate((videoNiceName + "_ShotReferencePlane"), name = ("ShotReferencePlane_" + attachToCamera), rr = True)[0]
        
        cmds.setAttr(dupePlane + ".tx", lock = False)
        cmds.setAttr(dupePlane + ".ty", lock = False)
        cmds.setAttr(dupePlane + ".tz", lock = False)
        
        cmds.setAttr(dupePlane + ".rx", lock = False)
        cmds.setAttr(dupePlane + ".ry", lock = False)
        cmds.setAttr(dupePlane + ".rz", lock = False)
        
        cmds.setAttr(dupePlane + ".sx", lock = False)
        cmds.setAttr(dupePlane + ".sy", lock = False)
        cmds.setAttr(dupePlane + ".sz", lock = False)
        
        
        dupePlaneGroup = cmds.group(empty = True, name = ("ShotReferencePlane_" + attachToCamera + "_Grp"))
        
        constraint = cmds.parentConstraint(dupePlane, dupePlaneGroup)
        cmds.delete(constraint)
        cmds.parent(dupePlane, dupePlaneGroup)
        cmds.makeIdentity(dupePlane, apply = True, t = 1, r = 1, s = 0)
        
        
        #constrain the dupePlane to the camera
        pointConstraint = cmds.pointConstraint(attachToCamera, dupePlaneGroup)[0]
        orientConstraint = cmds.orientConstraint(attachToCamera, dupePlaneGroup)[0]
        
        #offset the constraint values based on the PiP location set
        cmds.setAttr((orientConstraint + ".offsetX"), -90)
        
        
        #scale down the plane and offset it back
        cmds.setAttr(dupePlane + ".sx", .008)
        cmds.setAttr(dupePlane + ".sy", .008)
        cmds.setAttr(dupePlane + ".sz", .008)
        
        if attachToCamera.find("SHOT_CAM") != -1:
            cmds.setAttr(dupePlane + ".ty", 30)
            
        if attachToCamera.find("Face_Cam") != -1:
            cmds.setAttr(dupePlane + ".ty", 10)
        
        
        if PiPLoc == "Bottom Right Corner":
            cmds.setAttr(dupePlane + ".tx", 3.25)
            cmds.setAttr(dupePlane + ".tz", -2)
            
        if PiPLoc == "Bottom Left Corner":
            cmds.setAttr(dupePlane + ".tx", -3.25)
            cmds.setAttr(dupePlane + ".tz", -2)
            
        if PiPLoc == "Top Right Corner":
            cmds.setAttr(dupePlane + ".tx", 3.25)
            cmds.setAttr(dupePlane + ".tz", 2)
            
        if PiPLoc == "Top Left Corner":
            cmds.setAttr(dupePlane + ".tx", -3.25)
            cmds.setAttr(dupePlane + ".tz", 2)
            
            
        #delete the constraints
        cmds.delete([pointConstraint, orientConstraint])
        
        #create a parent constraint on the group to the cam
        cmds.parentConstraint(attachToCamera, dupePlaneGroup, mo = True)
        
        
        #clear selection
        cmds.select(clear = True)
        
        
        
def hideVideo(*args):
    
    selectVideo = cmds.optionMenu("referenceMoviesOptionMenu", q = True, v = True)
    videoNiceName = selectVideo.rpartition("/")[2].partition(".")[0]
    
    videoLayer = videoNiceName + "_Layer"
    
    vis = cmds.checkBox("hideVideoCheckBox", q = True, v = True)
    
    if vis == True:
        cmds.setAttr(videoLayer + ".visibility", 0)
        
    if vis == False:
        cmds.setAttr(videoLayer + ".visibility", 1)
        
    
    

