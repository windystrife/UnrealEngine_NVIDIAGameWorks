import maya.cmds as cmds

def suffixHeirarchyNames(*args):
    selection = cmds.ls(sl=True, type="transform")
    result = cmds.promptDialog(title="Suffix Seleceted", message="Enter Suffix:", button=["Ok", "Cancel"])
    if result == "Ok":
        suffix = cmds.promptDialog(q=True, text=True)
        for one in selection:
            cmds.rename(one, one+suffix)
#suffixHeirarchyNames()