'''
uExport
Christopher Evans, Version 0.1, Feb 2014
@author = Chris Evans
version = 0.2

Add this to a shelf:
import uExport as ue
uExportToolWindow = ue.show()

'''

import os

import shiboken
import time
from PySide import QtGui, QtCore
from cStringIO import StringIO
import xml.etree.ElementTree as xml
import pysideuic

import maya.cmds as cmds
import maya.OpenMayaUI as openMayaUI
import maya.mel as mel

def show():
    global uExportToolWindow
    try:
        uExportToolWindow.close()
    except:
        pass

    uExportToolWindow = uExportTool()
    uExportToolWindow.show()
    return uExportToolWindow

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

try:
    selfDirectory = os.path.dirname(__file__)
    uiFile = selfDirectory + '/uExport.ui'
except:
    uiFile = 'D:\\Build\\usr\\jeremy_ernst\\MayaTools\\General\\Scripts\\Modules\\uExport\\uExport.ui'
    form_class, base_class = loadUiType(uiFile)

if os.path.isfile(uiFile):
    form_class, base_class = loadUiType(uiFile)
else:
    cmds.error('Cannot find UI file: ' + uiFile)

#this is used by both classes below, this method is usually in the utils lib at Epic
def attrExists(attr):
    if '.' in attr:
        node, att = attr.split('.')
        return cmds.attributeQuery(att, node=node, ex=1)
    else:
        cmds.warning('attrExists: No attr passed in: ' + attr)
        return False

class uExport():
    '''
    Just a little basket to store things.
    TODO: Add properties to get/set values
    '''
    def __init__(self, node):
        self.meshes = cmds.listConnections(node + '.rendermesh')
        self.root = cmds.listConnections(node + '.export_root')

        #joints
        self.joints = [self.root]
        children = cmds.listRelatives(self.root, type='joint',allDescendents=True)
        if children:
            self.joints.extend(children)

        self.version = cmds.getAttr(node + '.uexport_ver')
        self.node = node
        self.name = node.split('|')[-1]
        self.asset_name = node
        self.folder_path = None
        if attrExists(node + '.asset_name'):
            self.asset_name = cmds.getAttr(node + '.asset_name')
        if attrExists(node + '.fbx_name'):
            self.fbx_name = cmds.getAttr(node + '.fbx_name')
        if attrExists(node + '.folder_path'):
            self.folder_path = cmds.getAttr(node + '.folder_path')


########################################################################
## UEXPORT TOOL
########################################################################


class uExportTool(base_class, form_class):
    title = 'uExportTool 0.2'

    currentMesh = None
    currentSkin = None
    currentInf = None
    currentVerts = None
    currentNormalization = None

    scriptJobNum = None
    copyCache = None

    jointLoc = None

    iconLib = {}
    iconPath = os.environ.get('MAYA_LOCATION', None) + '/icons/'
    iconLib['joint'] = QtGui.QIcon(QtGui.QPixmap(iconPath + 'kinJoint.png'))
    iconLib['ikHandle'] = QtGui.QIcon(QtGui.QPixmap(iconPath + 'kinHandle.png'))
    iconLib['transform'] = QtGui.QIcon(QtGui.QPixmap(iconPath + 'orientJoint.png'))

    def __init__(self, parent=getMayaWindow()):
        self.closeExistingWindow()
        super(uExportTool, self).__init__(parent)

        self.setupUi(self)
        self.setWindowTitle(self.title)

        wName = openMayaUI.MQtUtil.fullName(long(shiboken.getCppPointer(self)[0]))

        ## Connect UI
        ########################################################################
        self.connect(self.export_BTN, QtCore.SIGNAL("clicked()"), self.export_FN)
        self.connect(self.createUexportNode_BTN, QtCore.SIGNAL("clicked()"), self.createUexportNode_FN)
        self.connect(self.replaceUnknownNodes, QtCore.SIGNAL("clicked()"), self.replaceUnknownNodes_FN)
        self.refreshBTN.clicked.connect(self.refreshUI)

        #context menu
        self.export_tree.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.connect(self.export_tree, QtCore.SIGNAL("customContextMenuRequested(QPoint)" ), self.openMenu)

        self.export_tree.itemClicked.connect(self.check_status)

        self.snapRoot_CMB.setHidden(True)
        self.refreshUI()

## GENERAL
########################################################################

    def closeExistingWindow(self):
        for qt in QtGui.qApp.topLevelWidgets():
            try:
                if qt.__class__.__name__ == self.__class__.__name__:
                    qt.deleteLater()
                    print 'uExport: Closed ' + str(qt.__class__.__name__)
            except:
                pass

    def convertSkelSettingsToNN(delete=1):
        orig = 'SkeletonSettings_Cache'
        if cmds.objExists(orig):
            if cmds.nodeType(orig) == 'unknown':
                print orig
                new = cmds.createNode('network')
                for att in cmds.listAttr(orig):
                    if not cmds.attributeQuery(att, node=new, exists=1):
                        typ = cmds.attributeQuery(att, node=orig, at=1)
                        if typ == 'typed':
                            cmds.addAttr(new, longName=att, dt='string')
                            if cmds.getAttr(orig + '.' + att):
                                cmds.setAttr(new + '.' + att, cmds.getAttr(orig + '.' + att), type='string')
                        elif typ == 'enum':
                            cmds.addAttr(new, longName=att, at='enum', enumName=cmds.attributeQuery(att, node=orig, listEnum=1)[0])
                cmds.delete(orig)
                cmds.rename(new, 'SkeletonSettings_Cache')

    def replaceUnknownNodes_FN(self):
        self.convertSkelSettingsToNN()

## UI RELATED
########################################################################

    def openMenu(self, position):
        menu = QtGui.QMenu()
        rendermeshRewire = menu.addAction("Re-wire rendermesh attr to current mesh selection")
        rootRewire = menu.addAction("Re-wire root joint attr to current selected joint")
        pos = self.export_tree.mapToGlobal(position)
        action = menu.exec_(pos)
        if action == rendermeshRewire:
            for index in self.export_tree.selectedIndexes():
                uExportNode = self.export_tree.itemFromIndex(index).uExport.node
                meshes = cmds.ls(sl=1)
                #TODO: check if theyre actually meshes
                self.connectRenderMeshes(uExportNode, meshes)
    	elif action == rootRewire:
            index = self.export_tree.selectedIndexes()[0]
            uExportNode = self.export_tree.itemFromIndex(index).uExport.node
            root = cmds.ls(sl=1)
            if len(root) == 1:
                self.connectRoot(uExportNode, root[0])
            else:
                cmds.error('Select a single joint, you == fail, bro. ' + str(root))
        self.refreshUI()

    def refreshUI(self):
        self.export_tree.clear()
        self.uNodes = []
        for node in self.getExportNodes():
            self.uNodes.append(uExport(node))
        self.buildExportTree(self.uNodes)


    def buildExportTree(self, uNodes):
        for uNode in uNodes:

            #top level
            wid1 = QtGui.QTreeWidgetItem()
            font = wid1.font(0)
            font.setPointSize(15)

            wid1.setText(0,uNode.asset_name)
            wid1.uExport = uNode

            wid1.setText(1, uNode.version)
            self.export_tree.addTopLevelItem(wid1)
            wid1.setExpanded(True)
            wid1.setFont(0,font)

            font = wid1.font(0)
            font.setPointSize(10)

            wid1.setFlags(QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemIsEditable | QtCore.Qt.ItemIsUserCheckable | QtCore.Qt.ItemIsSelectable)
            wid1.setCheckState(0, QtCore.Qt.Checked)

            #mesh branch
            meshTop = QtGui.QTreeWidgetItem()
            meshes = uNode.meshes
            if meshes:
                meshTop.setText(0, 'RENDER MESHES: (' + str(len(uNode.meshes)) + ')')
            else:
                meshTop.setText(0, 'RENDER MESHES: NONE')
            meshTop.setFlags(QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemIsEditable | QtCore.Qt.ItemIsUserCheckable)
            meshTop.setCheckState(0, QtCore.Qt.Checked)
            wid1.addChild(meshTop)

            #meshes
            if uNode.meshes:
                for mesh in uNode.meshes:
                    meshWid = QtGui.QTreeWidgetItem()
                    meshWid.setText(0, mesh)
                    meshTop.addChild(meshWid)

            if uNode.root:
                #anim branch
                animTop = QtGui.QTreeWidgetItem()
                jnts = uNode.joints
                if jnts:
                    animTop.setText(0, 'ANIMATION:  (' + str(len(uNode.joints)) + ' JOINTS)')
                else:
                    animTop.setText(0, 'ANIMATION: NONE')
                animTop.setFlags(QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemIsEditable | QtCore.Qt.ItemIsUserCheckable)
                animTop.setCheckState(0, QtCore.Qt.Checked)
                wid1.addChild(animTop)

                #anims
                animWid = QtGui.QTreeWidgetItem()
                animWid.setText(0, '<< CURRENT TIME RANGE >>')
                animTop.addChild(animWid)

            #meta branch
            metaTop = QtGui.QTreeWidgetItem()
            metaTop.setText(0, 'METADATA')
            wid1.addChild(metaTop)

            #meta
            fpathWid = QtGui.QTreeWidgetItem()
            fpathWid.setText(0, 'FOLDER_PATH:   ' + str(uNode.folder_path))
            metaTop.addChild(fpathWid)
            #meta
            fbxName = QtGui.QTreeWidgetItem()
            fbxName.setText(0, 'FBX_FILE:   ' + str(uNode.fbx_name))
            metaTop.addChild(fbxName)

    def createUexportNode_FN(self):
        if cmds.ls(sl=1):
            if self.useRoot_CHK.isChecked():
                rootName = self.rootName_CMB.currentText()
                self.create(renderMeshes=cmds.ls(sl=1), rootJoint=rootName)
            else:
                #TODO: modal picker with joint filter
                pass

    def export_FN(self):
        #get what should be export in an exportTask fn
        exportWidgets = self.getExportNodeWidgets()
        for wid in exportWidgets:
            snapConst = None
            if self.snapRoot_CHK.isChecked():
                node = self.snapRoot_CMB.currentText()
                try:
                    if cmds.objExists(node):
                        snapConst = cmds.parentConstraint(node, wid.uExport.root)
                    else:
                        cmds.error('Unable to find node: ' + node)
                except:
                    cmds.warning('Could not constrain root' + wid.uExport.root + ', is it already constrained?')
            #export
            mpath = cmds.file(sceneName=1, q=1)
            fname = mpath.split('/')[-1]
            fpath = mpath.replace(fname,'').replace('/','\\')
            if wid.uExport.fbx_name:
                if wid.uExport.fbx_name != '':
                    fname = wid.uExport.fbx_name
            if wid.uExport.folder_path:
                if wid.uExport.folder_path != '':
                    fpath = wid.uExport.folder_path

            #prompt user for save location
            userPath = QtGui.QFileDialog.getSaveFileName(caption = 'Export ' + wid.uExport.name + ' to:', filter='FBX Files (*.fbx)', dir=fpath + '\\' + fname)[0]

            if userPath:
                wid.uExport.fbx_name = userPath.split('/')[-1]
                wid.uExport.folder_path = userPath.replace(wid.uExport.fbx_name,'')

                #if set to save loc, save it
                if self.rememberSaveCHK.isChecked():
                    cmds.setAttr(wid.uExport.name + '.folder_path', wid.uExport.folder_path, type='string')
                    cmds.setAttr(wid.uExport.name + '.fbx_name', wid.uExport.fbx_name, type='string')

                meshChk = 1
                animChk = 1
                for c in range(0, wid.childCount()):
                    if 'MESH' in wid.child(c).text(0):
                        if wid.child(c).checkState(0) == QtCore.Qt.Checked:
                            pass
                        else:
                            meshChk = 0

                start = time.time()
                #put export into the uExport class later
                self.export(wid.uExport.name, path=userPath, mesh=meshChk)
                elapsed = (time.time() - start)
                print 'uExport>>> Exported ', wid.text(0), 'to', userPath ,'in %.2f seconds.' % elapsed

            else:
                cmds.warning('Invalid path specified: [' + str(userPath) + ']')
            #cleanup constraint
            if snapConst: cmds.delete(snapConst)

    def getExportNodeWidgets(self):
        nodes = []
        for i in range(0, self.export_tree.topLevelItemCount()):
            if self.export_tree.topLevelItem(i).checkState(0) == QtCore.Qt.Checked:
                nodes.append(self.export_tree.topLevelItem(i))
        return nodes


    def check_status(self):
        for i in range(0, self.export_tree.topLevelItemCount()):
            if self.export_tree.topLevelItem(i).checkState(0) == QtCore.Qt.Unchecked:
                for c in range(0, self.export_tree.topLevelItem(i).childCount()):
                    self.export_tree.topLevelItem(i).child(c).setCheckState(0,QtCore.Qt.Unchecked)

## P4 CRAP

## UEXPORT NODE
########################################################################
    def getExportNodes(self):
        return cmds.ls('*.uexport_ver', o=1, r=1)

    def connectRenderMeshes(self, uNode, renderMeshes, rewire=1):
        try:
            cmds.undoInfo(openChunk=True)
            if rewire:
                for conn in cmds.listConnections(uNode + '.rendermesh', plugs=1, destination=1):
                    cmds.disconnectAttr(uNode + '.rendermesh', conn)
        
            for node in renderMeshes:
                if not attrExists(node+'.export'):
                    cmds.addAttr(node, longName='export', attributeType='message')
                cmds.connectAttr(uNode + '.rendermesh', node + '.export' )
        except Exception as e:
            print e
        finally:
            cmds.undoInfo(closeChunk=True)

    def connectRoot(self, uNode, root, rewire=1):
        try:
            cmds.undoInfo(openChunk=True)
            if rewire:
                conns = cmds.listConnections(uNode + '.export_root', plugs=1, source=1)
                if conns:
                    for conn in conns:
                        cmds.disconnectAttr(conn, uNode + '.export_root')
        
            if not attrExists(root+'.export'):
                cmds.addAttr(root, longName='export', attributeType='message')
            cmds.connectAttr(root + '.export', uNode + '.export_root' )
        except Exception as e:
            print e
        finally:
            cmds.undoInfo(closeChunk=True)
        
    def create(self, renderMeshes=None, rootJoint=None, strName='uExport'):
        uExport = None
        if cmds.objExists(strName):
            #later re-hook up
            #set uExport
            pass
        else:
            uExport = cmds.group(em=1, name=strName)
            cmds.addAttr(uExport, ln='rendermesh', at='message')
            cmds.addAttr(uExport, ln='export_root', at='message')
            cmds.addAttr(uExport, ln='materials', at='message')
            cmds.addAttr(uExport, ln='uexport_ver', dt='string')
            cmds.setAttr(uExport + '.uexport_ver', '1.0', type='string')
            cmds.addAttr(uExport, ln='folder_path', dt='string')
            cmds.addAttr(uExport, ln='asset_name', dt='string')
            cmds.addAttr(uExport, ln='fbx_name', dt='string')

        if uExport:
            try:
                if renderMeshes:
                    self.connectRenderMeshes(uExport, renderMeshes, rewire=0)

                #rootJoint
                if rootJoint:
                    if not attrExists(rootJoint+'.export'):
                        cmds.addAttr(rootJoint, longName='export', attributeType='message')
                    cmds.connectAttr(rootJoint + '.export', uExport + '.export_root')
                else:
                    cmds.warning('No root joint or could not find root: ' + str(rootJoint))
            except Exception as e:
                print cmds.warning(e)



## EXPORT
########################################################################

    #TODO: Find and export blendshape meshes!
    def setExportFlags(self):

        # Mesh
        mel.eval("FBXExportSmoothingGroups -v true")
        mel.eval("FBXExportHardEdges -v false")
        mel.eval("FBXExportTangents -v false")
        mel.eval("FBXExportInstances -v false")
        mel.eval("FBXExportInAscii -v true")
        mel.eval("FBXExportSmoothMesh -v false")

        # Animation
        mel.eval("FBXExportBakeComplexAnimation -v true")
        mel.eval("FBXExportBakeComplexStart -v "+str(cmds.playbackOptions(minTime=1, q=1)))
        mel.eval("FBXExportBakeComplexEnd -v "+str(cmds.playbackOptions(maxTime=1, q=1)))
        mel.eval("FBXExportReferencedAssetsContent -v true")
        mel.eval("FBXExportBakeComplexStep -v 1")
        mel.eval("FBXExportUseSceneName -v false")
        if self.euler_CHK.isChecked():
            mel.eval("FBXExportQuaternion -v euler")
        if self.quat_CHK.isChecked():
            mel.eval("FBXExportQuaternion -v quaternion")
        if self.resample_CHK.isChecked():
            mel.eval("FBXExportQuaternion -v resample")
        mel.eval("FBXExportShapes -v true")
        mel.eval("FBXExportSkins -v true")

        #up axis, the rather large elephant in the room
        idx = self.upAxisCMB.currentIndex()
        if idx != 0:
            if idx == 1:
                mel.eval("FBXExportUpAxis y")
            if idx == 2:
                mel.eval("FBXExportUpAxis z")


        #garbage we don't want
        # Constraints
        mel.eval("FBXExportConstraints -v false")
        # Cameras
        mel.eval("FBXExportCameras -v false")
        # Lights
        mel.eval("FBXExportLights -v false")
        # Embed Media
        mel.eval("FBXExportEmbeddedTextures -v false")
        # Connections
        mel.eval("FBXExportInputConnections -v false")
        # Axis Conversion

    def export(self, node, mesh=1, anim=1, path=None):

        toExport = []

        if not path:
            oldPath = path
            path = cmds.file(sceneName=1, q=1)
            cmds.warning('No valid path set for export [' + str(oldPath) + ']/nExporting to Maya file loc: ' + path)
            toExport.extend(cmds.listRelatives(cmds.listConnections(node + '.export_root'), type='joint',allDescendents=True,f=1))

        #kvassey -- adding support for baking to root in rig before export
        if self.bakeRoot_CHK.isChecked():
            print "Bake Root Checked"
            #copy root skeleton with input connections under new group
            cmds.select(cl=True)
            currRoot = cmds.listConnections(node + '.export_root')
            exportSkel = cmds.duplicate(currRoot, un=True, rc=False, po=False)
            print exportSkel
            tempGrp = cmds.group(exportSkel[0])
            #rename root joint
            dupRoot = cmds.rename(exportSkel[0], currRoot)
            #bake
            startTime = cmds.playbackOptions(min=True, q=True)
            endTime = cmds.playbackOptions(max=True, q=True)
            cmds.bakeResults(dupRoot, sm=True,  hi="below", s=True, sb=1, dic=True, t=(startTime, endTime))

            #move FBX export inside here, skip rest.
            #toExport.extend(cmds.listRelatives(cmds.listConnections(dupRoot), type='joint',allDescendents=True))
            #cmds.select(toExport)
            cmds.select(dupRoot, r=True, hi=True)
            toExport = cmds.ls(sl=True, type='joint')
            cmds.select(toExport, r=True)
            self.setExportFlags()
            # Export!
            print "FBXExport -f \""+ path +"\" -s"
            mel.eval("FBXExport -f \""+ path +"\" -s")

            cmds.delete(tempGrp)
            toExport = []


        #Assuming the meshes are skinned, maybe work with static later
        if not self.bakeRoot_CHK.isChecked():
            if not self.fbxPerMesh_CHK.isChecked():
                if mesh:
                    toExport.extend(cmds.listConnections(node + '.rendermesh'))
                if anim:
                    toExport.extend(cmds.listRelatives(cmds.listConnections(node + '.export_root'), type='joint', allDescendents=True, f=1))

                cmds.select(toExport)
                self.setExportFlags()
                # Export!
                print "FBXExport -f \""+ path +"\" -s"
                mel.eval("FBXExport -f \""+ path +"\" -s")

            else:
                for msh in cmds.listConnections(node + '.rendermesh'):
                    toExport.append(msh)
                    toExport.extend(cmds.listRelatives(cmds.listConnections(node + '.export_root'), type='joint',allDescendents=True, f=1))
                    cmds.select(toExport)
                    self.setExportFlags()
                    filePath = os.path.dirname(path)
                    new_fpath = filePath + '/' + msh + '.fbx'
                    # Export!
                    print "FBXExport -f \"" + new_fpath + "\" -s"
                    mel.eval("FBXExport -f \"" + new_fpath + "\" -s")

                    #clear sel and export list
                    cmds.select(d=1)
                    toExport = []


if __name__ == '__main__':
    show()