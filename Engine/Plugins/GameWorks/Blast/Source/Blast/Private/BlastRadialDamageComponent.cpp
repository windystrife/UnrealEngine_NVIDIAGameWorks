#include "BlastRadialDamageComponent.h"
#include "BlastScratch.h"
#include "NvBlastGlobals.h"
#include "NvBlast.h"
#include "NvBlastExtDamageShaders.h"
#include "BlastMeshComponent.h"

#include "GameFramework/Actor.h"


UBlastRadialDamageComponent::UBlastRadialDamageComponent(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer),
	Damage(100.0f),
	MinRadius(100.0f),
	MaxRadius(100.0f),
	ForceComponent(nullptr)
{
	bWantsInitializeComponent = true;
	DamageProgram.DamageComponent = this;
}

void UBlastRadialDamageComponent::InitializeComponent()
{
	auto radial = this->GetOwner()->FindComponentByClass<URadialForceComponent>();

	ForceComponent = radial;

	Super::InitializeComponent();
}


bool RadialDamageProgramWithForce::Execute(uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const
{
	NvBlastExtRadialDamageDesc damage[] = {
		{
			DamageComponent->Damage, { input.localOrigin.X, input.localOrigin.Y, input.localOrigin.Z }, DamageComponent->MinRadius, DamageComponent->MaxRadius
		}
	};

	NvBlastExtProgramParams programParams(damage, input.material);

	NvBlastDamageProgram program = {
		NvBlastExtFalloffGraphShader,
		NvBlastExtFalloffSubgraphShader
	};

	return owner.ExecuteBlastDamageProgram(actorIndex, program, programParams, TEXT("Damage Component"));
}

void RadialDamageProgramWithForce::ExecutePostSplit(const FInput& input, UBlastMeshComponent& owner) const
{
	if (DamageComponent->bAddPhysicsImpulse && DamageComponent->ForceComponent!=nullptr)
	{
		DamageComponent->ForceComponent->FireImpulse();
	}
}

FCollisionShape RadialDamageProgramWithForce::GetCollisionShape() const
{
	return FCollisionShape::MakeSphere(DamageComponent->MaxRadius);
}
