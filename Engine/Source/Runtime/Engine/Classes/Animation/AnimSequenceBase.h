// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Abstract base class of animation sequence that can be played and evaluated to produce a pose.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "AnimSequenceBase.generated.h"

UENUM()
enum ETypeAdvanceAnim
{
	ETAA_Default,
	ETAA_Finished,
	ETAA_Looped
};

UCLASS(abstract, MinimalAPI, BlueprintType)
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

	/** Animation notifies, sorted by time (earliest notification first). */
	UPROPERTY()
	TArray<struct FAnimNotifyEvent> Notifies;

	/** Length (in seconds) of this AnimSequence if played back with a speed of 1.0. */
	UPROPERTY(Category=Length, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float SequenceLength;

	/** Number for tweaking playback rate of this animation globally. */
	UPROPERTY(EditAnywhere, Category=Animation)
	float RateScale;
	
	/**
	 * Raw uncompressed float curve data 
	 */
	UPROPERTY()
	struct FRawCurveTracks RawCurveData;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	UPROPERTY()
	TArray<FAnimNotifyTrack> AnimNotifyTracks;
#endif // WITH_EDITORONLY_DATA

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	/** Returns the total play length of the montage, if played back with a speed of 1.0. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	ENGINE_API virtual float GetPlayLength();

	/** Sort the Notifies array by time, earliest first. */
	ENGINE_API void SortNotifies();	

	/** Remove the notifies specified */
	ENGINE_API bool RemoveNotifies(const TArray<FName>& NotifiesToRemove);

	/** 
	 * Retrieves AnimNotifies given a StartTime and a DeltaTime.
	 * Time will be advanced and support looping if bAllowLooping is true.
	 * Supports playing backwards (DeltaTime<0).
	 * Returns notifies between StartTime (exclusive) and StartTime+DeltaTime (inclusive)
	 */
	ENGINE_API void GetAnimNotifies(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	/** 
	 * Retrieves AnimNotifies between two time positions. ]PreviousPosition, CurrentPosition]
	 * Between PreviousPosition (exclusive) and CurrentPosition (inclusive).
	 * Supports playing backwards (CurrentPosition<PreviousPosition).
	 * Only supports contiguous range, does NOT support looping and wrapping over.
	 */
	ENGINE_API virtual void GetAnimNotifiesFromDeltaPositions(const float& PreviousPosition, const float & CurrentPosition, TArray<const FAnimNotifyEvent *>& OutActiveNotifies) const;

	/** Evaluate curve data to Instance at the time of CurrentTime **/
	ENGINE_API virtual void EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData=false) const;

	ENGINE_API virtual const FRawCurveTracks& GetCurveData() const { return RawCurveData; }

#if WITH_EDITOR
	/** Return Number of Frames **/
	virtual int32 GetNumberOfFrames() const;

	/** Get the frame number for the provided time */
	ENGINE_API virtual int32 GetFrameAtTime(const float Time) const;

	/** Get the time at the given frame */
	ENGINE_API virtual float GetTimeAtFrame(const int32 Frame) const;
	
	// @todo document
	ENGINE_API void InitializeNotifyTrack();

	/** Fix up any notifies that are positioned beyond the end of the sequence */
	ENGINE_API void ClampNotifiesAtEndOfSequence();

	/** Calculates what (if any) offset should be applied to the trigger time of a notify given its display time */ 
	ENGINE_API virtual EAnimEventTriggerOffsets::Type CalculateOffsetForNotify(float NotifyDisplayTime) const;

	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	
	// Get a pointer to the data for a given Anim Notify
	ENGINE_API uint8* FindNotifyPropertyData(int32 NotifyIndex, UArrayProperty*& ArrayProperty);

	// Get a pointer to the data for a given array property item
	ENGINE_API uint8* FindArrayProperty(const TCHAR* PropName, UArrayProperty*& ArrayProperty, int32 ArrayIndex);

protected:
	ENGINE_API virtual void RefreshParentAssetData() override;
#endif	//WITH_EDITORONLY_DATA
public: 
	// update cache data (notify tracks, sync markers)
	ENGINE_API virtual void RefreshCacheData();

#if WITH_EDITOR
	ENGINE_API void RefreshCurveData();
#endif // WITH_EDITOR

	//~ Begin UAnimationAsset Interface
	ENGINE_API virtual void TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const override;

	void TickByMarkerAsFollower(FMarkerTickRecord &Instance, FMarkerTickContext &MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const;

	void TickByMarkerAsLeader(FMarkerTickRecord& Instance, FMarkerTickContext& MarkerContext, float& CurrentTime, float& OutPreviousTime, const float MoveDelta, const bool bLooping) const;

	// this is used in editor only when used for transition getter
	// this doesn't mean max time. In Sequence, this is SequenceLength,
	// but for BlendSpace CurrentTime is normalized [0,1], so this is 1
	virtual float GetMaxCurrentTime() override { return SequenceLength; }
	//~ End UAnimationAsset Interface

	/**
	* Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	* This returns different transform based on additive or not. Or what kind of additive.
	*
	* @param	OutPose				Pose object to fill
	* @param	OutCurve			Curves to fill
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	ENGINE_API virtual void GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const PURE_VIRTUAL(UAnimSequenceBase::GetAnimationPose, );
	
	virtual void HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const;

	virtual bool HasRootMotion() const { return false; }

	virtual void Serialize(FArchive& Ar) override;

	virtual void AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	virtual void AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	virtual void GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const { check(false); return FMarkerSyncAnimPosition(); /*Should never call this (either missing override or calling on unsupported asset */ }
	virtual void GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& CurrentTime) const { check(false); /*Should never call this (either missing override or calling on unsupported asset */ }
	
	virtual float GetFirstMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition) const { return 0.f; }
	virtual float GetNextMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const { return 0.f; }
	virtual float GetPrevMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const { return 0.f; }

	// default implementation, no additive
	virtual EAdditiveAnimationType GetAdditiveAnimType() const { return AAT_None; }
	virtual bool CanBeUsedInMontage() const { return true;  }

	// to support anim sequence base to montage
	virtual void EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock) {};

#if WITH_EDITOR
	// Store that our raw data has changed so that we can get correct compressed data later on
	virtual void MarkRawDataAsModified(bool bForceNewRawDatGuid = true) {}

private:
	DECLARE_MULTICAST_DELEGATE( FOnNotifyChangedMulticaster );
	FOnNotifyChangedMulticaster OnNotifyChanged;

public:
	typedef FOnNotifyChangedMulticaster::FDelegate FOnNotifyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnNotifyChanged(const FOnNotifyChanged& Delegate);
	ENGINE_API void UnregisterOnNotifyChanged(void* Unregister);
	ENGINE_API virtual bool IsValidToPlay() const { return true; }
	// ideally this would be animsequcnebase, but we might have some issue with that. For now, just allow AnimSequence
	virtual class UAnimSequence* GetAdditiveBasePose() const { return nullptr; }

#endif
	// return true if anim notify is available 
	ENGINE_API virtual bool IsNotifyAvailable() const;

#if WITH_EDITOR
private:
	DECLARE_MULTICAST_DELEGATE(FOnAnimCurvesChangedMulticaster);
	FOnAnimCurvesChangedMulticaster OnAnimCurvesChanged;

	DECLARE_MULTICAST_DELEGATE(FOnAnimTrackCurvesChangedMulticaster);
	FOnAnimTrackCurvesChangedMulticaster OnAnimTrackCurvesChanged;

public:
	typedef FOnAnimCurvesChangedMulticaster::FDelegate FOnAnimCurvesChanged;	
	/** Registers a delegate to be called after anim curves have changed*/
	ENGINE_API void RegisterOnAnimCurvesChanged(const FOnAnimCurvesChanged& Delegate);
	ENGINE_API void UnregisterOnAnimCurvesChanged(void* Unregister);

	typedef FOnAnimTrackCurvesChangedMulticaster::FDelegate FOnAnimTrackCurvesChanged;
	/** Registers a delegate to be called after anim track curves have changed*/
	ENGINE_API void RegisterOnAnimTrackCurvesChanged(const FOnAnimTrackCurvesChanged& Delegate);
	ENGINE_API void UnregisterOnAnimTrackCurvesChanged(void* Unregister);
#endif



protected:
	template <typename DataType>
	void VerifyCurveNames(USkeleton& Skeleton, const FName& NameContainer, TArray<DataType>& CurveList)
	{
		for (DataType& Curve : CurveList)
		{
			Skeleton.VerifySmartName(NameContainer, Curve.Name);
		}
	}
};
