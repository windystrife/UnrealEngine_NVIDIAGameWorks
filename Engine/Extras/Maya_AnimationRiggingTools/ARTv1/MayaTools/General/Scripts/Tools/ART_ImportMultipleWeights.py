import maya.cmds as cmds
import os, cPickle
from PySide import QtGui
import maya.OpenMayaUI as mui
import shiboken
from PySide import QtCore
from functools import partial

#############################################################################################
#############################################################################################
#############################################################################################
# Select all of the meshes in your scene that you want to import onto.  
# If there is a .txt file in the SkinWeights folder on disk that matches its name it will import the weights onto the mesh.
def importMultipleSkinWeights(suffix, rui, *args):
    
    #tools path
    toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
    if os.path.exists(toolsPath):

        f = open(toolsPath, 'r')
        mayaToolsDir = f.readline()
        f.close()	


    selection = cmds.ls(sl = True)
    if selection == []:
        cmds.confirmDialog(icon = "warning", title = "Import Skin Weights", message = "Nothing Selected")
        return
        
    warnings = False

    if selection != []:
        #find all files in the system dir
        path = mayaToolsDir + "\General\ART\SkinWeights\\"
        print "path is "+path
        
        for one in selection:
            '''
            if len(selection) > 1:
                cmds.confirmDialog(icon = "warning", title = "Import Skin Weights", message = "Too many objects selected. Please only selected 1 mesh.")
                return
            '''
            #try:
                #fileToImport = cmds.fileDialog2(fileFilter="*.txt", dialogStyle=2, fm = 1, startingDirectory = path)[0]

            #except:
                #cmds.warning("Operation Cancelled.")
                #return
            filename = one
            print "SUFFIX: ", suffix
            if suffix:
                filename = one.partition(suffix)[0]
            
            print "FILENAME: ", filename

            fileToImport = path+filename+".txt"
            if cmds.file(fileToImport, q=True, ex=True):
                print "file is "+fileToImport

                #load the file with cPickle and parse out the information
                file = open(fileToImport, 'r')
                weightInfoList = cPickle.load(file)

                #get the number of vertices in the mesh. This will be used to update our progress window
                verts = cmds.polyEvaluate(one, vertex = True)
                if verts < 20:
                    verts = verts * 20

                increment = int(verts/20)
                matchNum = increment

                #find the transforms(joints) from the weightInfoList. format = [ [vertNum, [jointName, value], [jointName, value] ] ]
                transforms = []
                newList = weightInfoList[:]
                for info in newList:

                    for i in info:
                        if i != info[0]:
                            transform = i[0]
                            transforms.append(transform)

                #remove duplicates from the transforms(joints) list
                transforms = set(transforms)
                print "Files Transforms List:"
                print transforms

                #clear selection. Add transforms(joints) to selection and geo
                cmds.select(clear = True)
                for t in transforms:
                    if cmds.objExists(t):
                        cmds.select(t, add = True)


                #check if the geometry that is selected is already skinned or not
                skinClusters = cmds.ls(type='skinCluster')
                skinnedGeometry = []
                for cluster in skinClusters:
                    geometry = cmds.skinCluster(cluster, q = True, g = True)[0]
                    geoTransform = cmds.listRelatives(geometry, parent = True)[0]
                    skinnedGeometry.append(geoTransform)

                    if geoTransform == one:
                        skin = cluster
                        
                        currentTransforms = cmds.skinCluster(skin, q=True, inf=True)
                        print "Current Transforms List:"
                        print currentTransforms
                        
                        for tr in transforms:
                            if tr not in currentTransforms:
                                cmds.skinCluster(skin, e=True, ai=tr, lw=False)
                                print (cmds.skinCluster(skin, q=True, inf=True))


                #if a skin cluster does not exist already on the mesh:
                if one not in skinnedGeometry:

                    #put a skin cluster on the geo normalizeWeights = 1, dr=4.5, mi = 4,
                    cmds.select(one, add = True)
                    skin = cmds.skinCluster( tsb = True, skinMethod = 0, name = (one +"_skinCluster"))[0]


                #setup a progress window to track length of progress
                cmds.progressWindow(title='Skin Weights', progress = 0, status = "Reading skin weight information from file...")

                #get the vert number, and the weights for that vertex
                for info in weightInfoList:
                    vertNum = info[0]
                    info.pop(0)

                    #progress bar update
                    if vertNum == matchNum:
                        cmds.progressWindow( edit=True, progress = ((matchNum/increment) * 5), status=  "Importing skin weights for " + one + ".\n Please be patient")
                        matchNum += increment

                    #set the weight for each vertex
                    try:
                        cmds.skinPercent(skin, one + ".vtx[" + str(vertNum) + "]", transformValue= info)
                    except:
                        pass

                # remove unused influences
                if rui == 1:
                    infToRemove = []
                    weightedInfs = cmds.skinCluster(skin, q=True, weightedInfluence=True)
                    finalTransforms = cmds.skinCluster(skin, q=True, inf=True)
                    for inf in finalTransforms:
                        if inf not in weightedInfs:
                            infToRemove.append(inf)
                    for rem in infToRemove:
                        cmds.skinCluster(skin, e=True, ri=rem)

                #close the file
                file.close()
            else:
                cmds.warning(fileToImport+" does not exist.")
                warnings = True

            #close progress window
            cmds.progressWindow(endProgress = 1)

    cmds.select(selection)
    if warnings == False:
        cmds.headsUpMessage( "Importing weights is complete!!", time=3.0 )
        #cmds.confirmDialog(icon = "Complete", title = "Import Skin Weights", message = "Importing weights is complete!!")
    elif warnings == True:
        cmds.headsUpMessage( "Importing weights is complete but with Warnings!!", time=3.0 )
        #cmds.confirmDialog(icon = "Complete", title = "Import Skin Weights", message = "Importing weights is complete but with Warnings!!")

    
######################## UI FUNCTIONS ###############################
#-----------------------------------------------------------------------------------------------
# This function finds the main Maya Window
def getMayaWindow():
    pointer = mui.MQtUtil.mainWindow()
    return shiboken.wrapInstance(long(pointer), QtGui.QWidget)

#-----------------------------------------------------------------------------------------------
def resetUI (fromList, toList):
    fromList.clear()
    toList.clear()

#-----------------------------------------------------------------------------------------------
def deleteUI ():
    cmds.deleteUI("importMultipleWeightsWindow", wnd=True)
    
#-----------------------------------------------------------------------------------------------
def importFunction (suffixField, RICheckBox, closeUI):
    suffixField = suffixField.text()
    RICheckBox = RICheckBox.isChecked()

    importMultipleSkinWeights(suffixField, RICheckBox)
    if closeUI == True:
        cmds.deleteUI("importMultipleWeightsWindow", wnd=True)

#-----------------------------------------------------------------------------------------------
def importMultipleWeightsUI(*args):
    # check to see if this window exists and if so delete it.
    objectName = "importMultipleWeightsWindow"
    if cmds.window("importMultipleWeightsWindow", exists=True):
        cmds.deleteUI("importMultipleWeightsWindow", wnd=True)

    # Create the Weights window
    parent = getMayaWindow()
    currentWindow = QtGui.QMainWindow(parent)
    currentWindow.setObjectName(objectName)
    currentWindow.setWindowTitle("Import Multiple Weights Tool")
    currentWindow.resize(50, 50)

    # create a widget
    widget = QtGui.QWidget()
    currentWindow.setCentralWidget(widget)

    # create the Main vertical layout
    mainLayout = QtGui.QVBoxLayout(widget)
    subLayout = QtGui.QHBoxLayout()
    mainLayout.addLayout(subLayout)

    # create a horizontal layout for the lists
    optionsLayout = QtGui.QVBoxLayout()
    mainLayout.addLayout(optionsLayout)

    buttonsLayout = QtGui.QHBoxLayout()
    mainLayout.addLayout(buttonsLayout)
    
    # Suffix UI
    suffixLayout = QtGui.QHBoxLayout()
    optionsLayout.addLayout(suffixLayout)
    textFieldLabel = QtGui.QLabel("Suffix to be removed:")
    suffixLayout.addWidget(textFieldLabel)
    suffixField = QtGui.QLineEdit()
    suffixLayout.addWidget(suffixField)
    suffixField.setPlaceholderText("Enter Suffix...")

    # Remove Unused Influences UI
    RUILayout = QtGui.QHBoxLayout()
    optionsLayout.addLayout(RUILayout)
    RICheckBox = QtGui.QCheckBox("Remove Unused Influences")
    RUILayout.addWidget(RICheckBox)
    RICheckBox.setChecked(True)
    
    # Import Button
    importButton = QtGui.QPushButton("Import")
    buttonsLayout.addWidget(importButton)
    importButton.setMinimumHeight(30)
    importButton.setMaximumHeight(30)
    importButton.setMinimumWidth(200)
    importButton.setToolTip("Adds the selected meshes to the list above.")
    importButton.clicked.connect(partial(importFunction, suffixField, RICheckBox, True))

    # Apply Button
    applyButton = QtGui.QPushButton("Apply")
    buttonsLayout.addWidget(applyButton)
    applyButton.setMinimumHeight(30)
    applyButton.setMaximumHeight(30)
    applyButton.setMinimumWidth(200)
    applyButton.clicked.connect(partial(importFunction, suffixField, RICheckBox, False))

    # Close Button
    closeFromButton = QtGui.QPushButton("Close")
    buttonsLayout.addWidget(closeFromButton)
    closeFromButton.setMinimumHeight(30)
    closeFromButton.setMaximumHeight(30)
    closeFromButton.setMinimumWidth(200)
    closeFromButton.clicked.connect(deleteUI)
    
    # show the window
    currentWindow.show()

#importMultipleWeightsUI()
    
#importMultipleSkinWeights("_LOD4")
