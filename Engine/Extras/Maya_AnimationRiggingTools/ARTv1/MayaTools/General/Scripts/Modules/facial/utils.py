import maya.api.OpenMaya as om2
import maya.cmds as cmds

import os
import traceback

## GENERAL
#######################################################################

def dagPathFromName(obj):
    '''
    Given a maya object name, returns an openMaya2 dagPath object, dagPath, and a full dagPath in a string
    '''
    sel = om2.MSelectionList()
    sel.add(obj)
    dag = sel.getDagPath(0)
    dagStr = sel.getDagPath(0).fullPathName()
    mobj = sel.getDependNode(0)
    return mobj, dag, dagStr



### ATTRIBUTE MANAGEMENT
#######################################################################
def attrExists(attr):
    if '.' in attr:
        node, att = attr.split('.')
        return cmds.attributeQuery(att, node=node, ex=1)
    else:
        cmds.warning('attrExists: No attr passed in: ' + attr)
        return False

def msgConnect(attribFrom, attribTo, debug=0):
    #TODO needs a mode to dump all current connections (overwrite/force)
    objFrom, attFrom = attribFrom.split('.')
    objTo, attTo = attribTo.split('.')
    if debug: print 'msgConnect>>> Locals:', locals()
    if not attrExists(attribFrom):
        cmds.addAttr(objFrom, longName=attFrom, attributeType='message')
    if not attrExists(attribTo):
        cmds.addAttr(objTo, longName=attTo, attributeType='message')
    
    #check that both atts, if existing are msg atts
    for a in (attribTo, attribFrom):
        if cmds.getAttr(a, type=1) != 'message':
            cmds.warning('msgConnect: Attr, ' + a + ' is not a message attribute. CONNECTION ABORTED.')
            return False
    
    try:
        return cmds.connectAttr(attribFrom, attribTo)
    except Exception as e:
        print e
        return False

def snapMeshPivot(mesh1, mesh2):
	'''
	move mesh2 pivot to mesh1 pivot
	@author=chrise
	'''

	dupe = cmds.duplicate(mesh2)[0]
	mesh1Xform = cmds.getAttr(mesh1+'.worldMatrix')
	cmds.xform(mesh2, m=mesh1Xform)

	cmds.undoInfo(openChunk=True)
	try:
		for v in range(0, cmds.polyEvaluate(mesh2, v=1)):
			m2Vtx = mesh2+'.vtx['+str(v)+']'
			dupeVtx = dupe+'.vtx['+str(v)+']'
			cmds.xform(m2Vtx, m=cmds.xform(dupeVtx, m=1, q=1, ws=1), ws=1)
		cmds.delete(dupe)
	finally:
		cmds.undoInfo(closeChunk=True)
	'''
	#TODO: here's some sample code for working with one nurbs shape
	#I stopped because it didn't work with multiple shape nodes
	surf = cmds.ls(sl=1)[0]
    if cmds.nodeType(surf) == 'transform':
        #get all cvs
        cmds.select(surf + '.cv[*]')
        cvs = cmds.ls(sl=1, flatten=1)
        cmds.select(cl=1)
        for cv in cvs:
            print cv
            print cmds.pointPosition(cv)
    '''

def snapRotatePivotPosition(nodeFrom, nodeTo):
    '''
    sel = cmds.ls(sl=1)
    snapRotatePivotPosition(sel[0], sel[1])
    '''
    p = cmds.xform(nodeTo, q=1, ws=1, rp=1)
    cmds.xform(nodeFrom, ws=1, piv=p)

def snapPivotWithZeroParent(fromNode, toNode):
    '''
    Mainly used for snapping the pivot of a controller that is in a topnode GRP's space
    '''
    #TODO: This won't work if there is more than one child
    try:
        parent = cmds.listRelatives(fromNode, parent=1)

        cmds.parent(fromNode, w=1)
        toNodeMx = cmds.xform(toNode, q=1, ws=1, m=1)
        cmds.xform(parent, m=toNodeMx, ws=1)

        toNodePivot = cmds.xform(toNode, ws=1, q=1, t=1)
        cmds.xform(fromNode, ws=1, a=1, piv=toNodePivot)
        cmds.parent(fromNode, parent)
        cmds.makeIdentity(fromNode, t=1, r=1, a=1)
    except Exception as e:
        print(traceback.format_exc())

def getUType(type):
    '''
    Returns nodes of the requested uType
    example: getUType('faceModule')
    '''
    #TODO: Check with namespaces
    return [node.split('.')[0] for node in cmds.ls('*.uType', r=1) if cmds.getAttr(node) == type]

def hideShapes(node):
    for shape in cmds.listRelatives(node, shapes=1, f=1):
        cmds.hide(shape)

def unhideShapes(node):
    for shape in cmds.listRelatives(node, shapes=1, f=1):
        cmds.showHidden(shape)

def shapesHidden(node):
    for shape in cmds.listRelatives(node, shapes=1, f=1):
        if cmds.getAttr(shape + '.visibility') == 1:
            return False
    return True

### CONTROLLER MANAGEMENT
#######################################################################
def toggleShapeSelection(node, selectable):
    '''
    example: makeShapesUnselectable(cmds.ls(sl=1)[0], False)
    '''
    for shape in cmds.listRelatives(node, fullPath=1, shapes=1):
        if selectable:
            cmds.setAttr(shape + '.overrideEnabled', 0)
            cmds.setAttr(shape + '.overrideDisplayType', 0)
        else:
            cmds.setAttr(shape + '.overrideEnabled', 1)    
            cmds.setAttr(shape + '.overrideDisplayType', 2)

def getAxisWorldVectors(node):
    matrix = om2.MGlobal.getSelectionListByName(node).getDagPath(0).inclusiveMatrix()
    x = ((om2.MVector.kXaxisVector) * matrix).normal()
    y = ((om2.MVector.kYaxisVector) * matrix).normal()
    z = ((om2.MVector.kZaxisVector) * matrix).normal()
    return [x,y,z]

def createArrow(name=None, thickness=0.1, length=2, vector=[1,0,0], point=[0,0,0]):
    '''
    Creates an arrow in the direction of the vector
    Example:
    arrow = createArrow(vector=[0,1,0], length=4)
    '''
    if not name:
        name = 'arrow_CTRL'
    #calc length for thickness
    ratio = length/thickness
    cyl = cmds.cylinder(radius=thickness, sections=4, heightRatio=ratio, pivot=[length/2, 0, 0], ch=0)[0]
    cone = cmds.cone(radius=thickness*2, sections=4, ch=0, pivot=(length*1.0005,0,0))[0]
    xform = cmds.createNode('transform', ss=1, name=name)
    
    shapes = []
    transforms = []
    for node in [cone,cyl]:
        shapes.append(cmds.listRelatives(node, fullPath=1, shapes=1)[0])
        transforms.append(node)
    cmds.parent(shapes, xform, r=1, s=1)
    rotateBy = cmds.angleBetween(euler=True, v1=(1,0,0), v2=vector)
    cmds.rotate(rotateBy[0], rotateBy[1], rotateBy[2], xform)
    cmds.xform(xform, t=point)
    cmds.delete(transforms)
    return xform


def colorControl(node, name=None, color=None, useExisting=1, shapes=0):
    '''
    Colors an object by creating and assigning a shader with the color you input
    Example:
    colorControl('arrow', color=(1,0,0))
    '''
    if not name:
        name = 'colorControl_%s_%s_%s' % (color[0], color[1], color[2])
    
    shader = None
    shading_group = None
    
    if cmds.objExists(name):
        if 'shadingDependNode' in cmds.nodeType(name, i=1):
            if useExisting:
                shader = name
                shading_group = cmds.listConnections(name + '.outColor')[0]
    else:
        if len(color) == 3:
            shader=cmds.shadingNode('blinn',asShader=True, name = name)
            cmds.setAttr(shader + '.color', color[0], color[1], color[2], type='double3')
            shading_group = cmds.sets(renderable=True,noSurfaceShader=True,empty=True, name=name + '_colorControlSG')
            cmds.connectAttr('%s.outColor' %shader ,'%s.surfaceShader' %shading_group)
        else:
            cmds.warning('Color input is not a 3 element list or tuple. COLOR: ' + str(color))
            return False
    if shapes:
        for shape in cmds.listRelatives(node, fullPath=1, shapes=1):
            cmds.sets(shape, e=1, forceElement=shading_group)
    else:
        cmds.sets(node, e=1, forceElement=shading_group)
    cmds.setAttr(shader + '.diffuse', 1)
    cmds.setAttr(shader + '.ambientColor', 1,1,1, type='double3')

    #Need to check if it's a blinn before setting these
    #cmds.setAttr(shader + '.eccentricity', 0)
    #cmds.setAttr(shader + '.specularColor', 0,0,0, type='double3')
    return shader, shading_group
    
def createLRA(node=None, matrix=None, name=None, color=True, thickness=0.1, length=2, vector=[1,0,0], point=[0,0,0], arrowRadiusMult=2):
    '''
    Creates an LRA at the origin, or at a node, or a mmx
    Example:
    createLRA(length=2, thickness=0.05, arrowRadiusMult=3)
    '''
    if not name:
        name = 'arrow_CTRL'
    nodes = []
    x,y,z = [],[],[]
    #calc length for thickness
    ratio = length/thickness
    xform = cmds.createNode('transform', ss=1, name=name)

    x.append(cmds.cylinder(radius=thickness, sections=4, heightRatio=ratio, pivot=[length/2, 0, 0], ch=0, axis=[1,0,0])[0])
    x.append(cmds.cone(radius=thickness*arrowRadiusMult, sections=4, ch=0, pivot=(length*1.0005,0,0))[0])
    
    y.append(cmds.cylinder(radius=thickness, sections=4, heightRatio=ratio, pivot=[0, length/2, 0], ch=0, axis=[0,1,0])[0])
    y.append(cmds.cone(radius=thickness*arrowRadiusMult, sections=4, ch=0, pivot=(0,length*1.0005,0), axis=[0,1,0])[0])
    
    z.append(cmds.cylinder(radius=thickness, sections=4, heightRatio=ratio, pivot=[0, 0, length/2], ch=0, axis=[0,0,1])[0])
    z.append(cmds.cone(radius=thickness*arrowRadiusMult, sections=4, ch=0, pivot=(0,0,length*1.0005), axis=[0,0,1])[0])
    
    nodes.extend(x)
    nodes.extend(y)
    nodes.extend(z)
    
    if color:
        for node in x: colorControl(node, name='red_m', color=(1,0,0))
        for node in y: colorControl(node, name='green_m', color=(0,1,0))
        for node in z: colorControl(node, name='blue_m', color=(0,0,1))
    
    shapes = []
    transforms = []
    for node in nodes:
        shapes.append(cmds.listRelatives(node, fullPath=1, shapes=1)[0])
        transforms.append(node)
    cmds.parent(shapes, xform, r=1, s=1)
    rotateBy = cmds.angleBetween(euler=True, v1=(1,0,0), v2=vector)
    cmds.rotate(rotateBy[0], rotateBy[1], rotateBy[2], xform)
    cmds.xform(xform, t=point)
    cmds.delete(transforms)
    return xform
    

def createCircleControl(name=None, x=1, y=1, z=1, radius=1, color=None, type='strip', heightRatio=0.007):
    if not name:
        name = 'circle_CTRL'
    xform = cmds.group(name=name, em=1)
    circleShapes = []
    circleTransforms = []
    vectors = getAxisWorldVectors(xform)
    
    xyz = [x,y,z]
    for i in range(0, len(xyz)):
        if xyz[i]:
            circ = None
            if type == 'curve':
                circ = cmds.circle(radius=radius, name='jointMoverAxisCtrl', normal=vectors[i], sections=1, ch=0)
                if color:
                    cmds.color(circ, ud=color)
            if type == 'strip' or type == 'toroid':
                circ = None
                if type == 'strip':
                    circ = cmds.cylinder(radius=radius, name='jointMoverAxisCtrl', axis=vectors[i], sections=7, ch=0, heightRatio=heightRatio)
                elif type == 'toroid':
                    circ = cmds.torus(radius=radius, name='jointMoverAxisCtrl', axis=vectors[i], sections=7, ch=0, heightRatio=heightRatio)
                if color:
                    if color == 'xyz':
                        if i == 0: colorControl(circ, name='red_m', color=(1,0,0))
                        if i == 1: colorControl(circ, name='green_m', color=(0,1,0))
                        if i == 2: colorControl(circ, name='blue_m', color=(0,0,1))
                    else:
                        if len(color) == 3: colorControl(circ, color=color)
            circleShapes.append(cmds.listRelatives(circ[0], fullPath=1, shapes=1)[0])
            circleTransforms.append(circ[0])
    cmds.parent(circleShapes, xform, r=1, s=1)
    cmds.delete(circleTransforms)
    return xform
    
### ART V2 WRANGLING
#######################################################################

def setArtV2StyleSheet(widget, sheetTemplate=None, imageDirectory=None):
    
    css, lines = None, None
    if not sheetTemplate:
        sheetTemplate = (os.path.dirname(__file__) + '\\artV2_css_template.txt').replace('\\','/')
        imageDirectory = (os.path.dirname(__file__) + imageDirectory).replace('\\','/')
    if os.path.isfile(sheetTemplate):
        f = open(sheetTemplate, 'r')
        cssTemplate = f.read()
        f.close()
        css = cssTemplate.replace('___PATH___', imageDirectory)
    else:
        print('Cannot find CSS template: ' + sheetTemplate)
        return False
    
    #check to see if all images found
    for line in css.split('\n'):
        if '.png' in line.lower():
            image = line.split('(')[1].split(')')[0]
            if not os.path.isfile(image):
                print 'ERROR:setArtV2StyleSheet: Image not found [' + image + ']'
    
    widget.setStyleSheet(css)

## SURFACE CONSTRAINING
#######################################################################

def square_distance(pointA, pointB):
    '''
    Both points must have the same dimensions
    '''
    if len(pointA) != len(pointB):
        print 'SQUARED_DISTANCE: incoming points do not have the same dimensions:', pointA, pointB
        return False
    distance = 0
    dimensions = len(pointA)
    for dimension in range(dimensions):
        distance += (pointA[dimension] - pointB[dimension])**2
    return distance

def closestVertOnMesh(mesh, node):
    '''
    Returns closest vertex component on mesh to transform/pivot of node
    '''
    closestVert = None
    minLength = None
    verts = cmds.getAttr(mesh+".vrts", multiIndices=True)
    nodePos = cmds.xform(node, q=1, translation=1, ws=1)
    for v in verts:
        vPos = cmds.xform(mesh + '.vtx['+str(v)+']', q=1, translation=1, ws=1)
        length = om2.MVector(om2.MVector(nodePos) - om2.MVector(vPos)).length()
        if minLength is None or length < minLength:
            minLength = length
            closestVert = v
    return (mesh + '.vtx[' + str(closestVert) + ']')

def closestPointonMesh(mesh, node, createLoc=False):
    pos = cmds.xform(node, q=1, rp=1, ws=1)
    nodePos = om2.MPoint(pos[0], pos[1], pos[2],1.0)
    print 'point', pos[0], pos[1], pos[2]

    obj,dag,stringPath = dagPathFromName(mesh)
    meshFn = om2.MFnMesh(dag)

    resultPoint = meshFn.getClosestPoint(nodePos,om2.MSpace.kWorld)[0]
    
    if createLoc:
        loc = cmds.spaceLocator()[0]
        cmds.setAttr(loc+'.t', resultPoint[0], resultPoint[1], resultPoint[2])
        return loc
    else:
        return (resultPoint[0], resultPoint[1], resultPoint[2])

def constrainToMesh(mesh, node, mo=True, type='follicle'):
    '''
    Attaches node to closest vertex of mesh
    '''
    
    #get closest vertex
    vtx = closestVertOnMesh(mesh, node)
    
    surfaceXform = None
    #build/place follicle
    if type == 'follicle':
        surfaceXform = cmds.createNode("transform", n=(mesh+"_follicle"))
        follicle = cmds.createNode("follicle", n=(mesh+"_follicleShape"), p=surfaceXform)
        cmds.connectAttr((follicle+".outTranslate"), (surfaceXform+".translate"), f=1)
        cmds.connectAttr((follicle+".outRotate"), (surfaceXform+".rotate"), f=1)
        cmds.connectAttr((mesh+".outMesh"), (follicle+".inm"), f=1)
        cmds.connectAttr((mesh+".worldMatrix[0]"), (follicle+".inputWorldMatrix"), f=1)
        uv = cmds.polyListComponentConversion(vtx, fv=True, tuv=True)
        uvs = cmds.polyEditUV(uv ,q=1, u=1, v=1)
        u = uvs[0]
        v = uvs[1]
        cmds.setAttr((follicle+".parameterU"), u)
        cmds.setAttr((follicle+".parameterV"), v)
    cmds.parentConstraint(surfaceXform, node, mo=mo)

def rayMeshIntersect(mesh, pos, vector):
    '''
    Example:
    mover = cmds.ls(sl=1)[0]
    mesh = 'steel'
    pos = cmds.xform(mover, q=1, rp=1, ws=1)
    newPos = rayMeshIntersect(mesh, pos, -utils.getAxisWorldVectors(mover)[1])
    cmds.xform(mover, t=newPos, ws=1)
    '''
    nodePos = om2.MPoint(pos[0], pos[1], pos[2],1.0)

    vector = om2.MVector(vector)
    obj,dag,stringPath = dagPathFromName(mesh)
    meshFn = om2.MFnMesh(dag)

    selectionList = om2.MSelectionList()
    selectionList.add(mesh)
    dagPath = selectionList.getDagPath(0)
    fnMesh = om2.MFnMesh(dagPath)
    intersection = fnMesh.closestIntersection(
    om2.MFloatPoint(pos),
    om2.MFloatVector(vector),
    om2.MSpace.kWorld, 99999, False)
    if intersection:
        hitPoint, hitRayParam, hitFace, hitTriangle, hitBary1, hitBary2 = intersection
        x,y,z,_ = hitPoint
        return (x,y,z)

def deleteConstraints(node):
    ch = cmds.listRelatives(node, children=1)
    if ch:
        for item in ch:
            if 'Constraint' in cmds.nodeType(item):
                cmds.delete(item)

def findNamingConflicts(ignoreShapes=1):
    #TODO: listRelatives to find parent, check relationship to make sure theyre shapes instead of namechecking
    nameDict = {}
    for node in cmds.ls('*', l=1):
        if ignoreShapes:
            if 'shape' in node.lower():
                continue
        shortName = node.split('|')[-1]
        if shortName not in nameDict.keys():
            nameDict[shortName] = [node]
        else:
            cmds.warning('Found duplicate node names: \n' + node + '\n' + str(nameDict[shortName]))
            nameDict[shortName].append(node)
    returnDict = {}
    for name in nameDict:
        if len(nameDict[name]) > 1:
            returnDict[name] = nameDict[name]
    return returnDict