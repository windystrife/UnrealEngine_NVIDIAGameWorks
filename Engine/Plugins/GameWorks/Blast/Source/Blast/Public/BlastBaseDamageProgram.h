#pragma once
#include "CoreMinimal.h"

#include "Vector.h"
#include "WorldCollision.h"

struct NvBlastActor;
struct FBodyInstance;
struct FBlastMaterial;
class UBlastMeshComponent;

namespace Nv
{
namespace Blast
{
class ExtStressSolver;
}
}


/**
Base Struct for Damage Programs to be used with BlastMeshComponent in order to apply damage on it.

Implement your own on demand, look for examples of default ones like 'BlastRadialDamageProgram'.
In order to apply it use UBlastMeshComponent's methods, it can be executed on particular UBlastMeshComponent
or by overlapping area and applying it on all UBlastMeshComponent touched (@see UBlastMeshComponent::ApplyDamageProgramOverlapAll).
*/
struct FBlastBaseDamageProgram
{
public:
	/**
	Default ctor. Sets DamageType to default.
	*/
	FBlastBaseDamageProgram() : DamageType("External") {}

	/**
	Input any damage program takes.

	When Damage Program is executed initially user provides world coordinates of damage origin, rotation (if capsule or something like that)
	and normal (if needed by damage shader). All of them are also transformed into FBodyInstance's local space, so that actual damage shader 
	can use it to apply damage on support graph and individual chunks. All of them are passed as input to be available and boost damage program 
	creativity. 
	*/
	struct FInput
	{
		FVector						worldOrigin;
		FQuat						worldRot;

		FVector						localOrigin;
		FQuat						localRot;

		const FBlastMaterial*		material;
	};


	/**
	Main Execute program function to be implemented by every damage program.

	/return 'true' iff damage was applied and therefore split must be called on actor.
	*/
	virtual bool Execute(uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const = 0;

	/**
	Execute Stress program function. It is called if stress solver is enabled and give opportunity to add forces/impulses to it.

	/return 'true' iff force or impulse was added to stress solver.
	*/
	virtual bool ExecuteStress(Nv::Blast::ExtStressSolver& StressSolver, uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const { return false; }

	/**
	Called if damage was applied on actor (Execute call returned 'true') and split is about to be called.
	*/
	virtual void ExecutePostDamage(uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const  {}

	/**
	Called if split happened.
	*/
	virtual void ExecutePostSplit(const FInput& input, UBlastMeshComponent& owner) const {}

	/**
	Called on each new actor creation after split by this damage program been execute.
	*/
	virtual void ExecutePostActorCreated(uint32 actorIndex, FBodyInstance* actorBody, const FInput& input, UBlastMeshComponent& owner) const {}

	/**
	Collision shape to be used for overlap damage. Program will execute on all actors inside collision shape in that case.
	*/
	virtual FCollisionShape GetCollisionShape() const { return FCollisionShape(); }

	/**
	Damage Type is some sort of damage ID. It is passed into all damage callbacks on actor. Redefine in if you want to separate different kinds of damage.
	*/
	FName DamageType;
};
