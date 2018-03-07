// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "InterpTrackMoveAxis.generated.h"

class UInterpTrackInst;

/**
  *
  * Subtrack for UInterpTrackMove
  * Transforms an interp actor on one axis
  */

/** List of axies this track can use */
UENUM()
enum EInterpMoveAxis
{
	AXIS_TranslationX,
	AXIS_TranslationY,
	AXIS_TranslationZ,
	AXIS_RotationX,
	AXIS_RotationY,
	AXIS_RotationZ,
};

UCLASS(MinimalAPI, meta=( DisplayName = "Move Axis Track" ) )
class UInterpTrackMoveAxis : public UInterpTrackFloatBase
{
	GENERATED_UCLASS_BODY()

	/** The axis which this track will use when transforming an actor */
	UPROPERTY()
	TEnumAsByte<enum EInterpMoveAxis> MoveAxis;

	/** Lookup track to use when looking at different groups for transform information*/
	UPROPERTY()
	struct FInterpLookupTrack LookupTrack;


	//~ Begin UInterpTrack Interface
	virtual int32 AddKeyframe( float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode ) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual int32 SetKeyframeTime( int32 KeyIndex, float NewKeyTime, bool bUpdateOrder ) override;
	virtual void RemoveKeyframe( int32 KeyIndex ) override;
	virtual int32 DuplicateKeyframe( int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL ) override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual void ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance ) override;
	//~ End UInterpTrack Interface
	


	//~ Begin FCurveEdInterface Interface.
	virtual FColor GetSubCurveButtonColor( int32 SubCurveIndex, bool bIsSubCurveHidden ) const override;
	virtual int32 CreateNewKey( float KeyIn ) override;
	virtual void DeleteKey( int32 KeyIndex ) override;
	virtual int32 SetKeyIn( int32 KeyIndex, float NewInVal ) override;
	virtual FColor GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor) override;
	//~ End FCurveEdInterface Interface.

	/** @todo document */
	void GetKeyframeValue( UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, float &OutValue, float* OutArriveTangent, float* OutLeaveTangent );

	/** @todo document */
	float EvalValueAtTime( UInterpTrackInst* TrInst, float Time );

	/**
	 * @param KeyIndex	Index of the key to retrieve the lookup group name for.
	 *
	 * @return Returns the groupname for the keyindex specified.
	 */
	FName GetLookupKeyGroupName( int32 KeyIndex );

	/**
	 * Sets the lookup group name for a movement track keyframe.
	 *
	 * @param KeyIndex			Index of the key to modify.
	 * @param NewGroupName		Group name to set the keyframe's lookup group to.
	 */
	ENGINE_API void SetLookupKeyGroupName( int32 KeyIndex, const FName& NewGroupName );

	/**
	 * Clears the lookup group name for a movement track keyframe.
	 *
	 * @param KeyIndex			Index of the key to modify.
	 */
	ENGINE_API void ClearLookupKeyGroupName( int32 KeyIndex );

};



