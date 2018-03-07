import maya.cmds as cmds
import maya.mel as mel

# Splits the selected meshes by their skinCLuster influences and creates a new heirarchy and constrains them to the skeleton.
# NOTE this script doesnt work well if the names of the meshes have ":" in them.
def splitMeshByInfluences(threshold, *args):
    import maya.mel as mel
    
    finalMeshList = []
    
    # find the meshes that the user has selected
    selected = cmds.ls(sl=True, type="transform")

    # Create an empty set.  We wil add to it later.
    cmds.select(cl=True)
    setName = cmds.sets(n="Rigid_Meshes")
    
    rigidMeshMasterGrp = cmds.group(em=True, name="Rigid_Mesh")

    # for each mesh:
    for mesh in selected:
        rigidMeshMeshGrp = cmds.group(em=True, name="Rigid_"+mesh, p=rigidMeshMasterGrp)
        #print "Mesh:"+mesh

        skinCluster = mel.eval('findRelatedSkinCluster %s' % mesh)
        influences = cmds.skinCluster(skinCluster, q=True, inf=True)
        #print influences

        for inf in influences:
            # Duplicate the mesh with input connections
            dupeMesh = cmds.duplicate(mesh, name=mesh+"__"+inf, rr=False)[0]
            #print "DupeMesh:       "+dupeMesh
            for attr in ["tx", "ty", "tz", "rx", "ry", "rz", "sx", "sy", "sz", "v"]:
                cmds.setAttr(dupeMesh+"."+attr, lock=False, k=True)
            cmds.makeIdentity(dupeMesh, t=1, r=1, s=1, apply=True)
            cmds.select(dupeMesh, r=True)
            mel.eval("CenterPivot")
            cmds.parent(dupeMesh, rigidMeshMeshGrp)
            
            finalMeshList.append(dupeMesh)
            cmds.sets(dupeMesh, add=setName)

            # Select the verts that are influenced by the inf joint.
            if len(influences) <= 1:
                const = cmds.parentConstraint(inf, rigidMeshMeshGrp, dupeMesh, mo=True)[0]
                cmds.setAttr(const+"."+rigidMeshMeshGrp+"W1", 0)
            else:
                verts = []
                for x in range(cmds.polyEvaluate(mesh, v=True)):
                    v = cmds.skinPercent(skinCluster, '%s.vtx[%d]' % (mesh, x), transform=inf, q=True)
                    if v > threshold:
                        verts.append('%s.vtx[%d]' % (dupeMesh, x))
                #print verts
                if verts:
                    cmds.select(verts)            
                    # Convert the selection to contained faces
                    if len(verts) != cmds.polyEvaluate(mesh, v=True):
                        mel.eval("InvertSelection;")
                        cmds.select(cmds.polyListComponentConversion(fv=True, tf=True, internal=True ))
                        cmds.delete()
                    cmds.select(dupeMesh, r=True)
                    mel.eval("CenterPivot")
                    const = cmds.parentConstraint(inf, rigidMeshMeshGrp, dupeMesh, mo=True)[0]
                    cmds.setAttr(const+"."+rigidMeshMeshGrp+"W1", 0)
                else:
                    print "No Valid Verts so we didnt use this influence."
                    cmds.delete(dupeMesh)

    cmds.select(rigidMeshMasterGrp, hi=True)
    cmds.createDisplayLayer(cmds.ls(sl=True), name="RigidMesh_L")
    
    return finalMeshList

#splitMeshByInfluences(0.5)

def findThreshold(*args):
    threshold = cmds.floatSliderGrp("skinWeightFloatSliderGrp", q=True, value=True)
    splitMeshByInfluences(threshold)


# Create a UI
def splitMeshByInfluencesUI(*args):
    if cmds.window("splitMeshByInfluencesUI", exists = True):
        cmds.deleteUI("splitMeshByInfluencesUI")

    window = cmds.window("splitMeshByInfluencesUI", title='Split Mesh by Influences', s=False)
    cmds.columnLayout()
    cmds.floatSliderGrp("skinWeightFloatSliderGrp", label='Skin Weight Threshold', field=True, minValue=0, maxValue=1, value=0.5, step=0.01, cw3=[110, 30, 150])
    cmds.button("Apply", label="Apply", w=290, c=findThreshold)

    cmds.showWindow(window)

#splitMeshByInfluencesUI()