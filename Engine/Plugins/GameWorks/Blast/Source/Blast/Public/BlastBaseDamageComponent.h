#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "BlastBaseDamageComponent.generated.h"

struct NvBlastActor;
struct FBodyInstance;
struct NvBlastExtMaterial;
struct FBlastBaseDamageProgram;


/**
Base Damage Component

This is a base class for Blast Damage Components. 
Damage Component are more blueprint-friendly way to create custom damage programs.
You can create your own damage component and call ApplyDamageComponent on BlastMeshComponent to apply it's damage.
DamageProgram returned provided will be used for it.
They are also used to automatically apply damage OnHit, @see bDamageOnHit below.
*/
UCLASS(abstract)
class BLAST_API UBlastBaseDamageComponent : public USceneComponent
{
	GENERATED_BODY()
public:

	UBlastBaseDamageComponent(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer) { }
	virtual ~UBlastBaseDamageComponent() = default;

	// By setting this to true this component will apply it's damage automatically on BlastMeshComponent if hits it. 
	// Also if this DamageComponent is attached to BlastMeshComponent's Actor then it will apply damage on itself on hit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Blast")
	bool					bDamageOnHit;

	// Damage Program to be used when this Damage Component is applied on BlastMeshComponent.
	virtual const FBlastBaseDamageProgram* GetDamagePorgram() { return nullptr; }
};