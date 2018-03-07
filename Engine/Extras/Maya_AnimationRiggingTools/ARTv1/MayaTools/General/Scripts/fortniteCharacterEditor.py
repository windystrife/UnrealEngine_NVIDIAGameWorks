import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os

class Fortnite_Character_Editor():

    def __init__(self):

        self.thumbList = [] #creating for icon storage and ease of clearing

        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()


        #check to see if window exists, if so, delete
        if cmds.window("fortniteCharacterEditor_UI", exists = True):
            cmds.deleteUI("fortniteCharacterEditor_UI")


        self.widgets = {}
        #build window
        self.widgets["window"] = cmds.window("fortniteCharacterEditor_UI", w = 400, h = 600, title = "Fortnite Character Editor", sizeable = False)

        #create the main layout
        self.widgets["topLevelLayout"] = cmds.columnLayout(w = 400)

        self.widgets["menuBarLayout"] = cmds.menuBarLayout(w = 200, parent = self.widgets["topLevelLayout"])
        menu = cmds.menu( label= "Help", parent = self.widgets["menuBarLayout"] )
        cmds.menuItem( label= "Weapon Skeleton Information", parent = menu, c = self.weaponInfo)



        #create the banner image area
        form = cmds.formLayout(w = 400, h = 60, parent = self.widgets["topLevelLayout"])
        self.widgets["bannerImage"] = cmds.image(image = self.mayaToolsDir + "/General/ART/Projects/Fortnite/banner.jpg", w = 400, h = 60, parent = form)
        self.widgets["characterList"] = cmds.optionMenu(w = 110, h = 54, label = "", parent = form, cc=self.changeChar)
        cmds.formLayout(form, edit = True, af = [(self.widgets["characterList"],'right', 8),(self.widgets["characterList"],'top', 4)])



        #create the 4 formLayouts (head, torso, legs, weapons)

        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        #HEAD TAB
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        self.widgets["headTab"] = cmds.formLayout(w = 400, parent = self.widgets["topLevelLayout"])
        backgroundImage = cmds.image(w = 380, h = 526, image = self.mayaToolsDir + "/General/Icons/Fortnite/headTab.jpg")

        #create the 3 buttons for the torso, pants, and weapons
        torsoButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/torsoTabButton.bmp", w = 61, h = 85, parent = self.widgets["headTab"], c = partial(self.switchTab, "torsoTab"))
        cmds.formLayout(self.widgets["headTab"], edit = True, af = [(torsoButton, 'left', 10),(torsoButton, 'top', 109)])

        weaponButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/weaponTabButton.bmp", w = 61, h = 85, parent = self.widgets["headTab"], c = partial(self.switchTab, "weaponTab"))
        cmds.formLayout(self.widgets["headTab"], edit = True, af = [(weaponButton, 'left', 10),(weaponButton, 'top', 211)])

        #add the scrollLayout for the head tab
        scrollLayout = cmds.scrollLayout(parent = self.widgets["headTab"], w = 320, h = 550, hst = 0)
        cmds.formLayout(self.widgets["headTab"], edit = True, af = [(scrollLayout, 'left', 80),(scrollLayout, 'top', 0)])
        self.widgets["headList"] = cmds.rowColumnLayout(nc = 3, cat = [(1, "both", 5), (2, "both", 5), (3, "both", 5)], rs = (1,5), parent = scrollLayout, w = 300)

        #populate heads based on current character's body type
        #add the None type part
        thumb = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/None.bmp"

        button = cmds.symbolButton(w = 90, h = 150, image = thumb, parent = self.widgets["headList"], c = self.clearHeadPart)



        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        #TORSO TAB
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        self.widgets["torsoTab"] = cmds.formLayout(w = 400, parent = self.widgets["topLevelLayout"], visible = False)
        backgroundImage = cmds.image(w = 380, h = 526, image = self.mayaToolsDir + "/General/Icons/Fortnite/torsoTab.jpg")

        #create the 3 buttons for the head, pants, and weapons
        headButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/headTabButton.bmp", w = 61, h = 85, parent = self.widgets["torsoTab"], c = partial(self.switchTab, "headTab"))
        cmds.formLayout(self.widgets["torsoTab"], edit = True, af = [(headButton, 'left', 10),(headButton, 'top', 6)])

        weaponButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/weaponTabButton.bmp", w = 61, h = 85, parent = self.widgets["torsoTab"], c = partial(self.switchTab, "weaponTab"))
        cmds.formLayout(self.widgets["torsoTab"], edit = True, af = [(weaponButton, 'left', 10),(weaponButton, 'top', 211)])


        #add the scrollLayout for the bodies tab
        scrollLayout = cmds.scrollLayout(parent = self.widgets["torsoTab"], w = 320, h = 550, hst = 0)
        cmds.formLayout(self.widgets["torsoTab"], edit = True, af = [(scrollLayout, 'left', 80),(scrollLayout, 'top', 0)])
        self.widgets["bodyList"] = cmds.rowColumnLayout(nc = 3, cat = [(1, "both", 5), (2, "both", 5), (3, "both", 5)], rs = (1,5), parent = scrollLayout, w = 300)

        #populate bodies based on current character's body type
        #add the None type part
        thumb = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/None.bmp"

        button = cmds.symbolButton(w = 90, h = 150, image = thumb, parent = self.widgets["bodyList"], c = self.clearBodyPart)







        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        #WEAPONS TAB
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        self.widgets["weaponTab"] = cmds.formLayout(w = 400, parent = self.widgets["topLevelLayout"], visible = False)
        backgroundImage = cmds.image(w = 380, h = 526, image = self.mayaToolsDir + "/General/Icons/Fortnite/weaponTab.jpg")

        #create the 3 buttons for the head, pants, and weapons
        headButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/headTabButton.bmp", w = 61, h = 85, parent = self.widgets["weaponTab"], c = partial(self.switchTab, "headTab"))
        cmds.formLayout(self.widgets["weaponTab"], edit = True, af = [(headButton, 'left', 10),(headButton, 'top', 6)])

        torsoButton = cmds.iconTextButton(image = self.mayaToolsDir + "/General/Icons/Fortnite/torsoTabButton.bmp", w = 61, h = 85, parent = self.widgets["weaponTab"], c = partial(self.switchTab, "torsoTab"))
        cmds.formLayout(self.widgets["weaponTab"], edit = True, af = [(torsoButton, 'left', 10),(torsoButton, 'top', 109)])

        #add the scrollLayout for the weapons tab
        scrollLayout = cmds.scrollLayout(parent = self.widgets["weaponTab"], w = 320, h = 550, hst = 0)
        cmds.formLayout(self.widgets["weaponTab"], edit = True, af = [(scrollLayout, 'left', 80),(scrollLayout, 'top', 0)])
        self.widgets["weaponsList"] = cmds.columnLayout(parent = scrollLayout, w = 300)


        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
        #PARTS TAB
        # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #






        #create the switch gender button
        #self.widgets["genderButton"] = cmds.button(w = 400, h = 50, label = "Switch Gender", parent = self.widgets["topLevelLayout"])


        #show window
        cmds.showWindow(self.widgets["window"])


        #get characters in scene
        characters = self.getCharacters()
        for character in characters:
            cmds.menuItem(label = character, parent = self.widgets["characterList"])


        #populate bodies
        self.populateBodies()

        #populate heads
        self.populateHeads()

        #populate layouts
        self.populateWeapons()




    def weaponInfo(self, *args):

        cmds.confirmDialog(title = "Weapon Skeleton Information", icon = "information", message = "Assault Rifle: Skeleton'/Game/Weapons/FORT_Rifles/Mesh/SK_MachineGun_Clip_Skeleton.SK_MachineGun_Clip_Skeleton'\n\nAuto Shotgun: Skeleton'/Game/Weapons/FORT_Shotguns/Mesh/shotgunAuto_Skeleton.shotgunAuto_Skeleton'\n\nPump Action Shotgun: Skeleton'/Game/Weapons/FORT_Shotguns/Mesh/pumpAction_Skeleton.pumpAction_Skeleton'\n\nRevolver: Skeleton'/Game/Weapons/FORT_Pistols/Mesh/SK_Revolver_Skeleton.SK_Revolver_Skeleton'\n\nSemi-Auto Rifle: Skeleton'/Game/Weapons/FORT_Pistols/Mesh/semiAuto_Skeleton.semiAuto_Skeleton'\n\nSniper Rifle: Skeleton'/Game/Weapons/FORT_Rifles/Mesh/boltAction_Skeleton.boltAction_Skeleton'\n\nTwo-Handed Melee: Skeleton'/Game/Weapons/FORT_Melee/Meshes/SK_Melee_Skeleton.SK_Melee_Skeleton'")



    def switchTab(self, layout, *args):

        #hide all formLayouts

        cmds.formLayout(self.widgets["headTab"] , edit = True, visible = False)
        cmds.formLayout(self.widgets["torsoTab"] , edit = True, visible = False)
        cmds.formLayout(self.widgets["weaponTab"] , edit = True, visible = False)

        #show the passed in layout
        cmds.formLayout(self.widgets[layout], edit = True, visible = True)




    def populateBodies(self, *args):

        #get current character
        character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)
        path = ""
        self.thumbList = []

        if character != None:
            #print(character)
            #find gender of character
            if character.find("Male")>-1:
                #find body type of character
                if character.find("Medium")>-1:
                    bodyType = "m_medium"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Male/Medium/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Male/Medium/"

                elif character.find("Small")>-1:
                    bodyType = "m_small"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Male/Small/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Male/Small/"

                elif character.find("Large")>-1:
                    bodyType = "m_large"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Male/Large/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Male/Large/"


            else:
                #print("Female Character")
                #female bodies
                if character.find("Medium")>-1:
                    bodyType = "f_medium"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Female/Medium/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Female/Medium/"

                elif character.find("Small")>-1:
                    bodyType = "f_small"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Female/Small/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Female/Small/"

                elif character.find("Large")>-1:
                    bodyType = "f_large"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Bodies/Female/Large/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Bodies/Female/Large/"



            #get body parts for the specified body type
            #print("Path is: "+path+"\n")
            if os.path.exists(path):
                files = os.listdir(path)
                parts = []

                #get files in the path
                for file in files:
                    if file.rpartition(".")[2] == "mb":
                        parts.append(file)


                for part in parts:
                    #print part
                    #create the button and add it to the list
                    thumb = part.partition(".mb")[0]
                    thumb = thumbPath + thumb + ".bmp"
                    if os.path.exists(thumb):
                        thumbButton = cmds.symbolButton(w = 90, h = 150, image = thumb, parent = self.widgets["bodyList"], c = partial(self.importBody, part, path, bodyType, character))
                        self.thumbList.append(thumbButton)


    def populateHeads(self, *args):
        print "Populate Heads"

        #get current character
        character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)
        path = ""
        self.thumbList = []

        if character != None:
            #print(character)
            #find gender of character
            if character.find("Male")>-1:
                #find body type of character
                if character.find("Medium")>-1:
                    bodyType = "m_medium"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Male/Medium/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Male/Medium/"

                elif character.find("Small")>-1:
                    bodyType = "m_small"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Male/Small/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Male/Small/"

                elif character.find("Large")>-1:
                    bodyType = "m_large"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Male/Large/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Male/Large/"


            else:
                #print("Female Character")
                #female Heads
                if character.find("Medium")>-1:
                    bodyType = "f_medium"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Female/Medium/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Female/Medium/"

                elif character.find("Small")>-1:
                    bodyType = "f_small"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Female/Small/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Female/Small/"

                elif character.find("Large")>-1:
                    bodyType = "f_large"
                    path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Heads/Female/Large/"
                    thumbPath = self.mayaToolsDir + "/General/Icons/Fortnite/Heads/Female/Large/"

            #get body parts for the specified body type
            #print("Path is: "+path+"\n")
            if os.path.exists(path):
                files = os.listdir(path)
                parts = []

                #get files in the path
                for file in files:
                    if file.rpartition(".")[2] == "mb":
                        parts.append(file)

                for part in parts:
                    #print part
                    #create the button and add it to the list
                    thumb = part.partition(".mb")[0]
                    thumb = thumbPath + thumb + ".bmp"
                    if os.path.exists(thumb):
                        thumbButton = cmds.symbolButton(w = 90, h = 150, image = thumb, parent = self.widgets["headList"], c = partial(self.importHead, part, path, bodyType, character))
                        self.thumbList.append(thumbButton)




    def populateWeapons(self, *args):

        #D:\Build\usr\jeremy_ernst\MayaTools\General\ART\Projects\Fortnite\Weapons
        path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Weapons/"

        items = os.listdir(path)

        weapons = []
        for item in items:
            if item.rpartition(".")[2] == "mb":
                weapons.append(item)


        for weapon in weapons:
            weapon = weapon.rpartition(".mb")[0]
            self.createEntry(weapon, "weaponsList")




    def createEntry(self, item, layout, *args):

        #find the thumbnbail for the entry
        thumb = self.mayaToolsDir + "/General/Icons/Fortnite/" + item + ".bmp"

        if os.path.exists(thumb):
            #add the thumbnail to the passed in layout
            if layout == "weaponsList":
                cmds.iconTextButton(image = thumb, w = 300, h = 100, parent = self.widgets[layout], c = partial(self.importWeapon, item))




    def importBody(self, part, path, bodyType, character, *args):

        #find the maya file path, if it exists, reference the file in:
        bodyFile = path + part


        if os.path.exists(bodyFile):

            #find references in scene:
            refs = cmds.ls(type = "reference")
            for ref in refs:
                namespace = cmds.referenceQuery(ref, namespace = True)

            if namespace == ":" + bodyType + "_bodyPart":
                fileName = cmds.referenceQuery(ref, filename = True)
                cmds.file(fileName, rr = True)


            #reference in the new body part
            cmds.file(bodyFile, r = True, type = "mayaBinary", loadReferenceDepth = "all", namespace = bodyType + "_bodyPart", options = "v=0")

            #constrain body part skeleton to main skeleton
            cmds.select(bodyType + "_bodyPart:root", hi = True)
            bodyPartSkel = cmds.ls(sl = True, type = "joint")

            for joint in bodyPartSkel:
                try:
                    rigJoint = joint.partition(":")[2]
                    rigJoint = character + ":" + rigJoint

                    cmds.parentConstraint(rigJoint, joint)

                    # commenting to make sure we get the new outfit controls - kvassey
                    #shapes = cmds.listRelatives(joint, s=True)
                    #if shapes == None:
                    #    print joint
                    #    cmds.setAttr(joint + ".v", 0)
                    #    cmds.setAttr(joint + ".v", lock = True)

                except:
                    pass

        #try to hide the original geometry
        if cmds.objExists(character + ":Geo_Layer"):
            cmds.setAttr(character + ":Geo_Layer.v", 0)


    def importHead(self, part, path, bodyType, character, *args):
        print "importing heads"

        #find the maya file path, if it exists, reference the file in:
        headFile = path + part

        if os.path.exists(headFile):
            #print headFile
            #find references in scene:
            refs = cmds.ls(type = "reference")
            for ref in refs:
                namespace = cmds.referenceQuery(ref, namespace = True)

            if namespace == ":" + bodyType + "_headPart":
                fileName = cmds.referenceQuery(ref, filename = True)
                cmds.file(fileName, rr = True)

            #reference in the new body part
            cmds.file(headFile, r = True, type = "mayaBinary", loadReferenceDepth = "all", namespace = bodyType + "_headPart", options = "v=0")

            #constrain body part skeleton to main skeleton
            cmds.select(bodyType + "_headPart:root", hi = True)
            headPartSkel = cmds.ls(sl = True, type = "joint")

            for joint in headPartSkel:
                try:
                    rigJoint = joint.partition(":")[2]
                    rigJoint = character + ":" + rigJoint

                    cmds.parentConstraint(rigJoint, joint)

                    # commenting to make sure we get the new outfit controls - kvassey
                    #shapes = cmds.listRelatives(joint, s=True)
                    #if shapes == None:
                    #    print joint
                    #    cmds.setAttr(joint + ".v", 0)
                    #    cmds.setAttr(joint + ".v", lock = True)

                except:
                    pass


    def importWeapon(self, weapon, *args):

        #find the maya file path, if it exists, reference file in.
        if weapon.find('emplate') > -1:#match against icon name in dir...it should match maya file name in templates dir
            path = "D:/this won't work because I can't know everyones perforce pathing"
        else:
            path = self.mayaToolsDir + "/General/ART/Projects/Fortnite/Weapons/" + weapon + ".mb"

        if os.path.exists(path):

            #find existing namespaces in scene
            namespaces = cmds.namespaceInfo(listOnlyNamespaces = True)

            #ask the user if they want the weapon attached to the weapon anim, or just simply imported
            result = cmds.confirmDialog(title = "Import Weapon", message = "Choose Import Method:", button = ["Import and Constrain", "Import and Place", "Just Import", "Character Part"], defaultButton = "Just Import")
            cmds.file(path, r = True, type = "mayaBinary", loadReferenceDepth = "all", namespace = weapon, options = "v=0")

            if result == "Import and Constrain":
                #find active character
                character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)

                rootCtrl = ""

                #find new namespaces in scene (this is here in case I need to do something later and I need the new name that was created)
                newWeaponName = weapon
                newNamespaces = cmds.namespaceInfo(listOnlyNamespaces = True)

                for name in newNamespaces:
                    if name not in namespaces:
                        newWeaponName = name

                if weapon == "Revolver_Pistol":
                    rootCtrl = newWeaponName + ":revolver_root_anim"
                    offsetZ = 0

                if weapon == "SemiAuto_Pistol":
                    rootCtrl = newWeaponName + ":semiAuto_root_anim"
                    offsetZ = 0

                if weapon == "Assault_Rifle":
                    rootCtrl = newWeaponName + ":assaultRifle_root_anim"
                    offsetZ = 0

                if weapon == "PumpAction_Shotgun":
                    rootCtrl = newWeaponName + ":pumpAction_root_anim"
                    offsetZ = 0

                if weapon == "Auto_Shotgun":
                    rootCtrl = newWeaponName + ":autoShotgun_root_anim"
                    offsetZ = 0

                if weapon == "Sniper_BoltAction":
                    rootCtrl = newWeaponName + ":boltAction_root_anim"
                    offsetZ = 0

                if weapon == "Twohanded_Sledgehammer":
                    rootCtrl = newWeaponName + ":twohanded_root_anim"
                    offsetZ = 0

                if weapon == "Twohanded_Axe":
                    rootCtrl = newWeaponName + ":twohanded_root_anim"
                    offsetZ = 0

                if weapon == "BreakAction_Shotgun":
                    rootCtrl = newWeaponName + ":breakAction_root_anim"
                    offsetZ = 0

                else:
                    rootCtrl = newWeaponName + ":*_root_anim"
                    offsetZ = 0


                #apply any needed offset to the constraints
                if cmds.objExists(character + ":" + "weapon_r_anim"):
                    cmds.pointConstraint(character + ":" + "weapon_r_anim", rootCtrl)
                    orientConstraint = cmds.orientConstraint(character + ":" + "weapon_r_anim", rootCtrl)[0]
                    cmds.setAttr(orientConstraint + ".offsetZ", offsetZ)


            if result == "Import and Place":
                #find active character
                character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)

                rootCtrl = ""

                #find new namespaces in scene (this is here in case I need to do something later and I need the new name that was created)
                newWeaponName = weapon
                newNamespaces = cmds.namespaceInfo(listOnlyNamespaces = True)

                for name in newNamespaces:
                    if name not in namespaces:
                        newWeaponName = name

                if weapon == "Revolver_Pistol":
                    rootCtrl = newWeaponName + ":revolver_root_anim"
                    offsetZ = 0

                if weapon == "SemiAuto_Pistol":
                    rootCtrl = newWeaponName + ":semiAuto_root_anim"
                    offsetZ = 0

                if weapon == "Assault_Rifle":
                    rootCtrl = newWeaponName + ":assaultRifle_root_anim"
                    offsetZ = 0

                if weapon == "PumpAction_Shotgun":
                    rootCtrl = newWeaponName + ":pumpAction_root_anim"
                    offsetZ = 0

                if weapon == "Auto_Shotgun":
                    rootCtrl = newWeaponName + ":autoShotgun_root_anim"
                    offsetZ = 0

                if weapon == "Sniper_BoltAction":
                    rootCtrl = newWeaponName + ":boltAction_root_anim"
                    offsetZ = 0

                if weapon == "Twohanded_Sledgehammer":
                    rootCtrl = newWeaponName + ":twohanded_root_anim"
                    offsetZ = 0

                if weapon == "Twohanded_Axe":
                    rootCtrl = newWeaponName + ":twohanded_root_anim"
                    offsetZ = 0

                if weapon == "BreakAction_Shotgun":
                    rootCtrl = newWeaponName + ":breakAction_root_anim"
                    offsetZ = 0

                else:
                    rootCtrl = newWeaponName + ":*_root_anim"
                    offsetZ = 0


                #apply any needed offset to the constraints
                if cmds.objExists(character + ":" + "weapon_r_anim"):
                    pc = cmds.pointConstraint(character + ":" + "weapon_r_anim", rootCtrl)
                    orientConstraint = cmds.orientConstraint(character + ":" + "weapon_r_anim", rootCtrl)[0]
                    cmds.setAttr(orientConstraint + ".offsetZ", offsetZ)
                    cmds.delete(orientConstraint)
                    cmds.delete(pc)

            if result == "Character Part":
                #Attach char parts differently
                character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)
                charSkeleton = cmds.listRelatives(character+":root", ad=True, c=True, typ="joint")
                #Gotta find the root joint of the referenced char part, which is not necessarily called root
                cmds.select(weapon+":*", r=True)
                partSkeleton = cmds.listRelatives(ad=True, c=True, typ="joint")# this requires all the rig nodes/groups be grouped.  Otherwise it will fail to parent.
                cmds.select(cl=True)
                for charJoint in charSkeleton:
                    for parts in partSkeleton:
                        part = parts.split(":")
                        #print "joint: "+charJoint
                        #print "part: "+part[1]
                        if charJoint.find(part[1])>-1 and charJoint.find("ik_")==-1:
                            #print "CONSTRAINING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                            cmds.parentConstraint(charJoint, parts, mo=False, n=parts+"_parentConst")




    def clearBodyPart(self, *args):

        #find the current character
        try:
            character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)

        except:
            cmds.warning("No valid character selected")

        #find any body part references
        refs = cmds.ls(type = "reference")
        bodyPartRefs = []
        for ref in refs:
            namespace = cmds.referenceQuery(ref, namespace = True)

            if namespace.find("_bodyPart") != -1:
                bodyPartRefs.append([ref, namespace])


        #find out which, if any, are tied to this character and remove
        for ref in bodyPartRefs:
            joint = ref[1] + ":root"
            constraint = cmds.listConnections(joint, type = "parentConstraint")[0]
            target = cmds.listConnections(constraint + ".target")[0].partition(":")[0]

            if target == character:
                filename = cmds.referenceQuery(ref[0], filename = True)
                cmds.file(filename, rr = True)

            #unhide the original geometry
            if cmds.objExists(character + ":Geo_Layer"):
                cmds.setAttr(character + ":Geo_Layer.v", 1)



    def clearHeadPart(self, *args):

        #find the current character
        try:
            character = cmds.optionMenu(self.widgets["characterList"], q = True, v = True)

        except:
            cmds.warning("No valid character selected")

        #find any body part references
        refs = cmds.ls(type = "reference")
        headPartRefs = []
        for ref in refs:
            namespace = cmds.referenceQuery(ref, namespace = True)

            if namespace.find("_headPart") != -1:
                headPartRefs.append([ref, namespace])


        #find out which, if any, are tied to this character and remove
        for ref in headPartRefs:
            joint = ref[1] + ":root"
            constraint = cmds.listConnections(joint, type = "parentConstraint")[0]
            target = cmds.listConnections(constraint + ".target")[0].partition(":")[0]

            if target == character:
                filename = cmds.referenceQuery(ref[0], filename = True)
                cmds.file(filename, rr = True)

            #unhide the original geometry
            if cmds.objExists(character + ":Geo_Layer"):
                cmds.setAttr(character + ":Geo_Layer.v", 1)


    def getCharacters(self):

        referenceNodes = []
        references = cmds.ls(type = "reference")

        for reference in references:
            niceName = reference.rpartition("RN")[0]
            suffix = reference.rpartition("RN")[2]
            if suffix != "":
                if cmds.objExists(niceName + suffix + ":" + "Skeleton_Settings"):
                    referenceNodes.append(niceName + suffix)

            else:
                if cmds.objExists(niceName + ":" + "Skeleton_Settings"):
                    referenceNodes.append(niceName)

        return referenceNodes



    def changeChar(self, newChar, *args):
        #print("Changed Char: "+newChar)
        #empty dictionary thumDict by deleting icons
        for thumbs in self.thumbList:
            cmds.deleteUI(thumbs)
        #populate bodies
        self.populateBodies()