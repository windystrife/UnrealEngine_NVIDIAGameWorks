import maya.cmds as cmds
import maya.OpenMayaUI as mui
from PySide import QtGui, QtCore
import shiboken, os, inspect
from functools import partial
import json


def getMainWindow():
    pointer = mui.MQtUtil.mainWindow()
    return shiboken.wrapInstance(long(pointer), QtGui.QWidget)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def returnNicePath(toolsPath, imagePath):
    image =  os.path.normpath(os.path.join(toolsPath, imagePath))
    if image.partition("\\")[2] != "":
        image = image.replace("\\", "/")
    return image

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
def writeQSS(filePath):

    #this function takes the qss file given, and finds and replaces any image path URLs using the user's settings for the icons path and changes the file on disk
    iconPath = os.path.dirname(os.path.normpath(__file__))
    iconPath = returnNicePath(iconPath, "icons")

    f = open(filePath, "r")
    lines = f.readlines()
    f.close()

    newLines = []
    for line in lines:
        if line.find("url(") != -1:
            oldPath = line.partition("(")[2].rpartition("/")[0]


            newLine =  line.replace(oldPath, iconPath)
            newLines.append(newLine)
        else:
            newLines.append(line)

    f = open(filePath, "w")
    for line in newLines:
        f.write(line)
    f.close()

class CreateJointFromGeo(QtGui.QMainWindow):

    #Original Author: Jeremy Ernst

    def __init__(self, parent = None):

        super(CreateJointFromGeo, self).__init__(parent)

        filePath = os.path.normpath(__file__)
        stylesheet = os.path.join(os.path.dirname(filePath), "mainScheme.qss")
        writeQSS(stylesheet)

        self.buildUI()

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def buildUI(self):
        #Original Author: Jeremy Ernst

        #create the main widget
        self.mainWidget = QtGui.QWidget()
        self.mainWidget.setStyleSheet("background-color: rgb(0, 0, 0);, color: rgb(0,0,0);")
        self.setCentralWidget(self.mainWidget)

        #load stylesheet
        filePath = os.path.normpath(__file__)
        stylesheet = os.path.join(os.path.dirname(filePath), "mainScheme.qss")
        f = open(stylesheet, "r")
        style = f.read()
        f.close()

        self.mainWidget.setStyleSheet(style)

        #set qt object name
        self.setObjectName("createJointFromGeoWIN")
        self.setWindowTitle("Create Joint From GEO")

        #create the mainLayout for the rig creator UI
        self.layout = QtGui.QVBoxLayout(self.mainWidget)

        self.resize(400, 230)
        self.setMinimumSize(QtCore.QSize( 400, 230 ))
        self.setMaximumSize(QtCore.QSize( 400, 230 ))
        self.setStyleSheet("""QToolTip { background-color: black; color: white;border: black solid 1px}""")


        #vert selection 1

        self.selection1Layout = QtGui.QHBoxLayout()
        self.layout.addLayout(self.selection1Layout)

        self.selection1 = QtGui.QLineEdit()
        self.selection1.setReadOnly(True)
        self.selection1Layout.addWidget(self.selection1)
        self.selection1.setMinimumWidth(300)
        self.selection1.setPlaceholderText("Load in component selection")

        self.sel1Btn = QtGui.QPushButton("<-")
        self.selection1Layout.addWidget(self.sel1Btn)
        self.sel1Btn.setObjectName("blueButton")
        self.sel1Btn.clicked.connect(partial(self.loadSelection, self.selection1, 1))


        #vert selection 2

        self.selection2Layout = QtGui.QHBoxLayout()
        self.layout.addLayout(self.selection2Layout)

        self.selection2 = QtGui.QLineEdit()
        self.selection2.setReadOnly(True)
        self.selection2Layout.addWidget(self.selection2)
        self.selection2.setMinimumWidth(300)
        self.selection2.setPlaceholderText("Load in component selection")

        self.sel2Btn = QtGui.QPushButton("<-")
        self.selection2Layout.addWidget(self.sel2Btn)
        self.sel2Btn.setObjectName("blueButton")
        self.sel2Btn.clicked.connect(partial(self.loadSelection, self.selection2, 2))



        #slider
        label = QtGui.QLabel("Position of joint: Start of selection -> End of selection")
        self.layout.addWidget(label)
        self.slider = QtGui.QSlider()
        self.slider.setOrientation(QtCore.Qt.Horizontal)
        self.slider.setRange(0,100)
        self.layout.addWidget(self.slider)
        self.slider.setTickInterval(10)
        self.slider.setPageStep(10)
        self.slider.setTickPosition(QtGui.QSlider.TicksAbove)
        self.slider.setSingleStep(10)

        #jointName
        self.jointName = QtGui.QLineEdit()
        self.jointName.setPlaceholderText("createdJoint")
        self.layout.addWidget(self.jointName)



        #Create Button
        self.createBtn = QtGui.QPushButton("CREATE JOINT")
        self.layout.addWidget(self.createBtn)
        self.createBtn.setMinimumHeight(50)
        self.createBtn.setObjectName("blueButton")
        self.createBtn.clicked.connect(self.createJoint)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def loadSelection(self, field, num):

        currentSelection = cmds.ls(sl = True, fl = True)
        cluster = cmds.cluster()
        joint = cmds.createNode("joint", name = "selection" + str(num) + "_joint")
        constraint = cmds.pointConstraint(cluster[1], joint)[0]
        cmds.delete(constraint)

        cmds.delete(cluster)

        center = cmds.xform(joint, q = True, ws = True, rp = True)
        field.setText(json.dumps(center))

        cmds.select(currentSelection)

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
    def createJoint(self):

        if not cmds.objExists("selection1_joint"):
            joint = cmds.createNode("joint", name = "selection1_joint")
            pos = json.loads(self.selection1.text())
            cmds.xform(joint, ws = True, t = pos)

        if not cmds.objExists("selection2_joint"):
            joint = cmds.createNode("joint", name = "selection2_joint")
            pos = json.loads(self.selection2.text())
            cmds.xform(joint, ws = True, t = pos)

        constraint = cmds.aimConstraint("selection1_joint", "selection2_joint", aimVector = [1,0,0], upVector = [0,0,1], worldUpType = "scene")[0]
        cmds.delete(constraint)

        constraint = cmds.orientConstraint("selection2_joint", "selection1_joint")[0]
        cmds.delete(constraint)

        #get joint name
        name = self.jointName.text()

        newJoint = cmds.createNode("joint", name = name)
        orientConstraint = cmds.orientConstraint("selection1_joint", newJoint)[0]
        cmds.delete(orientConstraint)
        pointConstraint = cmds.pointConstraint(["selection1_joint", "selection2_joint"], newJoint)[0]

        #get slider value
        value = self.slider.value()
        value = float(float(value)/100.0)

        weight1Value = 1.0 - value
        weight2Value = 1.0 - weight1Value

        cmds.setAttr(pointConstraint + ".selection1_jointW0", weight1Value)
        cmds.setAttr(pointConstraint + ".selection2_jointW1", weight2Value)

        cmds.delete(pointConstraint)
        cmds.delete(["selection1_joint", "selection2_joint"])

def run():

    if cmds.window("createJointFromGeoWIN", exists = True):
        cmds.deleteUI("createJointFromGeoWIN", wnd = True)

    inst = CreateJointFromGeo(getMainWindow())
    inst.show()

