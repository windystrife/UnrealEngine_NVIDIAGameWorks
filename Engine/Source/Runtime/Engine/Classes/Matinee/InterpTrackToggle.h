// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackToggle.generated.h"

class FCanvas;
class UInterpGroup;
class UInterpTrackInst;

/**
 *	A track containing toggle actions that are triggered as its played back. 
 */




/** Enumeration indicating toggle action	*/
UENUM()
enum ETrackToggleAction
{
	ETTA_Off,
	ETTA_On,
	ETTA_Toggle,
	ETTA_Trigger,
	ETTA_MAX,
};

/** Information for one toggle in the track. */
USTRUCT()
struct FToggleTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=ToggleTrackKey)
	TEnumAsByte<enum ETrackToggleAction> ToggleAction;


	FToggleTrackKey()
		: Time(0)
		, ToggleAction(0)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Toggle Track" ) )
class UInterpTrackToggle : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of events to fire off. */
	UPROPERTY()
	TArray<struct FToggleTrackKey> ToggleTrack;

	/** 
	 *	If true, the track will call ActivateSystem on the emitter each update (the old 'incorrect' behavior).
	 *	If false (the default), the System will only be activated if it was previously inactive.
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bActivateSystemEachUpdate:1;

	/** 
	 *	If true, the track will activate the system w/ the 'Just Attached' flag.
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bActivateWithJustAttachedFlag:1;

	/** If events should be fired when passed playing the sequence forwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bFireEventsWhenForwards:1;

	/** If events should be fired when passed playing the sequence backwards. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
	uint32 bFireEventsWhenBackwards:1;

	/** If true, events on this track are fired even when jumping forwads through a sequence - for example, skipping a cinematic. */
	UPROPERTY(EditAnywhere, Category=InterpTrackToggle)
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
	//~ Begin UInterpTrack Interface.
};



