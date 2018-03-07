import maya.cmds as cmds
import maya.OpenMayaUI as mui
import maya.mel as mel
from PySide import QtGui, QtCore
import shiboken, os, inspect, sys
from functools import partial
import json



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def getMainWindow():
    pointer = mui.MQtUtil.mainWindow()
    return shiboken.wrapInstance(long(pointer), QtGui.QWidget)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def returnNicePath(toolsPath, imagePath):
    image =  os.path.normpath(os.path.join(toolsPath, imagePath))
    if image.partition("\\")[2] != "":
        image = image.replace("\\", "/")
    return image

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def writeQSS(filePath):

    #this function takes the qss file given, and finds and replaces any image path URLs using the user's settings for the icons path and changes the file on disk
    iconPath = os.path.dirname(os.path.normpath(__file__))
    iconPath = returnNicePath(iconPath, "icons")

    print iconPath

    f = open(filePath, "r")
    lines = f.readlines()
    f.close()

    newLines = []
    for line in lines:
        if line.find("url(") != -1:
            oldPath = line.partition("(")[2].rpartition("/")[0]


            newLine =  line.replace(oldPath, iconPath)
            newLines.append(newLine)
        else:
            newLines.append(line)

    f = open(filePath, "w")
    for line in newLines:
        f.write(line)
    f.close()





class RigTools(QtGui.QMainWindow):
    #Original Author: Jeremy Ernst

    def __init__(self, parent = None):

        super(RigTools, self).__init__(parent)

        filePath = os.path.normpath(__file__)
        stylesheet = os.path.join(os.path.dirname(filePath), "mainScheme.qss")
        writeQSS(stylesheet)


        self.buildUI()

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def buildUI(self):


        #create the main widget
        self.mainWidget = QtGui.QWidget()
        self.setCentralWidget(self.mainWidget)

        #load stylesheet
        filePath = os.path.normpath(__file__)
        stylesheet = os.path.join(os.path.dirname(filePath), "mainScheme.qss")
        f = open(stylesheet, "r")
        style = f.read()
        f.close()

        iconPath = os.path.dirname(os.path.normpath(__file__))
        iconPath = returnNicePath(iconPath, "icons")

        self.mainWidget.setStyleSheet(style)

        #set qt object name
        self.setObjectName("jeRigToolsWIN")
        self.setWindowTitle("Rigging Tools")

        #create the mainLayout for the rig creator UI
        self.layout = QtGui.QVBoxLayout(self.mainWidget)

        self.setMinimumSize(QtCore.QSize( 220, 300 ))
        self.setMaximumSize(QtCore.QSize( 220, 1100 ))
        self.setStyleSheet("""QToolTip { background-color: black; color: white;border: black solid 1px}""")
        self.resize(220, 600)

        #jump to section
        layout = QtGui.QHBoxLayout()
        self.layout.addLayout(layout)

        label = QtGui.QLabel("Jump To:")
        label.setStyleSheet("background: transparent;")
        layout.addWidget(label)

        self.section = QtGui.QComboBox()


        layout.addWidget(self.section)
        self.section.addItem("Constraints")
        self.section.addItem("Common")
        self.section.addItem("Freeze Xforms")
        self.section.addItem("Lock/Unlock")
        self.section.addItem("Rigging Tools")
        self.section.addItem("Rig Controls")

        #scroll layout
        self.scrollArea = QtGui.QScrollArea()
        self.scrollArea.setMinimumSize(QtCore.QSize(210, 600))
        self.scrollArea.setMaximumSize(QtCore.QSize(210, 1200))
        self.scrollArea.setSizePolicy(QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Expanding))
        self.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.layout.addWidget(self.scrollArea)

        #frame
        self.scrollContents = QtGui.QFrame()
        self.scrollContents.setSizePolicy(QtGui.QSizePolicy(QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Expanding))
        self.scrollContents.setObjectName("mid")
        self.scrollContents.setMinimumSize(QtCore.QSize(210, 1200))
        self.scrollContents.setMaximumSize(QtCore.QSize(210, 1200))
        self.scrollArea.setWidget(self.scrollContents)

        self.mainLayout = QtGui.QVBoxLayout(self.scrollContents)


        #=======================================================================
        # #section 1: constraints
        #=======================================================================
        self.constraintFrame = QtGui.QGroupBox("Constraints:")
        self.constraintFrame.setMinimumSize(180, 250)
        self.constraintFrame.setMaximumSize(180, 250)
        self.mainLayout.addWidget(self.constraintFrame)

        constraintLayout = QtGui.QVBoxLayout(self.constraintFrame)

        self.constraintLayoutH = QtGui.QHBoxLayout()
        constraintLayout.addLayout(self.constraintLayoutH)
        self.pointConst = QtGui.QCheckBox("Position")
        self.orientConst = QtGui.QCheckBox("Rotation")
        self.constraintLayoutH.addWidget(self.pointConst)
        self.constraintLayoutH.addWidget(self.orientConst)


        label = QtGui.QLabel("Skip Translate:")
        label.setStyleSheet("background: transparent;")
        constraintLayout.addWidget(label)


        self.constraintSkipLayout = QtGui.QHBoxLayout()
        constraintLayout.addLayout(self.constraintSkipLayout)

        self.skipTX = QtGui.QCheckBox("X")
        self.skipTY = QtGui.QCheckBox("Y")
        self.skipTZ = QtGui.QCheckBox("Z")

        self.constraintSkipLayout.addWidget(self.skipTX)
        self.constraintSkipLayout.addWidget(self.skipTY)
        self.constraintSkipLayout.addWidget(self.skipTZ)



        label = QtGui.QLabel("Skip Rotate:")
        label.setStyleSheet("background: transparent;")
        constraintLayout.addWidget(label)


        self.constraintSkipLayoutR = QtGui.QHBoxLayout()
        constraintLayout.addLayout(self.constraintSkipLayoutR)

        self.skipRX = QtGui.QCheckBox("X")
        self.skipRY = QtGui.QCheckBox("Y")
        self.skipRZ = QtGui.QCheckBox("Z")

        self.constraintSkipLayoutR.addWidget(self.skipRX)
        self.constraintSkipLayoutR.addWidget(self.skipRY)
        self.constraintSkipLayoutR.addWidget(self.skipRZ)

        maintainLayout = QtGui.QHBoxLayout()
        constraintLayout.addLayout(maintainLayout)

        label = QtGui.QLabel("Maintain Offsets:")
        label.setStyleSheet("background: transparent;")

        maintainLayout.addWidget(label)

        self.maintainOffsets = QtGui.QCheckBox()
        maintainLayout.addWidget(self.maintainOffsets)



        self.createConstButton = QtGui.QPushButton("Create Constraint")
        self.createConstButton.setObjectName("blueButton")
        constraintLayout.addWidget(self.createConstButton)
        self.createConstButton.clicked.connect(partial(self.createConstraint, False))

        self.snapConstButton = QtGui.QPushButton("Snap")
        self.snapConstButton.setToolTip("Create and delete constraint given settings.")
        self.snapConstButton.setObjectName("blueButton")
        constraintLayout.addWidget(self.snapConstButton)
        self.snapConstButton.clicked.connect(partial(self.createConstraint, True))

        #=======================================================================
        # #section 2: common tools
        #=======================================================================
        self.commonFrame = QtGui.QGroupBox("Common Tools:")
        self.commonFrame.setMinimumSize(180, 160)
        self.commonFrame.setMaximumSize(180, 160)
        self.mainLayout.addWidget(self.commonFrame)

        commonToolsLayout = QtGui.QVBoxLayout(self.commonFrame)


        #row1
        row1layout = QtGui.QHBoxLayout()
        commonToolsLayout.addLayout(row1layout)

        self.deleteHistBtn = QtGui.QPushButton("HIST")
        self.deleteHistBtn.setToolTip("delete history")
        self.deleteHistBtn.setObjectName("blueButton")
        row1layout.addWidget(self.deleteHistBtn)
        self.deleteHistBtn.clicked.connect(self.deleteHistory)

        self.deleteNonDHistBtn = QtGui.QPushButton("NON-D")
        self.deleteNonDHistBtn.setToolTip("delete non-deformer history")
        self.deleteNonDHistBtn.setObjectName("blueButton")
        row1layout.addWidget(self.deleteNonDHistBtn)
        self.deleteNonDHistBtn.clicked.connect(self.deleteNonDeformHistory)

        #row2
        row2layout = QtGui.QHBoxLayout()
        commonToolsLayout.addLayout(row2layout)

        self.centerPivotBtn = QtGui.QPushButton("CP")
        self.centerPivotBtn.setToolTip("center pivot")
        self.centerPivotBtn.setObjectName("blueButton")
        row2layout.addWidget(self.centerPivotBtn)
        self.centerPivotBtn.clicked.connect(self.centerPivot)

        self.copyPivotBtn = QtGui.QPushButton("Copy Piv")
        self.copyPivotBtn.setToolTip("copy pivot: source -> target")
        self.copyPivotBtn.setObjectName("blueButton")
        row2layout.addWidget(self.copyPivotBtn)
        self.copyPivotBtn.clicked.connect(self.copyPivot)

        #row3
        row3layout = QtGui.QHBoxLayout()
        commonToolsLayout.addLayout(row3layout)

        self.invertSelBtn = QtGui.QPushButton("IS")
        self.invertSelBtn.setToolTip("invert selection")
        self.invertSelBtn.setObjectName("blueButton")
        row3layout.addWidget(self.invertSelBtn)
        self.invertSelBtn.clicked.connect(self.invertSelection)

        self.selectHiBtn = QtGui.QPushButton("SH")
        self.selectHiBtn.setToolTip("select hierarchy")
        self.selectHiBtn.setObjectName("blueButton")
        row3layout.addWidget(self.selectHiBtn)
        self.selectHiBtn.clicked.connect(self.selectHierarchy)

        #row4
        row4layout = QtGui.QHBoxLayout()
        commonToolsLayout.addLayout(row4layout)

        label = QtGui.QLabel("Convert To:")
        label.setStyleSheet("background: transparent;")
        row4layout.addWidget(label)

        self.convertSelection = QtGui.QComboBox()
        row4layout.addWidget(self.convertSelection)

        self.convertSelection.addItem("Object")
        self.convertSelection.addItem("Vertices")
        self.convertSelection.addItem("Faces")
        self.convertSelection.addItem("Edges")


        self.convertGoBtn = QtGui.QPushButton("GO")
        self.convertGoBtn.setMinimumSize(30,30)
        self.convertGoBtn.setMaximumSize(30,30)
        self.convertGoBtn.setObjectName("blueButton")
        row4layout.addWidget(self.convertGoBtn)
        self.convertGoBtn.clicked.connect(self.convertSel)

        self.convertSelection.currentIndexChanged.connect(self.convertSel)

        #=======================================================================
        # #Freeze Transforms
        #=======================================================================
        self.freezeXformsFrame = QtGui.QGroupBox("Freeze Transforms:")
        self.freezeXformsFrame.setMinimumSize(180, 90)
        self.freezeXformsFrame.setMaximumSize(180, 90)
        self.mainLayout.addWidget(self.freezeXformsFrame)

        freezeMainLayout = QtGui.QVBoxLayout(self.freezeXformsFrame)

        freeze1Layout = QtGui.QHBoxLayout()
        freezeMainLayout.addLayout(freeze1Layout)

        self.translateFreeze = QtGui.QCheckBox("T")
        self.translateFreeze.setChecked(True)
        self.rotateFreeze = QtGui.QCheckBox("R")
        self.rotateFreeze.setChecked(True)
        self.scaleFreeze = QtGui.QCheckBox("S")
        self.scaleFreeze.setChecked(True)

        freeze1Layout.addWidget(self.translateFreeze)
        freeze1Layout.addWidget(self.rotateFreeze)
        freeze1Layout.addWidget(self.scaleFreeze)


        self.freezeBtn = QtGui.QPushButton("Freeze Xforms")
        self.freezeBtn.setObjectName("blueButton")
        freezeMainLayout.addWidget(self.freezeBtn)
        self.freezeBtn.clicked.connect(self.freezeTransformations)

        #=======================================================================
        # #lock/unlock
        #=======================================================================
        self.lockUnlockFrame = QtGui.QGroupBox("Lock/Unlock:")
        self.lockUnlockFrame.setMinimumSize(180, 160)
        self.lockUnlockFrame.setMaximumSize(180, 160)
        self.mainLayout.addWidget(self.lockUnlockFrame)

        lockMainLayout = QtGui.QVBoxLayout(self.lockUnlockFrame)

        #row 1
        lock1Layout = QtGui.QHBoxLayout()
        lockMainLayout.addLayout(lock1Layout)

        self.lockBtn = QtGui.QPushButton("Lock")
        self.lockBtn.setToolTip("Lock Selected Channels")
        self.lockBtn.setObjectName("blueButton")
        lock1Layout.addWidget(self.lockBtn)
        self.lockBtn.clicked.connect(partial(self.modifyChannels, True, False, False))

        self.hideBtn = QtGui.QPushButton("Hide")
        self.hideBtn.setToolTip("Hide Selected Channels")
        self.hideBtn.setObjectName("blueButton")
        lock1Layout.addWidget(self.hideBtn)
        self.hideBtn.clicked.connect(partial(self.modifyChannels, False, False, True))



        #row 2
        lock2Layout = QtGui.QHBoxLayout()
        lockMainLayout.addLayout(lock2Layout)

        self.lockHideBtn = QtGui.QPushButton("Lock/Hide")
        self.lockHideBtn.setToolTip("Lock & Hide Selected Channels")
        self.lockHideBtn.setObjectName("blueButton")
        lock2Layout.addWidget(self.lockHideBtn)
        self.lockHideBtn.clicked.connect(partial(self.modifyChannels, True, False, True))

        self.unlockHideBtn = QtGui.QPushButton("Unlock")
        self.unlockHideBtn.setToolTip("Unlock Selected Channels")
        self.unlockHideBtn.setObjectName("blueButton")
        lock2Layout.addWidget(self.unlockHideBtn)
        self.unlockHideBtn.clicked.connect(partial(self.modifyChannels, False, True, False))

        #row 3
        self.channelControlBtn = QtGui.QPushButton("Channel Control")
        self.channelControlBtn.setObjectName("blueButton")
        lockMainLayout.addWidget(self.channelControlBtn)
        self.channelControlBtn.clicked.connect(self.openChannelControl)

        #lock/ unlock node

        lock3Layout = QtGui.QHBoxLayout()
        lockMainLayout.addLayout(lock3Layout)

        self.lockNodeBtn = QtGui.QPushButton("Lock Node")
        self.lockNodeBtn.setObjectName("blueButton")
        lock3Layout.addWidget(self.lockNodeBtn)
        self.lockNodeBtn.clicked.connect(partial(self.lockNodes, True))

        self.unlockNodeBtn = QtGui.QPushButton("Unlock Node")
        self.unlockNodeBtn.setObjectName("blueButton")
        lock3Layout.addWidget(self.unlockNodeBtn)
        self.unlockNodeBtn.clicked.connect(partial(self.lockNodes, False))

        #=======================================================================
        # #joint tools/weight tools
        #=======================================================================
        self.jointToolsFrame = QtGui.QGroupBox("Rigging Tools:")
        self.jointToolsFrame.setMinimumSize(180, 170)
        self.jointToolsFrame.setMaximumSize(180, 170)
        self.mainLayout.addWidget(self.jointToolsFrame)

        rigMainLayout = QtGui.QVBoxLayout(self.jointToolsFrame)


        #create joint from selection
        self.createJoitnFromSelBtn = QtGui.QPushButton("Create Joint From Sel")
        self.createJoitnFromSelBtn.setObjectName("blueButton")
        rigMainLayout.addWidget(self.createJoitnFromSelBtn)
        self.createJoitnFromSelBtn.clicked.connect(self.createJointFromSelection)

        #create joint from geo
        self.createJoitnFromGeoBtn = QtGui.QPushButton("Create Joint From Geo")
        self.createJoitnFromGeoBtn.setObjectName("blueButton")
        rigMainLayout.addWidget(self.createJoitnFromGeoBtn)
        self.createJoitnFromGeoBtn.clicked.connect(self.createJointFromGeo)

        row1Layout = QtGui.QHBoxLayout()
        rigMainLayout.addLayout(row1Layout)


        #bindPose
        self.goToBindPoseBtn = QtGui.QPushButton("BindPose")
        self.goToBindPoseBtn.setToolTip("Go To BindPose")
        self.goToBindPoseBtn.setObjectName("blueButton")
        row1Layout.addWidget(self.goToBindPoseBtn)
        self.goToBindPoseBtn.clicked.connect(self.goToBindPose)


        #reset bindPose
        self.resetBpBtn = QtGui.QPushButton("Reset BP")
        self.resetBpBtn.setToolTip("Reset BindPose")
        self.resetBpBtn.setObjectName("blueButton")
        row1Layout.addWidget(self.resetBpBtn)
        self.resetBpBtn.clicked.connect(self.resetBindPose)


        row2Layout = QtGui.QHBoxLayout()
        rigMainLayout.addLayout(row2Layout)


        #weight hammer
        self.weightHammerBtn = QtGui.QPushButton("Hammer")
        self.weightHammerBtn.setToolTip("weight hammer")
        self.weightHammerBtn.setObjectName("blueButton")
        row2Layout.addWidget(self.weightHammerBtn)
        self.weightHammerBtn.clicked.connect(self.weightHammer)

        #skin wrangler or artv2 skinTools
        self.skinWranglerBtn = QtGui.QPushButton("Wrangle")
        self.skinWranglerBtn.setToolTip("Chris Evans: Skin Wrangler")
        self.skinWranglerBtn.setObjectName("blueButton")
        row2Layout.addWidget(self.skinWranglerBtn)
        self.skinWranglerBtn.clicked.connect(self.skinWrangle)

        #paint
        self.paintWeightsBtn = QtGui.QPushButton()
        self.paintWeightsBtn.setMinimumSize(30,30)
        self.paintWeightsBtn.setMaximumSize(30,30)
        icon = QtGui.QIcon(os.path.join(iconPath, "paint.png"))
        self.paintWeightsBtn.setIconSize(QtCore.QSize(25,25))
        self.paintWeightsBtn.setIcon(icon)
        self.paintWeightsBtn.clicked.connect(self.paint)
        row2Layout.addWidget(self.paintWeightsBtn)
        #=======================================================================
        # #create rig controls
        #=======================================================================
        self.rigControlsFrame = QtGui.QGroupBox("Rig Controls:")
        self.rigControlsFrame.setMinimumSize(180, 200)
        self.rigControlsFrame.setMaximumSize(180, 200)
        self.mainLayout.addWidget(self.rigControlsFrame)

        rigControlsMainLayout = QtGui.QVBoxLayout(self.rigControlsFrame)


        #comboBox of shapes
        rowlayout = QtGui.QHBoxLayout()
        rigControlsMainLayout.addLayout(rowlayout)

        label = QtGui.QLabel("Control Shape:")
        label.setStyleSheet("background: transparent;")
        rowlayout.addWidget(label)

        self.createControlShape = QtGui.QComboBox()
        rowlayout.addWidget(self.createControlShape)

        self.createControlShape.addItem("Circle")
        self.createControlShape.addItem("Square")
        self.createControlShape.addItem("Cross")
        self.createControlShape.addItem("Arrow")
        self.createControlShape.addItem("2 Arrow")
        self.createControlShape.addItem("Star")
        self.createControlShape.addItem("90 Arrow")
        self.createControlShape.addItem("180 Arrow")
        self.createControlShape.addItem("Sphere")
        self.createControlShape.addItem("Cube")
        self.createControlShape.addItem("Pyramid")
        self.createControlShape.addItem("Ball")


        #color selector
        row2layout = QtGui.QHBoxLayout()
        rigControlsMainLayout.addLayout(row2layout)

        frame = QtGui.QFrame()
        frame.setMinimumSize(24,24)
        frame.setMaximumSize(24,24)
        row2layout.addWidget(frame)
        frame.setStyleSheet("background: black;")
        frameLayout = QtGui.QVBoxLayout(frame)
        frameLayout.setContentsMargins(2,2,2,2)

        self.currentColor = QtGui.QLabel()

        self.currentColor.setMinimumSize(20,20)
        self.currentColor.setMaximumSize(20,20)
        self.currentColor.setStyleSheet("background: rgb(120, 120, 120);")
        frameLayout.addWidget(self.currentColor)

        self.colorChanger = QtGui.QSlider()
        self.colorChanger.setOrientation(QtCore.Qt.Horizontal)
        self.colorChanger.setRange(0, 31)
        row2layout.addWidget(self.colorChanger)
        self.colorChanger.valueChanged.connect(self.changeControlColor)

        #create button
        self.createControlBtn = QtGui.QPushButton("Create Control")
        self.createControlBtn.setObjectName("blueButton")
        rigControlsMainLayout.addWidget(self.createControlBtn)
        self.createControlBtn.clicked.connect(self.createControl)

        #hookup button

        self.hookupLayout = QtGui.QHBoxLayout()
        self.hookItUpBtn = QtGui.QPushButton("Hook It Up")
        self.hookItUpBtn.setObjectName("blueButton")
        rigControlsMainLayout.addWidget(self.hookItUpBtn)
        self.hookItUpBtn.clicked.connect(self.hookup)

        self.hookupCB = QtGui.QCheckBox("maintain offsets")
        rigControlsMainLayout.addWidget(self.hookupCB)

        #spacer
        self.mainLayout.addSpacerItem(QtGui.QSpacerItem(0,50, QtGui.QSizePolicy.Fixed, QtGui.QSizePolicy.Expanding))


        #=======================================================================
        # #signals/slots
        #=======================================================================
        self.section.currentIndexChanged.connect(self.jumpToSection)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def jumpToSection(self):

        value = self.section.currentText()

        if value == "Constraints":

            self.scrollArea.ensureWidgetVisible(self.constraintFrame)

        if value == "Common":

            self.scrollArea.ensureWidgetVisible(self.commonFrame)

        if value == "Freeze Xforms":

            self.scrollArea.ensureWidgetVisible(self.freezeXformsFrame)

        if value == "Lock/Unlock":

            self.scrollArea.ensureWidgetVisible(self.lockUnlockFrame)

        if value == "Rigging Tools":

            self.scrollArea.ensureWidgetVisible(self.jointToolsFrame)

        if value == "Rig Controls":

            self.scrollArea.ensureWidgetVisible(self.rigControlsFrame)



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def createConstraint(self, snap):

        #get selection
        selection = cmds.ls(sl = True)

        if not len(selection) > 0:
            cmds.warning("Need at least two objects selected..")
            return


        target = selection[-1]
        selection.pop(-1)

        maintain = self.maintainOffsets.isChecked()

        #get all values for point
        if self.pointConst.isChecked():
            skipValues = []
            if self.skipTX.isChecked():
                skipValues.append("x")

            if self.skipTY.isChecked():
                skipValues.append("x")

            if self.skipTZ.isChecked():
                skipValues.append("x")

            constraint = cmds.pointConstraint(selection, target, mo = maintain, skip = skipValues)
            if snap:
                cmds.delete(constraint)


        #get all values for orient
        if self.orientConst.isChecked():
            skipValues = []
            if self.skipRX.isChecked():
                skipValues.append("x")

            if self.skipRY.isChecked():
                skipValues.append("x")

            if self.skipRZ.isChecked():
                skipValues.append("x")

            constraint = cmds.orientConstraint(selection, target, mo = maintain, skip = skipValues)
            if snap:
                cmds.delete(constraint)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def deleteHistory(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        cmds.delete(selection, ch = True)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def deleteNonDeformHistory(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        cmds.bakePartialHistory(selection, prePostDeformers = True)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def centerPivot(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        for each in selection:
            cmds.xform(each, cp = True)



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def copyPivot(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least two objects selected..")
            return

        piv = cmds.xform(selection[0], q = True, ws = True, rp = True)
        cmds.xform(selection[1], ws = True, piv = piv)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def invertSelection(self):

        cmds.select("*", tgl = True)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def selectHierarchy(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        cmds.select(selection, hi = True)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def convertSel(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        #get value
        mode = self.convertSelection.currentText()

        if mode == "Vertices":
            for each in selection:
                vertexSel =  cmds.polyListComponentConversion(each, tv = True)
                cmds.selectMode(co = True)
                cmds.select(vertexSel)

        if mode == "Faces":
            for each in selection:
                faceSel =  cmds.polyListComponentConversion(each, tf = True)
                cmds.selectMode(co = True)
                cmds.select(faceSel)

        if mode == "Edges":
            for each in selection:
                edgeSel =  cmds.polyListComponentConversion(each, te = True)
                cmds.selectMode(co = True)
                cmds.select(edgeSel)

        if mode == "Object":
            for each in selection:
                cmds.selectMode(o = True)



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def freezeTransformations(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        for each in selection:
            freezeTranslate = self.translateFreeze.isChecked()
            freezeRotate = self.rotateFreeze.isChecked()
            freezeScale = self.scaleFreeze.isChecked()

            cmds.makeIdentity(each, t = freezeTranslate, r = freezeRotate, s = freezeScale, apply = True)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def modifyChannels(self, lock, unlock, hide):
        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        channels = mel.eval("channelBox -q -sma mainChannelBox;")

        for each in selection:
            for channel in channels:
                if lock:
                    cmds.setAttr(each + "." + channel, lock = True)
                if unlock:
                    cmds.setAttr(each + "." + channel, lock = False)
                if hide:
                    cmds.setAttr(each + "." + channel, channelBox = False, keyable = False)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def openChannelControl(self):
        mel.eval("ChannelControlEditor;")


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def lockNodes(self, lock):
        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        for each in selection:
            cmds.lockNode(each, lock = lock)

            if lock:
                sys.stdout.write(each + " is now locked.\n")
            else:
                sys.stdout.write(each + " is now unlocked.\n")


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def createJointFromSelection(self):
        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        emitter = cmds.EmitFromObject()
        newPos = cmds.xform(emitter, q=True, t=True, ws=True)
        cmds.select("emitter1", "particle1", r=True)
        cmds.delete()
        cmds.joint(position=newPos)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def createJointFromGeo(self):

        import createJointFromGeo as createJointFromGeo
        reload(createJointFromGeo)
        createJointFromGeo.run()

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def goToBindPose(self):

        mel.eval("GoToBindPose")

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def resetBindPose(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        bindPoses = cmds.ls(type = "dagPose")
        deletePoses = []
        for pose in bindPoses:
            joints = list(set(cmds.listConnections(pose, type = "joint")))
            if selection[0] in joints:
                deletePoses.append(pose)


        cmds.delete(deletePoses)
        cmds.select(selection[0])
        cmds.dagPose(s = True, bp = True)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def weightHammer(self):

        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        if selection[0].find("vtx") == -1:
            cmds.warning("Need to be in vertex component selection..")
            return

        mel.eval("weightHammerVerts;")



# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def skinWrangle(self):
        try:
            import skinWrangler as sw
            global skinWranglerWindow
            skinWranglerWindow = sw.show()
        except Exception, e:
            sys.stdout.write(str(e) + "\n")


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def paint(self):
        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least one object selected..")
            return

        mel.eval("ArtPaintSkinWeightsToolOptions;")

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def changeControlColor(self, *args):

        #get slider value
        color = self.colorChanger.value()

        if color == 0:
            self.currentColor.setStyleSheet("background: rgb(120,120,120);")

        if color == 1:
            self.currentColor.setStyleSheet("background: rgb(0,0,0);")

        if color == 2:
            self.currentColor.setStyleSheet("background: rgb(64,64,64);")

        if color == 3:
            self.currentColor.setStyleSheet("background: rgb(128,128,128);")

        if color == 4:
            self.currentColor.setStyleSheet("background: rgb(155,0,40);")

        if color == 5:
            self.currentColor.setStyleSheet("background: rgb(0,4,96);")

        if color == 6:
            self.currentColor.setStyleSheet("background: rgb(0,0,255);")

        if color == 7:
            self.currentColor.setStyleSheet("background: rgb(0,70,25);")

        if color == 8:
            self.currentColor.setStyleSheet("background: rgb(38,0,67);")

        if color == 9:
            self.currentColor.setStyleSheet("background: rgb(200,0,200);")

        if color == 10:
            self.currentColor.setStyleSheet("background: rgb(138,72,51);")

        if color == 11:
            self.currentColor.setStyleSheet("background: rgb(63,35,31);")

        if color == 12:
            self.currentColor.setStyleSheet("background: rgb(153,38,0);")

        if color == 13:
            self.currentColor.setStyleSheet("background: rgb(255,0,0);")

        if color == 14:
            self.currentColor.setStyleSheet("background: rgb(0,255,0);")

        if color == 15:
            self.currentColor.setStyleSheet("background: rgb(0,65,153;")

        if color == 16:
            self.currentColor.setStyleSheet("background: rgb(255,255,255);")

        if color == 17:
            self.currentColor.setStyleSheet("background: rgb(255,255,0);")

        if color == 18:
            self.currentColor.setStyleSheet("background: rgb(100,220,255);")

        if color == 19:
            self.currentColor.setStyleSheet("background: rgb(67,255,163);")

        if color == 20:
            self.currentColor.setStyleSheet("background: rgb(255,176,176);")

        if color == 21:
            self.currentColor.setStyleSheet("background: rgb(228,172,121);")

        if color == 22:
            self.currentColor.setStyleSheet("background: rgb(255,255,99);")

        if color == 23:
            self.currentColor.setStyleSheet("background: rgb(0,153,84);")

        if color == 24:
            self.currentColor.setStyleSheet("background: rgb(161,105,48);")

        if color == 25:
            self.currentColor.setStyleSheet("background: rgb(159,161,48);")

        if color == 26:
            self.currentColor.setStyleSheet("background: rgb(104,161,48);")

        if color == 27:
            self.currentColor.setStyleSheet("background: rgb(48,161,93);")

        if color == 28:
            self.currentColor.setStyleSheet("background: rgb(48,161,161);")

        if color == 29:
            self.currentColor.setStyleSheet("background: rgb(48,103,161);")

        if color == 30:
            self.currentColor.setStyleSheet("background: rgb(111,48,161);")

        if color == 31:
            self.currentColor.setStyleSheet("background: rgb(161,48,105);")

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def createControl(self):

        selection = cmds.ls(sl = True)


        controlType = self.createControlShape.currentText()
        group = cmds.group(empty = True, name = "control_grp")

        color = self.colorChanger.value()

        #=======================================================================
        # #Circle
        #=======================================================================
        if controlType == "Circle":
            control = cmds.circle(c = (0,0,0), sw = 360, r = 30, d = 3, name = "control")[0]

        #=======================================================================
        # #Square
        #=======================================================================
        if controlType == "Square":
            control = cmds.circle(c = (0,0,0), s = 4, sw = 360, r = 30, d = 1, name = "control")[0]
            cmds.setAttr(control + ".rz", 45)
            cmds.makeIdentity(control, r = True, apply = True)

        #=======================================================================
        # #Sphere
        #=======================================================================
        if controlType == "Sphere":

            points = [(0, 0, 1), (0, 0.5, 0.866), (0, 0.866025, 0.5), (0, 1, 0), (0, 0.866025, -0.5), (0, 0.5, -0.866025), (0, 0, -1), (0, -0.5, -0.866025), (0, -0.866025, -0.5), (0, -1, 0), (0, -0.866025, 0.5), (0, -0.5, 0.866025), (0, 0, 1), (0.707107, 0, 0.707107), (1, 0, 0), (0.707107, 0, -0.707107), (0, 0, -1), (-0.707107, 0, -0.707107), (-1, 0, 0), (-0.866025, 0.5, 0), (-0.5, 0.866025, 0), (0, 1, 0), (0.5, 0.866025, 0), (0.866025, 0.5, 0), (1, 0, 0), (0.866025, -0.5, 0), (0.5, -0.866025, 0), (0, -1, 0), (-0.5, -0.866025, 0), (-0.866025, -0.5, 0), (-1, 0, 0), (-0.707107, 0, 0.707107), (0, 0, 1)]
            control = cmds.curve(name = "control", d = 1, p = points)
            cmds.setAttr(control + ".scaleX", 10)
            cmds.setAttr(control + ".scaleY", 10)
            cmds.setAttr(control + ".scaleZ", 10)

        #=======================================================================
        # #Ball
        #=======================================================================
        if controlType == "Ball":
            control = cmds.curve(name = "control", d = 1, p = [(0.80718, 0.830576, 8.022739), (0.80718, 4.219206, 7.146586 ), (0.80718, 6.317059, 5.70073), (2.830981, 6.317059, 5.70073), (0, 8.422749, 2.94335), (-2.830981, 6.317059, 5.70073), (-0.80718, 6.317059, 5.70073), (-0.80718, 4.219352, 7.146486), (-0.80718, 0.830576, 8.022739), (-4.187851, 0.830576, 7.158003), (-6.310271, 0.830576, 5.705409), (-6.317059, 2.830981, 5.7007), (-8.422749, 0, 2.94335), (-6.317059, -2.830981, 5.70073), (-6.317059, -0.830576, 5.70073), (-4.225134, -0.830576, 7.142501), (-0.827872, -0.830576, 8.017446), (-0.80718, -4.176512, 7.160965), (-0.80718, -6.317059, 5.70073), (-2.830981, -6.317059, 5.70073), (0, -8.422749, 2.94335), (2.830981, -6.317059, 5.70073), (0.80718, -6.317059, 5.70073), (0.80718, -4.21137, 7.151987), (0.80718, -0.830576, 8.022739), (4.183345, -0.830576, 7.159155), (6.317059, -0.830576, 5.70073), (6.317059, -2.830981, 5.70073), (8.422749, 0, 2.94335), (6.317059, 2.830981, 5.70073), (6.317059, 0.830576, 5.70073), (4.263245, 0.830576, 7.116234), (0.80718, 0.830576, 8.022739)])
            cmds.setAttr(control + ".scaleX", 5)
            cmds.setAttr(control + ".scaleY", 5)
            cmds.setAttr(control + ".scaleZ", 5)

        #=======================================================================
        # #Cube
        #=======================================================================
        if controlType == "Cube":
            points = [[6.02647, -6.02647, 12.04647], [6.02647, 6.02647, 12.04647], [-6.02647, 6.02647, 12.04647], [-6.02647, -6.02647, 12.04647], [6.02647, -6.02647, 12.04647], [6.02647, -6.02647, -0.00647034],
                      [-6.02647, -6.02647, -0.00647034], [-6.02647, -6.02647, 12.04647], [-6.02647, 6.02647, 12.04647], [-6.02647, 6.02647, -0.00647034], [-6.02647, -6.02647, -0.00647034], [6.02647, -6.02647, -0.00647034],
                      [6.02647, -6.02647, 12.04647], [6.02647, 6.02647, 12.04647], [6.02647, 6.02647, -0.00647034], [6.02647, -6.02647, -0.00647034], [6.02647, -6.02647, 12.04647], [6.02647, 6.02647, 12.04647],
                      [6.02647, 6.02647, -0.00647034], [-6.02647, 6.02647, -0.00647034], [-6.02647, 6.02647, 12.04647], [6.02647, 6.02647, 12.04647]]

            control = cmds.curve(name = "control", d = 1, p = points)
            cmds.setAttr(control + ".scaleX", 2)
            cmds.setAttr(control + ".scaleY", 2)
            cmds.setAttr(control + ".scaleZ", 2)

        #=======================================================================
        # #Star
        #=======================================================================
        if controlType == "Star":

            points = [[0, -50, 0], [-9.384594, -28.817275, 0], [-13.30763, -27.227791, 0], [-17.480905, -24.77712, 0], [-20.596755, -22.274624, 0], [-23.833962, -18.757781, 0],
                      [-26.349207, -14.98229, 0], [-28.759575, -9.558683, 0], [-50, 0, 0], [-28.770723, 9.525313, 0], [-26.651022, 14.434428, 0],  [-24.193906, 18.286961, 0],
                      [-21.755189, 21.146159, 0], [-18.927363, 23.700649, 0], [-15.486534, 26.058461, 0], [-12.377072, 27.6617, 0], [-10.022831, 28.600144, 0], [0, 50, 0],
                      [10.518781, 28.42069, 0], [13.171175, 27.293763, 0], [16.092683, 25.691857, 0], [19.166776, 23.509024, 0], [21.810012, 21.089516, 0], [24.005562, 18.525015, 0],
                      [25.905111, 15.743927, 0], [27.583159, 12.551569, 0], [28.60507, 10.008838, 0], [50, 0, 0], [28.657458, -9.858696, 0], [27.801009, -12.060318, 0],
                      [26.358507, -14.965793, 0], [23.550257, -19.115686, 0], [20.51645, -22.348314, 0], [17.109316, -24.885162, 0], [14.229729, -26.760131, 0], [10.358914, -28.479572, 0], [0, -50, 0]]

            control = cmds.curve(name = "control", d = 1, p = points)

        #=======================================================================
        # #Pyramid
        #=======================================================================
        if controlType == "Pyramid":

            points = [[17.227867, 17.227867, 0.418059], [-17.227864, 17.227869, 0.418059], [-17.227868, -17.227865, 0.418059], [17.227866, -17.227867, 0.418059], [17.227867, 17.227867, 0.418059],
                      [0, 0, 24.781941], [17.227866, -17.227867, 0.418059], [-17.227868, -17.227865, 0.418059], [0, 0, 24.781941], [-17.227864, 17.227869, 0.418059],
                      [17.227867, 17.227867, 0.418059],[0, 0, 24.781941]]

            control = cmds.curve(name = "control", d = 1, p = points)

        #=======================================================================
        # #Double Arrow
        #=======================================================================
        if controlType == "2 Arrow":

            points = [[-4.352071, -4.747714, 0], [-8.704142, -4.747714, 0], [0, -17.803927, 0], [8.704142, -4.747714, 0], [4.352071, -4.747714, 0], [4.352071, 4.747714, 0],
                      [8.704142, 4.747714, 0], [0, 17.803927, 0], [-8.704142, 4.747714, 0], [-4.352071, 4.747714, 0], [-4.352071, -4.747714, 0]]

            control = cmds.curve(name = "control", d = 1, p = points)

        #=======================================================================
        # #Single Arrow
        #=======================================================================
        if controlType == "Arrow":

            points = [[-4.658613, -9.317226, 0], [4.658613, -9.317226, 0], [4.658613, 0, 0], [9.317226, 0, 0], [0, 13.975838, 0], [-9.317226, 0, 0], [-4.658613, 0, 0], [-4.658613, -9.317226, 0]]
            control = cmds.curve(name = "control", d = 1, p = points)

        #=======================================================================
        # #Cross
        #=======================================================================
        if controlType == "Cross":

            points = [[-2.004224, -2.004224, 0], [-2.004224, -10.02112, 0], [2.004224, -10.02112, 0], [2.004224, -2.004224, 0], [10.02112, -2.004224, 0], [10.02112, 2.004224, 0],
                      [2.004224, 2.004224, 0], [2.004224, 10.02112, 0], [-2.004224, 10.02112, 0], [-2.004224, 2.004224, 0], [-10.02112, 2.004224, 0], [-10.02112, -2.004224, 0], [-2.004224, -2.004224, 0]]
            control = cmds.curve(name = "control", d = 1, p = points)

        #=======================================================================
        # #90 Arrow
        #=======================================================================
        if controlType == "90 Arrow":

            points = [[1.307282, -0.0501891, 0], [2.437608, -5.599536, 0], [5.505557, -10.185733, 0], [8.861274, -12.361307, 0], [7.3419, -5.481389, 0], [13.817009, -17.186448, 0],
                      [0.473643, -16.243087, 0], [6.978455, -14.831142, 0], [3.281864, -12.52095, 0], [-0.541275, -6.817733, 0], [-1.920613, 0, 0], [1.297319, 0, 0]]

            control = cmds.curve(name = "control", d = 1, p = points)


        #=======================================================================
        # #180 Arrow
        #=======================================================================
        if controlType == "180 Arrow":

            points = [[1.209242, -0.0218849, 0], [2.311397, -5.533185, 0], [5.399375, -10.149323, 0], [8.729472, -12.300559, 0], [7.217565, -5.454451, 0],
                      [13.660854, -17.101988, 0], [0.383062, -16.163263, 0], [6.855906, -14.758256, 0], [3.157234, -12.446763, 0], [-0.658571, -6.736789, 0],
                      [-1.993435, 0.0301111, 0], [-0.658571, 6.736789, 0], [3.157234, 12.446763, 0], [6.884714, 14.727421, 0], [0.383062, 16.163263, 0],
                      [13.660854, 17.101988, 0], [7.217565, 5.454451, 0], [8.754146, 12.252552, 0], [5.423028, 10.184681, 0], [2.296727, 5.511255, 0], [1.209242, 0.0330112, 0 ]]

            control = cmds.curve(name = "control", d = 1, p = points)


        cmds.setAttr(control + ".overrideEnabled", 1)
        cmds.setAttr(control + ".overrideColor", color)

        if len(selection) > 0:

            constraint = cmds.parentConstraint(selection[0], group)[0]
            cmds.delete(constraint)

            constraint = cmds.pointConstraint(selection[0], control)[0]
            cmds.delete(constraint)

            control = cmds.rename(control, selection[0] + "_anim")
            group = cmds.rename(group, selection[0] + "_anim_grp")

        cmds.parent(control, group)
        cmds.select(group)


# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def hookup(self):
        selection = cmds.ls(sl = True)
        if not len(selection) > 0:
            cmds.warning("Need at least two objects selected.. (Control, bone)")
            return
        #get value of checkbox
        value = self.hookupCB.isChecked()


        cmds.makeIdentity(selection[0], t = True, s = True, r = True, apply = True)
        cmds.pointConstraint(selection[0], selection[1], mo = value)
        cmds.orientConstraint(selection[0], selection[1], mo = value)
        cmds.scaleConstraint(selection[0], selection[1], mo = value)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def run():

    if cmds.window("jeRigToolsWIN", exists = True):
        cmds.deleteUI("jeRigToolsWIN", wnd = True)

    inst = RigTools(getMainWindow())
    inst.show()

