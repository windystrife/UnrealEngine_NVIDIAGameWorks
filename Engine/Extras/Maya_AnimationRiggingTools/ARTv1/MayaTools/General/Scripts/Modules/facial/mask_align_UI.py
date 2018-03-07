
from PySide import QtCore, QtGui

import maya.cmds as cmds
import maya.OpenMayaUI as mui
import shiboken

from Modules.facial import utils
reload(utils)
from Modules.facial import face
reload(face)


import os

def getMayaWindow():
    ptr = mui.MQtUtil.mainWindow()
    if ptr is not None:
        return shiboken.wrapInstance(long(ptr), QtGui.QWidget)

class maskAlignWin(QtGui.QMainWindow):
    def __init__(self, parent=getMayaWindow(), debug=0):
        QtGui.QMainWindow.__init__(self, parent)

        #facial folder
        #facialDir = os.path.dirname(__file__)

        ### BUILD USER INTERFACE
        ##############################

        #create/set a central widget
        wid = QtGui.QWidget()
        self.setCentralWidget(wid)

        #setArt v2 stylesheet
        utils.setArtV2StyleSheet(wid, imageDirectory='/art2images')
        
        self.verticalLayout = QtGui.QVBoxLayout(wid)
        self.verticalLayout.setSpacing(2)
        
        #top face module picker
        self.label = QtGui.QLabel(self)
        self.label.setText("FacialNode:")
        self.horizontalLayout = QtGui.QHBoxLayout()
        self.horizontalLayout.addWidget(self.label)
        self.currentFacialNodeCMB = QtGui.QComboBox(self)
        self.horizontalLayout.addWidget(self.currentFacialNodeCMB)
        
        self.resize(530, 390)

        self.verticalLayout.addLayout(self.horizontalLayout)
        
        self.page1 = QtGui.QFrame()
        self.page1.setMinimumSize(QtCore.QSize( 530, 380 ))
        self.page1.setMaximumSize(QtCore.QSize( 530, 380 ))
        
        self.page2 = QtGui.QFrame()
        self.page2.setMinimumSize(QtCore.QSize( 530, 380 ))
        self.page2.setMaximumSize(QtCore.QSize( 530, 380 ))

        self.page3 = QtGui.QFrame()
        self.page3.setMinimumSize(QtCore.QSize( 530, 380 ))
        self.page3.setMaximumSize(QtCore.QSize( 530, 380 ))

        self.page4 = QtGui.QFrame()
        self.page4.setMinimumSize(QtCore.QSize( 530, 380 ))
        self.page4.setMaximumSize(QtCore.QSize( 530, 380 ))
        
        self.stackedWidget = QtGui.QStackedWidget()
        self.stackedWidget.addWidget(self.page1)
        self.stackedWidget.addWidget(self.page2)
        self.stackedWidget.addWidget(self.page3)
        self.stackedWidget.addWidget(self.page4)
        self.verticalLayout.addWidget(self.stackedWidget)
        
        #set the image backdrops
        image = (os.path.dirname(__file__) + '\\process_splash_align_mask.png').replace('\\','/')
        self.page1.setStyleSheet("background-image: url(" + image + ");")
        
        image = (os.path.dirname(__file__) + '\\process_splash_align_eyes.png').replace('\\','/')
        self.page2.setStyleSheet("background-image: url(" + image + ");")

        image = (os.path.dirname(__file__) + '\\process_splash_snap.png').replace('\\','/')
        self.page3.setStyleSheet("background-image: url(" + image + ");")

        image = (os.path.dirname(__file__) + '\\process_splash_goodbye.png').replace('\\','/')
        self.page4.setStyleSheet("background-image: url(" + image + ");")

        #Add next button
        self.nextBTN = QtGui.QPushButton(self)
        self.nextBTN.setText('NEXT')
        font = QtGui.QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.nextBTN.setFont(font)
        self.nextBTN.setGeometry(420, 300, 90, 25)
        
        #Add back button
        self.backBTN = QtGui.QPushButton(self)
        self.backBTN.setText('BACK')
        font = QtGui.QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.backBTN.setFont(font)
        self.backBTN.setGeometry(420, 330, 90, 25)
        self.backBTN.setHidden(True)
        
        self.stackedWidget.setCurrentIndex(0)

        #build and hide the 'mesh chooser'
        self.meshBTN = QtGui.QPushButton(self)
        self.meshBTN.setText('<<')
        font = QtGui.QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.meshBTN.setFont(font)
        self.meshBTN.setGeometry(0, 0, 25, 25)

        #build and hide the 'mesh chooser'
        self.snapBTN = QtGui.QPushButton(self)
        self.snapBTN.setText('SNAP')
        font = QtGui.QFont()
        font.setPointSize(12)
        font.setBold(True)
        self.snapBTN.setFont(font)
        self.snapBTN.setGeometry(300, 330, 90, 25)

        self.meshLBL = QtGui.QLabel(self)
        self.meshLBL.setText("Facial Mesh:")
        self.meshEdit = QtGui.QLineEdit(self)
        self.hMeshLayout = QtGui.QHBoxLayout()
        self.hMeshLayout.addWidget(self.meshLBL)
        self.hMeshLayout.addWidget(self.meshEdit)
        self.hMeshLayout.addWidget(self.meshBTN)
        self.hMeshLayout.setGeometry(QtCore.QRect(100, 300, 300, 25))

        #close button
        self.closeBTN = QtGui.QPushButton(self)
        self.closeBTN.setText('Thank you Masky,\nI will alsways remember you..')
        font = QtGui.QFont()
        font.setPointSize(16)
        font.setBold(True)
        self.closeBTN.setFont(font)
        self.closeBTN.setGeometry(25, 250, 500, 70)
        self.closeBTN.setHidden(True)

        #button connects
        self.nextBTN.clicked.connect(self.nextFn)
        self.backBTN.clicked.connect(self.backFn)
        self.meshBTN.clicked.connect(self.meshFn)
        self.snapBTN.clicked.connect(self.snapFn)
        self.closeBTN.clicked.connect(self.close)

        self.snapVis(hide=True)

        self.refreshUI()

        #show joint movers and sdks
        cmds.showHidden(self.currentFace.jointMovers)
        cmds.showHidden(self.currentFace.sdks)

    def nextFn(self):
        index = self.stackedWidget.currentIndex()+1
        self.stackedWidget.setCurrentIndex(index)
        if index > 0:
            #disable / hide mask
            if self.currentMask.active:
                self.currentMask.active = False

            self.backBTN.setHidden(False)
            if index == 2:
                self.snapVis(hide=False)
            if index == 3:
                self.snapVis(hide=True)
                self.closeBTN.setHidden(False)
                self.nextBTN.setHidden(True)
                self.backBTN.setHidden(True)

        #show sdks
        if index != 1:
            cmds.showHidden(self.currentFace.sdks)
            cmds.hide(self.currentMask.maskGeo)
            cmds.hide(self.currentMask.controls)
        else:
            cmds.hide(self.currentFace.sdks)

    def backFn(self):
        index = self.stackedWidget.currentIndex()-1
        self.stackedWidget.setCurrentIndex(index)
        if index == 0:
            self.backBTN.setHidden(True)
            self.currentMask.active = True
            #show joint movers and sdks
            cmds.showHidden(self.currentFace.jointMovers)
            cmds.showHidden(self.currentFace.sdks)
        if index != 2:
            self.snapVis(hide=True)
        else:
            self.snapVis(hide=False)
            cmds.hide(self.currentMask.maskGeo)
            cmds.hide(self.currentMask.controls)

        #show sdks
        if index != 1:
            cmds.showHidden(self.currentFace.sdks)
        else:
            cmds.hide(self.currentFace.sdks)

        self.nextBTN.setHidden(False)
        self.closeBTN.setHidden(True)

    def snapVis(self, hide=True):
        if hide:
            self.snapBTN.setHidden(True)
            self.meshLBL.setHidden(True)
            self.meshBTN.setHidden(True)
            self.meshEdit.setHidden(True)

        else:
            self.snapBTN.setHidden(False)
            self.meshLBL.setHidden(False)
            self.meshBTN.setHidden(False)
            self.meshEdit.setHidden(False)

    def meshFn(self):
        mesh = cmds.ls(sl=1)[0]
        self.meshEdit.setText(mesh)
        self.currentFace.renderMeshes = [mesh]

    def snapFn(self):
        self.currentFace.faceMask.active = False
        mesh = self.meshEdit.text()
        self.currentFace.faceMask.snapMovers('y', mesh)
        #self.currentFace.snapEyeSdkPivots()

    def updateFaceAndMask(self):
        currentModule = self.currentFacialNodeCMB.currentText()
        self.currentFace = face.FaceModule(faceNode=currentModule)
        self.currentMask = self.currentFace.faceMask

    def refreshUI(self):
        oldIndex = self.currentFacialNodeCMB.currentIndex()
        for f in utils.getUType('faceModule'):
            self.currentFacialNodeCMB.addItem(f)
        if oldIndex != -1:
            self.currentFacialNodeCMB.setCurrentIndex(oldIndex)
        self.updateFaceAndMask()
        if self.currentFace.renderMeshes:
            self.meshEdit.setText(self.currentFace.renderMeshes[0])