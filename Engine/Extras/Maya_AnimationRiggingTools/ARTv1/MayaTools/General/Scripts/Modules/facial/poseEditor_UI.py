from PySide import QtCore, QtGui

import maya.cmds as cmds
import maya.OpenMayaUI as mui
import shiboken

from Modules.facial import face
reload(face)
from Modules.facial import utils
reload(utils)

import functools
import json

def getMayaWindow():
    ptr = mui.MQtUtil.mainWindow()
    if ptr is not None:
        return shiboken.wrapInstance(long(ptr), QtGui.QWidget)

class poseWidget(QtGui.QWidget):
    def __init__(self, edit=0):
        super(poseWidget, self).__init__()

        self.edit = edit
        self.pose = None
        self.driven = None
        self.verticalLayout = QtGui.QVBoxLayout(self)

        #horiz layout for top half
        self.topHorizLayout = QtGui.QHBoxLayout()


        #pose label
        self.poseLBL = QtGui.QLabel(self)
        font = QtGui.QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.poseLBL.setFont(font)
        self.topHorizLayout.addWidget(self.poseLBL)

        #edit label
        self.editLBL = QtGui.QLabel(self)
        self.editLBL.setText('<font color=\'#CC5200\'>POSE EDITED</font>')
        self.editLBL.setHidden(True)
        font = QtGui.QFont()
        font.setPointSize(8)
        font.setBold(True)
        self.editLBL.setFont(font)
        self.topHorizLayout.insertStretch(1)
        self.topHorizLayout.addWidget(self.editLBL)

        self.verticalLayout.addLayout(self.topHorizLayout)

        #horiz layout for bottom half
        self.horizontalLayout = QtGui.QHBoxLayout()

        #slider
        self.poseSlider = QtGui.QSlider(self)
        self.poseSlider.setMaximum(100)
        self.poseSlider.setOrientation(QtCore.Qt.Horizontal)
        self.horizontalLayout.addWidget(self.poseSlider)
        self.sliderValueLBL = QtGui.QLabel(self)
        self.horizontalLayout.addWidget(self.sliderValueLBL)
        self.sliderValueLBL.setText('0.0')

        if self.edit:
            #update button
            self.updateBTN = QtGui.QPushButton(self)
            self.updateBTN.setMinimumSize(QtCore.QSize(92, 0))
            self.updateBTN.setText('UPDATE POSE')
            font = QtGui.QFont()
            font.setPointSize(12)
            font.setBold(True)
            self.updateBTN.setFont(font)
            self.horizontalLayout.addWidget(self.updateBTN)
            #reset button
            self.resetBTN = QtGui.QPushButton(self)
            self.resetBTN.setMinimumSize(QtCore.QSize(50, 0))
            self.resetBTN.setText('RESET')
            font = QtGui.QFont()
            font.setPointSize(12)
            font.setBold(True)
            self.resetBTN.setFont(font)
            self.horizontalLayout.addWidget(self.resetBTN)
            self.resetBTN.setHidden(True)
            self.resetBTN.setObjectName("orangeButton")
        self.verticalLayout.addLayout(self.horizontalLayout)

        #margins
        self.horizontalLayout.setContentsMargins(2,2,2,2)
        self.horizontalLayout.setSpacing(3)
        self.verticalLayout.setContentsMargins(2,2,2,2)
        self.verticalLayout.setSpacing(1)

    def mousePressEvent(self, event):
        nodes = list(set([x.split('.')[0] for x in self.driven]))
        cmds.select(nodes)


class poseEditorWin(QtGui.QMainWindow):
    def __init__(self, parent=getMayaWindow(), mode='choose', edit=0):
        QtGui.QMainWindow.__init__(self, parent)

        self.edit = edit
        self.currentFace = None

        #TODO this won't work with multiple faces
        for f in utils.getUType('faceModule'):
            print 'poseChooser_UI: Face initialized from:', f
            self.currentFace = face.FaceModule(faceNode=f)

        ### BUILD USER INTERFACE
        ##############################

        #create/set a central widget
        wid = QtGui.QWidget()
        self.setCentralWidget(wid)

        #setArt v2 stylesheet
        utils.setArtV2StyleSheet(wid, imageDirectory='/art2images')

        self.resize(400, 514)

        #top face module picker
        self.label = QtGui.QLabel(self)
        self.label.setText("FacialNode:")
        self.horizontalLayout = QtGui.QHBoxLayout()
        self.horizontalLayout.setSpacing(2)
        self.label.setMaximumSize(QtCore.QSize(78, 16777215))
        self.horizontalLayout.addWidget(self.label)
        self.currentFacialNodeCMB = QtGui.QComboBox(self)
        self.horizontalLayout.addWidget(self.currentFacialNodeCMB)

        #header and instruction
        self.h2 = QtGui.QHBoxLayout()
        self.attachJointLBL = QtGui.QLabel(self)

        if self.edit:
            self.attachJointLBL.setText("Edit Face Poses")
        else:
            self.attachJointLBL.setText("Available Face Poses")

        font = self.attachJointLBL.font()
        font.setBold(True)
        font.setPointSize(13)
        self.attachJointLBL.setFont(font)

        #options horizontal layout
        self.optionsLayout = QtGui.QHBoxLayout()

        if self.edit:
            #zero out button
            self.zeroBTN = QtGui.QPushButton(self)
            self.zeroBTN.setText('ZERO ALL')
            self.zeroBTN.setMaximumSize(QtCore.QSize(75, 50))

            #mirror edit out button
            self.mirrorPoseBTN = QtGui.QPushButton(self)
            self.mirrorPoseBTN.setText('MIRROR SELECTED POSE')

            #checkboxes
            self.moveMirrorsCHK = QtGui.QCheckBox(self)
            self.moveMirrorsCHK.setText('Sync Movement of Mirror Poses')

        self.info = QtGui.QLabel(self)
        if self.edit:
            self.info.setText("Check and edit the facial poses to make them better match your character")
        else:
            self.info.setText("These poses are available for your character's facial rig, based on the Facial Joint Movers you have selected")
        self.info.setWordWrap(1)

        self.h2.setSpacing(2)
        self.h2.addWidget(self.attachJointLBL)


        #main vertical layout
        self.verticalLayout = QtGui.QVBoxLayout(wid)
        self.verticalLayout.addLayout(self.horizontalLayout)
        self.verticalLayout.addLayout(self.h2)
        self.verticalLayout.addWidget(self.info)

        #pose list
        self.poseList = QtGui.QListWidget(self)
        self.verticalLayout.addWidget(self.poseList)

        QtCore.QMetaObject.connectSlotsByName(self)

        self.setWindowTitle("Available Facial Poses")

        if self.edit:
            #options area
            self.verticalLayout.addWidget(self.moveMirrorsCHK)
            self.verticalLayout.addLayout(self.optionsLayout)
            self.optionsLayout.addWidget(self.zeroBTN)
            self.optionsLayout.addWidget(self.mirrorPoseBTN)


        #continue button
        self.continueBTN = QtGui.QPushButton(self)
        self.continueBTN.setText('>> Continue >>')
        self.continueBTN.setObjectName("blueButton")
        font = self.continueBTN.font()
        font.setBold(True)
        font.setPointSize(12)
        self.continueBTN.setFont(font)

        #back button
        self.backBTN = QtGui.QPushButton(self)
        self.backBTN.setText('<< Back <<')
        self.backBTN.setObjectName("blueButton")
        font = self.backBTN.font()
        font.setBold(True)
        font.setPointSize(12)
        self.backBTN.setFont(font)

        #button layout on bottom
        self.h3 = QtGui.QHBoxLayout()
        self.verticalLayout.addLayout(self.h3)
        self.h3.setSpacing(2)
        self.h3.addWidget(self.backBTN)
        self.h3.addWidget(self.continueBTN)

        #self.verticalLayout.setContentsMargins(2,2,2,2)
        self.verticalLayout.setSpacing(3)

        #Make SDK transforms editable
        #hide facial mask controls and mask

        #show special eye and jaw movers

        #unhide jointMovers, make LRAs unselectable

        #connections
        self.currentFacialNodeCMB.currentIndexChanged.connect(self.faceChangedFn)
        #self.moverTree.itemClicked.connect(self.check_status)
        #self.continueBTN.pressed.connect(self.continueFn)

        #set the mask
        self.mask = face.FaceMask
        self.mask.active = True

        ### CONNECTS
        if self.edit:
            self.zeroBTN.pressed.connect(self.zeroAllFn)

        ### FILL UI
        ##############################

        self.refreshUI()

    ### UI HOOKS
    ##############################

    def refreshUI(self):
        for f in utils.getUType('faceModule'):
            self.currentFacialNodeCMB.addItem(f)
            self.currentFace = face.FaceModule(faceNode=f)
            self.buildPoseList()

    def updateFn(self, wid):
        #check that the pose is fully on
        if wid.poseSlider.value() != 100:
            cmds.warning('Pose slider is not fully on, must be 1.0 in able to save edit.')
            return False

        cmds.setDrivenKeyframe(wid.driven, cd=wid.pose)
        self.checkForEditedPoses()

    def resetFn(self, wid):
        print 'resetting', wid
        try:
            cmds.undoInfo(openChunk=True)
            #get original pose from driven dict
            initialPoseDict = json.loads(cmds.getAttr(self.currentFace.blendBoard + '.initialPoseDict'))
            initialPoseAttrs = initialPoseDict[wid.pose]

            #store current slider/attr val
            preVal = cmds.getAttr(wid.pose)

            #iterate over the attrs and set them bvack to original
            #TODO: rotate order?
            cmds.setAttr(wid.pose, 1.0)
            for att in wid.driven:
                #print att, initialPoseAttrs[att]
                cmds.setAttr(att, initialPoseAttrs[att])

            #set the old driven keyframe
            cmds.setDrivenKeyframe(wid.driven, cd=wid.pose)

            #reset the slider/attr val
            cmds.setAttr(wid.pose, preVal)
            self.checkForEditedPoses()

        except Exception, err:
            import traceback
            print(traceback.format_exc())
        finally:
            cmds.undoInfo(closeChunk=True)

    def sliderChanged(self, wid, value):
        val = float(value)/100
        floatVal = str(val)
        wid.sliderValueLBL.setText(floatVal)
        cmds.setAttr(wid.pose, val)

    def zeroAllFn(self):
        for i in range(0, self.poseList.count()):
            wid = self.poseList.itemWidget(self.poseList.item(i))
            wid.poseSlider.setValue(0)

    def getAllDrivenAttrs(self):
        ret = {}
        for i in range(0, self.poseList.count()):
            wid = self.poseList.itemWidget(self.poseList.item(i))
            ret[wid.pose] = cmds.getAttr(wid.pose)
        return ret

    def setAllDrivenAttrs(self, dict):
        for pose in dict:
            cmds.setAttr(pose, dict[pose])

    def zeroAllAttrs(self):
        for i in range(0, self.poseList.count()):
            wid = self.poseList.itemWidget(self.poseList.item(i))
            cmds.setAttr(wid.pose, 0.0)

    #not used atm
    def getSliders(self):
        ret = []
        for i in range(0, self.poseList.count()):
            wid = self.poseList.itemWidget(self.poseList.item(i))
            ret.append(wid.poseSlider.value())
        return ret

    #not used atm
    def setSliders(self, sliders):
        for i in range(0, len(sliders)):
            wid = self.poseList.itemWidget(self.poseList.item(i))
            wid.poseSlider.setValue(sliders[i])

    def checkForEditedPoses(self, threshold=0.001):
        initialPoseDict = json.loads(cmds.getAttr(self.currentFace.blendBoard + '.initialPoseDict'))

        for i in range(0, self.poseList.count()):
            edited = False
            wid = self.poseList.itemWidget(self.poseList.item(i))
            preVals = self.getAllDrivenAttrs()
            self.zeroAllAttrs()
            cmds.setAttr(wid.pose, 1.0)

            #iterate and check driven att values
            initialPoseAttrs = initialPoseDict[wid.pose]
            for att in wid.driven:
                error = abs(initialPoseAttrs[att] - cmds.getAttr(att))
                if error > threshold:
                    edited = True
            if edited:
                wid.editLBL.setHidden(False)
                wid.resetBTN.setHidden(False)
            else:
                wid.editLBL.setHidden(True)
                wid.resetBTN.setHidden(True)
            self.setAllDrivenAttrs(preVals)

    def addPose(self, pose, driven):
        wid = poseWidget(edit=self.edit)
        wid.pose = pose
        wid.driven = driven
        currentVal = cmds.getAttr(pose)
        wid.poseLBL.setText(pose.split('.')[-1])
        wid2 = QtGui.QListWidgetItem()
        self.poseList.addItem(wid2)
        wid2.setSizeHint(QtCore.QSize(100, 60))
        self.poseList.setItemWidget(wid2, wid)

        #hookup slider
        wid.poseSlider.valueChanged.connect(functools.partial(self.sliderChanged, wid))
        wid.poseSlider.setValue(currentVal*100)

        if self.edit:
            #hookup buttons
            wid.updateBTN.pressed.connect(functools.partial(self.updateFn, wid))
            wid.resetBTN.pressed.connect(functools.partial(self.resetFn, wid))

        #store driven for checking later
        wid.driven = driven

    def buildPoseList(self):
        self.poseList.clear()
        for pose in self.currentFace.poses:
            self.addPose(pose, self.currentFace.poses[pose])
        if self.edit:
            self.checkForEditedPoses()

    def faceChangedFn(self):
        self.currentFace = face.FaceModule(faceNode=self.currentFacialNodeCMB.currentText())
        self.buildPoseList()

if __name__ == '__main__':
    global poseWin
    try:
        poseWin.close()
    except:
        pass


    poseWin = poseEditorWin(edit=1)
    poseWin.show()





