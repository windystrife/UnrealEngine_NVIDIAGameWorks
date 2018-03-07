// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BoneContainer.h"
#include "AnimationRuntime.h"
#include "BlendProfile.generated.h"

/** A single entry for a blend scale within a profile, mapping a bone to a blendscale */
USTRUCT()
struct FBlendProfileBoneEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=BoneSettings)
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category=BoneSettings)
	float BlendScale;
};

//////////////////////////////////////////////////////////////////////////

/** A blend profile is a set of per-bone scales that can be used in transitions and blend lists
 *  to tweak the weights of specific bones. The scales are applied to the normal weight for that bone
 */
UCLASS(Within=Skeleton)
class ENGINE_API UBlendProfile : public UObject, public IInterpolationIndexProvider
{
public:

	GENERATED_BODY()

	UBlendProfile();

	/** Get the number of entries in the profile (an entry is any blend scale that isn't 1.0f) */
	int32 GetNumBlendEntries() const { return ProfileEntries.Num(); }

	/** Set the blend scale for a specific bone 
	 *  @param InBoneIdx Index of the bone to set the blend scale of
	 *  @param InScale The scale to set the bone to
	 *  @param bRecurse Whether or not to set the scale on all children of this bone
	 *  @param bCreate Whether or not to create a blend profile entry if one does not exist for the specified bone
	 */
	void SetBoneBlendScale(int32 InBoneIdx, float InScale, bool bRecurse = false, bool bCreate = false);

	/** Set the blend scale for a specific bone 
	 *  @param InBoneName Name of the bone to set the blend scale of
	 *  @param InScale The scale to set the bone to
	 *  @param bRecurse Whether or not to set the scale on all children of this bone
	 *  @param bCreate Whether or not to create a blend profile entry if one does not exist for the specified bone
	 */
	void SetBoneBlendScale(const FName& InBoneName, float InScale, bool bRecurse = false, bool bCreate = false);

	/** Get the set blend scale for the specified bone, will return 1.0f if no entry was found (no scale)
	 *  @param InBoneIdx Index of the bone to retrieve
	 */
	float GetBoneBlendScale(int32 InBoneIdx) const;

	/** Get the set blend scale for the specified bone, will return 1.0f if no entry was found (no scale)
	 *  @param InBoneName Name of the bone to retrieve
	 */
	float GetBoneBlendScale(const FName& InBoneName) const;

	/** Get the index of the entry for the specified bone
	 *  @param InBoneIdx Index of the bone
	 */
	int32 GetEntryIndex(const int32 InBoneIdx) const;

	/** Get the index of the entry for the specified bone
	 *  @param InBoneIdx Index of the bone
	 */
	int32 GetEntryIndex(const FName& BoneName) const;

	/** Get the blend scale stored in a specific entry 
	 *  @param InEntryIdx Index of the entry to retrieve
	 */
	float GetEntryBlendScale(const int32 InEntryIdx) const;

	// IInterpolationIndexProvider
	virtual int32 GetPerBoneInterpolationIndex(int32 BoneIndex, const FBoneContainer& RequiredBones) const override;
	// End IInterpolationIndexProvider

	// UObject
	virtual bool IsSafeForRootSet() const override {return false;}
	virtual void PostLoad() override;
	// End UObject

private:

	/** Sets the skeleton this blend profile is used with */
	void SetSkeleton(USkeleton* InSkeleton);

	/** Set the blend scale for a single bone (ignore children) */
	void SetSingleBoneBlendScale(int32 InBoneIdx, float InScale, bool bCreate = false);

public:
	// The skeleton that owns this profile
	UPROPERTY()
	USkeleton* OwningSkeleton;

	// List of blend scale entries
	UPROPERTY()
	TArray<FBlendProfileBoneEntry> ProfileEntries;
};
