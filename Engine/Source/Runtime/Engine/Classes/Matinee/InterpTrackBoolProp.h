// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackBoolProp.generated.h"

class UInterpTrackInst;

/** Information for one event in the track. */
USTRUCT()
struct FBoolTrackKey
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float Time;

	UPROPERTY(EditAnywhere, Category=BoolTrackKey)
	uint32 Value:1;


	FBoolTrackKey()
		: Time(0)
		, Value(false)
	{
	}

};

UCLASS(MinimalAPI, meta=( DisplayName = "Bool Property Track" ) )
class UInterpTrackBoolProp : public UInterpTrack
{
	GENERATED_UCLASS_BODY()

	/** Array of booleans to set. */
	UPROPERTY()
	TArray<struct FBoolTrackKey> BoolTrack;

	/** Name of property in Group  AActor  which this track will modify over time. */
	UPROPERTY(Category=InterpTrackBoolProp, VisibleAnywhere)
	FName PropertyName;


	//~ Begin UInterpTrack Interface
	virtual int32 GetNumKeyframes() const override;
	virtual float GetTrackEndTime() const override;
	virtual float GetKeyframeTime( int32 KeyIndex ) const override;
	virtual int32 GetKeyframeIndex( float KeyTime ) const override;
	virtual void GetTimeRange( float& StartTime, float& EndTime ) const override;
	virtual int32 SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder = true ) override;
	virtual void RemoveKeyframe( int32 KeyIndex ) override;
	virtual int32 DuplicateKeyframe( int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL ) override;
	virtual bool GetClosestSnapPosition( float InPosition, TArray<int32>& IgnoreKeys, float& OutPosition ) override;
	virtual int32 AddKeyframe( float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode ) override;
	virtual bool CanAddKeyframe( UInterpTrackInst* TrackInst ) override;
	virtual void UpdateKeyframe( int32 KeyIndex, UInterpTrackInst* TrackInst ) override;
	virtual void PreviewUpdateTrack( float NewPosition, UInterpTrackInst* TrackInst ) override;
	virtual void UpdateTrack( float NewPosition, UInterpTrackInst* TrackInst, bool bJump ) override;
	virtual bool AllowStaticActors() override { return true; }
	virtual const FString GetEdHelperClassName() const override;
	virtual const FString GetSlateHelperClassName() const override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	//~ End UInterpTrack Interface
};



