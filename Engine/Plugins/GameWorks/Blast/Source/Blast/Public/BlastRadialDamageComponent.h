#pragma once
#include "CoreMinimal.h"

#include "BlastBaseDamageComponent.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "BlastBaseDamageProgram.h"
#include "BlastRadialDamageComponent.generated.h"

class UBlastRadialDamageComponent;

struct RadialDamageProgramWithForce final : public FBlastBaseDamageProgram
{
	virtual bool Execute(uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const override;
	virtual void ExecutePostSplit(const FInput& input, UBlastMeshComponent& owner) const override;
	virtual FCollisionShape GetCollisionShape() const override;

	UBlastRadialDamageComponent* DamageComponent;
};


UCLASS(ClassGroup = Blast, Blueprintable, editinlinenew, meta = (BlueprintSpawnableComponent))
class UBlastRadialDamageComponent : public UBlastBaseDamageComponent
{
	GENERATED_BODY()
public:
	UBlastRadialDamageComponent(const FObjectInitializer& ObjectInitializer);
	virtual ~UBlastRadialDamageComponent() = default;

	// damage value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	float					Damage;

	// inner radius of damage action
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	float					MinRadius;

	// outer radius of damage action
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blast")
	float					MaxRadius;

	// When checked, this will FireImpulse() on a URadialImpulseComponent attached to the same actor as this one.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Blast")
	bool					bAddPhysicsImpulse;

	virtual const FBlastBaseDamageProgram* GetDamagePorgram() override { return &DamageProgram;  }

	virtual void InitializeComponent() override;


private:
	friend struct RadialDamageProgramWithForce;

	RadialDamageProgramWithForce DamageProgram;

	UPROPERTY()
	URadialForceComponent*	ForceComponent;
};
