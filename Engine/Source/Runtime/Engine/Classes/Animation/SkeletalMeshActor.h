// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Matinee/MatineeAnimInterface.h"
#include "SkeletalMeshActor.generated.h"

class UAnimMontage;
class UAnimSequence;

/**
 * SkeletalMeshActor is an instance of a USkeletalMesh in the world.
 * Skeletal meshes are deformable meshes that can be animated and change their geometry at run-time.
 * Skeletal meshes dragged into the level from the Content Browser are automatically converted to StaticMeshActors.
 * 
 * @see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/SkeletalMeshes/
 * @see USkeletalMesh
 */
UCLASS(ClassGroup=ISkeletalMeshes, Blueprintable, ComponentWrapperClass, ConversionRoot, meta=(ChildCanTick))
class ENGINE_API ASkeletalMeshActor : public AActor, public IMatineeAnimInterface
{
	GENERATED_UCLASS_BODY()

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const override;

	/** Whether or not this actor should respond to anim notifies - CURRENTLY ONLY AFFECTS PlayParticleEffect NOTIFIES**/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Animation, AdvancedDisplay)
	uint32 bShouldDoAnimNotifies:1;

	UPROPERTY()
	uint32 bWakeOnLevelStart_DEPRECATED:1;

private:
	UPROPERTY(Category = SkeletalMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Components|SkeletalMesh,Animation,Physics", AllowPrivateAccess = "true"))
	class USkeletalMeshComponent* SkeletalMeshComponent;
public:

	/** Used to replicate mesh to clients */
	UPROPERTY(replicatedUsing=OnRep_ReplicatedMesh, transient)
	class USkeletalMesh* ReplicatedMesh;

	/** Used to replicate physics asset to clients */
	UPROPERTY(replicatedUsing=OnRep_ReplicatedPhysAsset, transient)
	class UPhysicsAsset* ReplicatedPhysAsset;

	/** used to replicate the material in index 0 */
	UPROPERTY(replicatedUsing=OnRep_ReplicatedMaterial0)
	class UMaterialInterface* ReplicatedMaterial0;

	UPROPERTY(replicatedUsing=OnRep_ReplicatedMaterial1)
	class UMaterialInterface* ReplicatedMaterial1;

	/** Replication Notification Callbacks */
	UFUNCTION()
	virtual void OnRep_ReplicatedMesh();

	UFUNCTION()
	virtual void OnRep_ReplicatedPhysAsset();

	UFUNCTION()
	virtual void OnRep_ReplicatedMaterial0();

	UFUNCTION()
	virtual void OnRep_ReplicatedMaterial1();


	//~ Begin UObject Interface
protected:
	virtual FString GetDetailedInfoInternal() const override;
public:
	//~ End UObject Interface

	//~ Begin AActor Interface
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const override;
	virtual void EditorReplacedActor(AActor* OldActor) override;
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
#endif
	virtual void PostInitializeComponents() override;
	//~ End AActor Interface

	//~ Begin IMatineeAnimInterface Interface
	virtual void PreviewBeginAnimControl(class UInterpGroup* InInterpGroup) override;
	virtual void PreviewSetAnimPosition(FName SlotName, int32 ChannelIndex, UAnimSequence* InAnimSequence, float InPosition, bool bLooping, bool bFireNotifies, float AdvanceTime) override;
	virtual void PreviewSetAnimWeights(TArray<FAnimSlotInfo>& SlotInfos) override;
	virtual void PreviewFinishAnimControl(class UInterpGroup* InInterpGroup) override;
	virtual void GetAnimControlSlotDesc(TArray<struct FAnimSlotDesc>& OutSlotDescs) override {};
	virtual void SetAnimWeights( const TArray<struct FAnimSlotInfo>& SlotInfos ) override;
	virtual void BeginAnimControl(class UInterpGroup* InInterpGroup) override;
	virtual void SetAnimPosition(FName SlotName, int32 ChannelIndex, class UAnimSequence* InAnimSequence, float InPosition, bool bFireNotifies, bool bLooping) override;
	virtual void FinishAnimControl(class UInterpGroup* InInterpGroup) override;
	//~ End IMatineeAnimInterface Interface

private:
	// utility function to see if it can play animation or not
	bool CanPlayAnimation(class UAnimSequenceBase* AnimAssetBase=NULL) const;

	// currently actively playing montage
	TMap<FName, TWeakObjectPtr<class UAnimMontage>> CurrentlyPlayingMontages;

public:
	/** Returns SkeletalMeshComponent subobject **/
	class USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
};



