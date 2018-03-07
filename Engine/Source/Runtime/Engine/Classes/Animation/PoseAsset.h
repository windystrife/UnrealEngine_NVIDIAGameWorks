// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class of pose asset that can evaluate pose by weights
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/SmartName.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "PoseAsset.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/** 
 * Pose data 
 * 
 * This is one pose data structure
 * This will let us blend poses quickly easily
 * All poses within this asset should contain same number of tracks, 
 * so that we can blend quickly
 */

USTRUCT()
struct ENGINE_API FPoseData
{
	GENERATED_USTRUCT_BODY()

	// local space pose, # of array match with # of Tracks
	UPROPERTY()
	TArray<FTransform>		LocalSpacePose;

	// whether or not, the joint contains dirty transform
	// it only blends if this is true 
	// this allows per bone blend
	// @todo: convert to bit field?
	UPROPERTY()
	TArray<bool>			LocalSpacePoseMask;

	// # of array match with # of Curves in PoseDataContainer
 	UPROPERTY()
 	TArray<float>			CurveData;
};

/**
* Pose data container
* 
* Contains animation and curve for all poses
*/
USTRUCT()
struct ENGINE_API FPoseDataContainer
{
	GENERATED_USTRUCT_BODY()

private:
	// pose names - horizontal data
	// # of poses - there is no compression across tracks - 	
	// unfortunately, tried TMap, but it is not great because it changes order whenever add/remove
	// we need consistent array of names, so that it doesn't change orders
	UPROPERTY()
	TArray<FSmartName>						PoseNames;

	UPROPERTY()
	TArray<FPoseData>						Poses;

	UPROPERTY()
	TArray<FName>							Tracks;

	// vertical data - the track names for bone position, and skeleton index 
	UPROPERTY(transient)
	TMap<FName, int32>						TrackMap;
	
	// curve meta data # of Curve UIDs should match with Poses.CurveValues.Num
	UPROPERTY()
	TArray<FAnimCurveBase>					Curves;

	void Reset();

	void AddOrUpdatePose(const FSmartName& InPoseName, const TArray<FTransform>& InlocalSpacePose, const TArray<float>& InCurveData);

	// remove track if all poses has identity key
	// Shrink currently works with only full pose
	// we could support addiitve with comparing with identity but right now PoseContainer it self doesn't know if it's additive or not
	// not sure if that's a good or not yet
	void Shrink(USkeleton* InSkeleton, FName& InRetargetSourceName);

	bool InsertTrack(const FName& InTrackName, USkeleton* InSkeleton, FName& InRetargetSourceName);
	bool FillUpDefaultPose(const FSmartName& InPoseName, USkeleton* InSkeleton, FName& InRetargetSourceName);
	bool FillUpDefaultPose(FPoseData* PoseData, USkeleton* InSkeleton, FName& InRetargetSourceName);
	FTransform GetDefaultTransform(int32 SkeletonIndex, USkeleton* InSkeleton, const FName& InRetargetSourceName) const;
	FTransform GetDefaultTransform(const FName& InTrackName, USkeleton* InSkeleton, const FName& InRetargetSourceName) const;
	void RenamePose(FSmartName OldPoseName, FSmartName NewPoseName);
	bool DeletePose(FSmartName PoseName);
	bool DeleteCurve(FSmartName CurveName);
	void DeleteTrack(int32 TrackIndex);

	FPoseData* FindPoseData(FSmartName PoseName);
	FPoseData* FindOrAddPoseData(FSmartName PoseName);

	int32 GetNumPoses() const { return Poses.Num();  }
	bool Contains(FSmartName PoseName) const { return PoseNames.Contains(PoseName); }

	bool IsValid() const { return PoseNames.Num() == Poses.Num() && Tracks.Num() == TrackMap.Num(); }
	void GetPoseCurve(const FPoseData* PoseData, FBlendedCurve& OutCurve) const;
	void ConvertToFullPose(int32 InBasePoseIndex, const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve);
	void ConvertToAdditivePose(int32 InBasePoseIndex, const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve);

	friend class UPoseAsset;
};

/*
 * Pose Asset that can be blended by weight of curves 
 */
UCLASS(MinimalAPI, BlueprintType)
class UPoseAsset : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

private:
	/** Animation Pose Data*/
	UPROPERTY()
	struct FPoseDataContainer PoseContainer;

	/** Whether or not Additive Pose or not - these are property that needs post process, so */
	UPROPERTY()
	bool bAdditivePose;

	/** if -1, use ref pose */
	UPROPERTY()
	int32 BasePoseIndex;

public: 
	/** Base pose to use when retargeting */
	UPROPERTY(Category=Animation, EditAnywhere)
	FName RetargetSource;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category=Source, EditAnywhere)
	UAnimSequence* SourceAnimation;
#endif // WITH_EDITORONLY_DATA

	/**
	* Get Animation Pose from one pose of PoseIndex and with PoseWeight
	* This returns OutPose and OutCurve of one pose of PoseIndex with PoseWeight
	*
	* @param	OutPose				Pose object to fill
	* @param	InOutCurve			Curves to fill
	* @param	PoseIndex			Index of Pose
	* @param	PoseWeight			Weight of pose
	*/
	ENGINE_API bool GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;
	ENGINE_API void GetBaseAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve) const;
	virtual bool HasRootMotion() const { return false; }
	virtual bool IsValidAdditive() const { return bAdditivePose; }

	//Begin UObject Interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//End UObject Interface

public:
	ENGINE_API int32 GetNumPoses() const;
	ENGINE_API int32 GetNumCurves() const;
	ENGINE_API int32 GetNumTracks() const;
	ENGINE_API const TArray<FSmartName> GetPoseNames() const;
	ENGINE_API const TArray<FName>		GetTrackNames() const;
	ENGINE_API const TArray<FSmartName> GetCurveNames() const;
	ENGINE_API const TArray<FAnimCurveBase> GetCurveData() const;
	ENGINE_API const TArray<float> GetCurveValues(const int32 PoseIndex) const;

	/** Find index of a track with a given bone name. Returns INDEX_NONE if not found. */
	ENGINE_API const int32 GetTrackIndexByName(const FName& InTrackName) const;

	/** 
	 *	Get local space pose for a particular track (by name) in a particular pose (by index) 
	 *	@return	Returns true if OutTransform is valid, false if not
	 */
	ENGINE_API bool GetLocalPoseForTrack(const int32 PoseIndex, const int32 TrackIndex, FTransform& OutTransform) const;

	/** 
	 *	Return value of a curve for a particular pose 
	 *	@return	Returns true if OutValue is valid, false if not
	 */
	ENGINE_API bool GetCurveValue(const int32 PoseIndex, const int32 CurveIndex, float& OutValue) const;
	 
	ENGINE_API bool ContainsPose(const FSmartName& InPoseName) const { return PoseContainer.Contains(InPoseName); }
	ENGINE_API bool ContainsPose(const FName& InPoseName) const;

#if WITH_EDITOR
	ENGINE_API void AddOrUpdatePose(const FSmartName& PoseName, USkeletalMeshComponent* MeshComponent);
	ENGINE_API bool AddOrUpdatePoseWithUniqueName(USkeletalMeshComponent* MeshComponent, FSmartName* OutPoseName = nullptr);
	ENGINE_API void AddOrUpdatePose(const FSmartName& PoseName, const TArray<FName>& TrackNames, const TArray<FTransform>& LocalTransform, const TArray<float>& CurveValues);

	ENGINE_API void CreatePoseFromAnimation(class UAnimSequence* AnimSequence, const TArray<FSmartName>* InPoseNames = nullptr);
	ENGINE_API void UpdatePoseFromAnimation(class UAnimSequence* AnimSequence);

	// Begin AnimationAsset interface
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive = true) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
	// End AnimationAsset interface

	ENGINE_API bool ModifyPoseName(FName OldPoseName, FName NewPoseName, const SmartName::UID_Type* NewUID);

	// Rename the smart names used by this Pose Asset
	ENGINE_API void RenameSmartName(const FName& InOriginalName, const FName& InNewName);

	// Remove poses or curves using the smart names supplied
	ENGINE_API void RemoveSmartNames(const TArray<FName>& InNamesToRemove);
#endif

	ENGINE_API int32 DeletePoses(TArray<FName> PoseNamesToDelete);
	ENGINE_API int32 DeleteCurves(TArray<FName> CurveNamesToDelete);
	ENGINE_API bool ConvertSpace(bool bNewAdditivePose, int32 NewBasePoseInde);

	ENGINE_API int32 GetBasePoseIndex() const { return BasePoseIndex;  }
	ENGINE_API const int32 GetPoseIndexByName(const FName& InBasePoseName) const;
	ENGINE_API const FName GetPoseNameByIndex(int32 InBasePoseIndex) const { return PoseContainer.PoseNames.IsValidIndex(InBasePoseIndex)? PoseContainer.PoseNames[InBasePoseIndex].DisplayName : NAME_None; }

	ENGINE_API const int32 GetCurveIndexByName(const FName& InCurveName) const;

	/** Return full (local space, non additive) pose. Will do conversion if PoseAsset is Additive. */
	ENGINE_API bool GetFullPose(int32 PoseIndex, TArray<FTransform>& OutTransforms) const;


private: 
	DECLARE_MULTICAST_DELEGATE(FOnPoseListChangedMulticaster)
	FOnPoseListChangedMulticaster OnPoseListChanged;

public:
	typedef FOnPoseListChangedMulticaster::FDelegate FOnPoseListChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	FDelegateHandle RegisterOnPoseListChanged(const FOnPoseListChanged& Delegate)
	{
		return OnPoseListChanged.Add(Delegate);
	}
	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnPoseListChanged(FDelegateHandle Handle)
	{
		OnPoseListChanged.Remove(Handle);
	}

#if WITH_EDITOR
protected:
	virtual void RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces) override;
#endif // WITH_EDITOR	

private:

	// this will do multiple things, it will add tracks and make sure it fix up all poses with it
	// use same as retarget source system we have for animation
	void CombineTracks(const TArray<FName>& NewTracks);

	bool ConvertToFullPose();
	bool ConvertToAdditivePose(int32 NewBasePoseIndex);

	bool GetBasePoseTransform(TArray<FTransform>& OutBasePose, TArray<float>& OutCurve) const;
	void RecacheTrackmap();

	void Reinitialize();
};
