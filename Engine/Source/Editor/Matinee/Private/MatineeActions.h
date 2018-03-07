// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/*-----------------------------------------------------------------------------
   FMatineeCommands
-----------------------------------------------------------------------------*/

class FMatineeCommands : public TCommands<FMatineeCommands>
{
public:
	/** Constructor */
	FMatineeCommands() 
		: TCommands<FMatineeCommands>("Matinee", NSLOCTEXT("Contexts", "Matinee", "Matinee"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** Standard Actions */
	TSharedPtr<FUICommandInfo> AddKey;

	TSharedPtr<FUICommandInfo> Play;
	TSharedPtr<FUICommandInfo> PlayLoop;
	TSharedPtr<FUICommandInfo> Stop;
	TSharedPtr<FUICommandInfo> PlayReverse;
	TSharedPtr<FUICommandInfo> PlayPause;

	TSharedPtr<FUICommandInfo> CreateCameraActor;

	TSharedPtr<FUICommandInfo> ToggleSnap;
	TSharedPtr<FUICommandInfo> ToggleSnapTimeToFrames;
	TSharedPtr<FUICommandInfo> FixedTimeStepPlayback;

	TSharedPtr<FUICommandInfo> FitSequence;
	TSharedPtr<FUICommandInfo> FitViewToSelected;
	TSharedPtr<FUICommandInfo> FitLoop;
	TSharedPtr<FUICommandInfo> FitLoopSequence;
	TSharedPtr<FUICommandInfo> ViewEndofTrack;

	TSharedPtr<FUICommandInfo> ToggleGorePreview;

	TSharedPtr<FUICommandInfo> LaunchRecordWindow;
	TSharedPtr<FUICommandInfo> CreateMovie;

	TSharedPtr<FUICommandInfo> FileImport;
	TSharedPtr<FUICommandInfo> FileExport;
	TSharedPtr<FUICommandInfo> ExportSoundCueInfo;
	TSharedPtr<FUICommandInfo> ExportAnimInfo;
	TSharedPtr<FUICommandInfo> FileExportBakeTransforms;
	TSharedPtr<FUICommandInfo> FileExportKeepHierarchy;
	
	TSharedPtr<FUICommandInfo> DeleteSelectedKeys;
	TSharedPtr<FUICommandInfo> DuplicateKeys;
	TSharedPtr<FUICommandInfo> InsertSpace;
	TSharedPtr<FUICommandInfo> StretchSection;
	TSharedPtr<FUICommandInfo> StretchSelectedKeyFrames;
	TSharedPtr<FUICommandInfo> DeleteSection;
	TSharedPtr<FUICommandInfo> SelectInSection;
	TSharedPtr<FUICommandInfo> ReduceKeys;
	TSharedPtr<FUICommandInfo> SavePathTime;
	TSharedPtr<FUICommandInfo> JumpToPathTime;

	
	TSharedPtr<FUICommandInfo> Draw3DTrajectories;
	TSharedPtr<FUICommandInfo> ShowAll3DTrajectories;
	TSharedPtr<FUICommandInfo> HideAll3DTrajectories;
	TSharedPtr<FUICommandInfo> PreferFrameNumbers;
	TSharedPtr<FUICommandInfo> ShowTimeCursorPosForAllKeys;

	TSharedPtr<FUICommandInfo> ZoomToTimeCursorPosition;
	TSharedPtr<FUICommandInfo> ViewFrameStats;
	TSharedPtr<FUICommandInfo> EditingCrosshair;

	TSharedPtr<FUICommandInfo> EnableEditingGrid;
	TSharedPtr<FUICommandInfo> TogglePanInvert;
	TSharedPtr<FUICommandInfo> ToggleAllowKeyframeBarSelection;
	TSharedPtr<FUICommandInfo> ToggleAllowKeyframeTextSelection;
	TSharedPtr<FUICommandInfo> ToggleLockCameraPitch;

	//Misc Context Menu Commands
	TSharedPtr<FUICommandInfo> EditCut;
	TSharedPtr<FUICommandInfo> EditCopy;
	TSharedPtr<FUICommandInfo> EditPaste;

	//Tab Context menu
	TSharedPtr<FUICommandInfo> GroupDeleteTab;

	//Group Context menu
	TSharedPtr<FUICommandInfo> ActorSelectAll;
	TSharedPtr<FUICommandInfo> ActorAddAll;
	TSharedPtr<FUICommandInfo> ActorReplaceAll;
	TSharedPtr<FUICommandInfo> ActorRemoveAll;
	TSharedPtr<FUICommandInfo> ExportCameraAnim;
	TSharedPtr<FUICommandInfo> ExportAnimTrackFBX;
	TSharedPtr<FUICommandInfo> ExportAnimGroupFBX;
	TSharedPtr<FUICommandInfo> GroupDuplicate;
	TSharedPtr<FUICommandInfo> GroupDelete;
	TSharedPtr<FUICommandInfo> GroupCreateTab;
	TSharedPtr<FUICommandInfo> GroupRemoveFromTab;
	TSharedPtr<FUICommandInfo> RemoveFromGroupFolder;

	//Track Context Menu
	TSharedPtr<FUICommandInfo> TrackRename;
	TSharedPtr<FUICommandInfo> TrackDelete;
	TSharedPtr<FUICommandInfo> Show3DTrajectory;
	TSharedPtr<FUICommandInfo> TrackSplitTransAndRot;
	TSharedPtr<FUICommandInfo> TrackNormalizeVelocity;
	TSharedPtr<FUICommandInfo> ScaleTranslation;
	TSharedPtr<FUICommandInfo> ParticleReplayTrackContextStartRecording;
	TSharedPtr<FUICommandInfo> ParticleReplayTrackContextStopRecording;

	//Background Context Menu
	TSharedPtr<FUICommandInfo> NewFolder;
	TSharedPtr<FUICommandInfo> NewEmptyGroup;
	TSharedPtr<FUICommandInfo> NewCameraGroup;
	TSharedPtr<FUICommandInfo> NewParticleGroup;
	TSharedPtr<FUICommandInfo> NewSkeletalMeshGroup;
	TSharedPtr<FUICommandInfo> NewLightingGroup;
	TSharedPtr<FUICommandInfo> NewDirectorGroup;
	TSharedPtr<FUICommandInfo> ToggleDirectorTimeline;
	TSharedPtr<FUICommandInfo> ToggleCurveEditor;

	//Key Context Menu
	TSharedPtr<FUICommandInfo> KeyModeCurveAuto;
	TSharedPtr<FUICommandInfo> KeyModeCurveAutoClamped;
	TSharedPtr<FUICommandInfo> KeyModeCurveBreak;
	TSharedPtr<FUICommandInfo> KeyModeLinear;
	TSharedPtr<FUICommandInfo> KeyModeConstant;
	TSharedPtr<FUICommandInfo> KeySetTime;
	TSharedPtr<FUICommandInfo> KeySetValue;
	TSharedPtr<FUICommandInfo> KeySetBool; 
	TSharedPtr<FUICommandInfo> KeySetColor;
	TSharedPtr<FUICommandInfo> EventKeyRename;
	TSharedPtr<FUICommandInfo> DirKeySetTransitionTime;
	TSharedPtr<FUICommandInfo> DirKeyRenameCameraShot;
	TSharedPtr<FUICommandInfo> KeySetMasterVolume;
	TSharedPtr<FUICommandInfo> KeySetMasterPitch;
	TSharedPtr<FUICommandInfo> ToggleKeyFlip;
	TSharedPtr<FUICommandInfo> KeySetConditionAlways;
	TSharedPtr<FUICommandInfo> KeySetConditionGoreEnabled;
	TSharedPtr<FUICommandInfo> KeySetConditionGoreDisabled;
	TSharedPtr<FUICommandInfo> AnimKeyLoop;
	TSharedPtr<FUICommandInfo> AnimKeyNoLoop;
	TSharedPtr<FUICommandInfo> AnimKeySetStartOffset;
	TSharedPtr<FUICommandInfo> AnimKeySetEndOffset;
	TSharedPtr<FUICommandInfo> AnimKeySetPlayRate;
	TSharedPtr<FUICommandInfo> AnimKeyToggleReverse;
	TSharedPtr<FUICommandInfo> KeySyncGenericBrowserToSoundCue;
	TSharedPtr<FUICommandInfo> ParticleReplayKeySetClipIDNumber;
	TSharedPtr<FUICommandInfo> ParticleReplayKeySetDuration;
	TSharedPtr<FUICommandInfo> SoundKeySetVolume;
	TSharedPtr<FUICommandInfo> SoundKeySetPitch;
	TSharedPtr<FUICommandInfo> MoveKeySetLookup;
	TSharedPtr<FUICommandInfo> MoveKeyClearLookup;

	//Collapse/Expand context menu
	TSharedPtr<FUICommandInfo> ExpandAllGroups;
	TSharedPtr<FUICommandInfo> CollapseAllGroups;

	//Marker Context Menu
	TSharedPtr<FUICommandInfo> MarkerMoveToBeginning;
	TSharedPtr<FUICommandInfo> MarkerMoveToEnd;
	TSharedPtr<FUICommandInfo> MarkerMoveToEndOfLongestTrack;
	TSharedPtr<FUICommandInfo> MarkerMoveToEndOfSelectedTrack;
	TSharedPtr<FUICommandInfo> MarkerMoveToCurrentPosition;

	//Viewport/Key commands
	TSharedPtr<FUICommandInfo> ZoomIn;
	TSharedPtr<FUICommandInfo> ZoomOut;
	TSharedPtr<FUICommandInfo> ZoomInAlt;
	TSharedPtr<FUICommandInfo> ZoomOutAlt;
	TSharedPtr<FUICommandInfo> MarkInSection;
	TSharedPtr<FUICommandInfo> MarkOutSection;
	TSharedPtr<FUICommandInfo> IncrementPosition;
	TSharedPtr<FUICommandInfo> DecrementPosition;
	TSharedPtr<FUICommandInfo> MoveToNextKey;
	TSharedPtr<FUICommandInfo> MoveToPrevKey;
	TSharedPtr<FUICommandInfo> SplitAnimKey;
	TSharedPtr<FUICommandInfo> MoveActiveUp;
	TSharedPtr<FUICommandInfo> MoveActiveDown;
	TSharedPtr<FUICommandInfo> DuplicateSelectedKeys;
	TSharedPtr<FUICommandInfo> CropAnimationBeginning;
	TSharedPtr<FUICommandInfo> CropAnimationEnd;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeAuto;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeAutoClamped;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeUser;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeBreak;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeLinear;
	TSharedPtr<FUICommandInfo> ChangeKeyInterpModeConstant;
	TSharedPtr<FUICommandInfo> DeleteSelection;

	/** Action Types */
	struct EGroupAction
	{
		enum Type
		{
			NewFolder, 
			NewEmptyGroup,
			NewDirectorGroup, 
			NewLightingGroup,
			NewCameraGroup,
			NewParticleGroup,
			NewSkeletalMeshGroup,
			DuplicateGroup, 
			MoveGroupToActiveFolder,
			MoveActiveGroupToFolder,
			RemoveFromGroupFolder
		};
	};

	struct EKeyAction
	{
		enum Type
		{
			KeyModeCurveAuto,
			KeyModeCurveAutoClamped, 
			KeyModeCurveBreak,
			KeyModeLinear,
			KeyModeConstant,

			ConditionAlways,
			ConditionGoreEnabled,
			ConditionGoreDisabled
		};
	};


	/** Initialize commands */
	virtual void RegisterCommands() override;
};
