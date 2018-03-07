import maya.cmds as cmds
import maya.mel as mel
from functools import partial
import os, cPickle

class ART_Settings():

    def __init__(self):

        #get access to our maya tools
        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()


        #check to see if window exists, if so, delete
        if cmds.window("AnimationRiggingTool_SettingsUI", exists = True):
            cmds.deleteUI("AnimationRiggingTool_SettingsUI")

        self.path = self.mayaToolsDir
        self.widgets = {}

        #create the window
        self.widgets["window"] = cmds.window("AnimationRiggingTool_SettingsUI", w = 400, h = 300, titleBarMenu = False, title = "Settings", sizeable = False)


        #main layout
        self.widgets["main"] = cmds.formLayout(w = 400, h = 300)

        #background image
        image = self.mayaToolsDir + "/General/Icons/ART/settings.jpg"
        self.widgets["background"] = cmds.image(w = 400, h = 300, parent = self.widgets["main"], image = image)


        #close button
        self.widgets["closeButton"] = cmds.symbolButton(w = 26, h = 20, parent = self.widgets["main"], image = self.mayaToolsDir + "/General/Icons/ART/xbutton.bmp", c = self.close)
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["closeButton"], "top", 2), (self.widgets["closeButton"], "right", 2)])

        #tools path
        self.widgets["toolsPathLabel"] = cmds.text(h = 40, w = 100, label = "Tools Path: ", font = "boldLabelFont", parent = self.widgets["main"])
        self.widgets["toolsPathField"] = cmds.textField(h = 40, w = 225, text = "", parent = self.widgets["main"], editable = False)
        self.widgets["toolsPathBrowseButton"] = cmds.symbolButton(w = 40, h = 40, parent = self.widgets["main"], image = self.mayaToolsDir + "/General/Icons/ART/browse.bmp", c = self.browseNewToolsPath)

        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["toolsPathLabel"], "top", 40), (self.widgets["toolsPathLabel"], "left", 10)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["toolsPathField"], "top", 40), (self.widgets["toolsPathField"], "right", 50)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["toolsPathBrowseButton"], "top", 40), (self.widgets["toolsPathBrowseButton"], "right", 5)])

        #Source control settings
        divider = cmds.separator(w = 400, h = 10, style = "out")
        self.widgets["useSourceControl"] = cmds.checkBox( h = 40, label = "Use Source Control", v = False, parent = self.widgets["main"], onc = partial(self.useSourceControl, True), ofc = partial(self.useSourceControl, False))

        cmds.formLayout(self.widgets["main"], edit = True, af = [(divider, "top", 95), (divider, "left", 0)])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["useSourceControl"], "top", 105), (self.widgets["useSourceControl"], "left", 10)])

        #source control test button
        self.widgets["testConnection"] = cmds.button(w = 225, h = 40, label = "Test Perforce Connection", parent = self.widgets["main"], enable = False, c = self.testConnection)
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["testConnection"], "top", 105), (self.widgets["testConnection"], "right", 25)]) 


        #favorite project option
        favoriteProjLabel = cmds.text(label = "Set Favorite Project: ", parent = self.widgets["main"])
        cmds.formLayout(self.widgets["main"], edit = True, af = [(favoriteProjLabel, "top", 170), (favoriteProjLabel, "left", 10)]) 
        self.widgets["favoriteProject_OM"] = cmds.optionMenu(w = 225, h = 40, label = "", parent = self.widgets["main"])
        cmds.menuItem(label = "None", parent = self.widgets["favoriteProject_OM"])

        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["favoriteProject_OM"], "top", 160), (self.widgets["favoriteProject_OM"], "right", 25)]) 


        #get projects and add to menu
        self.findProjects()


        #save settings button
        self.widgets["saveSettings"] = cmds.button(w = 380, h = 40, label = "Save Settings and Close", parent = self.widgets["main"], c = self.saveSettings)
        cmds.formLayout(self.widgets["main"], edit = True, af = [(self.widgets["saveSettings"], "bottom", 10), (self.widgets["saveSettings"], "right", 10)])   

        #show window
        cmds.showWindow(self.widgets["window"])

        #get tools path
        self.getToolsPath()


        #load settings from disk
        settingsLocation = self.mayaToolsDir + "/General/Scripts/projectSettings.txt"
        if os.path.exists(settingsLocation):
            f = open(settingsLocation, 'r')
            settings = cPickle.load(f)	    
            useSource = settings.get("UseSourceControl")
            favoriteProject = settings.get("FavoriteProject")

            f.close()

            try:
                #set the checkbox to the settings value
                cmds.checkBox(self.widgets["useSourceControl"], edit = True, v = useSource)
                self.useSourceControl(useSource)

            except:
                pass

            try:
                #set the optionMenu to favorite project
                cmds.optionMenu(self.widgets["favoriteProject_OM"], edit = True, v = favoriteProject)

            except:
                pass




    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def close(self, *args):

        cmds.deleteUI("AnimationRiggingTool_SettingsUI")



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def getToolsPath(self, *args):

        toolsPath = cmds.internalVar(usd = True) + "mayaTools.txt"
        if os.path.exists(toolsPath):

            f = open(toolsPath, 'r')
            self.mayaToolsDir = f.readline()
            f.close()

        #edit text field
        cmds.textField(self.widgets["toolsPathField"], edit = True, text = self.mayaToolsDir)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def browseNewToolsPath(self, *args):

        newPath = cmds.fileDialog2(fm = 3)
        if newPath != None:
            newPath = newPath[0]

            if newPath.rpartition("/")[2] != "MayaTools":
                cmds.warning("Selected directory is not valid. Please locate the MayaTools directory.")      
                return

            else:
                #create file that contains this path
                path = cmds.internalVar(usd = True) + "mayaTools.txt"

                f = open(path, 'w')
                f.write(newPath)
                f.close()

                self.getToolsPath()
                self.close()
                cmds.confirmDialog(title = "Settings", icon = "warning", message = "New tools path will not be used until Maya has been restarted.")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def useSourceControl(self, state, *args):

        if state:
            cmds.button(self.widgets["testConnection"], edit = True, enable = True)
            cmds.menuItem("perforceSubmitMenuItem", edit = True, enable = True)
            cmds.menuItem("perforceAddAndSubmitMenuItem", edit = True, enable = True)
            cmds.menuItem("perforceCheckOutMenuItem", edit = True, enable = True)
            cmds.menuItem("perforceFileHistoryMenuItem", edit = True, enable = True)
            cmds.menuItem("perforceGetLatestMenuItem", edit = True, enable = True)
            cmds.menuItem("perforceProjectList", edit = True, enable = True)

        if state == False:
            cmds.button(self.widgets["testConnection"], edit = True, enable = False)
            cmds.menuItem("perforceSubmitMenuItem", edit = True, enable = False)
            cmds.menuItem("perforceAddAndSubmitMenuItem", edit = True, enable = False)
            cmds.menuItem("perforceCheckOutMenuItem", edit = True, enable = False)
            cmds.menuItem("perforceFileHistoryMenuItem", edit = True, enable = False)
            cmds.menuItem("perforceGetLatestMenuItem", edit = True, enable = False)
            cmds.menuItem("perforceProjectList", edit = True, enable = False)


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def testConnection(self, *args):


        try:
            from P4 import P4, P4Exception

        except:
            cmds.confirmDialog(title = "Test Connection", icon = "critical", message = "Perforce Python modules not found.")
            return


        try:
            p4 = P4()
            p4.connect()


            #client info
            spec = p4.run( "client", "-o" )[0]

            client = spec.get("Client")
            owner = spec.get("Owner")
            host = spec.get("Host")

            p4.disconnect()

            cmds.confirmDialog(title = "Test Connection", message = "Connection Sucessful!\n\nUsername: " + owner + "\nClient: " + client + "\nHost: " + host + ".")


        except:
            cmds.confirmDialog(title = "Test Connection", message = "Unable to connect to Perforce server.")


    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def saveSettings(self, *args):

        #this function will save out the user's preferences they have set in the UI to disk
        settingsLocation = self.mayaToolsDir + "/General/Scripts/projectSettings.txt"

        try:
            f = open(settingsLocation, 'w')


            #create a dictionary with  values
            settings = {}
            settings["UseSourceControl"] = cmds.checkBox(self.widgets["useSourceControl"], q = True, v = True)
            settings["FavoriteProject"] = cmds.optionMenu(self.widgets["favoriteProject_OM"], q = True, v = True)

            #write our dictionary to file
            cPickle.dump(settings, f)
            f.close()

        except:
            cmds.confirmDialog(title = "Error", icon = "critical", message = settingsLocation + " is not writeable. Please make sure this file is not set to read only.")



        #close the UI
        cmds.deleteUI("AnimationRiggingTool_SettingsUI")



    # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def findProjects(self):

        projectPath = self.mayaToolsDir + "/General/ART/Projects/"

        try:
            projects = os.listdir(projectPath)

            if len(projects) > 0:
                for proj in sorted(projects):
                    cmds.menuItem(label = proj, parent = self.widgets["favoriteProject_OM"])

        except:
            pass