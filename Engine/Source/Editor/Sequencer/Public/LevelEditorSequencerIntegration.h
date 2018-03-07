// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AcquiredResources.h"
#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Layout/Visibility.h"
#include "ISceneOutlinerColumn.h"
#include "UObject/ObjectKey.h"
#include "ISequencer.h"

class AActor;
class FExtender;
class FMenuBuilder;
class FSequencer;
class FUICommandList;
class ILevelViewport;
class ISequencer;
class SViewportTransportControls;
class ULevel;
struct FPropertyAndParent;


struct FLevelEditorSequencerIntegrationOptions
{
	FLevelEditorSequencerIntegrationOptions()
		: bRequiresLevelEvents(true)
		, bRequiresActorEvents(false)
		, bCanRecord(false)
	{}

	bool bRequiresLevelEvents : 1;
	bool bRequiresActorEvents : 1;
	bool bCanRecord : 1;
};


class FLevelEditorSequencerBindingData : public TSharedFromThis<FLevelEditorSequencerBindingData>
{
public:
	FLevelEditorSequencerBindingData() 
		: bActorBindingsDirty(true)
		, bPropertyBindingsDirty(true)
	{}

	DECLARE_MULTICAST_DELEGATE(FActorBindingsDataChanged);
	DECLARE_MULTICAST_DELEGATE(FPropertyBindingsDataChanged);

	FActorBindingsDataChanged& OnActorBindingsDataChanged() { return ActorBindingsDataChanged; }
	FPropertyBindingsDataChanged& OnPropertyBindingsDataChanged() { return PropertyBindingsDataChanged; }

	FString GetLevelSequencesForActor(TWeakPtr<FSequencer> Sequencer, const AActor*);
	bool GetIsPropertyBound(TWeakPtr<FSequencer> Sequencer, const struct FPropertyAndParent&);

	bool bActorBindingsDirty;
	bool bPropertyBindingsDirty;

private:
	void UpdateActorBindingsData(TWeakPtr<FSequencer> InSequencer);
	void UpdatePropertyBindingsData(TWeakPtr<FSequencer> InSequencer);

	TMap< FObjectKey, FString > ActorBindingsMap;
	TMap< FObjectKey, TArray<FString> > PropertyBindingsMap;

	FActorBindingsDataChanged ActorBindingsDataChanged;
	FPropertyBindingsDataChanged PropertyBindingsDataChanged;
};


class SEQUENCER_API FLevelEditorSequencerIntegration
{
public:

	static FLevelEditorSequencerIntegration& Get();

	void Initialize();

	void AddSequencer(TSharedRef<ISequencer> InSequencer, const FLevelEditorSequencerIntegrationOptions& Options);

	void OnSequencerReceivedFocus(TSharedRef<ISequencer> InSequencer);

	void RemoveSequencer(TSharedRef<ISequencer> InSequencer);

	void SetViewportTransportControlsVisibility(bool bVisible);

	bool GetViewportTransportControlsVisibility() const;

private:

	/** Called before the world is going to be saved. The sequencer puts everything back to its initial state. */
	void OnPreSaveWorld(uint32 SaveFlags, UWorld* World);

	/** Called after the world has been saved. The sequencer updates to the animated state. */
	void OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess);

	/** Called after a level has been added */
	void OnLevelAdded(ULevel* InLevel, UWorld* InWorld);

	/** Called after a level has been removed */
	void OnLevelRemoved(ULevel* InLevel, UWorld* InWorld);

	/** Called after a new level has been created. The sequencer editor mode needs to be enabled. */
	void OnNewCurrentLevel();

	/** Called after a map has been opened. The sequencer editor mode needs to be enabled. */
	void OnMapOpened(const FString& Filename, bool bLoadAsTemplate);

	/** Called when new actors are dropped in the viewport. */
	void OnNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors);

	/** Called when viewport tab content changes. */
	void OnTabContentChanged();

	/** Called before a PIE session begins. */
	void OnPreBeginPIE(bool bIsSimulating);

	/** Called after a PIE session ends. */
	void OnEndPIE(bool bIsSimulating);

	/** Called after PIE session ends and maps have been cleaned up */
	void OnEndPlayMap();

	/** Handles the actor selection changing externally .*/
	void OnActorSelectionChanged( UObject* );

	/** Called via UEditorEngine::GetActorRecordingStateEvent to check to see whether we need to record actor state */
	void GetActorRecordingState( bool& bIsRecording ) const;

	/** Called when an actor label has changed */
	void OnActorLabelChanged(AActor* ChangedActor);

	/** Called when sequencer has been evaluated */
	void OnSequencerEvaluated();

	/** Called when bindings have changed */
	void OnMovieSceneBindingsChanged();

	/** Called when data has changed */
	void OnMovieSceneDataChanged(EMovieSceneDataChangeType DataChangeType);

	/** Called when allow edits mode has changed */
	void OnAllowEditsModeChanged(EAllowEditsMode AllowEditsMode);

	/** Called when the user begins scrubbing */
	void OnBeginScrubbing();

	/** Called when the user stops scrubbing */
	void OnEndScrubbing();

	void OnPropertyEditorOpened();

	TSharedRef<FExtender> GetLevelViewportExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors);

	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);

	void RecordSelectedActors();

	EVisibility GetTransportControlVisibility(TSharedPtr<ILevelViewport> LevelViewport) const;
	
	/** Create a menu entry we can use to toggle the transport controls */
	void CreateTransportToggleMenuEntry(FMenuBuilder& MenuBuilder);

	bool IsPropertyReadOnly(const FPropertyAndParent& InPropertyAndParent);

private:

	void ActivateSequencerEditorMode();
	void AddLevelViewportMenuExtender();
	void ActivateDetailHandler();
	void AttachTransportControlsToViewports();
	void DetachTransportControlsFromViewports();
	void AttachOutlinerColumn();
	void DetachOutlinerColumn();
	void ActivateRealtimeViewports();
	void RestoreRealtimeViewports();
	void BindLevelEditorCommands();

	struct FSequencerAndOptions
	{
		TWeakPtr<FSequencer> Sequencer;
		FLevelEditorSequencerIntegrationOptions Options;
		FAcquiredResources AcquiredResources;
		TSharedRef<FLevelEditorSequencerBindingData> BindingData;
	};
	TArray<FSequencerAndOptions> BoundSequencers;

	TSharedRef< ISceneOutlinerColumn > CreateSequencerInfoColumn( ISceneOutliner& SceneOutliner ) const;

private:

	void IterateAllSequencers(TFunctionRef<void(FSequencer&, const FLevelEditorSequencerIntegrationOptions& Options)>) const;
	void UpdateDetails(bool bForceRefresh = false);

	FLevelEditorSequencerIntegration();

private:

	friend SViewportTransportControls;
	
	/** A map of all the transport controls to viewports that this sequencer has made */
	struct FTransportControl
	{
		TWeakPtr<ILevelViewport> Viewport;
		TSharedPtr<SViewportTransportControls> Widget;
	};
	TArray<FTransportControl> TransportControls;

	FAcquiredResources AcquiredResources;

	TSharedPtr<class FDetailKeyframeHandlerWrapper> KeyFrameHandler;

	bool bScrubbing;
};
