// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EditorModeTools.h"
#include "EdMode.h"

class FCanvas;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FSequencer;
class FViewport;
struct HMovieSceneKeyProxy;
class UMovieScene3DTransformSection;
class UMovieScene3DTransformTrack;
struct FMovieSceneEvaluationTrack;
struct FMovieSceneInterrogationData;

/** Stores the transform track and associated mesh trail for each drawn track */
struct FMeshTrailData
{
	class UMovieScene3DTransformTrack* Track;
	class ASequencerMeshTrail* Trail;

	FMeshTrailData(class UMovieScene3DTransformTrack* InTrack, class ASequencerMeshTrail* InTrail) :
		Track(InTrack),
		Trail(InTrail)
	{
	}
};

/**
 * FSequencerEdMode is the editor mode for additional drawing and handling sequencer hotkeys in the editor
 */
class FSequencerEdMode : public FEdMode
{
public:
	static const FEditorModeID EM_SequencerMode;

public:
	FSequencerEdMode();
	virtual ~FSequencerEdMode();

	/* FEdMode interface */
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas) override;
	virtual bool UsesTransformWidget() const override { return false; }
	virtual bool UsesTransformWidget(FWidget::EWidgetMode CheckMode) const override { return false; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	void AddSequencer(TWeakPtr<FSequencer> InSequencer) { Sequencers.AddUnique(InSequencer); }
	void RemoveSequencer(TWeakPtr<FSequencer> InSequencer) { Sequencers.Remove(InSequencer); }

	void OnSequencerReceivedFocus(TWeakPtr<FSequencer> InSequencer) { Sequencers.Sort([=](TWeakPtr<FSequencer> A, TWeakPtr<FSequencer> B){ return A == InSequencer; }); }

	void OnKeySelected(FViewport* Viewport, HMovieSceneKeyProxy* KeyProxy);

	/** Draw a single mesh transform track, given a key that is on that track */
	void DrawMeshTransformTrailFromKey(const class ASequencerKeyActor* KeyActor);

	/** Clean up any mesh trails and their associated key actors */
	void CleanUpMeshTrails();

protected:
	void DrawTracks3D(FPrimitiveDrawInterface* PDI);
	void DrawTransformTrack(const TSharedPtr<FSequencer>& Sequencer, FPrimitiveDrawInterface* PDI, UMovieScene3DTransformTrack* TransformTrack, const TArray<TWeakObjectPtr<UObject>>& BoundObjects, const bool bIsSelected);

protected:
	static void GetLocationAtTime(const FMovieSceneEvaluationTrack* Track, UObject* BoundObject, float KeyTime, FVector& KeyPos, FRotator& KeyRot, const TSharedPtr<FSequencer>& Sequencer);
	static FTransform GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const UObject* InObject, float KeyTime);
	static FTransform GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const AActor* Actor, float KeyTime);
	static FTransform GetRefFrame(const TSharedPtr<FSequencer>& Sequencer, const USceneComponent* SceneComponent, float KeyTime);

private:
	TArray<TWeakPtr<FSequencer>> Sequencers;

	/**Array of the transform tracks and their associated mesh trails */
	TArray<FMeshTrailData> MeshTrails;

	/** Interrogation data for extracting transforms */
	TSharedPtr<FMovieSceneInterrogationData> InterrogationData;

	/** If true, draw mesh trails instead of debug lines*/
	bool bDrawMeshTrails;
};

/**
 * FSequencerEdMode is the editor mode tool for additional drawing and handling sequencer hotkeys in the editor
 */
class FSequencerEdModeTool : public FModeTool
{
public:
	FSequencerEdModeTool(FSequencerEdMode* InSequencerEdMode);
	virtual ~FSequencerEdModeTool();

	virtual FString GetName() const override { return TEXT("Sequencer Edit"); }

	/**
	 * @return		true if the key was handled by this editor mode tool.
	 */
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;

private:
	FSequencerEdMode* SequencerEdMode;
};
