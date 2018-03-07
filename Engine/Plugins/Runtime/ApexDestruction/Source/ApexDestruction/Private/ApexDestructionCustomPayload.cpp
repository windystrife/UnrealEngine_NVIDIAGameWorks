// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ApexDestructionCustomPayload.h"
#include "DestructibleComponent.h"

void FApexDestructionSyncActors::SyncToActors_AssumesLocked(int32 SceneType, const TArray<physx::PxRigidActor*>& RigidActors)
{
#if WITH_APEX
	UDestructibleComponent::UpdateDestructibleChunkTM(RigidActors);
#endif
}

TWeakObjectPtr<UPrimitiveComponent> FApexDestructionCustomPayload::GetOwningComponent() const
{
	return OwningComponent;
}

int32 FApexDestructionCustomPayload::GetItemIndex() const
{
	return ChunkIndex;
}

FName FApexDestructionCustomPayload::GetBoneName() const
{
	if(UDestructibleComponent* RawOwningComponent = OwningComponent.Get())
	{
		return RawOwningComponent->GetBoneName(UDestructibleComponent::ChunkIdxToBoneIdx(ChunkIndex));
	}
	else
	{
		return NAME_None;
	}
}

FBodyInstance* FApexDestructionCustomPayload::GetBodyInstance() const
{
	return OwningComponent.IsValid() ? &OwningComponent->BodyInstance : nullptr;
}
