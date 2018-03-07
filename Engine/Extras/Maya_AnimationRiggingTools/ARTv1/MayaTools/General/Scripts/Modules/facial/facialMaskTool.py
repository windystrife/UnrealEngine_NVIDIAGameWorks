'''
FACIAL TOOL
'''

import os

#uic related
import shiboken
from PySide import QtGui, QtCore
reload(QtGui)
reload(QtCore)
from cStringIO import StringIO
import xml.etree.ElementTree as xml
import pysideuic

import maya.cmds as cmds
import maya.OpenMayaUI as openMayaUI
import maya.mel as mel

#internal libs
from Modules.facial import utils
reload(utils)
from Modules.facial import jointMover_UI as jm_ui
reload(jm_ui)
from Modules.facial import poseEditor_UI as pose_ui
reload(pose_ui)
from Modules.facial import face
reload(face)
from Modules.facial import mask_align_UI as ma_ui
reload(jm_ui)

########
## UIC
########
def show():
    global facialMaskToolWindow
    try:
        facialMaskToolWindow.close()
    except:
        pass
    facialMaskToolWindow = facialMaskTool()
    facialMaskToolWindow.show()
    return facialMaskToolWindow

def loadUiType(uiFile):
    """
    Pyside lacks the "loadUiType" command, so we have to convert the ui file to py code in-memory first
    and then execute it in a special frame to retrieve the form_class.
    http://tech-artists.org/forum/showthread.php?3035-PySide-in-Maya-2013
    """
    parsed = xml.parse(uiFile)
    widget_class = parsed.find('widget').get('class')
    form_class = parsed.find('class').text

    with open(uiFile, 'r') as f:
        o = StringIO()
        frame = {}

        pysideuic.compileUi(f, o, indent=0)
        pyc = compile(o.getvalue(), '<string>', 'exec')
        exec pyc in frame

        #Fetch the base_class and form class based on their type in the xml from designer
        form_class = frame['Ui_%s'%form_class]
        base_class = eval('QtGui.%s'%widget_class)
    return form_class, base_class

def getMayaWindow():
    ptr = openMayaUI.MQtUtil.mainWindow()
    if ptr is not None:
        return shiboken.wrapInstance(long(ptr), QtGui.QWidget)

uiFile = None	
try:
    selfDirectory = os.path.dirname(__file__)
    uiFile = selfDirectory + '/facialMaskTool.ui'
except:
    uiFile = 'D:\\Build\\usr\\jeremy_ernst\\MayaTools\\General\\Scripts\\Modules\\facial\\facialMaskTool.ui'
if os.path.isfile(uiFile):
    form_class, base_class = loadUiType(uiFile)
else:
    cmds.error('Cannot find UI file: ' + uiFile)



##############
## MASK TOOL
##############
class facialMaskTool(base_class, form_class):	  
    def __init__(self, parent=getMayaWindow()):
        self.closeExistingWindow()
        super(facialMaskTool, self).__init__(parent)
        
        self.setupUi(self)
        
        wName = openMayaUI.MQtUtil.fullName(long(shiboken.getCppPointer(self)[0]))
        
        self.currentFace = None
        
        ## Connect UI
        ########################################################################
        self.createJointMoverBTN.clicked.connect(self.createJointMoverFn)
        self.showHideAttachBTN.clicked.connect(self.showHideAttachFn)
        
        self.openJointMoverDlgBTN.clicked.connect(self.openJointMoverDlgFn)
        self.openPoseDlgBTN.clicked.connect(self.openPoseDlgFn)
        self.openMaskAlignmentDlgBTN.clicked.connect(self.openMaskAlignmentDlgFn)
        
        self.selectFaceBTN.clicked.connect(self.selectFaceFn)
        self.selectMaskBTN.clicked.connect(self.selectMaskFn)
        self.maskModeBTN.clicked.connect(self.maskModeFn)
        self.constrainToMaskBTN.pressed.connect(self.constrainToMaskFn)
        self.buildBlendBoardBTN.pressed.connect(self.buildBlendBoardFn)
        self.stampDeltasBTN.pressed.connect(self.stampDeltasFn)
        self.mirrorPlacementBTN.pressed.connect(self.mirrorPlacementFn)
        self.deleteSdkParentConstraintsBTN.pressed.connect(self.deleteSdkParentConstraintsFn)

        self.refreshUI()
    
    ##UI
    ######
    
    def stampDeltasFn(self):
        self.currentFace.stampDeltas()
    
    def createJointMoverFn(self):
        inputName, ok = QtGui.QInputDialog.getText(self, 'Create Facial Joint Mover', 'Base Name:')
        if ok:
            self.currentFace.createJointMover(name=inputName, scale=0.3,toroidRatio=0.05, facialNode=self.currentFace.node)
        self.refreshUI()
    
    def showHideAttachFn(self):
        if self.showHideAttachBTN.isChecked():
            print 'hiding'
            self.showHideAttachBTN.setText('Show Mask Attach Nodes')
        else:
            print 'showing'
            self.showHideAttachBTN.setText('Hide Mask Attach Nodes')
            
    def closeExistingWindow(self):
        for qt in QtGui.qApp.topLevelWidgets():
            try:
                if qt.__class__.__name__ == self.__class__.__name__:
                    qt.deleteLater()
                    print 'uExport: Closed ' + str(qt.__class__.__name__)
            except:
                pass
    
    def buildBlendBoardFn(self):
        fName, ok = QtGui.QFileDialog.getOpenFileName(self, "Select Poses file")
        if ok:
            self.currentFace.buildBlendBoard(fName)
    
    def constrainToMaskFn(self):
        ma = cmds.ls(sl=1)[0]
        if '_maskAttach' in ma:
            utils.constrainToMesh(self.currentFace.faceMask.maskGeo, ma)
        else:
            cmds.warning('constrainToMask: Node does not appear to be a maskAttach nmode: ' + ma)
    
    def openJointMoverDlgFn(self):
        global jointMoverWin
        try:
            jointMoverWin.close()
        except:
            pass
        
        jointMoverWin = jm_ui.jointMoverWin()
        jointMoverWin.show()
    
    def openPoseDlgFn(self):
        global poseWin
        try:
            poseWin.close()
        except:
            pass
        
        poseWin = pose_ui.poseEditorWin()
        poseWin.show()

    def openMaskAlignmentDlgFn(selfself):
        reload(jm_ui)
        global maskAlignWin
        try:
            maskAlignWin.close()
        except:
            pass

        maskAlignWin = ma_ui.maskAlignWin()
        maskAlignWin.show()

    def selectFaceFn(self):
        cmds.select(self.currentFace.node)
        
    def selectMaskFn(self):
        cmds.select(self.currentFace.faceMask)
        
    def maskModeFn(self):
        if self.maskModeBTN.isChecked():
            self.currentFace.faceMask.active = True
            self.maskModeBTN.setText('Mask Mode: Active')
            print 'MASK MODE: ACTIVE'
        else:
            self.currentFace.faceMask.active = False
            self.maskModeBTN.setText('Mask Mode: Disabled')
            print 'MASK MODE: DISABLED'
    
    def mirrorPlacementFn(self):
        try:
            cmds.undoInfo(openChunk=True)
            for node in cmds.ls(sl=1):
                prefix = node.lower()[:1]
                if prefix == 'l':
                    mirrorNode = 'r_' + node[2:]
                    if not cmds.objExists(mirrorNode):
                        print 'Creating mirrorNode:', mirrorNode
                        inputName = mirrorNode.split('_mover_')[0]
                        nodes = self.currentFace.createJointMover(name=inputName, scale=0.3,toroidRatio=0.05, facialNode=self.currentFace.node)
                        
                    self.currentFace.mirrorFacialXform(node, axis='x', debug=0)
            cmds.undoInfo(closeChunk=True)
        except Exception as e:
            print e
            cmds.undoInfo(closeChunk=True)

    def deleteSdkParentConstraintsFn(self):
        if self.deleteSdkParentConstraintsBTN.isChecked():
            print '>>>buildSdkParentConstraints'
            self.currentFace.mask.buildSdkParentConstraints()
            self.deleteSdkParentConstraintsBTN.setText('deleteSdkConstraints')
        else:
            print '>>>deleteSdkParentConstraints'
            self.currentFace.mask.deleteSdkParentConstraints()
            self.deleteSdkParentConstraintsBTN.setText('buildSdkConstraints')
            
    def refreshUI(self):
        for f in utils.getUType('faceModule'):
            self.currentFacialNodeCMB.clear()
            self.currentFacialNodeCMB.addItem(f)
            self.currentFace = face.FaceModule(faceNode=f)
            self.maskControlsLBL.setText('Mask Controls: ' + str(len(self.currentFace.faceMask.controls)))
            if self.currentFace.jointMovers:
                self.facialMoversLBL.setText('Facial Joint Movers: ' + str(len(self.currentFace.jointMovers)))
            self.maskGeoLBL.setText('Mask Geo: ' + self.currentFace.faceMask.maskGeo)
            if self.currentFace.sdks:
                self.drivenNodesLBL.setText('Driven Nodes: ' + str(len(self.currentFace.sdks)))

if __name__ == '__main__':
    show()
