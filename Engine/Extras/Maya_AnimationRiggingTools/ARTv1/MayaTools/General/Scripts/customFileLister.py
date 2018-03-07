import maya.cmds as cmds
import os
from functools import partial
import cPickle

def UI():
    
    startDirectory = "D:/"
    
    #check to see if window exists
    if cmds.dockControl("fileListerDock", exists = True):
        cmds.deleteUI("fileListerDock")
        
    if cmds.window("customFileLister", exists = True):
        cmds.deleteUI("customFileLister")
        
    #create our window
    window = cmds.window("customFileLister", w = 300, h = 400, sizeable = False, mnb = False, mxb = False, title = "File Lister")
    
    #create the layout
    form = cmds.formLayout(w = 300, h = 400)
    
    #create the widgets
    addressBar = cmds.textField("addressBarTF", w = 250, text = startDirectory, parent = form)
    backButton = cmds.button(label = "<--", w = 30, h = 20, command = back, parent = form)
    fileFilters = cmds.optionMenu("fileFiltersOM", label = "", w = 80, parent = form, cc = partial(getContents, None))
    searchField = cmds.textField("searchTF", w = 90, text = "search..", cc = partial(getContents, None))
    addFavoriteButton = cmds.button(label = "Add Favortite", w = 90, c = addFavorite)
    favoriteList = cmds.scrollLayout("favoriteListSL", w = 90, h = 210, parent = form)
    scrollLayout = cmds.scrollLayout("contentSL", w = 200, h = 300, hst = 0, parent = form)
    
    #add menuItems to the optionMenu
    for item in ["All Files", "Maya Files", "Import Files", "Texture Files"]:
        cmds.menuItem(label = item, parent = fileFilters)
    
    #attach the UI elements to the layout
    cmds.formLayout(form, edit = True, af = [(addressBar,  "top", 10), (addressBar, "left", 40)] )
    cmds.formLayout(form, edit = True, af = [(backButton,  "top", 10), (backButton, "left", 10)] )
    cmds.formLayout(form, edit = True, af = [(scrollLayout,  "top", 40), (scrollLayout, "left", 10)] )
    cmds.formLayout(form, edit = True, af = [(fileFilters,  "top", 40)], ac = [fileFilters, "left", 5, scrollLayout] )
    cmds.formLayout(form, edit = True, af = [(searchField,  "top", 70)], ac = [searchField, "left", 5, scrollLayout] )
    cmds.formLayout(form, edit = True, af = [(addFavoriteButton,  "top", 100)], ac = [addFavoriteButton, "left", 5, scrollLayout] )
    cmds.formLayout(form, edit = True, af = [(favoriteList,  "top", 130)], ac = [favoriteList, "left", 5, scrollLayout] )


    #show the window
    cmds.dockControl("fileListerDock", area = "right", content = window, w = 310, aa = "left")
    
    getContents(startDirectory)
    loadFavorites()
    
    

    
def back(*args):
    currentPath = cmds.textField("addressBarTF", q = True, text = True)
    parentPath = currentPath.rpartition("/")[0].rpartition("/")[0] + "/"
    cmds.textField("addressBarTF", edit = True, text = parentPath + "/")
    

    getContents(parentPath)
    
    
def forward(item, *args):
    currentPath = cmds.textField("addressBarTF", q = True, text = True)
    forwardPath = currentPath +  item + "/"
    print currentPath
    print forwardPath
    
    if os.path.isdir(forwardPath):
        cmds.textField("addressBarTF", edit = True, text = forwardPath )
        
        getContents(forwardPath)
        
    else:
        allFiles = ["mb", "ma", "fbx", "obj", "bmp", "jpg", "tga", "TGA"]
        mayaFiles = ["mb", "ma"]
        importFiles = ["obj", "fbx"]
        textureFiles = ["bmp", "jpg", "tga", "TGA"]
        
        
        #Check for Maya Files
        fileExtension = item.rpartition(".")[2]
        if fileExtension in mayaFiles:
            result = cmds.confirmDialog(title = "File Operation", button = ["Open", "Import", "Reference", "Cancel"], cancelButton = "Cancel", dismissString = "Cancel")
            
            if result == "Open":
                cmds.file(currentPath + item, open = True, force = True)
            
            if result == "Import":
                cmds.file(currentPath + item, i = True, force = True)
                
            if result == "Reference":
                cmds.file(currentPath + item, r = True, loadReferenceDepth = "all", namespace = item.rpartition(".")[0])
                
                
        #Check for Import Files
        if fileExtension in importFiles:
            result = cmds.confirmDialog(title = "File Operation", button = ["Import", "Cancel"], cancelButton = "Cancel", dismissString = "Cancel")
                    
            if result == "Import":
                cmds.file(currentPath + item, i = True, force = True)
                
                
        #Check for Texture Files
        if fileExtension in textureFiles:
            result = cmds.confirmDialog(title = "File Operation", button = ["Assign To Selected", "Cancel"], cancelButton = "Cancel", dismissString = "Cancel")
                    
            if result == "Assign To Selected":
                
                selection = cmds.ls(sl = True)
                if len(selection) > 0:
                    
                    material = cmds.shadingNode("phong", asShader = True, name = item.rpartition(".")[0] + "_M")
                    placeNode = cmds.shadingNode("place2dTexture", asUtility = True)
                    textureFile = currentPath + item
                    texture = cmds.shadingNode("file", asTexture = True)
                    
                    cmds.connectAttr(placeNode + ".outUV", texture + ".uvCoord")
                    cmds.connectAttr(texture + ".outColor", material + ".color")
                    cmds.setAttr(texture + ".fileTextureName", textureFile, type = "string")
                    
                    
                    for each in selection:
                        cmds.select(each)
                        cmds.hyperShade(assign = material)
                        cmds.select(clear = True)
                        
                
                
        
        
            
        

    
def addFavorite(*args):
    
    currentPath = cmds.textField("addressBarTF", q = True, text = True)
    niceName = currentPath.rpartition("/")[0].rpartition("/")[2]
    createEntry(niceName, "menuIconFile.png", "favoriteListSL", currentPath, True)
    
    path = cmds.internalVar(upd = True) + "favorites.txt"
    f = open(path, 'w')
    
    children = cmds.scrollLayout("favoriteListSL", q = True, childArray = True)
    buttons = []
    for child in children:
        button = cmds.rowColumnLayout(child, q = True, childArray = True)[0]
        buttons.append(button)
    
    favorites = []
    for button in buttons:
        annotation = cmds.iconTextButton(button, q = True, ann = True)
        favorites.append(annotation)
        
    cPickle.dump(favorites, f)
    f.close()
    
def getContents(path, *args):
    
    if path == None:
        path = cmds.textField("addressBarTF", q = True, text = True)
        
    else:
        cmds.textField("addressBarTF", edit = True, text = path)

    
    #remove all contents from the list
    children = cmds.scrollLayout("contentSL", q = True, childArray = True)
    if children != None:
        for child in children:
            cmds.deleteUI(child)
            
    allFiles = ["mb", "ma", "fbx", "obj", "bmp", "jpg", "tga", "TGA"]
    mayaFiles = ["mb", "ma"]
    importFiles = ["obj", "fbx"]
    textureFiles = ["bmp", "jpg", "tga", "TGA"]
    fileFilters = []
    
    #look up current filter selection in the option Menu
    currentFilter = cmds.optionMenu("fileFiltersOM", q = True, v = True)
    if currentFilter == "All Files":
        fileFilters = allFiles
    if currentFilter == "Maya Files":
        fileFilters = mayaFiles
    if currentFilter == "Import Files":
        fileFilters = importFiles
    if currentFilter == "Texture Files":
        fileFilters = textureFiles
        
    
    contents = os.listdir(path)
    
    validItems = []
    directories = []
    
    for item in contents:
        extension = item.rpartition(".")[2]
        if extension in fileFilters:
            validItems.append(item)
            
        if os.path.isdir(os.path.join(path, item)):
            directories.append(item)
            
            
    for item in directories:
        createEntry(item, "menuIconFile.png", "contentSL", "")

    searchString = cmds.textField("searchTF", q = True, text = True)
    
    if searchString != "search.." or "":
        for item in validItems:
            if item.find(searchString) != -1:
                createEntry(item, None, "contentSL", "")
                
    else:
        for item in validItems:
            createEntry(item, None, "contentSL", "")
        
        
            
        

def loadFavorites():
    path = cmds.internalVar(upd = True) + "favorites.txt"
    
    if os.path.exists(path):
        f = open(path, 'r')
        
        favorites = cPickle.load(f)
        
        for favorite in favorites:
            niceName = favorite.rpartition("/")[0].rpartition("/")[2]
            createEntry(niceName, "menuIconFile.png", "favoriteListSL", favorite, True)
            
        f.close()
        
    

def createEntry(item, icon, scrollLayout, annotationString, favorite = False):
    
    #create a rowColumnLayout with 2 columns, create an image for the icon, create button with the label
    layout= cmds.rowColumnLayout(w = 190, nc = 2, parent = scrollLayout)
    
    if favorite == False:
        if icon != None:
            icon = cmds.iconTextButton(command = partial(forward, item), parent = layout, image = icon, w = 190, h = 20, style = "iconAndTextHorizontal", label = item, ann = annotationString)
        
        else:
            icon = cmds.iconTextButton(command = partial(forward, item), parent = layout, w = 190, h = 20, style = "iconAndTextHorizontal", label = item, ann = annotationString)
    else:
        icon = cmds.iconTextButton(command = partial(getContents, annotationString), parent = layout, w = 190, h = 20, style = "iconAndTextHorizontal", label = item, image = icon, ann = annotationString)
        
        
    
    