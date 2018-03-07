import maya.cmds as cmds
from PySide import QtGui
import maya.OpenMayaUI as mui
import shiboken

#Written by: Charles R. Anderson
#Dec. 4, 2015 
#Questions or comments email me: charles.anderson@epicgames.com


# This function finds the main Maya Window
def getMayaWindow():
    pointer = mui.MQtUtil.mainWindow()
    return shiboken.wrapInstance(long(pointer), QtGui.QWidget)

# This function finds the cluster for the passed in mesh.
def findCluster(meshName):
    skinClusters = []
    skinHistory = []
    skinHistory = cmds.listHistory(meshName, lv=0)
    for node in skinHistory:
        if cmds.nodeType(node) == "skinCluster":
            skinClusters.append(node)

    return skinClusters[0]

# This function transfers the weighting for each mesh in the meshes list for the influences in the jointsToRemove list to the jointToTransferTo joint.
def LOD_transferWeights(meshes, jointsToRemove, jointToTransferTo, *args):
    for mesh in meshes:
        #print "MESH: ", mesh
        
        # Find the skin cluster for the current mesh
        cluster = findCluster(mesh)
        #print "CLUSTER: ", cluster
        
        # Prune weights on the current mesh
        cmds.skinPercent(cluster, mesh, prw=0.001)

        # Find all of the current influences on the current skin cluster.
        meshInfluences = cmds.skinCluster(cluster, q=True, inf=True)
        #print "Current Influences: ", meshInfluences

        for joint in jointsToRemove:
            if joint in meshInfluences:
                #print "Current Joint: ", joint
                
                # If the jointToTransferTo is not already an influence on the current mesh then add it.
                currentInfluences = cmds.skinCluster(cluster, q=True, inf=True)
                if jointToTransferTo not in currentInfluences:
                    cmds.skinCluster(cluster, e=True, ai=jointToTransferTo, lw=False)

                # Now transfer all of the influences we want to remove onto the jointToTransferTo.
                for x in range(cmds.polyEvaluate(mesh, v=True)):
                    #print "TRANSFERRING DATA....."
                    value = cmds.skinPercent(cluster, (mesh+".vtx["+str(x)+"]"), t=joint, q=True)
                    if value > 0:
                        cmds.skinPercent(cluster, (mesh+".vtx["+str(x)+"]"), tmw=[joint, jointToTransferTo])

        # Remove unused influences
        currentInfluences = cmds.skinCluster(cluster, q=True, inf=True)
        #print "Current Influences: ", currentInfluences
        influencesToRemove = []
        weightedInfs = cmds.skinCluster(cluster, q=True, weightedInfluence=True)
        #print "Weighted Influences: ", weightedInfs
        for inf in currentInfluences:
            #print "Influence: ", inf
            if inf not in weightedInfs:
                #print "Update Influences to Remove List: ", inf
                influencesToRemove.append(inf)
            
        #print "ToRemove Influences: ", influencesToRemove
        if influencesToRemove != []:
            for inf in influencesToRemove:
                cmds.skinCluster(cluster, e=True, ri=inf)


########  Example
'''
meshes = ["hands_geo_LOD4"]
jointsToRemove = ["index_01_r", "index_02_r", "middle_01_r", "middle_02_r", "pinky_01_r", "pinky_02_r", "ring_01_r", "ring_02_r", "thumb_01_r", "thumb_02_r"]
jointToTransferTo = "hand_r"
LOD_transferWeights(meshes, jointsToRemove, jointToTransferTo)
'''
##################






# THIS IS OLD CODE THAT I WAS USING TO CREATE A UI FOR TRANSFERING WEIGHTS.  MAY STILL USE SOME OF IT FOR A TOOL TO CREATE THE REMOVE BONES LIST.
'''
def create_LOD_TransferWeightsUI():

    # check for existing window.
    windowName = "LOD_TransferWeightsWindow"
    if cmds.window("LOD_TransferWeightsWindow", exists=True):
        cmds.deleteUI("LOD_TransferWeightsWindow", wnd=True)
        
    # Create a window
    mainMayaWindow = getMayaWindow()
    LOD_TransferWeightsWindow = QtGui.QMainWindow(mainMayaWindow)
    LOD_TransferWeightsWindow.setObjectName(windowName)

    # Create a font
    font = QtGui.QFont()
    font.setPointSize(20)
    font.setBold(True)

    # create a widget
    widget = QtGui.QWidget()
    LOD_TransferWeightsWindow.setCentralWidget(widget)

    # create a layout
    layout = QtGui.QVBoxLayout(widget)

    # create a button and add it to the layout
    button = QtGui.QPushButton("Create Locator")
    layout.addWidget(button)
    button.setFont(font)
    button.setMinimumSize(200, 40)
    button.setMaximumSize(200, 40)
    imagePath = cmds.internalVar(upd=True) + "icons/extractDeltasDuplicate.png"
    button.setStyleSheet("background-image: url("+imagePath+"); border:solid black 1px; color")
    button.clicked.connect(LOD_transferWeights)

    # Create a close button
    closeButton = QtGui.QPushButton("Close")
    layout.addWidget(closeButton)
    closeButton.setFont(font)
    closeButton.setMinimumSize(200, 40)
    closeButton.setMaximumSize(200, 40)
    imagePath = cmds.internalVar(upd=True) + "icons/extractDeltasDuplicate.png"
    closeButton.setStyleSheet("background-image: url("+imagePath+"); border:solid black 1px")
    closeButton.clicked.connect(LOD_TransferWeightsWindow.close)


    # show the button
    LOD_TransferWeightsWindow.show()

create_LOD_TransferWeightsUI()



def OutlinerSelection():
    itemSelection = cmds.ls(sl=True, type="joint")
    sc = cmds.selectionConnection(lst=True)
    if len(itemSelection):
        for node in itemSelection:
            print node
            cmds.selectionConnection(sc, e=True, select=node)

        # Create a window layout to hold outliner
        newWindow = cmds.window(title="Outliner (Custom)", iconName="Outliner*", rtf=0, h=800, w=500)
        frame = cmds.frameLayout(labelVisible = False)
        oped = cmds.outlinerEditor("jointOutliner", showSetMembers=True, unParent=True, mainListConnection=sc, selectionConnection="modelList", eo=True)
        testWin = cmds.outlinerEditor(oped, e=True, parent=frame)

        # scriptJob to delete selectionConnection
        print "Deleting of the connection"
        cmds.scriptJob(uiDeleted = [newWindow, "cmds.deleteUI('"+sc+"')"])

        cmds.showWindow(newWindow)

    else:
        cmds.warning("Nothing Selected! Custom Outliner will not be created")

OutlinerSelection() 

jointList = cmds.ls(sl=True, type="joint")
print jointList
'''