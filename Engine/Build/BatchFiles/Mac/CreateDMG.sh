#!/bin/bash

# Automatically abort if any command returns a non-zero exit code.
# Append something like "|| true" to the end of the command line if you really need to ignore errors for some reason.
set -e

APP_PATH=$(cd "$1" > /dev/null; pwd)
APP_SIZE=$(expr $(du -sm "$APP_PATH" | awk '{print $1}') + 500)
APP_NAME=$(basename "$APP_PATH")
DMG_PATH=$2
DMG_TEMP_PATH="${DMG_PATH}.temp.dmg"
VOLUME_NAME=$3
MOUNT_DIR="/Volumes/$VOLUME_NAME"
BACKGROUND_FILE_PATH=$4
BACKGROUND_FILE_NAME=$(basename "$BACKGROUND_FILE_PATH")
VOLUME_ICON_PATH=$5

# Create and mount the image
test -d "$MOUNT_DIR" && hdiutil detach "$MOUNT_DIR" > /dev/null
test -f "$DMG_TEMP_PATH" && rm -f "$DMG_TEMP_PATH"
hdiutil create -srcfolder "$APP_PATH" -volname "$VOLUME_NAME" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${APP_SIZE}m "$DMG_TEMP_PATH" > /dev/null
sleep 8
hdiutil attach -readwrite -noverify -noautoopen "$DMG_TEMP_PATH" > /dev/null
sleep 8

# Create /Applications link
ln -s /Applications "$MOUNT_DIR/Applications"

# Copy background image
mkdir -p "$MOUNT_DIR/.background"
cp "$BACKGROUND_FILE_PATH" "$MOUNT_DIR/.background/$BACKGROUND_FILE_NAME"

# Set volume icon - @todo: requires sudo
#cp "$VOLUME_ICON_PATH" "$MOUNT_DIR/.VolumeIcon.icns"
#SetFile -c icnC "$MOUNT_DIR/.VolumeIcon.icns"
#SetFile -a C "$MOUNT_DIR"

# Setup image window look
echo '
	tell application "Finder"
		tell disk "'$VOLUME_NAME'"
			open

			delay 12

			tell container window
				set current view to icon view
				set toolbar visible to false
				set statusbar visible to false
				set the bounds to {200, 200, 840, 575}
			end tell

			delay 12

			set Options to the icon view options of container window
			tell Options
				set icon size to 128
				set arrangement to not arranged
			end tell
			set background picture of Options to file ".background:'$BACKGROUND_FILE_NAME'"

			delay 12

			set position of item "'$APP_NAME'" of container window to {170, 220}
			set position of item "Applications" of container window to {485, 220}

			delay 12

			update without registering applications

			delay 12
			close
		end tell
	end tell
' | osascript

# Make the volume contents read-only - @todo: requires sudo
#chmod -Rf go-w "$MOUNT_DIR" > /dev/null

# Make the image open its window in Finder on mount
bless --folder "$MOUNT_DIR" --openfolder "$MOUNT_DIR"

# Unmount the image
hdiutil detach "$MOUNT_DIR" > /dev/null

# Compress the image
test -f "$DMG_PATH" && rm -f "$DMG_PATH"
hdiutil convert "$DMG_TEMP_PATH" -format UDZO -imagekey zlib-level=9 -o "$DMG_PATH" > /dev/null
rm -f "$DMG_TEMP_PATH"

exit 0
