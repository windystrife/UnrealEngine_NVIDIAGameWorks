#!/bin/sh

#Get the path/name of the Xcode project file from the script location.
#If the script is moved, the directory, and/or max depth will need to be changed.
ProjectFile=$(find "../../../.." -name "*.xcodeproj" -maxdepth 1)

echo $ProjectFile

#Check for string, because checking -e always seems to fail, despite the existence of the file/dir.
if [ -z $ProjectFile ] 
then
    echo "Couldn't find an xcode project file in directory"
    exit 1
fi

# Applescript uses : instead of / for some reason.
ProjectFile=${ProjectFile////:}

ProjectName="{$0}"
BuildConfiguration=$1
DeviceName=""

#Need to find a better way for setting up sim, where we can get the SDK version we want to run on.
if [ $4 = "SimDev" ]
then
	DeviceName=$2
else
	if [ $3 = "Tablet" ]
	then
		DeviceName="iPad 6.0 Simulator"
	else
		DeviceName="iPhone 6.0 Simulator"
	fi
fi


#osascript <<SCRIPT
#application "iPhone Simulator" quit
#application "iPhone Simulator" activate

#tell application "Xcode"
#	open "$ProjectFile"
#	set targetProject to ProjectName

#	tell targetProject
#		set active build configuration type to BuildConfiguration

#		if (build targetProject) is equal to "Build succeeded" then
#			launch targetProject
#		end if
#	end tell
#end tell
#SCRIPT