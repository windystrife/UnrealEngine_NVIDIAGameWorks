from PySide import QtGui
from PySide import QtCore
import maya.OpenMayaUI as mui
import shiboken
import maya.cmds as cmds
from functools import partial

#Written by: Charles R. Anderson
#Dec. 4, 2015 
#Questions or comments email me: charles.anderson@epicgames.com

def findCluster(meshName):
    skinClusters = []
    skinHistory = []
    skinHistory = cmds.listHistory(meshName, lv=0)
    for node in skinHistory:
        if cmds.nodeType(node) == "skinCluster":
            skinClusters.append(node)

    return skinClusters[0]
    
#-----------------------------------------------------------------------------------------------
def copyWeights(fromList, toList):
    #filterString = textField.text()
    #print filterString
    if fromList.count() > 0:
        for fromIndex in xrange(fromList.count()):
            fromMesh = fromList.item(fromIndex).text()
            print "From Mesh: ", fromMesh
            if toList.count() > 0:
                for toIndex in xrange(toList.count()):
                    toMesh = toList.item(toIndex).text()
                    if fromMesh.lower() in toMesh.lower():
                        print "Matched To Mesh found: ", toMesh
                        print "Copying weights from ", fromMesh, " to ", toMesh
                        toCluster = findCluster(toMesh)
                        fromCluster = findCluster(fromMesh)
                        cmds.copySkinWeights(ss=fromCluster, ds=toCluster, noMirror=True, surfaceAssociation="closestPoint", influenceAssociation=["closestJoint", "oneToOne", "name"])
            else:
                cmds.warning("Your Copy TO list is empty.")
    else:
        cmds.warning("Your Copy FROM list is empty.")

######################## UI FUNCTIONS ###############################
#-----------------------------------------------------------------------------------------------
# This function finds the main Maya Window
def getMayaWindow():
    pointer = mui.MQtUtil.mainWindow()
    return shiboken.wrapInstance(long(pointer), QtGui.QWidget)

#-----------------------------------------------------------------------------------------------
def addMeshItem (listName):
    listName.clear() # TEMPORARILY clears the list so that we never get dupes.  Figure out how to check for copies before adding and then remove this.
    addItems = cmds.ls(sl=True)
    #for one in addItems:
        #print listName.findItems(one, QtCore.Qt.MatchExactly)
    #for one in addItems:
    listName.addItems(addItems)
	
#-----------------------------------------------------------------------------------------------
def removeMeshItem (listName):
    selected = listName.selectedItems()
    for one in selected:
        index = listName.row(one)
        listName.takeItem(index)

#-----------------------------------------------------------------------------------------------
def clearMeshItems (listName):
    listName.clear()
    	
#-----------------------------------------------------------------------------------------------
def resetUI (fromList, toList):
    fromList.clear()
    toList.clear()

#-----------------------------------------------------------------------------------------------
def createUI(*args):
    # check to see if this window exists and if so delete it.
    objectName = "copyWeightsWindowNew"
    if cmds.window("copyWeightsWindowNew", exists=True):
        cmds.deleteUI("copyWeightsWindowNew", wnd=True)

    # Create the Copy Weights window
    parent = getMayaWindow()
    currentWindow = QtGui.QMainWindow(parent)
    currentWindow.setObjectName(objectName)
    currentWindow.setWindowTitle("Copy Weights Tool")
    currentWindow.resize(500, 700)

    # create a widget
    widget = QtGui.QWidget()
    currentWindow.setCentralWidget(widget)

    # create the Main vertical layout
    mainLayout = QtGui.QVBoxLayout(widget)

    # create a horizontal layout for the lists
    listsLayout = QtGui.QHBoxLayout()
    mainLayout.addLayout(listsLayout)

    # FROM section---------------------
    fromLayout = QtGui.QVBoxLayout()
    listsLayout.addLayout(fromLayout)

    fromListLabel = QtGui.QLabel("Copy from:")
    fromLayout.addWidget(fromListLabel)
    fromList = QtGui.QListWidget()
    fromLayout.addWidget(fromList)
    fromList.setToolTip("This is a list of meshes to copy weights from.")
    fromList.setSelectionMode(QtGui.QAbstractItemView.ExtendedSelection)

    # Add Button
    addFromButton = QtGui.QPushButton("Add Selected")
    fromLayout.addWidget(addFromButton)
    addFromButton.setMinimumHeight(30)
    addFromButton.setMaximumHeight(30)
    addFromButton.setMinimumWidth(200)
    addFromButton.setToolTip("Adds the selected meshes to the list above.")
    addFromButton.clicked.connect(partial(addMeshItem, fromList))

    removeFromButton = QtGui.QPushButton("Remove Selected")
    fromLayout.addWidget(removeFromButton)
    removeFromButton.setMinimumHeight(30)
    removeFromButton.setMaximumHeight(30)
    removeFromButton.setMinimumWidth(200)
    removeFromButton.clicked.connect(partial(removeMeshItem, fromList))

    clearFromButton = QtGui.QPushButton("Clear List")
    fromLayout.addWidget(clearFromButton)
    clearFromButton.setMinimumHeight(30)
    clearFromButton.setMaximumHeight(30)
    clearFromButton.setMinimumWidth(200)
    clearFromButton.clicked.connect(partial(clearMeshItems, fromList))

    # TO section-------------------------
    toLayout = QtGui.QVBoxLayout()
    listsLayout.addLayout(toLayout)

    toListLabel = QtGui.QLabel("Copy to:")
    toLayout.addWidget(toListLabel)
    toList = QtGui.QListWidget()
    toLayout.addWidget(toList)
    toList.setToolTip("This is a list of meshes to copy weights to.")
    toList.setSelectionMode(QtGui.QAbstractItemView.ExtendedSelection)

    # Add Button
    addToButton = QtGui.QPushButton("Add Selected")
    toLayout.addWidget(addToButton)
    addToButton.setMinimumHeight(30)
    addToButton.setMaximumHeight(30)
    addToButton.setMinimumWidth(200)
    addToButton.clicked.connect(partial(addMeshItem, toList))

    removeToButton = QtGui.QPushButton("Remove Selected")
    toLayout.addWidget(removeToButton)
    removeToButton.setMinimumHeight(30)
    removeToButton.setMaximumHeight(30)
    removeToButton.setMinimumWidth(200)
    removeToButton.clicked.connect(partial(removeMeshItem, toList))

    clearToButton = QtGui.QPushButton("Clear List")
    toLayout.addWidget(clearToButton)
    clearToButton.setMinimumHeight(30)
    clearToButton.setMaximumHeight(30)
    clearToButton.setMinimumWidth(200)
    clearToButton.clicked.connect(partial(clearMeshItems, toList))


    # Main widgets ---------------------
    resetButton = QtGui.QPushButton("Reset")
    mainLayout.addWidget(resetButton)
    resetButton.setMinimumHeight(30)
    resetButton.setMaximumHeight(30)
    resetButton.setMinimumWidth(200)
    resetButton.clicked.connect(partial(resetUI, fromList, toList))

    copyButton = QtGui.QPushButton("Copy")
    mainLayout.addWidget(copyButton)
    copyButton.setMinimumHeight(30)
    copyButton.setMaximumHeight(30)
    copyButton.setMinimumWidth(200)
    copyButton.setToolTip("Run through all of the meshes in the FROM list and copy its weights to any meshes that it matches in the TO list.  This matches based on name and is case insensitive.  For example it will find \"Art\" inside of \"fart\".")
    copyButton.clicked.connect(partial(copyWeights, fromList, toList))

    # show the window
    currentWindow.show()

#createUI()

