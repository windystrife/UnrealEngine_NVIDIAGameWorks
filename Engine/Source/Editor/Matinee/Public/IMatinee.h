// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "MatineeGroupData.h"
#include "MatineeTrackData.h"

class AMatineeActor;
class FCameraControllerConfig;
class FCanvas;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class UInterpData;

/*-----------------------------------------------------------------------------
   IMatineeBase.  Base class for matinee
  -----------------------------------------------------------------------------*/
class IMatineeBase
{
public:
	virtual AMatineeActor* GetMatineeActor() = 0;
	virtual UInterpData* GetInterpData() = 0;

	virtual void ActorModified( bool bUpdateViewportTransform = true ) = 0;
	virtual void ActorSelectionChange( const bool bClearSelectionIfInvalid = true ) = 0;
	virtual void CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation) = 0;
	virtual bool ProcessKeyPress(FKey Key, bool bCtrlDown, bool bAltDown) = 0;

	virtual void AddKey() = 0;

	virtual bool KeyIsInSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex) = 0;
	virtual void AddKeyToSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex, bool bAutoWind) = 0;
	virtual void RemoveKeyFromSelection(class UInterpGroup* InGroup, UInterpTrack* InTrack, int32 InKeyIndex) = 0;
	virtual void ClearKeySelection() = 0;

	virtual void DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI) = 0;
	virtual void DrawModeHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas) = 0;

	virtual void BeginDrag3DHandle(UInterpGroup* Group, int32 TrackIndex) = 0;
	virtual void Move3DHandle(UInterpGroup* Group, int32 TrackIndex, int32 KeyIndex, bool bArriving, const FVector& Delta) = 0;
	virtual void EndDrag3DHandle() = 0;
	virtual void MoveInitialPosition(const FVector& Delta, const FRotator& DeltaRot) = 0;

	virtual void SelectTrack( UInterpGroup* Group, UInterpTrack* TrackToSelect, bool bDeselectPreviousTracks = true ) = 0;

	virtual int32 GetSelectedGroupCount() const = 0;
	virtual int32 GetSelectedTrackCount() const = 0;
	virtual FSelectedGroupIterator GetSelectedGroupIterator() = 0;
	virtual FSelectedGroupConstIterator GetSelectedGroupIterator() const = 0;
	virtual FSelectedTrackIterator GetSelectedTrackIterator() = 0;	
	virtual FSelectedTrackConstIterator GetSelectedTrackIterator() const = 0;

	virtual void StartPlaying( bool bPlayLoop, bool bPlayForward ) = 0;
	virtual void StopPlaying() = 0;
	virtual void ResumePlaying() = 0;

	virtual bool Hide3DTrackView() = 0;
	virtual void ToggleRecordMenuDisplay() = 0;
	virtual void ToggleRecordInterpValues() = 0;
	virtual bool IsRecordingInterpValues() const = 0;
	virtual void ResetRecordingMenuValue(FEditorViewportClient* InClient) = 0;
	virtual bool IsRecordMenuChangeAllowedRepeat() const = 0;
	virtual void SetRecordMode(const uint32 InNewMode) = 0;
	virtual void ChangeRecordingMenu(const bool bInNext) = 0;
	virtual void ChangeRecordingMenuValue(FEditorViewportClient* InClient, const bool bInIncrease) = 0;

	virtual int32 GetCameraMovementScheme() const = 0;
	virtual int32 GetNumRecordRollSmoothingSamples() const = 0;
	virtual int32 GetNumRecordPitchSmoothingSamples() const = 0;
	virtual void LoadRecordingSettings(FCameraControllerConfig& InCameraConfig) = 0;

	virtual void InvalidateTrackWindowViewports() = 0;
	virtual bool Show(bool bShow) = 0;
	virtual void Close(bool bForce) = 0;

	//Slate exclusive
	virtual void FinishAddKey(UInterpTrack *Track, bool bCommitKeys) {check(false);}
};


/*-----------------------------------------------------------------------------
   IMatinee
  -----------------------------------------------------------------------------*/

class IMatinee : public IMatineeBase, public FAssetEditorToolkit
{
public:

	bool Show(bool bShow) override {return false; }
	void Close(bool bForce) override { CloseWindow(); }
};
	
