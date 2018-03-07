// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Components/PrimitiveComponent.h"

#if WITH_PHYSX

namespace physx
{
	class PxRigidActor;
}

struct FCustomPhysXSyncActors
{
	virtual ~FCustomPhysXSyncActors() {}

	/** Update any UE4 data as needed given the actors that moved as a result of the simulation*/
	virtual void SyncToActors_AssumesLocked(int32 SceneType, const TArray<physx::PxRigidActor*>& RigidActors) = 0;

private:
	friend class FPhysScene;
	TArray<physx::PxRigidActor*> Actors;
};

struct FBodyInstance;

struct FCustomPhysXPayload
{
	FCustomPhysXPayload(FCustomPhysXSyncActors* InCustomSyncActors) : CustomSyncActors(InCustomSyncActors)
	{
	}

	virtual ~FCustomPhysXPayload(){}
	
	virtual TWeakObjectPtr<UPrimitiveComponent> GetOwningComponent() const = 0;
	virtual int32 GetItemIndex() const = 0;
	virtual FName GetBoneName() const = 0;
	virtual FBodyInstance* GetBodyInstance() const = 0;

	FCustomPhysXSyncActors* CustomSyncActors;
};

#endif // WITH_PHYSICS
