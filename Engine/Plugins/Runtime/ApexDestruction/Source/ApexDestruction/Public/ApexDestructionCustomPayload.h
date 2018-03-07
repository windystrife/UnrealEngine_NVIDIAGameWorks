// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomPhysXPayload.h"

struct FApexDestructionSyncActors : public FCustomPhysXSyncActors
{
	virtual void SyncToActors_AssumesLocked(int32 SceneType, const TArray<physx::PxRigidActor*>& RigidActors) override;
};

struct FApexDestructionCustomPayload : public FCustomPhysXPayload
{
	FApexDestructionCustomPayload()
		: FCustomPhysXPayload(SingletonCustomSync)
	{
	}

	virtual TWeakObjectPtr<UPrimitiveComponent> GetOwningComponent() const override;

	virtual int32 GetItemIndex() const override;

	virtual FName GetBoneName() const override;

	virtual FBodyInstance* GetBodyInstance() const override;

	/** Index of the chunk this data belongs to*/
	int32 ChunkIndex;
	/** Component owning this chunk info*/
	TWeakObjectPtr<class UDestructibleComponent> OwningComponent;

private:
	friend class FApexDestructionModule;
	static FApexDestructionSyncActors* SingletonCustomSync;
};