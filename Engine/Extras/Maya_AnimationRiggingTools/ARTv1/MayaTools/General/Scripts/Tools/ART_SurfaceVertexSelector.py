import maya.cmds as cmds
import maya.OpenMaya as om
from functools import partial

# This script walks along the surface of a mesh from a single component and find the nearest N verts based on the number you pass into it (count).
# It then selects these verts and returns them as a list called "elements"
def surfaceVertSelector(startElement, *args):
    count = args[0]
    #print count
    vertCount = 0
    startDist = 0
    cmds.select(startElement)

    # Find all of the verts on the selected mesh and see if they are less than count.  If they are then set count to be all verts.
    allVerts = cmds.polyEvaluate(v=True)
    #print allVerts
    if count > allVerts:
        count = allVerts
    #print args[0]
        
    # Run through the mesh and using soft select to traverse out and find verts until we reach the count value.
    while vertCount < count:
        startDist = startDist+0.1
        #print "startDist: ", startDist
        cmds.softSelect(softSelectEnabled=True, ssf=1, ssd=startDist)
        selection = om.MSelectionList()
        surfaceVertSelector = om.MRichSelection()
        om.MGlobal.getRichSelection(surfaceVertSelector)
        surfaceVertSelector.getSelection(selection)

        dagPath = om.MDagPath()
        component = om.MObject()

        iter = om.MItSelectionList( selection,om.MFn.kMeshVertComponent )
        elements = []
        while not iter.isDone():
            iter.getDagPath( dagPath, component )
            dagPath.pop()
            node = dagPath.fullPathName()
            fnComp = om.MFnSingleIndexedComponent(component)

            for i in range(fnComp.elementCount()):
                elements.append('%s.vtx[%i]' % (node, fnComp.element(i)))
            iter.next()
        vertCount = len(elements)
        #print "vertCount: ", vertCount
    cmds.softSelect(softSelectEnabled=False)
    cmds.select(elements)

#startElement = cmds.ls(sl=True)
#surfaceVertSelector(startElement, 75)
    
# Create a UI that will drive the selection tool dynamically using dragCommand.
# The reset button will allow you to change the starting component(s) on your mesh.
def surfaceVertSelectorUI(*args):
    startElement = cmds.ls(sl=True, type="float3")
    if startElement:
        if cmds.window("surfaceVertSelector", exists = True):
            cmds.deleteUI("surfaceVertSelector")

        window = cmds.window("surfaceVertSelector", title='Surface Vert Selector', s=False)
        cmds.columnLayout()
        cmds.intSliderGrp("vertCountIntSliderGrp", label='Vert Count', field=True, minValue=1, maxValue=2000, value=1, step=1, cw3=[75, 50, 1000], dragCommand=partial(surfaceVertSelector, startElement))
        #surfaceVertSelector(startElement, 20)
        cmds.button("refreshButton", label="Reset", c=surfaceVertSelectorUI)

        cmds.showWindow(window)
    else:
        cmds.warning("You must select some components.")

#surfaceVertSelectorUI()