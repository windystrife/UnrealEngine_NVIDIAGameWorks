// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeActions.h"

#define LOCTEXT_NAMESPACE "MatineeCommands"

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FMatineeCommands::RegisterCommands()
{
	UI_COMMAND(AddKey, "Add Key", "Add Key", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::Enter));
	
	UI_COMMAND(Play, "Play", "Play", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::L));
	UI_COMMAND(PlayLoop, "Loop", "Loop Section", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Stop, "Stop", "Stop", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::K));
	UI_COMMAND(PlayReverse, "Reverse", "Play in Reverse | Plays the Matinee sequence backwards, starting from the time cursor's position", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::J));

	UI_COMMAND(PlayPause, "Play/Pause", "Toggle Play/Pause", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::P));

	UI_COMMAND(CreateCameraActor, "Camera", "Create Camera Actor at Current Camera Location", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ToggleSnap, "Snap", "Toggle Snap", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::None, EKeys::S));
	UI_COMMAND(ToggleSnapTimeToFrames, "Time to Frames", "Snap Time to Frames | Snaps the timeline cursor to the frame rate specified in the Snap Size setting", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(FixedTimeStepPlayback, "Fixed Time ", "Fixed Time Step Playback | Locks the playback rate to the frame rate specified in the Snap Size setting", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(FitSequence, "Sequence", "Fit View to Sequence", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::A));
	UI_COMMAND(FitViewToSelected, "Selected", "Fit View to Selected", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::F));
	UI_COMMAND(FitLoop, "Loop", "Fit View to Locp", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::A));
	UI_COMMAND(FitLoopSequence, "Loop Sequence", "Fit View / Loop Sequence", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::A));
	UI_COMMAND(ViewEndofTrack, "End", "Move to End of Track ", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::E));

	UI_COMMAND(ToggleGorePreview, "Gore", "Enable Gore in Editor Preview", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(LaunchRecordWindow, "Record", "Launch Recording Window for Matinee", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateMovie, "Movie", "Create a Movie", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(FileImport, "Import...", "Imports Sequence", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(FileExport, "Export All...", "Exports all Sequences", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportSoundCueInfo, "Export Sound Cue Info", "Exports Sound Cue info", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportAnimInfo, "Export Animation Track Info", "Exports Animation Track info", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(FileExportBakeTransforms, "Bake Transforms on Export", "Exports a key every frame, instead of just user created keys", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(FileExportKeepHierarchy, "Keep Hierarchy on Export", "Exports all objects to be in the same hierarchy in engine", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(DeleteSelectedKeys, "Delete Keys", "Deletes selected keys", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DuplicateKeys, "Duplicate Keys", "Duplicates selected keys", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(InsertSpace, "Insert Space", "Inserts space at the current position", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StretchSection, "Stretch Section", "Stretches the selected section", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StretchSelectedKeyFrames, "Stretch Selected Key Frames", "Stretches the selected keys", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DeleteSection, "Delete Section", "Deletes the selected section", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SelectInSection, "Select In Section", "Selects the keys in Section", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReduceKeys, "Reduce Keys", "Attempts to remove unnecessary keys", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SavePathTime, "Save Path Time", "Saves the path time at the current position", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(JumpToPathTime, "Jump to Path Time", "Jumps to the previously saved path time", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ScaleTranslation, "Scale Translation", "Scales the translation of the selected movement trac", EUserInterfaceActionType::Button, FInputChord() );

	UI_COMMAND(Draw3DTrajectories, "Draw 3D Trajectories", "Toggles 3D trajectories", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ShowAll3DTrajectories, "Show All 3D Trajectories", "Shows all movement track trajectories", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(HideAll3DTrajectories, "Hide All 3D Trajectories", "Hides all movement track trajectories", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PreferFrameNumbers, "Prefer Frame Numbers", "Toggles frame numbers on keys", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ShowTimeCursorPosForAllKeys, "Show Time Cursor Position", "Shows relative time cursor position on keys", EUserInterfaceActionType::Check, FInputChord());

	UI_COMMAND(ZoomToTimeCursorPosition, "Zoom To Time Cursor Position", "Toggles center zoom on time cursor position", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ViewFrameStats, "View Frame Stats", "Views frame stats", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EditingCrosshair, "Editing Crosshair", "Toggles editing crosshair", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(EnableEditingGrid,"Enable Editing Grid", "Toggles editing grid", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(TogglePanInvert,"Pan Invert", "Toggles pan invert", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleAllowKeyframeBarSelection,"Allow Keyframe Bar Selection", "Toggles keyframe bar selection", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleAllowKeyframeTextSelection,"Allow Keyframe Text Selection", "Toggles keyframe text selection", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(ToggleLockCameraPitch,"Lock Camera Pitch", "Toggles lock camera pitch", EUserInterfaceActionType::ToggleButton, FInputChord());

	//Context menu commands
	UI_COMMAND(EditCut,"Cut", "Cuts selected group or track", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::X));
	UI_COMMAND(EditCopy,"Copy", "Copies selected group or track", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(EditPaste,"Paste", "Pastes group or track", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));

	//Tab
	UI_COMMAND(GroupDeleteTab, "Delete Group Tab", "Deletes selected group", EUserInterfaceActionType::Button, FInputChord());

	//Group
	UI_COMMAND(ActorSelectAll, "Select Group Actors","Selects all group actors", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ActorAddAll, "Add Selected Actors","Adds selected actors to group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ActorReplaceAll, "Replace Group Actors With Selected Actors","Replaces group actors with selected actors", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ActorRemoveAll, "Remove Group Actors","Removes all group actors", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportCameraAnim, "Export To CameraAnim","Exports selected group to CameraAnim", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportAnimTrackFBX, "Export (.FBX)","Exports selected track as FBX", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportAnimGroupFBX, "Export (.FBX)","Exports selected group as FBX", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupDuplicate, "Duplicate Group","Duplicates selected group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupDelete, "Delete Group Tab","Deletes selected group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupCreateTab, "Create Group Tab","Creates new group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(GroupRemoveFromTab, "Remove From Selected Group Tab","Removes selected group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveFromGroupFolder, "Remove From Folder","Moves group to selected folder", EUserInterfaceActionType::Button, FInputChord());

	//Track
	UI_COMMAND(TrackRename, "Rename Track","Renames selected track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TrackDelete, "Delete Track","Deletes selected track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(Show3DTrajectory, "Show 3D Trajectory","Shows 3D trajectory", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TrackSplitTransAndRot, "Split Translation And Rotation","Splits movement into translation and rotation tracks", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(TrackNormalizeVelocity, "Normalize Velocity","Normalizes track velocity", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ParticleReplayTrackContextStartRecording, "Start Recording Particles","Starts recording particle replay track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ParticleReplayTrackContextStopRecording, "Stop Recording Particles","Stops recording particle replay track", EUserInterfaceActionType::Button, FInputChord());

	//Background
	UI_COMMAND(NewFolder, "Add New Folder", "Adds new Folder", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewEmptyGroup, "Add New Empty Group", "Adds new Empty Group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewCameraGroup, "Add New Camera Group", "Adds new Camera Group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewParticleGroup, "Add New Particle Group", "Adds new Particle Group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewSkeletalMeshGroup, "Add New Skeletal Group", "Adds new Skeletal Group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewLightingGroup, "Add New Lighting Group", "Adds new Lighting Group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NewDirectorGroup, "Add New Director Group", "Adds new Director Group", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ToggleCurveEditor, "Curves", "Toggles curve editor", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleDirectorTimeline, "Director Timeline","Toggles director timeline", EUserInterfaceActionType::ToggleButton, FInputChord());

	//Key
	UI_COMMAND(KeyModeCurveAuto, "Curve (Auto)","Sets mode to Curve (Auto)", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeyModeCurveAutoClamped, "Curve (Auto/Clamped)","Sets mode to Curve (Auto/Clamped)", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeyModeCurveBreak, "Curve (Break)","Sets mode to Curve (Break)", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeyModeLinear, "Linear","Sets mode to Linear", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeyModeConstant, "Constant","Sets mode to Constant", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetTime, "Set Time","Sets key time", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetValue, "Set Value","Sets key value", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetBool, "Set Bool","Sets key flag", EUserInterfaceActionType::Button, FInputChord()); 
	UI_COMMAND(KeySetColor, "Set Color","Sets key color", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MoveKeySetLookup, "Select Transform Lookup Group...","Selects transform lookup group...", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MoveKeyClearLookup, "Clear Transform Lookup Group","Clears transform lookup group", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EventKeyRename, "Rename Event","Renames event key", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DirKeySetTransitionTime, "Set Transition Time","Sets transition time", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DirKeyRenameCameraShot, "Rename Shot","Renames camera key", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetMasterVolume, "Set Master Volume","Sets master volume", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetMasterPitch, "Set Master Pitch","Sets master pitch", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleKeyFlip, "Flip Toggle","Flips toggle action", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(KeySetConditionAlways, "Always Active","Toggles always active", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(KeySetConditionGoreEnabled, "Active if Gore is Enabled","Toggles active if gore is enabled", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(KeySetConditionGoreDisabled, "Active if Gore is Disabled","Toggles active if gore is disabled", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(AnimKeyLoop, "Set Looping","Sets key looping", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnimKeyNoLoop, "Set Non-Looping","Sets key non-looping", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnimKeySetStartOffset, "Set Start Offset","Sets key start offset", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnimKeySetEndOffset, "Set End Offset","Sets key end offset", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnimKeySetPlayRate, "Set Play Rate","Sets key play rate", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(AnimKeyToggleReverse, "Reverse","Toggles key reverse", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(KeySyncGenericBrowserToSoundCue, "Sync Browser To Sound Cue","Finds Sound Cue in Content Browser", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ParticleReplayKeySetClipIDNumber, "Select Particle Replay Clip ID","Selects Particle Replay Clip ID", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ParticleReplayKeySetDuration, "Set Particle Replay Duration","Sets Particle Replay Clip duration", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SoundKeySetVolume, "Set Sound Volume","Sets key sound volume", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(SoundKeySetPitch, "Set Sound Pitch","Sets key sound pitch", EUserInterfaceActionType::Button, FInputChord());

	//Collapse/Expand context menu
	UI_COMMAND(ExpandAllGroups, "Expand All","Expand the entire hierarchy of folders, groups and tracks in UnrealMatinee's track window", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CollapseAllGroups, "Collapse All","Collapse the entire hierarchy of folders, groups and tracks in UnrealMatinee's track window", EUserInterfaceActionType::Button, FInputChord());

	//Marker Context Menu
	UI_COMMAND(MarkerMoveToBeginning, "Move To Sequence Start","Moves marker to sequence start", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MarkerMoveToEnd, "Move To Sequence End","Moves marker to sequence end", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MarkerMoveToEndOfLongestTrack, "Move To Longest Track Endpoint","Moves marker to end of longest track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MarkerMoveToEndOfSelectedTrack, "Move To Longest Selected Track Endpoint","Moves marker to end of selected track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(MarkerMoveToCurrentPosition, "Move To Current Timeline Position","Moves marker to current timeline position", EUserInterfaceActionType::Button, FInputChord());

	//Viewport/Key commands
	UI_COMMAND(ZoomIn,"Zoom In","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Add));
	UI_COMMAND(ZoomOut,"Zoom Out","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Subtract));
	UI_COMMAND(ZoomInAlt,"Zoom In Alt","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Equals));
	UI_COMMAND(ZoomOutAlt,"Zoom Out Alt","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Underscore));
	UI_COMMAND(MarkInSection,"Mark In Section","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::I));
	UI_COMMAND(MarkOutSection,"Mark Out Section","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::O));
	UI_COMMAND(IncrementPosition,"Increment Position","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Right));
	UI_COMMAND(DecrementPosition,"Decrement Position","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Left));
	UI_COMMAND(MoveToNextKey,"Move To Next Key","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Shift, EKeys::Right));
	UI_COMMAND(MoveToPrevKey,"Move To Previous Key","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Shift, EKeys::Left));
	UI_COMMAND(SplitAnimKey,"Split Anim Key","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::R));
	UI_COMMAND(MoveActiveUp,"Move Active Track/Group Up","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Up));
	UI_COMMAND(MoveActiveDown,"Move Active Track/Group Down","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Down));
#if PLATFORM_MAC
	UI_COMMAND(DuplicateSelectedKeys,"Duplicate Selected Keys","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Command, EKeys::W));
#else
	UI_COMMAND(DuplicateSelectedKeys,"Duplicate Selected Keys","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Control, EKeys::W));
#endif
	UI_COMMAND(CropAnimationBeginning,"Crop Animation Start","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Control, EKeys::I));
	UI_COMMAND(CropAnimationEnd,"Crop Animation End","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::Control, EKeys::O));
	UI_COMMAND(ChangeKeyInterpModeAuto,"Change Key Interp Mode Auto","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::One));
	UI_COMMAND(ChangeKeyInterpModeAutoClamped,"Change Key Interp Mode Auto Clamped","",EUserInterfaceActionType::Button,FInputChord());
	UI_COMMAND(ChangeKeyInterpModeUser,"Change Key Interp Mode User","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Two));
	UI_COMMAND(ChangeKeyInterpModeBreak,"Change Key Interp Mode Break","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Three));
	UI_COMMAND(ChangeKeyInterpModeLinear,"Change Key Interp Mode Linear","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Four));
	UI_COMMAND(ChangeKeyInterpModeConstant,"Change Key Interp Mode Constant","",EUserInterfaceActionType::Button,FInputChord(EModifierKey::None, EKeys::Five));
	UI_COMMAND(DeleteSelection, "Delete Selection", "", EUserInterfaceActionType::Button, FInputChord(EModifierKey::None, EKeys::Platform_Delete));
}
PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
