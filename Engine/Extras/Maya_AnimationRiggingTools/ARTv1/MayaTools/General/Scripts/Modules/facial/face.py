'''
FACIAL MODULE

'''

import os, sys
import traceback
import json

from PySide import QtGui, QtCore
reload(QtGui)
reload(QtCore)

import maya.cmds as cmds

# internal libs
from Modules.facial import utils

##################
# BASE
##################


class BaseFace(object):
    '''
    This class is the base to be inherited and does not serialize to anything in Maya
    '''
    def __init__(self, version=None, create=True, uType='uMeta'):
        pass

    # FUNCTIONS
    ##############

    # PROPERTIES
    ###############

    #get and set controllers
    @property
    def controls(self):
        return cmds.listConnections(self.node + '.controls')
    @controls.setter
    def controls(self, controls):
        for c in controls:
            utils.msgConnect(self.node + '.controls', c + '.facialNode')

    #get and set version
    @property
    def version(self):
        return cmds.getAttr(self.node+'.version')
    @version.setter
    def version(self, ver):
        cmds.setAttr(self.node+'.version', ver, type='string')

    #get and set uType identifier
    @property
    def uType(self):
        return cmds.getAttr(self.node+'.uType')
    @uType.setter
    def uType(self, type):
        cmds.setAttr(self.node+'.uType', type, type='string')

    #Build attributes on the node if it's being built
    def buildBase(self):
        cmds.addAttr(self.node, longName='version', dataType='string')
        cmds.addAttr(self.node, longName='controls', attributeType='message')
        cmds.addAttr(self.node, longName='uType', dataType='string')



##################
## MASK
##################
class FaceMask(BaseFace):
    '''
    This class serializes to a network node
    '''
    def __init__(self, maskGeoNode=None, faceModuleNode=None, maskNode=None, **kwargs):
        super(FaceMask, self).__init__(**kwargs)
        self.maskGeoNode = maskGeoNode
        self.faceModuleNode = faceModuleNode

        if maskNode:
            self.node = maskNode
            self.initialize()

        elif 'create' in kwargs:
            self.buildNode()

    ## FUNCTIONS
    ##############

    def bakeMovers(self):
        pass

    def zeroMovers(self):
        for mover in self.faceModule.jointMovers:
            cmds.xform()

    def snapMovers(self, axis, mesh, debug=1):
        '''
        for this system, in the tool UI itself that snaps the movers, I always bidirectionally cast against Y
        '''
        try:
            cmds.undoInfo(openChunk=True)
            #iterate through the *active* facial joint movers and snap them to the surface
            if debug: print 'activeJointMovers:', self.faceModule.activeJointMovers
            for mover in self.faceModule.activeJointMovers:
                if debug: print 'snapping', mover
                #do not snap manually placed movers
                if not cmds.getAttr(mover + '.alignmentType'):

                    #set the axis vector
                    axisVector = None
                    if axis == 'x': axisVector = utils.getAxisWorldVectors(mover)[0]
                    elif axis == 'y': axisVector = utils.getAxisWorldVectors(mover)[1]
                    elif axis == 'z': axisVector = utils.getAxisWorldVectors(mover)[2]

                    pos = cmds.xform(mover, q=1, rp=1, ws=1)

                    #raycast in and out and find the closest hit of the facial surface
                    pos1 = utils.rayMeshIntersect(mesh, pos, axisVector)
                    pos2 = utils.rayMeshIntersect(mesh, pos, -axisVector)
                    dist1 = utils.square_distance(pos, pos1)
                    dist2 = utils.square_distance(pos, pos2)
                    newPos = None
                    if dist1 <= dist2:
                        newPos = pos1
                    else:
                        newPos = pos2

                    cmds.xform(mover, t=newPos, ws=1)
                    if debug: print mover, 'snapped.'
        except Exception as e:
            print e
        finally:
            cmds.undoInfo(closeChunk=True)

    def deleteSdkParentConstraints(self):
        try:
            cmds.undoInfo(openChunk=True)
            #update facial SDK locations to current joint mover placement
            print '>>>deleteSdkParentConstraints'
            for mover in self.faceModule.jointMovers:
                sdk = cmds.listConnections(mover + '.sdk')[0]
                sdkParent = cmds.listRelatives(sdk, parent = True)[0]
                utils.deleteConstraints(sdkParent)
        except Exception as e:
            print(traceback.format_exc())
        finally:
            cmds.undoInfo(closeChunk=True)

    def buildSdkParentConstraints(self):
        try:
            cmds.undoInfo(openChunk=True)
            for mover in self.faceModule.jointMovers:
                sdk = cmds.listConnections(mover + '.sdk')[0]
                lra = cmds.listConnections(mover + '.lra')[0]
                sdkParent = cmds.listRelatives(sdk, parent = True)[0]
                #TODO: Maybe don't maintain offset later (?)
                # there are some relationships that have offsets (eyes) that should be maintained
                cmds.parentConstraint(lra, sdkParent, mo=1)
        except Exception as e:
            print(traceback.format_exc())
        finally:
            cmds.undoInfo(closeChunk=True)

    def toggleMaskActive(self, mode):
        try:
            cmds.undoInfo(openChunk=True)
            if mode:
                cmds.showHidden(self.controls)
                cmds.showHidden(self.maskGeo)

                #hide all movers
                cmds.hide(self.faceModule.jointMovers)
                #TODO: Need to check if the constraints were deleted and re-add them
                '''for mover in self.faceModule.jointMovers:
                    sdk = cmds.listConnections(mover + '.sdk')[0]
                    sdkParent = cmds.listRelatives(sdk, parent = True)[0]
                    lra = cmds.listConnections(mover + '.lra')[0]
                    cmds.parentConstraint(lra, sdkParent)'''
                print 'MASK: ACTIVATED'

            else:
                #show joint movers
                cmds.showHidden(self.faceModule.jointMovers)

                #hide the mask controls and mesh
                cmds.hide(self.controls)
                cmds.hide(self.maskGeo)
                print 'MASK: DEACTIVATED'
        except Exception as e:
            print(traceback.format_exc())
        finally:
            cmds.undoInfo(closeChunk=True)



    ## BUILD
    def buildNode(self):
        self.node = cmds.createNode('network', name='maskNode')
        self.buildBase()

        #maskGeo attr and connection if it was passed in
        cmds.addAttr(self.node, longName='maskGeo', attributeType='message')
        if self.maskGeoNode:
            self.maskGeo = self.maskGeoNode

        #FaceModule attr and connection if it was passed in
        cmds.addAttr(self.node, longName='faceModule', attributeType='message')
        if self.faceModuleNode:
            self.faceModule = self.faceModuleNode

        #Set other metadata
        self.version = '0.00'
        self.uType = 'faceMask'

    ##SERIALIZATION
    def initialize(self):
        pass


    ## PROPERTIES
    ###############
    @property
    def maskGeo(self):
        return cmds.listConnections(self.node + '.maskGeo')[0]
    @maskGeo.setter
    def maskGeo(self, maskG):
        utils.msgConnect(self.node + '.maskGeo', maskG + '.maskNode')

    @property
    def faceModule(self):
        return FaceModule(faceNode=cmds.listConnections(self.node + '.faceModule')[0])
    @faceModule.setter
    def faceModule(self, faceM):
        utils.msgConnect(faceM + '.maskNode', self.node + '.faceModule')

    @property
    def attachLocations(self):
        return cmds.listConnections(self.node + '.attachLocations')
    @attachLocations.setter
    def attachLocations(self, locs):
        for loc in locs:
            utils.msgConnect(self.node + '.attachLocations', loc + '.maskNode')

    @property
    def active(self):
        return cmds.listConnections(self.node + '.active')
    @active.setter
    def active(self, mode):
        if not utils.attrExists(self.node + '.active'):
            cmds.addAttr(self.node, longName='active', at='bool')
        cmds.setAttr(self.node+'.active', mode)

        self.toggleMaskActive(mode)


####################
### FACE
####################
class FaceModule(BaseFace):
    '''
    This class serializes to a network node
    '''
    def __init__(self, renderMeshNodes=None, faceNode=None, **kwargs):
        super(FaceModule, self).__init__(**kwargs)

        if faceNode:
            self.node = faceNode
            self.initialize()

        elif 'create' in kwargs:
            self.buildNode()
        else:
            cmds.error('No facial node set through faceNode, and no create flag used!\nLOCALS: ' + str(locals()))


    ## FUNCTIONS
    ##############

    def initialize(self):
        self.mask = self.faceMask

    def readDrivenDict(self, node):
        if utils.attrExists(node + '.drivenDict'):
            dict = cmds.getAttr(node + '.drivenDict')
            if dict:
                return json.loads(dict)
        else:
            cmds.warning('No drivenDict found on object: ' + node)
            return {}

    def updateConstraints(self, node):
        '''
        This is basically made for the method 'snapEyeSdkPivots' below.
        It is a general function, except the part where is pops the sdkNode connection.
        '''
        parent = cmds.listRelatives(node, parent=1)[0]
        listConnectionsSucks = cmds.listConnections(parent, type='parentConstraint')
        if listConnectionsSucks:
            for const in set(listConnectionsSucks):
                conns = list(set(cmds.listConnections(const, type='transform')))
                conns.pop(conns.index(const))
                conns.pop(conns.index(parent))
                if utils.attrExists(const + '.sdkNode'):
                    conns.pop(conns.index(cmds.listConnections(const + '.sdkNode')[0]))
                if len(conns) == 1:
                    cmds.parentConstraint(conns[0], parent, e=1, mo=1)

    def snapEyeSdkPivots(self):
        L_eyeSDKs = [u'l_eye_lid_upper_mid_SDK', u'l_eye_lid_upper_inner_SDK', u'l_eye_lid_upper_outer_SDK', u'l_eye_lid_lower_outer_SDK', u'l_eye_lid_lower_mid_SDK', u'l_eye_lid_lower_inner_SDK']
        R_eyeSDKs = [u'r_eye_lid_upper_outer_SDK', u'r_eye_lid_upper_mid_SDK', u'r_eye_lid_upper_inner_SDK', u'r_eye_lid_lower_inner_SDK', u'r_eye_lid_lower_mid_SDK', u'r_eye_lid_lower_outer_SDK']
        for node in L_eyeSDKs:
            utils.snapPivotWithZeroParent(node, 'l_eye_SDK')
            self.updateConstraints(node)
        for node in R_eyeSDKs:
            utils.snapPivotWithZeroParent(node, 'r_eye_SDK')
            self.updateConstraints(node)

    ## BUILD
    def buildNode(self):
        #create and pass the 'node' to the base
        self.node = cmds.createNode('network', name='faceNode')
        #call the base build on that node
        self.buildBase()

        #Set other useful metadata
        self.version = '0.00'
        self.uType = 'faceModule'

        cmds.addAttr(self.node, longName='jointMovers', attributeType='message')
        cmds.addAttr(self.node, longName='sdks', attributeType='message')

    def stampDeltas(self):
        try:
            cmds.undoInfo(openChunk=True)
            bb = self.blendBoard
            if bb:
                #build attr
                if not utils.attrExists(bb + '.initialPoseDict'):
                    cmds.addAttr(bb, longName='initialPoseDict', dt='string')
                    cmds.setAttr(bb + '.initialPoseDict', '{}', type='string')

                initialPoseDict={}

                drivenDict = self.readDrivenDict(bb)
                for drivingAttr in drivenDict:
                    attrDict = {}
                    for driven in drivenDict[drivingAttr]:
                        cmds.setAttr(drivingAttr, 1.0)
                        attrDict[driven] = cmds.getAttr(driven)
                    initialPoseDict[drivingAttr] = attrDict
                    cmds.setAttr(drivingAttr, 0.0)

                cmds.setAttr(bb + '.initialPoseDict', json.dumps(initialPoseDict), type='string')

            else:
                cmds.warning('No blendBoard/poses found!')
        except Exception as e:
            print(traceback.format_exc())
            cmds.undoInfo(closeChunk=True)


    def buildBlendBoard(self, fpath):
        try:
            cmds.undoInfo(openChunk=True)

            #TODO: rebuild and keep all connections and attrs
            if self.blendBoard:
                cmds.delete(self.blendBoard)

            lines = []
            if os.path.exists(fpath):
                f = open(fpath, 'r')
                lines = f.readlines()
                f.close()
            self.blendBoard = cmds.spaceLocator(name='blendBoard')[0]
            cmds.addAttr(self.blendBoard, longName='uType', dataType='string')
            cmds.setAttr(self.blendBoard + '.uType', 'blendBoard', type='string')
            utils.msgConnect(self.node + '.blendBoard', self.blendBoard + '.faceModule')

            #store input file path
            cmds.addAttr(self.blendBoard, longName='inputFile', dataType='string')
            cmds.setAttr(self.blendBoard + '.inputFile', fpath, type='string')

            #make purdy
            cmds.setAttr("blendBoard.tx", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.ty", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.tz", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.rx", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.ry", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.rz", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.sx", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.sy", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.sz", l=1, k=0, channelBox=0)
            cmds.setAttr("blendBoard.v", l=1, k=0, channelBox=0)

            #make pose attrs
            for line in lines:
                line = line.strip()
                if line != '':
                    print line
                    if '##' in line:
                        cmds.addAttr(self.blendBoard,enumName='#####', longName=line.replace('##',''), at='enum', k=1)
                        cmds.setAttr(self.blendBoard + '.' + line.replace('##',''), lock=1)
                    else:
                        cmds.addAttr(self.blendBoard, longName=line, at='float', min=0, max=1, k=1)

            cmds.undoInfo(closeChunk=True)
        except Exception as e:
            exc_type, exc_value, exc_tb = sys.exc_info()
            for line in traceback.format_exception(exc_type, exc_value, exc_tb):
                print line
            cmds.undoInfo(closeChunk=True)

    def createJointMover(self, name=None, pos=(0,0,0), matrix=None, scale=1, toroidRatio=0.02, ss=0, facialNode=None):
        try:
            cmds.undoInfo(openChunk=True)
            if not name:
                name = 'test'
            radius = 1

            #create mask attachment node
            maskAttach = cmds.spaceLocator(name=name+'_mover_maskAttach')[0]

            #create jeremy-joint-mover-stack
            topNode = cmds.group(name=name+'_mover_grp', em=1)
            mover = utils.createCircleControl(name=name+'_mover', radius=2, type='toroid', heightRatio=toroidRatio)
            #offset = utils.createCircleControl(name=name+'_mover_offset', radius=1.5, color='xyz', type='toroid', heightRatio=toroidRatio)
            offset = cmds.group(name=name+'_mover_offset', em=1)
            lra_grp = cmds.group(name=name+'_lra_grp', em=1)
            lra = utils.createLRA(name=name + '_LRA', length=2, thickness=0.05, arrowRadiusMult=3)
            cmds.parent(mover, topNode)
            cmds.parent(offset, mover)
            cmds.parent(lra_grp, offset)
            cmds.parent(lra, lra_grp)
            cmds.displaySmoothness(mover, f=1)
            if scale is not 1:
                cmds.scale(scale, scale, scale, topNode)
                cmds.makeIdentity(topNode, apply=1, t=0, r=0, s=1, n=0, pn=1)
            if not ss:
                cmds.select(topNode)
            cmds.parentConstraint(maskAttach, topNode)

            #create SDK pose stack
            sdkOffset = cmds.group(name=name+'_SDK_grp', em=1)
            sdk = utils.createLRA(name=name + '_SDK', length=2, color=False, thickness=0.05, arrowRadiusMult=3)
            tweak = cmds.group(name=name+'_TWEAK', em=1)
            if scale is not 1:
                cmds.scale(scale, scale, scale, sdk)
                cmds.makeIdentity(sdk, apply=1, t=0, r=0, s=1, n=0, pn=1)
            cmds.parent(tweak, sdk)
            cmds.parent(sdk, sdkOffset)
            cmds.parentConstraint(lra, sdkOffset)
            utils.toggleShapeSelection(sdk, False)

            #cleanup outliner
            if cmds.objExists('maskAttachNodes'):
                cmds.parent(maskAttach, 'maskAttachNodes')
            if cmds.objExists('sdkNodes'):
                cmds.parent(sdkOffset, 'sdkNodes')
            if cmds.objExists('jointMovers'):
                cmds.parent(topNode, 'jointMovers')

            #make internal attrs/connections
            utils.msgConnect(mover + '.lra', lra + '.jointMover')
            utils.msgConnect(mover + '.attachNode', maskAttach + '.jointMover')
            utils.msgConnect(mover + '.sdk', sdk + '.jointMover')

            #make external attrs/connections
            if facialNode:
                locs = None

                #mark attachLocations
                try:
                    locs = self.mask.attachLocations
                    locs.extend(maskAttach)
                    self.mask.attachLocations = locs
                except Exception as e:
                    print(traceback.format_exc())

                #hang the jm off the facial node
                utils.msgConnect(self.node + '.jointMovers', mover + '.facialNode')

                #mark jointMovers
                jm = self.jointMovers
                jm.append(mover)
                self.jointMovers = jm

                #mark sdks
                s = self.sdks
                s.append(sdk)
                self.sdks = s

            #stamp with alignment type
            cmds.addAttr(sdk, ln='alignmentType', enumName='mask surface:manual',  k=1, at='enum')

            cmds.undoInfo(closeChunk=True)
            cmds.select(maskAttach)
            return [topNode, lra, offset, maskAttach]
        except Exception as e:
            cmds.undoInfo(closeChunk=True)
            import sys
            exc_type, exc_obj, exc_tb = sys.exc_info()
            fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]
            print(e, fname, exc_tb.tb_lineno)

    def findMirrorJointMoverByName(self, jm):
        mirrorNode = None
        if not jm.lower().startswith('c_'):
            if jm.lower().startswith('r_'):
                mirrorNode = 'l' + jm[1:]
            else:
                mirrorNode = 'r' + jm[1:]
            if mirrorNode:
                if cmds.objExists(mirrorNode):
                    return mirrorNode
            else:
                cmds.warning('Cannot find l_ or r_ prefix in: ' + jm)
                return False

    def mirrorFacialXform(self, jm, axis='x', debug=0):
        mirror = self.findMirrorJointMoverByName(jm)
        if debug:
            print mirror
        if mirror:
            pos = cmds.xform(jm, q=1, ws=1, rp=1)
            rot = cmds.xform(jm, q=1, ws=1, ro=1)
            #scale = cmds.xform(jm, q=1, r=1, s=1)

            cmds.select(mirror)
            if axis == 'x':
                cmds.move(-pos[0], pos[1], pos[2], mirror,  ws=1, rpr=1 )
                cmds.rotate(rot[0], -rot[1], -rot[2], mirror,  ws=1, a=1 )
            else:
                cmds.warning('Currently works for faces created looking down y axis.')

    def connectControlBoard(self, controlBoard):
        bb = self.blendBoard
        utils.msgConnect(self.node + '.controlBoard', controlBoard + '.faceModule')
        if bb:
            for attr in cmds.listAttr(bb, k=1):
                bbAttr = bb + '.' + attr
                if cmds.listConnections(bbAttr):
                    cbAttr = controlBoard + '.' + attr
                    if utils.attrExists(cbAttr):
                        cmds.connectAttr(cbAttr, bbAttr, f=1)

    def connectSdksToFinalSkeleton(self):
        jmLocked = cmds.lockNode('JointMover', lock=1, q=1)[0]
        if jmLocked:
            cmds.lockNode('JointMover', lock=0)
        for mover in self.activeJointMovers:
            #TODO: this won't work with multiple heads
            #hate to use name string here, but there's only one skeleton
            sdk = cmds.listConnections(mover + '.sdk')[0]
            joint = mover.replace('_mover','')
            tweak = cmds.listRelatives(sdk, children=1, typ='transform')[0]
            cmds.parentConstraint(tweak, joint, mo=1)
            cmds.scaleConstraint(tweak, joint)

            #to get away from sting names later
            #TODO: detect locks and only unlock/relock if the item was locked
            moverLocked = cmds.lockNode(mover, lock=1, q=1)[0]
            if moverLocked:
                cmds.lockNode(mover, lock=0)
            utils.msgConnect(mover + '.deformingJoint', joint + '.mover')
            if moverLocked:
                cmds.lockNode(mover, lock=1)
        if jmLocked:
            cmds.lockNode('JointMover', lock=1)


    ## PROPERTIES
    ###############
    @property
    def renderMeshes(self):
        return cmds.listConnections(self.node + '.renderMeshes')
    @renderMeshes.setter
    def renderMeshes(self, meshes):
        for mesh in meshes:
            utils.msgConnect(self.node + '.renderMeshes', mesh + '.faceNode')

    @property
    def faceMask(self):
        return FaceMask(maskNode=cmds.listConnections(self.node + '.maskNode')[0])
    @faceMask.setter
    def faceMask(self, maskNode):
        utils.msgConnect(self.node + '.faceMask', maskNode + '.faceNode')

    @property
    def blendBoard(self):
        bb = cmds.listConnections(self.node + '.blendBoard')
        if bb:
            return cmds.listConnections(self.node + '.blendBoard')[0]
        else:
            return []
    @blendBoard.setter
    def blendBoard(self, bBoard):
        utils.msgConnect(self.node + '.blendBoard', bBoard + '.faceNode')

    @property
    def jointMovers(self):
        conns = cmds.listConnections(self.node + '.jointMovers')
        if conns:
            return conns
        else: return []
    @jointMovers.setter
    def jointMovers(self, movers):
        for m in movers:
            utils.msgConnect(self.node + '.jointMovers', m + '.facialNode')

    @property
    def sdks(self):
        conns = cmds.listConnections(self.node + '.sdks')
        if conns:
            return conns
        else: return []
    @sdks.setter
    def sdks(self, sdks):
        for sdk in sdks:
            utils.msgConnect(self.node + '.sdks', sdk + '.facialNode')

    @property
    def blendBoardConnections(self):
        if self.blendBoard:
            poses = []
            bb = self.blendBoard
            for att in cmds.listAttr(bb, k=1):
                conns = cmds.listConnections(bb + '.' + att)
                if conns:
                    poses.append(att)
            return poses
        else:
            cmds.warning('No blendBoard connected!')
            return None
    @blendBoardConnections.setter
    def blendBoardConnections(self, *args):
        print 'Attribute cannot be set: connections are derived from the blendBoard'

    @property
    def poses(self):
        bb = self.blendBoard
        return self.readDrivenDict(bb)
    @poses.setter
    def poses(self, poses):
        cmds.warning('Attribute cannot be set: poses are derived from the blendBoard drivenDict attr')

    @property
    def activeJointMovers(self, debug=0):
        conns = cmds.listConnections(self.node + '.jointMovers')
        activeCons = []
        if conns:
            for con in conns:
                if utils.shapesHidden(con) == False:
                    if debug: print con, 'is not hidden and active'
                    activeCons.append(con)
                else:
                    if debug: print con, 'shapes hidden! not added.'
            return activeCons
        else:
            return []
    @activeJointMovers.setter
    def activeJointMovers(self, *args):
        print 'Attribute cannot be set: connections are derived from the facial joint mover visibility'
        return False


####################
### TESTING
####################
def autoTest(build=True):
    #build
    if build:
        face = FaceModule(create=1)
        mask = FaceMask(create=1, maskGeoNode='maskGeo', faceModuleNode=face.node)
        mask.controls = [u'jaw', u'lips_CTRL_MASK', u'R_nose', u'nose_CTRL_MASK', u'L_nose', u'l_lip_corner_CTRL_MASK', u'r_lip_corner_CTRL_MASK', u'R_mask_CTRL_MASK', u'brow_ridge_CTRL_MASK', u'R_eye_LRA', u'L_eye_LRA', u'L_mask_CTRL_MASK', u'top_mask_CTRL_MASK']
        face.renderMeshes = ['steele']
        '''
        print face.faceMask
        print face.version
        print mask.maskGeo
        print mask.faceModule
        '''
    else:
        face = FaceModule(faceNode='faceNode')
        mask = FaceMask(faceModuleNode='faceNode')


#autoTest()


if __name__ == '__main__':
    show()
