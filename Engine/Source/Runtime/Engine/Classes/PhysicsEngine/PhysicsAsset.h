// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "PhysicsEngine/RigidBodyIndexPair.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsAsset.generated.h"

class FMeshElementCollector;
class USkeletalBodySetup;

/**
 * PhysicsAsset contains a set of rigid bodies and constraints that make up a single ragdoll.
 * The asset is not limited to human ragdolls, and can be used for any physical simulation using bodies and constraints.
 * A SkeletalMesh has a single PhysicsAsset, which allows for easily turning ragdoll physics on or off for many SkeletalMeshComponents
 * The asset can be configured inside the Physics Asset Editor.
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Physics/PhAT/Reference/index.html
 * @see USkeletalMesh
 */

UCLASS(hidecategories=Object, BlueprintType, MinimalAPI)
class UPhysicsAsset : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** 
	 *	Default skeletal mesh to use when previewing this PhysicsAsset etc. 
	 *	Is the one that was used as the basis for creating this Asset.
	 */
	UPROPERTY()
	class USkeletalMesh * DefaultSkelMesh_DEPRECATED;

	UPROPERTY(AssetRegistrySearchable)
	TSoftObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;

	UPROPERTY(EditAnywhere, Category = Profiles, meta=(DisableCopyPaste))
	TArray<FName> PhysicalAnimationProfiles;

	UPROPERTY(EditAnywhere, Category = Profiles, meta=(DisableCopyPaste))
	TArray<FName> ConstraintProfiles;

	UPROPERTY(transient)
	FName CurrentPhysicalAnimationProfileName;

	UPROPERTY(transient)
	FName CurrentConstraintProfileName;

#endif // WITH_EDITORONLY_DATA

	/** Index of bodies that are marked bConsiderForBounds */
	UPROPERTY()
	TArray<int32> BoundsBodies;

	/**
	*	Array of SkeletalBodySetup objects. Stores information about collision shape etc. for each body.
	*	Does not include body position - those are taken from mesh.
	*/
	UPROPERTY(instanced)
	TArray<USkeletalBodySetup*> SkeletalBodySetups;

	/** 
	 *	Array of RB_ConstraintSetup objects. 
	 *	Stores information about a joint between two bodies, such as position relative to each body, joint limits etc.
	 */
	UPROPERTY(instanced)
	TArray<class UPhysicsConstraintTemplate*> ConstraintSetup;

public:

	/**
	* If true, bodies of the physics asset will be put into the asynchronous physics scene. If false, they will be put into the synchronous physics scene.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Physics)
	uint8 bUseAsyncScene:1;

	/** This caches the BodySetup Index by BodyName to speed up FindBodyIndex */
	TMap<FName, int32>					BodySetupIndexMap;

	/** 
	 *	Table indicating which pairs of bodies have collision disabled between them. Used internally. 
	 *	Note, this is accessed from within physics engine, so is not safe to change while physics is running
	 */
	TMap<FRigidBodyIndexPair,bool>		CollisionDisableTable;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;

	ENGINE_API const TArray<FName>& GetPhysicalAnimationProfileNames() const
	{
		return PhysicalAnimationProfiles;
	}

	ENGINE_API const TArray<FName>& GetConstraintProfileNames() const
	{
		return ConstraintProfiles;
	}

	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

#endif
	//~ End UObject Interface

	// Find the index of the physics bone that is controlling this graphics bone.
	ENGINE_API int32		FindControllingBodyIndex(class USkeletalMesh* skelMesh, int32 BoneIndex);
	ENGINE_API int32		FindParentBodyIndex(class USkeletalMesh * skelMesh, int32 StartBoneIndex) const;
	ENGINE_API int32		FindConstraintIndex(FName ConstraintName);
	FName					FindConstraintBoneName(int32 ConstraintIndex);
	ENGINE_API int32		FindMirroredBone(class USkeletalMesh* skelMesh, int32 BoneIndex);

	/** Utility for getting indices of all bodies below (and including) the one with the supplied name. */
	ENGINE_API void			GetBodyIndicesBelow(TArray<int32>& OutBodyIndices, FName InBoneName, USkeletalMesh* InSkelMesh, bool bIncludeParent = true);

	ENGINE_API void			GetNearestBodyIndicesBelow(TArray<int32> & OutBodyIndices, FName InBoneName, USkeletalMesh * InSkelMesh);

	ENGINE_API FBox			CalcAABB(const class USkinnedMeshComponent* MeshComponent, const FTransform& LocalToWorld) const;

	/** Clears physics meshes from all bodies */
	ENGINE_API void ClearAllPhysicsMeshes();
	
#if WITH_EDITOR
	/**
	 * Check if the Bounds can be calculate for the specified MeshComponent.
	 * return true if the skeleton match with the physic asset and the bounds can be calculated, otherwise it will return false.
	 */
	ENGINE_API bool CanCalculateValidAABB(const class USkinnedMeshComponent* MeshComponent, const FTransform& LocalToWorld) const;

	/** Invalidates physics meshes from all bodies. Data will be rebuilt completely. */
	ENGINE_API void InvalidateAllPhysicsMeshes();
#endif

	// @todo document
	void GetCollisionMesh(int32 ViewIndex, FMeshElementCollector& Collector, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, const FVector& Scale3D);

	// @todo document
	void DrawConstraints(int32 ViewIndex, FMeshElementCollector& Collector, const USkeletalMesh* SkelMesh, const TArray<FTransform>& SpaceBases, const FTransform& LocalToWorld, float Scale);

	void GetUsedMaterials(TArray<UMaterialInterface*>& Materials);

	// Disable collsion between the bodies specified by index
	ENGINE_API void DisableCollision(int32 BodyIndexA, int32 BodyIndexB);

	// Enable collsion between the bodies specified by index
	ENGINE_API void EnableCollision(int32 BodyIndexA, int32 BodyIndexB);

	// Check whether the two bodies specified are enabled for collision
	ENGINE_API bool IsCollisionEnabled(int32 BodyIndexA, int32 BodyIndexB) const;

	/** Update the BoundsBodies array and cache the indices of bodies marked with bConsiderForBounds to BoundsBodies array. */
	ENGINE_API void UpdateBoundsBodiesArray();

	/** Update the BodySetup Array Index Map.  */
	ENGINE_API void UpdateBodySetupIndexMap();


	// @todo document
	ENGINE_API int32 FindBodyIndex(FName BodyName) const;

	/** Find all the constraints that are connected to a particular body.
	 * 
	 * @param	BodyIndex		The index of the body to find the constraints for
	 * @param	Constraints		Returns the found constraints
	 **/
	ENGINE_API void BodyFindConstraints(int32 BodyIndex, TArray<int32>& Constraints);

#if WITH_EDITOR
	/** Update skeletal meshes when physics asset changes*/
	ENGINE_API void RefreshPhysicsAssetChange() const;

	/** Set the preview mesh for this physics asset */
	ENGINE_API void SetPreviewMesh(USkeletalMesh* PreviewMesh);

	/** Get the preview mesh for this physics asset */
	ENGINE_API USkeletalMesh* GetPreviewMesh() const;

//#nv begin #Blast Physics asset change broadcast
	DECLARE_MULTICAST_DELEGATE_OneParam(FRefreshPhysicsAssetChangeDelegate, const UPhysicsAsset*);
	ENGINE_API static FRefreshPhysicsAssetChangeDelegate OnRefreshPhysicsAssetChange;
//nv end
#endif

private:

#if WITH_EDITORONLY_DATA
	/** Editor only arrays that are used for rename operations in pre/post edit change*/
	TArray<FName> PrePhysicalAnimationProfiles;
	TArray<FName> PreConstraintProfiles;
#endif


	UPROPERTY(instanced)
	TArray<class UBodySetup*> BodySetup_DEPRECATED;
};

USTRUCT()
struct FPhysicalAnimationProfile
{
	GENERATED_BODY()
	
	/** Profile name used to identify set of physical animation parameters */
	UPROPERTY()
	FName ProfileName;

	/** Physical animation parameters used to drive animation */
	UPROPERTY(EditAnywhere, Category = PhysicalAnimation)
	FPhysicalAnimationData PhysicalAnimationData;
};

UCLASS(MinimalAPI)
class USkeletalBodySetup : public UBodySetup
{
	GENERATED_BODY()
public:
	const FPhysicalAnimationProfile* FindPhysicalAnimationProfile(const FName ProfileName) const
	{
		return PhysicalAnimationData.FindByPredicate([ProfileName](const FPhysicalAnimationProfile& Profile){ return ProfileName == Profile.ProfileName; });
	}

	FPhysicalAnimationProfile* FindPhysicalAnimationProfile(const FName ProfileName)
	{
		return PhysicalAnimationData.FindByPredicate([ProfileName](const FPhysicalAnimationProfile& Profile) { return ProfileName == Profile.ProfileName; });
	}

	const TArray<FPhysicalAnimationProfile>& GetPhysicalAnimationProfiles() const
	{
		return PhysicalAnimationData;
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	ENGINE_API FName GetCurrentPhysicalAnimationProfileName() const;
	
	/** Creates a new physical animation profile entry */
	ENGINE_API void AddPhysicalAnimationProfile(FName ProfileName);

	/** Removes physical animation profile */
	ENGINE_API void RemovePhysicalAnimationProfile(FName ProfileName);

	ENGINE_API void UpdatePhysicalAnimationProfiles(const TArray<FName>& Profiles);

	ENGINE_API void DuplicatePhysicalAnimationProfile(FName DuplicateFromName, FName DuplicateToName);

	ENGINE_API void RenamePhysicalAnimationProfile(FName CurrentName, FName NewName);
#endif

#if WITH_EDITORONLY_DATA
	//dummy place for customization inside phat. Profiles are ordered dynamically and we need a static place for detail customization
	UPROPERTY(EditAnywhere, Category = PhysicalAnimation)
	FPhysicalAnimationProfile CurrentPhysicalAnimationProfile;
#endif

private:
	UPROPERTY()
	TArray<FPhysicalAnimationProfile> PhysicalAnimationData;
};
