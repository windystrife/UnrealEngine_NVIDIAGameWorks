// @third party code - BEGIN HairWorks
#pragma once

#include "PrimitiveComponent.h"
#include "Engine/HairWorksInstance.h"
#include "HairWorksComponent.generated.h"

namespace nvidia{namespace HairWorks{
	enum InstanceId;
}}

class USkeletalMesh;

/**
* HairWorksComponent manages and renders a hair asset.
*/
UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent), HideCategories = (Collision, Base, Object, PhysicsVolume, Physics, Mobile))
class ENGINE_API UHairWorksComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hair, meta = (ShowOnlyInnerProperties))
	FHairWorksInstance HairInstance;

	/** It requires a remapping progress to support morph target of skeletal mesh. This progress would be slow when vertex number is very large, and cause long halt in editor. If this option is on, remapping happens when any edit occurs. If this option is off, remapping happens only when the parent skeletal mesh of a HairWorks component changers. If you want to do remapping once when you need, just turn it on and then off. */
	UPROPERTY(EditDefaultsOnly, Category = Asset)
	bool bAutoRemapMorphTarget = false;

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnAttachmentChanged() override;
	//~ End UPrimitiveComponent interface

	//~ Begin UActorComponent interface
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual bool ShouldCreateRenderState() const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End UActorComponent interface

	//~ Begin USceneComponent interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent interface.

	//~ Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	//~ End UObject interface.

protected:
	/** Send data for rendering */
	void SendHairDynamicData(bool bForceSkinning = false)const;

	/** Bone mapping */
	void SetupBoneAndMorphMapping();

	/** Update bones */
	void UpdateBoneMatrices()const;

	/** Parent skeleton */
	UPROPERTY()
	USkinnedMeshComponent* ParentSkeleton;

	/** Bone remapping */
	TArray<uint16> BoneIndices;

	/** Morph remapping */
	UPROPERTY()
	TArray<int32> MorphIndices;

	/** Usually we do remapping for morph target only when parent skeletal mesh is changed. */
	UPROPERTY()
	USkeletalMesh* CachedSkeletalMeshForMorph;

	/** Skinning data*/
	mutable TArray<FMatrix> BoneMatrices;
};

// @third party code - END HairWorks