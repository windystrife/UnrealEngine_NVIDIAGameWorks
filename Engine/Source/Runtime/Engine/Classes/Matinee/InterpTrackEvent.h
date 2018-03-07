// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackEvent.generated.h"

class FCanvas;
class UInterpGroup;
class UInterpTrackInst;

/**
 *	A track containing discrete events that are triggered as its played back. 
 *	Events correspond to Outputs of the SeqAct_Interp in Kismet.
 *	There is no PreviewUpdateTrack function for this type - events are not triggered in editor.
 */


/** Information for one event in the track. */
USTRUCT()
struct FEventTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=EventTrackKey)
	FName EventName;


	FEventTrackKey()
		: Time(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Event Track" ) )
class UInterpTrackEvent : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FEventTrackKey> EventTrack;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bFireEventsWhenJumpingForwards:1;

	/** If checked each key's event name is the exact name of the custom event function in level script that will be called */
	UPROPERTY(EditAnywhere, Category=InterpTrackEvent)
	uint32 bUseCustomEventName:1;

	//~ Begin UInterpTrack Interface
	virtual int32 GetNumKeyframes() const override;
	virtual void GetTimeRange( float& StartTime, float& EndTime ) const override;
	virtual float GetTrackEndTime() const override;
	virtual float GetKeyframeTime( int32 KeyIndex ) const override;
	virtual int32 GetKeyframeIndex( float KeyTime ) const override;
	virtual int32 AddKeyframe( float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) override;
	virtual int32 SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder = true ) override;
	virtual void RemoveKeyframe( int32 KeyIndex ) override;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) override;
	virtual bool GetClosestSnapPosition( float InPosition, TArray<int32>& IgnoreKeys, float& OutPosition ) override;
	virtual void PreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack( float NewPosition, UInterpTrackInst* TrackInst, bool bJump ) override;
	virtual const FString GetEdHelperClassName() const override;
	virtual const FString GetSlateHelperClassName() const override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual bool AllowStaticActors() override { return true; }
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) override;
	//~ End UInterpTrack Interface
};



