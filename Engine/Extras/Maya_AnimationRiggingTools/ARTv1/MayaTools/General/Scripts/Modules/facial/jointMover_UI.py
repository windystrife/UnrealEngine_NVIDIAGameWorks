'''
from Modules.facial import jointMover_UI as jm_ui
reload(jm_ui)

global jointMoverWin
try:
    jointMoverWin.close()
except:
    pass
jointMoverWin = jm_ui.jointMoverWin()
jointMoverWin.show()
'''

from PySide import QtCore, QtGui

import maya.cmds as cmds
import maya.OpenMayaUI as mui
import shiboken

import os

from Modules.facial import face
from Modules.facial import utils

from Modules.facial import mask_align_UI as ma_ui
reload(ma_ui)


def getMayaWindow():
    ptr = mui.MQtUtil.mainWindow()
    if ptr is not None:
        return shiboken.wrapInstance(long(ptr), QtGui.QWidget)

class jointMoverWin(QtGui.QMainWindow):
    def __init__(self, parent=getMayaWindow(), debug=0):
        QtGui.QMainWindow.__init__(self, parent)
        
        self.currentFace = None
        
        #facial folder
        facialDir = os.path.dirname(__file__)

        for f in utils.getUType('faceModule'):
            print 'jointMover_UI: Face initialized from:', f
            self.currentFace = face.FaceModule(faceNode=f)
        
        ### BUILD USER INTERFACE
        ##############################
        
        #create/set a central widget
        wid = QtGui.QWidget()
        self.setCentralWidget(wid)
        
        #setArt v2 stylesheet
        utils.setArtV2StyleSheet(wid, imageDirectory='/art2images')
        
        self.resize(315, 514)
        
        #top face module picker
        self.label = QtGui.QLabel(self)
        self.label.setText("FacialNode:")
        self.horizontalLayout = QtGui.QHBoxLayout()
        self.horizontalLayout.setSpacing(2)
        self.label.setMaximumSize(QtCore.QSize(78, 16777215))
        self.horizontalLayout.addWidget(self.label)
        self.currentFacialNodeCMB = QtGui.QComboBox(self)
        self.horizontalLayout.addWidget(self.currentFacialNodeCMB)
        
        #attachment joint
        self.h2 = QtGui.QHBoxLayout()
        self.attachJointLBL = QtGui.QLabel(self)
        self.attachJointLBL.setText("Face Attachment Joint:")
        self.h2.setSpacing(2)
        self.h2.addWidget(self.attachJointLBL)
        self.attachJointLINE = QtGui.QLineEdit(self)
        self.h2.addWidget(self.attachJointLINE)
        
        #main vertical layout
        self.verticalLayout = QtGui.QVBoxLayout(wid)
        self.verticalLayout.addLayout(self.horizontalLayout)
        self.verticalLayout.addLayout(self.h2)
        
        #joint mover tree
        self.moverTree = QtGui.QTreeWidget(self)
        self.moverTree.setObjectName("treeWidget")
        self.moverTree.headerItem().setText(0, "1")
        self.verticalLayout.addWidget(self.moverTree)
        self.moverTree.setHeaderHidden(True)
        
        QtCore.QMetaObject.connectSlotsByName(self)
        
        self.setWindowTitle("Select Joint Movers")

        
        #snap button
        self.continueBTN = QtGui.QPushButton(self)
        self.continueBTN.setText('>> Continue >>')
        self.continueBTN.setObjectName("blueButton")
        font = self.continueBTN.font()
        font.setBold(True)
        font.setPointSize(12)
        self.continueBTN.setFont(font)
        
        #load button
        self.loadBTN = QtGui.QPushButton(self)
        self.loadBTN.setText('Template...')
        font = self.loadBTN.font()
        font.setBold(True)
        font.setPointSize(12)
        self.loadBTN.setFont(font)
        self.loadBTN.setMaximumSize(QtCore.QSize(75, 16777215))
        
        #button layout on bottom
        self.h3 = QtGui.QHBoxLayout()
        self.verticalLayout.addLayout(self.h3)
        self.h3.setSpacing(2)
        self.h3.addWidget(self.loadBTN)
        self.h3.addWidget(self.continueBTN)
        
        #connections
        self.moverTree.itemClicked.connect(self.check_status)
        self.continueBTN.pressed.connect(self.continueFn)
        
        #set the mask
        self.mask = self.currentFace.faceMask
        self.mask.active = True
        cmds.showHidden(self.currentFace.sdks)
        
        ### FILL UI
        ##############################
        
        self.attachJointLINE.setText('head')
        self.refreshUI()

    def check_status(self, item, column):
        for i in range(0, self.moverTree.topLevelItemCount()):
            if self.moverTree.topLevelItem(i).checkState(0) == QtCore.Qt.Unchecked:
                utils.hideShapes(self.moverTree.topLevelItem(i).mover)
            else:
                utils.unhideShapes(self.moverTree.topLevelItem(i).mover)
        cmds.select(item.text(0))


    def continueFn(self):
        global maskAlignWin
        try:
            maskAlignWin.close()
        except:
            pass

        maskAlignWin = ma_ui.maskAlignWin()
        maskAlignWin.show()
        self.close()
    
    def refreshUI(self):
        for f in utils.getUType('faceModule'):
            self.currentFacialNodeCMB.addItem(f)
            self.currentFace = face.FaceModule(faceNode=f)
            self.buildExportTree(self.currentFace.jointMovers)
            
    def buildExportTree(self, jointMovers):
        for jm in jointMovers:

            #top level
            wid1 = QtGui.QTreeWidgetItem()
            font = wid1.font(0)
            font.setPointSize(12)
            wid1.setFont(0, font)
            
            wid1.mover = jm.replace('_SDK', '_SDK_offset')

            wid1.setText(0,jm)

            self.moverTree.addTopLevelItem(wid1)
            
            wid1.setFlags(QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemIsEditable | QtCore.Qt.ItemIsUserCheckable)

            #check if enabled
            if not utils.shapesHidden(jm):
                wid1.setCheckState(0, QtCore.Qt.Checked)
            else:
                wid1.setCheckState(0, QtCore.Qt.Unchecked)
