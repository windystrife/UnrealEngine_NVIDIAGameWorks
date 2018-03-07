// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackVisibility.generated.h"

class AActor;
class FCanvas;
class UInterpGroup;
class UInterpTrackInst;

/**
 *	This track implements support for setting or toggling the visibility of the associated actor
 */



/** Visibility track actions */
UENUM()
enum EVisibilityTrackAction
{
	/** Hides the object */
	EVTA_Hide,
	/** Shows the object */
	EVTA_Show,
	/** Toggles visibility of the object */
	EVTA_Toggle,
	EVTA_MAX,
};

/** Required condition for firing this event */
UENUM()
enum EVisibilityTrackCondition
{
	/** Always play this event */
	EVTC_Always,
	/** Only play this event when extreme content (gore) is enabled */
	EVTC_GoreEnabled,
	/** Only play this event when extreme content (gore) is disabled */
	EVTC_GoreDisabled,
	EVTC_MAX,
};

/** Information for one toggle in the track. */
USTRUCT()
struct FVisibilityTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=VisibilityTrackKey)
	TEnumAsByte<enum EVisibilityTrackAction> Action;

	/** Condition that must be satisfied for this key event to fire */
	UPROPERTY()
	TEnumAsByte<enum EVisibilityTrackCondition> ActiveCondition;


	FVisibilityTrackKey()
		: Time(0)
		, Action(0)
		, ActiveCondition(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Visibility Track" ) )
class UInterpTrackVisibility : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FVisibilityTrackKey> VisibilityTrack;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackVisibility)
	uint32 bFireEventsWhenJumpingForwards:1;


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

	/** Shows or hides the actor */
	void HideActor( AActor* Actor, bool bHidden );
};



