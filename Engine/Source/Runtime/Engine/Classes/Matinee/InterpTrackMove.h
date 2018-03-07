// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Matinee/InterpTrack.h"
#include "InterpTrackMove.generated.h"

class FCanvas;
class FSceneView;
class UInterpGroup;
class UInterpTrackInst;
struct FPropertyChangedEvent;

UENUM()
enum EInterpTrackMoveRotMode
{
	/** Should take orientation from the keyframe. */
	IMR_Keyframed,
	/** Point the X-Axis of the controlled Actor at the group specified by LookAtGroupName. */
	IMR_LookAtGroup,
	/** Should look along the direction of the translation path, with Z always up. */
	// IMR_LookAlongPath // TODO!

	/** Do not change rotation. Ignore it. */
	IMR_Ignore,
	IMR_MAX,
};

/**
 * Array of group names to retrieve position and rotation data from instead of using the data stored in the keyframe.
 * A value of NAME_None means to use the PosTrack and EulerTrack data for the keyframe.
 * There needs to be the same amount of elements in this array as there are keyframes.
 */
USTRUCT()
struct FInterpLookupPoint
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName GroupName;

	UPROPERTY()
	float Time;


	FInterpLookupPoint()
		: Time(0)
	{
	}

};

USTRUCT()
struct FInterpLookupTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<struct FInterpLookupPoint> Points;



		/** Add a new keypoint to the LookupTrack.  Returns the index of the new key.*/
		int32 AddPoint( const float InTime, FName &InGroupName )
		{
			int32 PointIdx=0;

			for( PointIdx=0; PointIdx<Points.Num() && Points[PointIdx].Time < InTime; PointIdx++);

			Points.InsertUninitialized(PointIdx);
			Points[PointIdx].Time = InTime;
			Points[PointIdx].GroupName = InGroupName;

			return PointIdx;
		}

		/** Move a keypoint to a new In value. This may change the index of the keypoint, so the new key index is returned. */
		int32 MovePoint( int32 PointIndex, float NewTime )
		{
			if( PointIndex < 0 || PointIndex >= Points.Num() )
			{
				return PointIndex;
			}

			FName GroupName = Points[PointIndex].GroupName;

			Points.RemoveAt(PointIndex);

			const int32 NewPointIndex = AddPoint( NewTime, GroupName );

			return NewPointIndex;
		}
	
};

/** Track containing data for moving an actor around over time. */
UCLASS(MinimalAPI, meta=( DisplayName = "Movement Track" ) )
class UInterpTrackMove : public UInterpTrack
{
	GENERATED_UCLASS_BODY()
	
	/** Actual position keyframe data. */
	UPROPERTY(BlueprintReadOnly, Category=InterpTrackMove)
	FInterpCurveVector PosTrack;

	/** Actual rotation keyframe data, stored as Euler angles in degrees, for easy editing on curve. */
	UPROPERTY()
	FInterpCurveVector EulerTrack;

	UPROPERTY()
	struct FInterpLookupTrack LookupTrack;

	/** When using IMR_LookAtGroup, specifies the Group which this track should always point its actor at. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	FName LookAtGroupName;

	/** Controls the tightness of the curve for the translation path. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	float LinCurveTension;

	/** Controls the tightness of the curve for the rotation path. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	float AngCurveTension;

	/**
	 *	Use a Quaternion linear interpolation between keys.
	 *	This is robust and will find the 'shortest' distance between keys, but does not support ease in/out.
	 */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bUseQuatInterpolation:1;

	/** In the editor, show a small arrow at each keyframe indicating the rotation at that key. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bShowArrowAtKeys:1;

	/** Disable previewing of this track - will always position  AActor  at Time=0.0. Useful when keyframing an object relative to this group. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bDisableMovement:1;

	/** If false, when this track is displayed on the Curve Editor in Matinee, do not show the Translation tracks. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bShowTranslationOnCurveEd:1;

	/** If false, when this track is displayed on the Curve Editor in Matinee, do not show the Rotation tracks. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bShowRotationOnCurveEd:1;

	/** If true, 3D representation of this track in the 3D viewport is disabled. */
	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	uint32 bHide3DTrack:1;

	UPROPERTY(EditAnywhere, Category=InterpTrackMove)
	TEnumAsByte<enum EInterpTrackMoveRotMode> RotMode;


	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
#endif
	//~ End UObject Interface.

	//~ Begin UInterpTrack Interface.
	virtual int32 GetNumKeyframes() const override;
	virtual void GetTimeRange(float& StartTime, float& EndTime) const override;
	virtual float GetTrackEndTime() const override;
	virtual float GetKeyframeTime(int32 KeyIndex) const override;
	virtual int32 GetKeyframeIndex( float KeyTime ) const override;
	virtual int32 AddKeyframe(float Time, UInterpTrackInst* TrInst, EInterpCurveMode InitInterpMode) override;
	virtual int32 AddChildKeyframe(class UInterpTrack* ChildTrack, float Time, UInterpTrackInst* TrackInst, EInterpCurveMode InitInterpMode) override;
	virtual bool CanAddKeyframe( UInterpTrackInst* TrackInst ) override;
	virtual bool CanAddChildKeyframe( UInterpTrackInst* TrackInst ) override;
	virtual void UpdateKeyframe(int32 KeyIndex, UInterpTrackInst* TrInst) override;
	virtual void UpdateChildKeyframe( class UInterpTrack* ChildTrack, int32 KeyIndex, UInterpTrackInst* TrackInst ) override;
	virtual int32 SetKeyframeTime(int32 KeyIndex, float NewKeyTime, bool bUpdateOrder=true) override;
	virtual void RemoveKeyframe(int32 KeyIndex) override;
	virtual int32 DuplicateKeyframe(int32 KeyIndex, float NewKeyTime, UInterpTrack* ToTrack = NULL) override;
	virtual bool GetClosestSnapPosition(float InPosition, TArray<int32> &IgnoreKeys, float& OutPosition) override;
	virtual void ConditionalPreviewUpdateTrack(float NewPosition, class UInterpTrackInst* TrInst) override;
	virtual void PreviewUpdateTrack(float NewPosition, UInterpTrackInst* TrInst) override;
	virtual void UpdateTrack(float NewPosition, UInterpTrackInst* TrInst, bool bJump) override;
#if WITH_EDITORONLY_DATA
	virtual class UTexture2D* GetTrackIcon() const override;
#endif // WITH_EDITORONLY_DATA
	virtual FColor GetKeyframeColor(int32 KeyIndex) const override;
	virtual void DrawTrack( FCanvas* Canvas, UInterpGroup* Group, const FInterpTrackDrawParams& Params ) override;
	virtual void Render3DTrack(UInterpTrackInst* TrInst, const FSceneView* View, class FPrimitiveDrawInterface* PDI, int32 TrackIndex, const FColor& TrackColor, TArray<struct FInterpEdSelKey>& SelectedKeys) override;
	virtual void SetTrackToSensibleDefault() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UInterpTrack Interface.

	//~ Begin FCurveEdInterface Interface.
	virtual int32		GetNumKeys() const override;
	virtual int32		GetNumSubCurves() const override;
	virtual FColor	GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const override;
	virtual float	GetKeyIn(int32 KeyIndex) override;
	virtual float	GetKeyOut(int32 SubIndex, int32 KeyIndex) override;
	virtual void	GetInRange(float& MinIn, float& MaxIn) const override;
	virtual void	GetOutRange(float& MinOut, float& MaxOut) const override;
	virtual FColor	GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor) override;
	virtual EInterpCurveMode	GetKeyInterpMode(int32 KeyIndex) const override;
	virtual void	GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const override;
	virtual float	EvalSub(int32 SubIndex, float InVal) override;
	virtual int32		CreateNewKey(float KeyIn) override;
	virtual void	DeleteKey(int32 KeyIndex) override;
	virtual int32		SetKeyIn(int32 KeyIndex, float NewInVal) override;
	virtual void	SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) override;
	virtual void	SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) override;
	virtual void	SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent) override;
	//~ End FCurveEdInterface Interface.


	/**
	 * @param KeyIndex	Index of the key to retrieve the lookup group name for.
	 *
	 * @return Returns the groupname for the keyindex specified.
	 */
	virtual FName GetLookupKeyGroupName(int32 KeyIndex);

	/**
	 * Sets the lookup group name for a movement track keyframe.
	 *
	 * @param KeyIndex			Index of the key to modify.
	 * @param NewGroupName		Group name to set the keyframe's lookup group to.
	 */
	virtual void SetLookupKeyGroupName(int32 KeyIndex, const FName &NewGroupName);
	
	/**
	 * Clears the lookup group name for a movement track keyframe.
	 *
	 * @param KeyIndex			Index of the key to modify.
	 */
	virtual void ClearLookupKeyGroupName(int32 KeyIndex);

	/**
	 * Replacement for the PosTrack eval function that uses GetKeyframePosition.  This is so we can replace keyframes that get their information from other tracks.
	 *
	 * @param TrInst	TrackInst to use for looking up groups.
	 * @param Time		Time to evaluate position at.
	 * @return			Final position at the specified time.
	 */
	ENGINE_API FVector EvalPositionAtTime(UInterpTrackInst* TrInst, float Time);

	/**
	 * Replacement for the EulerTrack eval function that uses GetKeyframeRotation.  This is so we can replace keyframes that get their information from other tracks.
	 *
	 * @param TrInst	TrackInst to use for looking up groups.
	 * @param Time		Time to evaluate rotation at.
	 * @return			Final rotation at the specified time.
	 */
	FVector EvalRotationAtTime(UInterpTrackInst* TrInst, float Time);

	/**
	 * Get the position of a keyframe given its key index.  Also optionally retrieves the Arrive and Leave tangents for the key.
	 * This function respects the LookupTrack.
	 *
	 * @param TrInst			TrackInst to use for lookup track positions.
	 * @param KeyIndex			Index of the keyframe to get the position of.
	 * @param OutTime           Final time of the keyframe.
	 * @param OutPos			Final position of the keyframe.
	 * @param OutArriveTangent	Pointer to a FVector to store the arrive tangent in, can be NULL.
	 * @param OutLeaveTangent	Pointer to a FVector to store the leave tangent in, can be NULL.
	 */
	void GetKeyframePosition(UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, FVector &OutPos, FVector *OutArriveTangent, FVector *OutLeaveTangent);

	/**
	 * Get the rotation of a keyframe given its key index.  Also optionally retrieves the Arrive and Leave tangents for the key.
	 * This function respects the LookupTrack.
	 *
	 * @param TrInst			TrackInst to use for lookup track rotations.
	 * @param KeyIndex			Index of the keyframe to get the rotation of.
	 * @param OutTime           Final time of the keyframe.
	 * @param OutRot			Final rotation of the keyframe.
	 * @param OutArriveTangent	Pointer to a FVector to store the arrive tangent in, can be NULL.
	 * @param OutLeaveTangent	Pointer to a FVector to store the leave tangent in, can be NULL.
	 */
	void GetKeyframeRotation(UInterpTrackInst* TrInst, int32 KeyIndex, float& OutTime, FVector &OutRot, FVector *OutArriveTangent, FVector *OutLeaveTangent);

	/**
	 * Compute the world space coordinates for a key; handles keys that use IMF_RelativeToInitial, basing, etc.
	 *
	 * @param MoveTrackInst		An instance of this movement track
	 * @param RelativeSpacePos	Key position value from curve
	 * @param RelativeSpaceRot	Key rotation value from curve
	 * @param OutPos			Output world space position
	 * @param OutRot			Output world space rotation
	 */
	ENGINE_API void ComputeWorldSpaceKeyTransform( class UInterpTrackInstMove* MoveTrackInst,
										const FVector& RelativeSpacePos,
										const FRotator& RelativeSpaceRot,
										FVector& OutPos,
										FRotator& OutRot );

	/** Get the keyed relative transform at the specified time - this does not include any special rotation mode support. Returns zero vector/rotator if no track keys. */
	virtual void GetKeyTransformAtTime(UInterpTrackInst* TrInst, float Time, FVector& OutPos, FRotator& OutRot);

	/** Calculate the world-space location/rotation at the specified time, including. Includes any special rotation mode support. leave OutPos and OutRot unchanged if no track keys */
	virtual bool GetLocationAtTime(UInterpTrackInst* TrInst, float Time, FVector& OutPos, FRotator& OutRot);

	/** 
	 *	Return the reference frame that the animation is currently working within.
	 *	Looks at the current MoveFrame setting and whether the Actor is based on something.
	 */
	virtual FTransform GetMoveRefFrame(class UInterpTrackInstMove* MoveTrackInst);

	/** Calculate the world space rotation needed to look at the current LookAtGroupName target. Returns (0,0,0) if LookAtGroupName is None.  */
	virtual FRotator GetLookAtRotation(UInterpTrackInst* TrInst);

	/**
	 * Find Best Matching Time From Position
	 * This function simply try to find Time from input Position using simple Lerp
	 *
	 * @param : Pos  - input position
	 * @param : StartKeyIndex - optional
	 *
	 * @return : Interp Time
	 */
	float FindBestMatchingTimefromPosition(UInterpTrackInst* TrInst, const FVector& Pos, int32 StartKeyIndex=0, EAxisList::Type WeightAxis = EAxisList::XY);

	int32 CalcSubIndex(bool bPos, int32 InIndex) const;

	/**
	 * Create and adds subtracks to this track
	 *
	 * @param bCopy	If subtracks are being added as a result of a copy
	 */
	virtual void CreateSubTracks( bool bCopy ) override;

	/**
	 * Split this movment track in to seperate tracks for translation and rotation
	 */
	ENGINE_API void SplitTranslationAndRotation();

	/**
	 * Reduce Keys within Tolerance
	 *
	 * @param bIntervalStart	start of the key to reduce
	 * @param bIntervalEnd		end of the key to reduce
	 * @param Tolerance			tolerance
	 */
	virtual void ReduceKeys( float IntervalStart, float IntervalEnd, float Tolerance ) override;
};



