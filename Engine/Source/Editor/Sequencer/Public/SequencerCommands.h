// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FSequencerCommands : public TCommands<FSequencerCommands>
{

public:
	FSequencerCommands() : TCommands<FSequencerCommands>
	(
		"Sequencer",
		NSLOCTEXT("Contexts", "Sequencer", "Sequencer"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Toggle play */
	TSharedPtr< FUICommandInfo > TogglePlay;

	/** Play forward */
	TSharedPtr< FUICommandInfo > PlayForward;

	/** Jump to start of playback */
	TSharedPtr< FUICommandInfo > JumpToStart;

	/** Jump to end of playback */
	TSharedPtr< FUICommandInfo > JumpToEnd;

	/** Shuttle forward */
	TSharedPtr< FUICommandInfo > ShuttleForward;

	/** Shuttle backward */
	TSharedPtr< FUICommandInfo > ShuttleBackward;

	/** Pause */
	TSharedPtr< FUICommandInfo > Pause;

	/** Step forward */
	TSharedPtr< FUICommandInfo > StepForward;

	/** Step backward */
	TSharedPtr< FUICommandInfo > StepBackward;

	/** Step to next key */
	TSharedPtr< FUICommandInfo > StepToNextKey;

	/** Step to previous key */
	TSharedPtr< FUICommandInfo > StepToPreviousKey;

	/** Step to next camera key */
	TSharedPtr< FUICommandInfo > StepToNextCameraKey;

	/** Step to previous camera key */
	TSharedPtr< FUICommandInfo > StepToPreviousCameraKey;

	/** Step to next shot */
	TSharedPtr< FUICommandInfo > StepToNextShot;

	/** Step to previous shot */
	TSharedPtr< FUICommandInfo > StepToPreviousShot;

	/** Set start playback range */
	TSharedPtr< FUICommandInfo > SetStartPlaybackRange;

	/** Set end playback range */
	TSharedPtr< FUICommandInfo > SetEndPlaybackRange;

	/** Reset the view range to the playback range */
	TSharedPtr< FUICommandInfo > ResetViewRange;

	/** Zoom into the view range */
	TSharedPtr< FUICommandInfo > ZoomInViewRange;

	/** Zoom out of the view range */
	TSharedPtr< FUICommandInfo > ZoomOutViewRange;

	/** Set the selection range to the next shot. */
	TSharedPtr< FUICommandInfo > SetSelectionRangeToNextShot;

	/** Set the selection range to the previous shot. */
	TSharedPtr< FUICommandInfo > SetSelectionRangeToPreviousShot;

	/** Set the playback range to all the shots. */
	TSharedPtr< FUICommandInfo > SetPlaybackRangeToAllShots;

	/** Toggle locking the playback range. */
	TSharedPtr< FUICommandInfo > TogglePlaybackRangeLocked;

	/** Forces playback both in editor and at runtime to be evaluated at fixed frame intervals. */
	TSharedPtr< FUICommandInfo > ToggleForceFixedFrameIntervalPlayback;

	/** Reruns construction scripts on bound actors every frame. */
	TSharedPtr< FUICommandInfo > ToggleRerunConstructionScripts;

	/** Toggle constraining the time cursor to the playback range while scrubbing */
	TSharedPtr< FUICommandInfo > ToggleKeepCursorInPlaybackRangeWhileScrubbing;

	/** Toggle constraining the time cursor to the playback range during playback */
	TSharedPtr< FUICommandInfo > ToggleKeepCursorInPlaybackRange;

	/** Toggle constraining the playback range to the section bounds */
	TSharedPtr< FUICommandInfo > ToggleKeepPlaybackRangeInSectionBounds;

	/** Expand all nodes and descendants */
	TSharedPtr< FUICommandInfo > ExpandAllNodesAndDescendants;

	/** Collapse all nodes and descendants */
	TSharedPtr< FUICommandInfo > CollapseAllNodesAndDescendants;

	/** Expand/collapse nodes */
	TSharedPtr< FUICommandInfo > ToggleExpandCollapseNodes;

	/** Expand/collapse nodes and descendants */
	TSharedPtr< FUICommandInfo > ToggleExpandCollapseNodesAndDescendants;

	/** Sets the upper bound of the selection range */
	TSharedPtr< FUICommandInfo > SetSelectionRangeEnd;

	/** Sets the lower bound of the selection range */
	TSharedPtr< FUICommandInfo > SetSelectionRangeStart;

	/** Clear and reset the selection range */
	TSharedPtr< FUICommandInfo > ResetSelectionRange;

	/** Select all keys that fall into the selection range*/
	TSharedPtr< FUICommandInfo > SelectKeysInSelectionRange;

	/** Select all sections that fall into the selection range*/
	TSharedPtr< FUICommandInfo > SelectSectionsInSelectionRange;

	/** Select all keys and sections that fall into the selection range*/
	TSharedPtr< FUICommandInfo > SelectAllInSelectionRange;

	/** Sets a key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > SetKey;

	/** Sets the interp tangent mode for the selected keys to auto */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicAuto;

	/** Sets the interp tangent mode for the selected keys to user */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicUser;

	/** Sets the interp tangent mode for the selected keys to break */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicBreak;

	/** Sets the interp tangent mode for the selected keys to linear */
	TSharedPtr< FUICommandInfo > SetInterpolationLinear;

	/** Sets the interp tangent mode for the selected keys to constant */
	TSharedPtr< FUICommandInfo > SetInterpolationConstant;

	/** Trim section to the left, keeping the right portion */
	TSharedPtr< FUICommandInfo > TrimSectionLeft;

	/** Trim section to the right, keeping the left portion */
	TSharedPtr< FUICommandInfo > TrimSectionRight;

	/** Translate the selected keys and section to the left */
	TSharedPtr< FUICommandInfo > TranslateLeft;

	/** Translate the selected keys and section to the right */
	TSharedPtr< FUICommandInfo > TranslateRight;

	/** Split section */
	TSharedPtr< FUICommandInfo > SplitSection;

	/** Set the auto change mode to Key. */
	TSharedPtr< FUICommandInfo > SetAutoKey;

	/** Set the auto change mode to Track. */
	TSharedPtr< FUICommandInfo > SetAutoTrack;

	/** Set the auto change mode to None. */
	TSharedPtr< FUICommandInfo > SetAutoChangeAll;

	/** Set the auto change mode to None. */
	TSharedPtr< FUICommandInfo > SetAutoChangeNone;

	/** Set allow edits to all. */
	TSharedPtr< FUICommandInfo > AllowAllEdits;

	/** Set allow edits to sequencer only. */
	TSharedPtr< FUICommandInfo > AllowSequencerEditsOnly;

	/** Set allow edits to levels only. */
	TSharedPtr< FUICommandInfo > AllowLevelEditsOnly;

	/** Turns autokey on and off. */
	TSharedPtr< FUICommandInfo > ToggleAutoKeyEnabled;

	/** Turns key all on and off. */
	TSharedPtr< FUICommandInfo > ToggleKeyAllEnabled;

	/** Turns show frame numbers on and off. */
	TSharedPtr< FUICommandInfo > ToggleShowFrameNumbers;

	/** Toggle the visibility of the goto box. */
	TSharedPtr< FUICommandInfo > ToggleShowGotoBox;

	/** Toggle the visibility of the transform box. */
	TSharedPtr< FUICommandInfo > ToggleShowTransformBox;

	/** Turns the range slider on and off. */
	TSharedPtr< FUICommandInfo > ToggleShowRangeSlider;

	/** Turns snapping on and off. */
	TSharedPtr< FUICommandInfo > ToggleIsSnapEnabled;

	/** Toggles whether or not keys should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapKeyTimesToInterval;

	/** Toggles whether or not keys should snap to other keys in the section. */
	TSharedPtr< FUICommandInfo > ToggleSnapKeyTimesToKeys;

	/** Toggles whether or not sections should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapSectionTimesToInterval;

	/** Toggles whether or not sections should snap to other sections. */
	TSharedPtr< FUICommandInfo > ToggleSnapSectionTimesToSections;

	/** Toggles whether or not snap to key times while scrubbing. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToKeys;

	/** Toggles whether or not the play time should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToInterval;

	/** Toggles whether or not the play time should snap to the pressed key. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToPressedKey;

	/** Toggles whether or not the play time should snap to the dragged key. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToDraggedKey;

	/** Toggles whether or not to snap curve values to the interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapCurveValueToInterval;

	/** Finds the viewed sequence asset in the content browser. */
	TSharedPtr< FUICommandInfo > FindInContentBrowser;

	/** Toggles whether to show combined keys at the top node level. */
	TSharedPtr< FUICommandInfo > ToggleCombinedKeyframes;

	/** Toggles whether to show channel colors in the track area. */
	TSharedPtr< FUICommandInfo > ToggleChannelColors;

	/** Toggles whether the label browser is enabled in the level editor. */
	TSharedPtr< FUICommandInfo > ToggleLabelBrowser;

	/** Turns auto scroll on and off. */
	TSharedPtr< FUICommandInfo > ToggleAutoScroll;

	/** Toggles whether or not the curve editor should be shown. */
	TSharedPtr< FUICommandInfo > ToggleShowCurveEditor;

	/** Toggles whether or not the curve editor time range should be linked to the sequencer. */
	TSharedPtr< FUICommandInfo > ToggleLinkCurveEditorTimeRange;

	/** Toggles visualization of pre and post roll */
	TSharedPtr< FUICommandInfo > ToggleShowPreAndPostRoll;

	/** Enable the move tool */
	TSharedPtr< FUICommandInfo > MoveTool;

	/** Enable the marquee selection tool */
	TSharedPtr< FUICommandInfo > MarqueeTool;

	/** Open a panel that enables exporting the sequence to a movie */
	TSharedPtr< FUICommandInfo > RenderMovie;

	/** Create camera and set it as the current camera cut */
	TSharedPtr< FUICommandInfo > CreateCamera;

	/** Paste from the sequencer clipboard history */
	TSharedPtr< FUICommandInfo > PasteFromHistory;

	/** Convert the selected possessed objects to spawnables. These will be spawned and destroyed by sequencer as per object's the spawn track. */
	TSharedPtr< FUICommandInfo > ConvertToSpawnable;

	/** Convert the selected spawnable objects to possessables. The newly created possessables will be created in the current level. */
	TSharedPtr< FUICommandInfo > ConvertToPossessable;

	/** Saves the current state of this object as the default spawnable state. */
	TSharedPtr< FUICommandInfo > SaveCurrentSpawnableState;

	/** Restores all animated state for the current sequence. */
	TSharedPtr< FUICommandInfo > RestoreAnimatedState;

	/** Discard all changes to the current movie scene. */
	TSharedPtr< FUICommandInfo > DiscardChanges;

	/** Attempts to fix broken actor references. */
	TSharedPtr< FUICommandInfo > FixActorReferences;

	/** Rebinds all possessable references with their current bindings. */
	TSharedPtr< FUICommandInfo > RebindPossessableReferences;

	/** Attempts to move all time data for this sequence on to a valid frame */
	TSharedPtr< FUICommandInfo > FixFrameTiming;

	/** Record the selected actors into a sub sequence of the currently active sequence */
	TSharedPtr< FUICommandInfo > RecordSelectedActors;

	/** Imports animation from fbx. */
	TSharedPtr< FUICommandInfo > ImportFBX;

	/** Exports animation to fbx. */
	TSharedPtr< FUICommandInfo > ExportFBX;

	/** Toggle whether we should evaluate sub sequences in isolation */
	TSharedPtr< FUICommandInfo > ToggleEvaluateSubSequencesInIsolation;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
