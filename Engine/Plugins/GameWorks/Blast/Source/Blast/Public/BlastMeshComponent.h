#pragma once

#include "CoreMinimal.h"
#include "NvBlastTypes.h"
#include "NvBlastExtDamageShaders.h"
#include "BlastMesh.h"
#include "BlastAsset.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "SkeletalMeshTypes.h"
#include "PhysicsEngine/BodySetup.h"
#include "BoneContainer.h"
#include "BlastBaseDamageComponent.h"
#include "BlastBaseDamageProgram.h"
#include "BlastMeshComponent.generated.h"

struct NvBlastActor;
struct NvBlastFamily;
class AVolume;
class ABlastExtendedSupportStructure;

namespace Nv
{
namespace Blast
{
class ExtStressSolver;
}
}

namespace physx
{
class PxScene;
}

UENUM()
enum class EBlastDamageResult : uint8
{
	None,
	Damaged,
	Split,
};

#if WITH_EDITORONLY_DATA
UENUM(BlueprintType)
enum class EBlastDebugRenderMode : uint8
{
	None UMETA(DisplayName = "None"),
	SupportGraph UMETA(DisplayName = "SupportGraph"),
	StressSolverStress UMETA(DisplayName = "StressSolverStress"),
	StressSolverBondImpulses UMETA(DisplayName = "StressSolverBondImpulses"),
	ChunkCentroids UMETA(DisplayName = "ChunkCentroids"),
};
#endif

/**
Bond damage event data
*/
USTRUCT(BlueprintType)
struct FBondDamageEvent
{
	GENERATED_USTRUCT_BODY()

	// Chunk connected with this bond. The lowest chunk index of two.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	int32 ChunkIndex;

	// Other Chunk connected with this bond. The highest chunk index of two. Can be invalid if bond connects to the "world".
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	int32 OtherChunkIndex;

	// Amount of damage applied
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	float Damage;

	// Amount of health left after damage, if <= 0 bond is broken
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	float HealthLeft;

	// Contact surface area
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	float BondArea;

	// Bond centroid in world coordinates
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	FVector WorldCentroid;

	// Bond normal in world coordinates
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	FVector WorldNormal;
};

/**
Chunk damage event data
*/
USTRUCT(BlueprintType)
struct FChunkDamageEvent
{
	GENERATED_USTRUCT_BODY()

	// Chunk index in NvBlastAsset
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	int32 ChunkIndex;

	// Amount of damage applied
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	float Damage;

	// Chunk centroid in world coordinates
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blast")
	FVector WorldCentroid;
};

// Delagates/Events signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBlastMeshComponentOnDamagedSignature, UBlastMeshComponent*, Component, FName, ActorName, FVector, DamageOrigin, FRotator, DamageRot, FName, DamageType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlastMeshComponentOnActorCreatedSignature, UBlastMeshComponent*, Component, FName, ActorName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBlastMeshComponentOnActorDestroyedSignature, UBlastMeshComponent*, Component, FName, ActorName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBlastMeshComponentOnActorCreatedFromDamageSignature, UBlastMeshComponent*, Component, FName, ActorName, FVector, DamageOrigin, FRotator, DamageRot, FName, DamageType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBlastMeshComponentOnBondsDamagedSignature, UBlastMeshComponent*, Component, FName, ActorName, bool, bIsSplit, FName, DamageType, const TArray<FBondDamageEvent>&, Events);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBlastMeshComponentOnChunksDamagedSignature, UBlastMeshComponent*, Component, FName, ActorName, bool, bIsSplit, FName, DamageType, const TArray<FChunkDamageEvent>&, Events);

//BlastMeshComponent is used to create an instance of an BlastMesh asset.
UCLASS(ClassGroup=Blast, editinlinenew, hidecategories=(Object, Mesh), meta=(BlueprintSpawnableComponent))
class BLAST_API UBlastMeshComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()
public:
	UBlastMeshComponent(const FObjectInitializer& ObjectInitializer);

protected:
	UPROPERTY(EditAnywhere, Category = "BlastMesh", meta = (DisplayThumbnail = "true"))
	UBlastMesh*						BlastMesh;

	//Usually these are the same object, but in the case where the modified asset happens to not actually need modifications it's useful to be able to reference the asset directly,
	//and that requires a non-Instanced property. This is the same behavior as if ModifiedAsset, but it marks the component as "clean" for Blast Glue build dirtiness.
	//So ModifiedAssetOwned holds the object which can be serialized inline as if it's a one-off instance.
	UPROPERTY(VisibleAnywhere, Instanced, Category = "BlastMesh", AdvancedDisplay)
	UBlastAsset*					ModifiedAssetOwned;

	UPROPERTY(VisibleAnywhere, Category="BlastMesh", AdvancedDisplay)
	UBlastAsset*					ModifiedAsset;

	UPROPERTY(VisibleAnywhere, Category = "BlastMesh", AdvancedDisplay)
	FTransform						ModifiedAssetComponentToWorldAtBake;

	UPROPERTY(VisibleAnywhere, Category = "BlastMesh", AdvancedDisplay)
	ABlastExtendedSupportStructure*	OwningSupportStructure;

	UPROPERTY(VisibleAnywhere, Category = "BlastMesh", AdvancedDisplay)
	int32							OwningSupportStructureIndex;
public:

	UPROPERTY(EditAnywhere, Category = "Blast")
	bool							bSupportedByWorld;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast", meta = (PinHiddenByDefault, InlineEditConditionToggle, CantUseWithExtendedSupport))
	bool							bOverride_BlastMaterial;

	// Blast material (overrides BlastMaterial from UBlastMesh)
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (editcondition = "bOverride_BlastMaterial", CantUseWithExtendedSupport))
	FBlastMaterial					BlastMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast", meta = (PinHiddenByDefault, InlineEditConditionToggle, CantUseWithExtendedSupport))
	bool							bOverride_ImpactDamageProperties;

	// Impact damage properties
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (editcondition = "bOverride_ImpactDamageProperties", CantUseWithExtendedSupport))
	FBlastImpactDamageProperties	ImpactDamageProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast", meta = (PinHiddenByDefault, InlineEditConditionToggle, CantUseWithExtendedSupport))
	bool							bOverride_StressProperties;

	// Stress properties (overrides StressProperties from UBlastMesh)
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (editcondition = "bOverride_StressProperties", CantUseWithExtendedSupport))
	FBlastStressProperties			StressProperties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast", meta = (PinHiddenByDefault, InlineEditConditionToggle, CantUseWithExtendedSupport))
	bool							bOverride_DebrisProperties;

	// Debris properties (overrides DebrisProperties from UBlastMesh)
	UPROPERTY(EditAnywhere, Category = "Blast", meta = (editcondition = "bOverride_DebrisProperties", CantUseWithExtendedSupport))
	FBlastDebrisProperties			DebrisProperties;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = DynamicChunkCollision, meta = (ShowOnlyInnerProperties, CantUseWithExtendedSupport))
	FBodyInstance DynamicChunkBodyInstance;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Blast", AdvancedDisplay, meta=(CantUseWithExtendedSupport))
	EBlastDebugRenderMode			BlastDebugRenderMode;
#endif

	/* Gets the current UBlastAsset - modified if it exists, or the unmodified if not. */
	UBlastAsset*					GetBlastAsset(bool bAllowModifiedAsset = true) const;

	UBlastMesh*						GetBlastMesh() const { return BlastMesh;  }
	void SetBlastMesh(UBlastMesh* NewBlastMesh);

	UBlastAsset*					GetModifiedAsset() const { return ModifiedAsset; }
	void SetModifiedAsset(UBlastAsset* newModifiedAsset);

	//You probably shouldn't call this directly. Instead use the Add/Remove methods on ABlastExtendedSupportStructure
	void SetOwningSuppportStructure(ABlastExtendedSupportStructure* NewStructure, int32 Index);
	void MarkDirtyOwningSuppportStructure();

#if WITH_EDITOR
	bool IsWorldSupportDirty() const;
	bool IsExtendedSupportDirty() const;
#endif

	ABlastExtendedSupportStructure*	GetOwningSupportStructure() const { return OwningSupportStructure; }
	int32	GetOwningSupportStructureIndex() const { return OwningSupportStructureIndex; }

	const FBlastMaterial&			GetUsedBlastMaterial() const
	{
		return bOverride_BlastMaterial ? BlastMaterial : BlastMesh->BlastMaterial;
	}

	const FBlastImpactDamageProperties& GetUsedImpactDamageProperties() const
	{
		return bOverride_ImpactDamageProperties ? ImpactDamageProperties : BlastMesh->ImpactDamageProperties;
	}

	const FBlastStressProperties&	GetUsedStressProperties() const
	{
		return bOverride_StressProperties || BlastMesh == nullptr ? StressProperties : BlastMesh->StressProperties;
	}

	const FBlastDebrisProperties&	GetUsedDebrisProperties() const
	{
		return bOverride_DebrisProperties || BlastMesh == nullptr ? DebrisProperties : BlastMesh->DebrisProperties;
	}


	///////////////////////////////////////////////////////////////////////////////
	//  Events / Delegates
	///////////////////////////////////////////////////////////////////////////////

	/**
	*	Event called when any actor is damaged. This event always occurs before actor create/destroyed events (split).
	*	But not every damaged events lead to split.
	*/
	UPROPERTY(BlueprintAssignable, Category="Blast")
	FBlastMeshComponentOnDamagedSignature OnDamaged;

	/**
	*	Event called when any new actor is created.
	*/
	UPROPERTY(BlueprintAssignable, Category="Blast")
	FBlastMeshComponentOnActorCreatedSignature OnActorCreated;

	/**
	*	Event called when any actor is about to be destroyed. Actor is still valid in the scope of this event.
	*/
	UPROPERTY(BlueprintAssignable, Category="Blast")
	FBlastMeshComponentOnActorDestroyedSignature OnActorDestroyed;

	/**
	*	Event called when any new actor is created as the result of damage, therefore it contains damage data.
	*/
	UPROPERTY(BlueprintAssignable, Category="Blast")
	FBlastMeshComponentOnActorCreatedFromDamageSignature OnActorCreatedFromDamage;

	/**
	*	Event called when any actor's bonds are damaged. Called per actor.
	*	bIsSplit signals if actor is about to be split (destroyed and new smaller actors are to be created).
	*	IMPORTANT: subscribing to this event adds small overhead to fill all the data. Subscribe only if you need it. 
	*	Use less detailed event like OnDamaged if possible.
	*/
	UPROPERTY(BlueprintAssignable, Category = "Blast")
	FBlastMeshComponentOnBondsDamagedSignature OnBondsDamaged;

	/**
	*	Event called when any actor's chunks are damaged. Called per actor.
	*	Chunk damage happens only below support graph level (also called subsupport damage), so for some assets chunk damage
	*	won't happen at all. For example if support level is on leaf chunks. Chunk damage is usually used for small chunk, debris-like.
	*	bIsSplit signals if actor is about to be split (destroyed and new smaller actors are to be created).
	*	IMPORTANT: subscribing to this event adds small overhead to fill all the data. Subscribe only if you need it.
	*	Use less detailed event like OnDamaged if possible.
	*/
	UPROPERTY(BlueprintAssignable, Category = "Blast")
	FBlastMeshComponentOnChunksDamagedSignature OnChunksDamaged;

	virtual void BroadcastOnDamaged(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType);
	virtual void BroadcastOnActorCreated(FName ActorName);
	virtual void BroadcastOnActorDestroyed(FName ActorName);
	virtual void BroadcastOnActorCreatedFromDamage(FName ActorName, const FVector& DamageOrigin, const FRotator& DamageRot, FName DamageType);
	virtual void BroadcastOnBondsDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FBondDamageEvent>& Events);
	virtual void BroadcastOnChunksDamaged(FName ActorName, bool bIsSplit, FName DamageType, const TArray<FChunkDamageEvent>& Events);
	virtual bool OnBondsDamagedBound() const { return OnBondsDamaged.IsBound(); }
	virtual bool OnChunksDamagedBound() const { return OnChunksDamaged.IsBound(); }

	///////////////////////////////////////////////////////////////////////////////
	//  Damage Functions
	///////////////////////////////////////////////////////////////////////////////

	/**
	NOTE: FRotator is used in Blueprint compatible functions, because it doesn't support FQuat well.
	*/

	/**
	* Apply damage on this component using Damage Program from Damage Component. Damage is applied on all live actors or explicitly passed bone.
	*
	* @param DamageComponent Damage Component to be used
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @param BoneName Bone name to apply damage on particular Bone (Actor), if NAME_None damage is applied on every live actor.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	EBlastDamageResult ApplyDamageComponent(UBlastBaseDamageComponent* DamageComponent, FVector Origin, FRotator Rot = FRotator::ZeroRotator, FName BoneName = NAME_None);

	/**
	* Apply damage on this component using Damage Program from Damage Component. Damage is applied on all live actors inside of overlap collision shape from FBlastBaseDamageProgram. 
	*
	* @param DamageComponent Damage Component to be used
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	EBlastDamageResult ApplyDamageComponentOverlap(UBlastBaseDamageComponent* DamageComponent, FVector Origin, FRotator Rot = FRotator::ZeroRotator);

	/**
	* Apply damage on all BlastMeshComponents inside of overlap collision shape from FBlastBaseDamageProgram using Damage Program from Damage Component. 
	*
	* @param DamageComponent Damage Component to be used
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	static EBlastDamageResult ApplyDamageComponentOverlapAll(UBlastBaseDamageComponent* DamageComponent, FVector Origin, FRotator Rot = FRotator::ZeroRotator);

	/**
	* Execute Damage Program on this component. Damage is applied on all live actors or explicitly passed bone.
	*
	* @param DamageProgram Damage Program to be executed
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @param BoneName Bone name to apply damage on particular Bone (Actor), if NAME_None damage is applied on every live actor.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	EBlastDamageResult ApplyDamageProgram(const FBlastBaseDamageProgram& DamageProgram, FVector Origin, FQuat Rot = FQuat::Identity, FName BoneName = NAME_None);

	/**
	* Execute Damage Program on this component. Damage is applied on all live actors inside of overlap collision shape from FBlastBaseDamageProgram.
	*
	* @param DamageProgram Damage Program to be executed
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	EBlastDamageResult ApplyDamageProgramOverlap(const FBlastBaseDamageProgram& DamageProgram, FVector Origin, FQuat Rot = FQuat::Identity);

	/**
	* Execute Damage Program on all BlastMeshComponents inside of overlap collision shape from FBlastBaseDamageProgram.
	*
	* @param DamageProgram Damage Program to be executed
	* @param Origin Damage origin
	* @param Rot Damage rotation
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	static EBlastDamageResult ApplyDamageProgramOverlapAll(const FBlastBaseDamageProgram& DamageProgram, FVector Origin, FQuat Rot = FQuat::Identity);

	/**
	* Apply sphere-shaped damage on this component. BlastRadialDamageProgram is used.
	*
	* @param Origin Damage origin
	* @param MinRadius Damage min radius. Damage is maximum inside of this radius.
	* @param MinRadius Damage max radius. Damage linearly falloffs to 0 from min to max radius.
	* @param Damage Damage value in health units.
	* @param ImpulseStrength Impulse to apply after actor splitting.
	* @param bImpulseVelChange If true, the impulse will ignore mass of objects and will always result in a fixed velocity change.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	EBlastDamageResult ApplyRadialDamage(FVector Origin, float MinRadius, float MaxRadius, float Damage = 100.0f, float ImpulseStrength = 0.0f, bool bImpulseVelChange = true);

	/**
	* Apply sphere-shaped damage on all BlastMeshComponents inside of sphere overlap. BlastRadialDamageProgram is used.
	*
	* @param Origin Damage origin
	* @param MinRadius Damage min radius. Damage is maximum inside of this radius.
	* @param MinRadius Damage max radius. Damage linearly falloffs to 0 from min to max radius.
	* @param Damage Damage value in health units.
	* @param ImpulseStrength Impulse to apply after actor splitting.
	* @param bImpulseVelChange If true, the impulse will ignore mass of objects and will always result in a fixed velocity change.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	static EBlastDamageResult ApplyRadialDamageAll(FVector Origin, float MinRadius, float MaxRadius, float Damage = 100.0f, float ImpulseStrength = 0.0f, bool bImpulseVelChange = true);

	/**
	* Apply capsule-shaped damage on this component. BlastCapsuleDamageProgram is used.
	*
	* @param Origin Damage origin
	* @param Rot Capsule rotation
	* @param HalfHeight Capsule half height
	* @param MinRadius Damage min radius. Damage is maximum inside of this radius.
	* @param MinRadius Damage max radius. Damage linearly falloffs to 0 from min to max radius.
	* @param Damage Damage value in health units.
	* @param ImpulseStrength Impulse to apply after actor splitting.
	* @param bImpulseVelChange If true, the impulse will ignore mass of objects and will always result in a fixed velocity change.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	EBlastDamageResult ApplyCapsuleDamage(FVector Origin, FRotator Rot, float HalfHeight, float MinRadius, float MaxRadius, float Damage = 100.0f, float ImpulseStrength = 0.0f, bool bImpulseVelChange = true);

	/**
	* Apply capsule-shaped damage on on all BlastMeshComponents inside of capsule overlap. BlastCapsuleDamageProgram is used.
	*
	* @param Origin Damage origin
	* @param Rot Capsule rotation
	* @param HalfHeight Capsule half height
	* @param MinRadius Damage min radius. Damage is maximum inside of this radius.
	* @param MinRadius Damage max radius. Damage linearly falloffs to 0 from min to max radius.
	* @param Damage Damage value in health units.
	* @param ImpulseStrength Impulse to apply after actor splitting.
	* @param bImpulseVelChange If true, the impulse will ignore mass of objects and will always result in a fixed velocity change.
	* @return EBlastDamageResult Damage result enum. @see EBlastDamageResult
	*/
	UFUNCTION(BlueprintCallable, Category = "Blast")
	static EBlastDamageResult ApplyCapsuleDamageAll(FVector Origin, FRotator Rot, float HalfHeight, float MinRadius, float MaxRadius, float Damage = 100.0f, float ImpulseStrength = 0.0f, bool bImpulseVelChange = true);

	/* Directly executes LL Blast damage program. To be used by BlastDamagePrograms. */
	bool ExecuteBlastDamageProgram(uint32 actorIndex, const NvBlastDamageProgram& program, const NvBlastExtProgramParams& programParams, FName DamageType);


	///////////////////////////////////////////////////////////////////////////////
	//  Helpers
	///////////////////////////////////////////////////////////////////////////////

	static const FName ActorBaseName;

	static FName ActorIndexToActorName(int32 ActorIndex)
	{
		if (ActorIndex == INDEX_NONE)
		{
			return NAME_None;
		}
		//Use the base name to avoid the char* -> name index lookup each time
		return FName(ActorBaseName, ActorIndex);
	}

	static int32 ActorNameToActorIndex(const FName& Name)
	{
		//Compare only the non-number part
		if (ActorBaseName.GetComparisonIndex() == Name.GetComparisonIndex())
		{
			return Name.GetNumber();
		}
		return INDEX_NONE;
	}

	FBodyInstance* GetActorBodyInstance(uint32 ActorIndex) const;

	FBodyInstance* GetActorBodyInstance(FName ActorName) const
	{
		return GetActorBodyInstance(ActorNameToActorIndex(ActorName));
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FName GetActorBoneName(FName ActorName) const
	{
		//They are the same now, but possibly could change in the future
		return ActorName;
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FTransform GetActorWorldTransform(FName ActorName) const;

	FTransform GetActorWorldTransform(uint32 ActorIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetActorCOMWorldPosition(FName ActorName) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FBox GetActorWorldBounds(FName ActorName) const;

	DEPRECATED(4.18, "Use GetActorWorldAngularVelocityInDegrees instead.")
	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetActorWorldAngularVelocity(FName ActorName) const
	{
		return GetActorWorldAngularVelocityInDegrees(ActorName);
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetActorWorldAngularVelocityInDegrees(FName ActorName) const
	{
		return FVector::RadiansToDegrees(GetActorWorldAngularVelocityInRadians(ActorName));
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetActorWorldAngularVelocityInRadians(FName ActorName) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetActorWorldVelocity(FName ActorName) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	float GetActorMass(FName ActorName) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FName GetActorForChunk(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	int32 HasChunkInSphere(FVector center, float radius) const;


	int32 GetActorIndexForChunk(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FTransform GetChunkWorldTransform(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FTransform GetChunkActorRelativeTransform(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetChunkCenterWorldPosition(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FBoxSphereBounds GetChunkWorldBounds(int32 ChunkIndex) const;

	DEPRECATED(4.18, "Use GetChunkWorldAngularVelocityInDegrees instead.")
	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetChunkWorldAngularVelocity(int32 ChunkIndex) const
	{
		return GetChunkWorldAngularVelocityInDegrees(ChunkIndex);
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetChunkWorldAngularVelocityInDegrees(int32 ChunkIndex) const
	{
		return FVector::RadiansToDegrees(GetChunkWorldAngularVelocityInRadians(ChunkIndex));
	}

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetChunkWorldAngularVelocityInRadians(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	FVector GetChunkWorldVelocity(int32 ChunkIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	void SetDynamicChunkCollisionEnabled(ECollisionEnabled::Type NewType);
	
	UFUNCTION(BlueprintCallable, Category = "Blast")
	void SetDynamicChunkCollisionProfileName(FName InCollisionProfileName);

	/** Get the collision profile name */
	UFUNCTION(BlueprintPure, Category = "Blast")
	FName GetDynamicChunkCollisionProfileName() const;

	UFUNCTION(BlueprintCallable, Category = "Blast")
	void SetDynamicChunkCollisionObjectType(ECollisionChannel Channel);

	UFUNCTION(BlueprintCallable, Category = "Blast")
	void SetDynamicChunkCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
		
	UFUNCTION(BlueprintCallable, Category = "Blast")
	void SetDynamicChunkCollisionResponseToAllChannels(ECollisionResponse NewResponse);

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditComponentMove(bool bFinished) override;
	virtual void CheckForErrors() override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;

	/* Return indices of all support chunks that overlap the specified volume. This should really only be called by Blast glue build, and before the mesh is fractured. */
	bool GetSupportChunksInVolumes(const TArray<class ABlastGlueVolume*>& Volumes, TArray<uint32>& OverlappingChunks, TArray<FVector>& GlueVectors, TSet<class ABlastGlueVolume*>& OverlappingVolumes, bool bDrawDebug);
#endif

	/**
	* Called after importing property values for this object (paste, duplicate or .t3d import)
	* Allow the object to perform any cleanup for properties which shouldn't be duplicated or
	* are unsupported by the script serialization
	*/
	virtual void PostEditImport() override;

	virtual class UBodySetup* GetBodySetup() override;
	virtual FBodyInstance* GetBodyInstance(FName BoneName = NAME_None, bool bGetWelded = true) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual bool HasAnySockets() const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual void BeginPlay() override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;

	virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const override;

	virtual void AddRadialImpulse(FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange = false) override;
	virtual void AddRadialForce(FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bAccelChange = false) override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void SetChunkVisible(int32 ChunkIndex, bool bInVisible);
	bool IsChunkVisible(int32 ChunkIndex) const;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) override;

	friend class FBlastMeshComponentInstanceData;
	virtual class FActorComponentInstanceData* GetComponentInstanceData() const override;

	//We don't actually store static lighting data, but it's a good hook to know when our glue data is out of date
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;

	virtual bool ShouldRenderSelected() const override;

protected:
	void RefreshDynamicChunkBodyInstanceFromBodyInstance();

	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	
	virtual bool AllocateTransformData() override;

	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;

	virtual void OnRegister() override;

	virtual bool ShouldUpdateTransform(bool bLODHasChanged) const override;
	virtual bool ShouldTickPose() const override;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

	// A map from the actors internal index to an array of visible chunk indices belonging to the actor in that slot. NOTE: These are Blast chunk indices and so must go through indirection
	struct FActorChunkData
	{
		uint32 ChunkIndex;
	};
	struct FActorData
	{
		NvBlastActor* BlastActor;
		FBodyInstance* BodyInstance;
		FTransform PreviousBodyWorldTransform;
		TArray<FActorChunkData, TInlineAllocator<1>> Chunks;
		bool bIsAttachedToComponent;
		FTimerHandle TimerHandle;
		FVector StartLocation;

		FActorData() : BlastActor(nullptr), BodyInstance(nullptr), bIsAttachedToComponent(false) {}
	};
	//These are indexed by the blast actor index
	TArray<FActorData>					BlastActors;
	int32								BlastActorsBeginLive, BlastActorsEndLive;

	/* The root "family" of this mesh component. */
	TSharedPtr<NvBlastFamily>			BlastFamily;

	// Stress solver 
	Nv::Blast::ExtStressSolver*			StressSolver;

	EBlastDamageResult ApplyDamageOnActor(uint32 actorIndex, const FBlastBaseDamageProgram& DamageProgram, const FVector& Origin, const FQuat& Rot, bool bAssumeLocked);
	static EBlastDamageResult ApplyDamageProgramOverlapFiltered(UBlastMeshComponent* mesh, const FBlastBaseDamageProgram& DamageProgram, const FVector& Origin, const FQuat& Rot);

	void ApplyFracture(uint32 actorIndex, const NvBlastFractureBuffers& fractureBuffers, FName DamageType);

	struct FBlastActorCreateInfo
	{
		FTransform	Transform;
		FVector		ParentActorLinVel;
		FVector		ParentActorAngVel; // In Radians
		FVector		ParentActorCOM;

		FBlastActorCreateInfo(const FTransform& Transform_) : Transform(Transform_) {}
	};

	void SetupNewBlastActor(NvBlastActor* actor, const FBlastActorCreateInfo& CreateInfo, const FBlastBaseDamageProgram* DamageProgram = nullptr, const FBlastBaseDamageProgram::FInput* Input = nullptr, FName DamageType = FName());
	virtual void ShowActorsVisibleChunks(uint32 actorIndex);
	void BreakDownBlastActor(uint32 actorIndex);
	virtual void HideActorsVisibleChunks(uint32 actorIndex);

	/*
		Match up the visible chunks with their physics representations
	*/
	bool SyncChunksAndBodies();

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void UpdateFractureBufferSize();

	void TickStressSolver();
	
	void UpdateDebris();
	void UpdateDebris(int32 AcotrIndex, const FTransform& ActorTransform);

#if WITH_EDITOR
	void DrawDebugChunkCentroids();
	void DrawDebugSupportGraph();
	void DrawDebugStressGraph();

	TArray<FBatchedLine> PendingDebugLines;
	TArray<FBatchedPoint> PendingDebugPoints;

	void DrawDebugLine(FVector const& LineStart, FVector const& LineEnd, FColor const& Color, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugBox(FVector const& Center, FVector const& Extent, FColor const& Color, uint8 DepthPriority = 0, float Thickness = 0.f);
	void DrawDebugPoint(FVector const& Position, float Size, FColor const& PointColor, uint8 DepthPriority = 0);
#endif

	void InitBlastFamily();
	void UninitBlastFamily();
	void ShowRootChunks();
	void InitBodyForActor(FActorData& ActorData, uint32 ActorIndex, const FTransform& ParentActorWorldTransform, FPhysScene* PhysScene);

	bool HandlePostDamage(NvBlastActor* actor, FName DamageType, const FBlastBaseDamageProgram* DamageProgram = nullptr, const FBlastBaseDamageProgram::FInput* Input = nullptr, bool bAssumeReadLocked = false);
	void FillInitialComponentSpaceTransformsFromMesh();

	void RebuildChunkVisibility();

	TBitArray<>					ChunkVisibility;
	TArray<int32>				ChunkToActorIndex;

	uint32 DepthCount; // max chunk depth in support graph

	uint32 DebrisCount; // number of BlastActors marked as "debris" (BlastActor with active timer)

	// Buffer damage event to fire them right before splitting
	struct DamageEventsBuffer
	{
		uint32						ActorIndex;
		FName						DamageType;
		TArray<FBondDamageEvent>	BondEvents;
		TArray<FChunkDamageEvent>	ChunkEvents;

		void Reset()
		{
			BondEvents.SetNumUnsafeInternal(0);
			ChunkEvents.SetNumUnsafeInternal(0);
		}
	};
	DamageEventsBuffer			RecentDamageEventsBuffer;

	//These are stored in the body instance by a weak pointer so we keep a reference here to keep them alive
	UPROPERTY(Transient, DuplicateTransient)
	TArray<UBodySetup*>	ActorBodySetups;

	bool						bAddedOrRemovedActorSinceLastRefresh;
	bool						bChunkVisibilityChanged;

	class FBlastMeshSceneProxyBase* BlastProxy;

	physx::PxScene* GetPXScene() const;

};

class BLAST_API FBlastMeshSceneProxyBase
{
public:
	FBlastMeshSceneProxyBase(const UBlastMeshComponent* Component) : BlastMeshForDebug(Component->GetBlastMesh()) {}

	/**
	* Render physics asset for debug display
	*/
	void RenderPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags, const FMatrix& ProxyLocalToWorld, const TArray<FTransform>* BoneSpaceBases) const;

	void UpdateVisibleChunks(TArray<int32>&& VisibleChunks)
	{
		VisibleChunkIndices = MoveTemp(VisibleChunks);
	}

#if WITH_EDITOR
	void UpdateDebugDrawLines(TArray<FBatchedLine>&& NewDebugDrawLines, TArray<FBatchedPoint>&& NewDebugDrawPoints)
	{
		DebugDrawLines = MoveTemp(NewDebugDrawLines);
		DebugDrawPoints = MoveTemp(NewDebugDrawPoints);
	}

	void RenderDebugLines(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const;
#endif

private:
	const UBlastMesh* BlastMeshForDebug;
	TArray<int32> VisibleChunkIndices;
#if WITH_EDITOR
	TArray<FBatchedLine> DebugDrawLines;
	TArray<FBatchedPoint> DebugDrawPoints;
#endif
};

/**
* Blast mesh component scene proxy.
* Added to debug render collision shapes
*/
class BLAST_API FBlastMeshSceneProxy : public FBlastMeshSceneProxyBase, public FSkeletalMeshSceneProxy
{
public:
	FBlastMeshSceneProxy(const UBlastMeshComponent* Component, FSkeletalMeshResource* InSkelMeshResource);

	/**
	* Render physics asset for debug display
	*/
	virtual void DebugDrawPhysicsAsset(int32 ViewIndex, FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags) const override;

#if WITH_EDITOR
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		FSkeletalMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		RenderDebugLines(Views, ViewFamily, VisibilityMap, Collector);
	}
#endif
};
