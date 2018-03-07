// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "Engine/EngineTypes.h"

namespace ImmediatePhysics
{

/** handle associated with a physics actor. This is the proper way to read/write to the physics simulation */
struct IMMEDIATEPHYSICS_API FActorHandle
{
public:
	/** Sets the world transform.*/
	void SetWorldTransform(const FTransform& WorldTM)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).body2World = U2PTransform(ActorToBody * WorldTM);
#endif
	}

	/** Sets the kinematic target. This will affect velocities as expected*/
	void SetKinematicTarget(const FTransform& WorldTM)
	{
#if WITH_PHYSX
		FKinematicTarget& KinematicTarget = OwningSimulation.GetKinematicTarget(ActorDataIndex);
		KinematicTarget.BodyToWorld = U2PTransform(ActorToBody * WorldTM);
		KinematicTarget.bTargetSet = true;
#endif
	}

	/** Whether the body is simulating */
	bool IsSimulated() const
	{
		return OwningSimulation.IsSimulated(ActorDataIndex);
	}

	/** Get the world transform */
	FTransform GetWorldTransform() const
	{
#if WITH_PHYSX
		return ActorToBody.GetRelativeTransformReverse( P2UTransform(OwningSimulation.GetLowLevelBody(ActorDataIndex).body2World) );
#else
		return FTransform::Identity;
#endif
	}

	/** Set the linear velocity */
	void SetLinearVelocity(const FVector& NewLinearVelocity)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).linearVelocity = U2PVector(NewLinearVelocity);
#endif
	}

	/** Get the linear velocity */
	FVector GetLinearVelocity() const
	{
#if WITH_PHYSX
		return P2UVector(OwningSimulation.GetLowLevelBody(ActorDataIndex).linearVelocity);
#else
		return FVector::ZeroVector;
#endif
	}

	/** Set the angular velocity */
	void SetAngularVelocity(const FVector& NewAngularVelocity)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).angularVelocity = U2PVector(NewAngularVelocity);
#endif
	}

	/** Get the angular velocity */
	FVector GetAngularVelocity() const
	{
#if WITH_PHYSX
		return P2UVector(OwningSimulation.GetLowLevelBody(ActorDataIndex).angularVelocity);
#else
		return FVector::ZeroVector;
#endif
	}

	void AddForce(const FVector& Force)
	{
#if WITH_PHYSX
		OwningSimulation.AddForce(ActorDataIndex, Force);
#endif
	}

	void AddRadialForce(const FVector& Origin, float Strength, float Radius, ERadialImpulseFalloff Falloff, FSimulation::EForceType ForceType)
	{
#if WITH_PHYSX
		OwningSimulation.AddRadialForce(ActorDataIndex, Origin, Strength, Radius, Falloff, ForceType);
#endif
	}

	/** Set the linear damping*/
	void SetLinearDamping(float NewLinearDamping)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).linearDamping = NewLinearDamping;
#endif
	}

	/** Get the linear damping*/
	float GetLinearDamping() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).linearDamping;
#else
		return 0.f
#endif
	}

	/** Set the angular damping*/
	void SetAngularDamping(float NewAngularDamping)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).angularDamping = NewAngularDamping;
#endif
	}

	/** Get the angular damping*/
	float GetAngularDamping() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).angularDamping;
#else
		return 0.f
#endif
	}

	/** Set the max linear velocity squared*/
	void SetMaxLinearVelocitySquared(float NewMaxLinearVelocitySquared)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).maxLinearVelocitySq = NewMaxLinearVelocitySquared;
#endif
	}

	/** Get the max linear velocity squared*/
	float GetMaxLinearVelocitySquared() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).maxLinearVelocitySq;
#else
		return 0.f
#endif
	}

	/** Set the max angular velocity squared*/
	void SetMaxAngularVelocitySquared(float NewMaxAngularVelocitySquared)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).maxAngularVelocitySq = NewMaxAngularVelocitySquared;
#endif
	}

	/** Get the max angular velocity squared*/
	float GetMaxAngularVelocitySquared() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).maxAngularVelocitySq;
#else
		return 0.f
#endif
	}

	/** Set the inverse mass. 0 indicates kinematic object */
	void SetInverseMass(float NewInverseMass)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).invMass = NewInverseMass;
#endif
	}

	/** Get the inverse mass. */
	float GetInverseMass() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).invMass;
#else
		return 0.f
#endif
	}

	/** Set the inverse inertia. Mass-space inverse inertia diagonal vector */
	void SetInverseInertia(const FVector& NewInverseInertia)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).invInertia = U2PVector(NewInverseInertia);
#endif
	}

	/** Get the inverse inertia. Mass-space inverse inertia diagonal vector */
	FVector GetInverseInertia() const
	{
#if WITH_PHYSX
		return P2UVector(OwningSimulation.GetLowLevelBody(ActorDataIndex).invInertia);
#else
		return 0.f
#endif
	}

	/** Set the max depenetration velocity*/
	void SetMaxDepenetrationVelocity(float NewMaxDepenetrationVelocity)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).maxDepenetrationVelocity = NewMaxDepenetrationVelocity;
#endif
	}

	/** Get the max depenetration velocity*/
	float GetMaxDepenetrationVelocity(float NewMaxDepenetrationVelocity) const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).maxDepenetrationVelocity;
#else
		return 0.f
#endif
	}

	/** Set the max contact impulse*/
	void SetMaxContactImpulse(float NewMaxContactImpulse)
	{
#if WITH_PHYSX
		OwningSimulation.GetLowLevelBody(ActorDataIndex).maxContactImpulse = NewMaxContactImpulse;
#endif
	}

	/** Get the max contact impulse*/
	float GetMaxContactImpulse() const
	{
#if WITH_PHYSX
		return OwningSimulation.GetLowLevelBody(ActorDataIndex).maxContactImpulse;
#else
		return 0.f
#endif
	}

private:
	/** Converts from actor space (i.e. the transform in world space as the client gives us) to body space (body with its origin at the COM and oriented to inertia tensor) */
	FTransform ActorToBody;
	FSimulation& OwningSimulation;
	int32 ActorDataIndex;

	friend FSimulation;
	FActorHandle(FSimulation& InOwningSimulation, int32 InActorDataIndex)
		: ActorToBody(FTransform::Identity)
		, OwningSimulation(InOwningSimulation)
		, ActorDataIndex(InActorDataIndex)
	{
	}

	~FActorHandle()
	{
	}

	FActorHandle(const FActorHandle& );	//Ensure no copying of handles
};


}