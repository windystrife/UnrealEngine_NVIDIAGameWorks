// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackParticleReplay.generated.h"

class FCanvas;
class UInterpGroup;
class UInterpTrackInst;

/**
 *	This track implements support for creating and playing back captured particle system data
 */

/** Data for a single key in this track */
USTRUCT()
struct FParticleReplayTrackKey
{
	GENERATED_USTRUCT_BODY()

	/** Position along timeline */
	UPROPERTY()
	float Time;

	/** Time length this clip should be captured/played for */
	UPROPERTY(EditAnywhere, Category=ParticleReplayTrackKey)
	float Duration;

	/** Replay clip ID number that identifies the clip we should capture to or playback from */
	UPROPERTY(EditAnywhere, Category=ParticleReplayTrackKey)
	int32 ClipIDNumber;


	FParticleReplayTrackKey()
		: Time(0)
		, Duration(0)
		, ClipIDNumber(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Particle Replay Track" ) )
class UInterpTrackParticleReplay : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of keys */
	UPROPERTY()
	TArray<struct FParticleReplayTrackKey> TrackKeys;

#if WITH_EDITORONLY_DATA
	/** True in the editor if track should be used to capture replay frames instead of play them back */
	UPROPERTY(transient)
	uint32 bIsCapturingReplay:1;

	/** Current replay fixed time quantum between frames (one over frame rate) */
	UPROPERTY(transient)
	float FixedTimeStep;

#endif // WITH_EDITORONLY_DATA

	//~ Begin UInterpTrack Interface.
	virtual int32 GetNumKeyframes() const override;
	virtual void GetTimeRange(float& StartTime, float& EndTime) const override;
	virtual float GetTrackEndTime() const override;
	virtual float GetKeyframeTime(int32 KeyIndex) const override;
	virtual int32 GetKeyframeIndex( float KeyTime ) const override;
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual int32 SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder=true) override;
	virtual void RemoveKeyframe(int32 KeyIndex) override;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) override;
	virtual bool GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
	virtual const FString	GetEdHelperClassName() const override;
	virtual const FString	GetSlateHelperClassName() const override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual bool AllowStaticActors() override { return true; }
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) override;
	//~ End UInterpTrack Interface.

	//~ Begin FInterpEdInputInterface Interface
	virtual void BeginDrag(FInterpEdInputData &InputData) override;
	virtual void EndDrag(FInterpEdInputData &InputData) override;
	virtual EMouseCursor::Type GetMouseCursor(FInterpEdInputData &InputData) override;
	virtual void ObjectDragged(FInterpEdInputData& InputData) override;
	//~ End FInterpEdInputInterface Interface

};



