#pragma once
#include "CoreMinimal.h"

#include "GameFramework/Info.h"
#include "BlastMeshComponent.h"

#include "BlastExtendedSupport.generated.h"

class UBlastExtendedSupportMeshComponent;

USTRUCT()
struct FBlastExtendedStructureComponent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	UBlastMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	FGuid GUIDAtMerge;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	FTransform TransformAtMerge;

	UPROPERTY()
	TArray<int32> ChunkIDs;

	UPROPERTY(Transient)
	TArray<FTransform> LastActorTransforms;
};

UCLASS(ClassGroup = Blast)
class BLAST_API ABlastExtendedSupportStructure : public AInfo
{
	GENERATED_BODY()
public:
	ABlastExtendedSupportStructure();

protected:
	UPROPERTY(EditInstanceOnly, Category = "Blast")
	TArray<AActor*> StructureActors;

	/**
		Maximal distance between chunks in which bond generation allowed. If equal to zero, only touching chunks will be connected.
	*/
	UPROPERTY(EditInstanceOnly, Category = "Blast")
	float bondGenerationDistance;

	UPROPERTY(VisibleAnywhere, Category = "Blast")
	UBlastExtendedSupportMeshComponent* ExtendedSupportMesh;
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	bool bEnabled;

	UBlastExtendedSupportMeshComponent* GetExtendedSupportMeshComponent() { return ExtendedSupportMesh; }

	void GetStructureComponents(TArray<UBlastMeshComponent*>& OutComponents);
	static void GetStructureComponents(const TArray<AActor*>& StructureActors, TArray<UBlastMeshComponent*>& OutComponents);

	const TArray<AActor*>& GetStructureActors() const { return StructureActors;  }
	float GetBondGenerationDistance() const { return bondGenerationDistance; }


	virtual void PostActorCreated() override;
	//PostActorCreated is only called when spawing a new actor, not loading one from disk
	virtual void PostLoad() override;
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

	void AddStructureActor(AActor* NewActor);
	void RemoveStructureActor(AActor* NewActor);

	void StoreSavedComponents(const TArray<FBlastExtendedStructureComponent>& SavedData, const TArray<FIntPoint>& ChunkMap, UBlastMesh* CombinedAsset);
	void ResetActorAssociations();
#endif
};

UCLASS(ClassGroup = Blast, Within = BlastExtendedSupportStructure, HideCategories=(BlastMesh,SkeletalMesh,Optimization,Mobile,Lighting,LOD))
class BLAST_API UBlastExtendedSupportMeshComponent : public UBlastMeshComponent
{
	GENERATED_BODY()
public:

	UPROPERTY(VisibleInstanceOnly, Category = "Blast", AdvancedDisplay)
	TArray<FBlastExtendedStructureComponent> SavedComponents;

	UPROPERTY()
	TArray<FIntPoint> ChunkToOriginalChunkMap;

	UBlastExtendedSupportMeshComponent(const FObjectInitializer& ObjectInitializer);

	virtual void SetChunkVisible(int32 ChunkIndex, bool bInVisible) override;

	bool PopulateComponentBoneTransforms(TArray<FTransform>& Transforms, TBitArray<>& BonesTouched, int32 ComponentIndex);
	FBox GetWorldBoundsOfComponentChunks(int32 ComponentIndex) const;

	const TArray<FBlastExtendedStructureComponent>& GetSavedComponents() const { return SavedComponents; }

	int32 GetCombinedChunkIndex(int32 ComponentIndex, int32 ComponentChunkIndex) const;
	int32 GetComponentChunkIndex(int32 CombinedIndex, int32* OutComponentIndex) const;

#if WITH_EDITOR
	void InvalidateSupportData();
#endif
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;

	virtual bool ShouldUpdateTransform(bool bLODHasChanged) const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual void OnRegister() override;

	virtual void BroadcastOnDamaged(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType) override;
	virtual void BroadcastOnActorCreated(FName ActorName) override;
	virtual void BroadcastOnActorDestroyed(FName ActorName) override;
	virtual void BroadcastOnActorCreatedFromDamage(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType) override;
	virtual void BroadcastOnBondsDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FBondDamageEvent>& Events) override;
	virtual void BroadcastOnChunksDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FChunkDamageEvent>& Events) override;
	virtual bool OnBondsDamagedBound() const override;
	virtual bool OnChunksDamagedBound() const override;
protected:
	virtual void ShowActorsVisibleChunks(uint32 actorIndex) override;
	virtual void HideActorsVisibleChunks(uint32 actorIndex) override;
	void RefreshBoundsForActor(uint32 actorIndex);
};

//This class doesn't do much other than make it easier to tell if an asset is a generated support asset or not
UCLASS(ClassGroup = Blast)
class BLAST_API UBlastMeshExtendedSupport : public UBlastMesh
{
	GENERATED_BODY()
public:
	UBlastMeshExtendedSupport(const FObjectInitializer& ObjectInitializer);
};