#include "ViewportBlastMeshComponent.h"
#include "UnrealMathUtility.h"
#include "PhysicsEngine/PhysicsAsset.h"

UViewportBlastMeshComponent::UViewportBlastMeshComponent(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer)	
{
	//disable debris processing
	bOverride_DebrisProperties = true;
	DebrisProperties.DebrisFilters.Empty();
}

void UViewportBlastMeshComponent::SetChunkLocation(int32 ChunkIndex, FVector newLocation)
{
	const int32 BoneIndex = BlastMesh->ChunkIndexToBoneIndex[ChunkIndex];
	const FTransform& ComponentSpaceTransform = BlastMesh->GetComponentSpaceInitialBoneTransform(BoneIndex);
	newLocation = ComponentSpaceTransform.GetRotation().RotateVector(newLocation);
	GetEditableComponentSpaceTransforms()[BoneIndex].SetTranslation(newLocation);
	GetEditableComponentSpaceTransforms()[BoneIndex].SetRotation(FQuat::Identity);
}

void UViewportBlastMeshComponent::SetChunkLocationWorldspace(int32 ChunkIndex, FVector newLocation)
{
	const FVector BoneTranslation = GetComponentTransform().InverseTransformPosition(newLocation);
	SetChunkLocation(ChunkIndex, BoneTranslation);
}

void UViewportBlastMeshComponent::ForceBoneTransformUpdate()
{
	bAddedOrRemovedActorSinceLastRefresh = true;
	bNeedToFlipSpaceBaseBuffers = true;

	RefreshBoneTransforms(nullptr);

	DoDeferredRenderUpdates_Concurrent();
}

void UViewportBlastMeshComponent::InitAllActors()
{
	if (ActorBodySetups.Num() == 0)
	{
		return;
	}

	for (int32 A = 0; A < ActorBodySetups.Num(); A++)
	{
		if (ActorBodySetups[A] != nullptr)
		{
			BreakDownBlastActor(A);
		}
	}

	const int32 ChunkCount = GetBlastAsset()->GetChunkCount();
	ActorBodySetups.SetNumZeroed(ChunkCount);
	BlastActors.SetNum(ChunkCount);
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
	{
		FActorChunkData VisibleChunk;
		VisibleChunk.ChunkIndex = ChunkIndex;
		BlastActors[ChunkIndex].Chunks.Add(VisibleChunk);
		InitBodyForActor(BlastActors[ChunkIndex], ChunkIndex, GetComponentTransform(), GetWorld()->GetPhysicsScene());
	}
	BlastActorsBeginLive = 0;
	BlastActorsEndLive = ChunkCount;
}

void UViewportBlastMeshComponent::BuildChunkDisplacements()
{
	if (BlastFamily.IsValid())
	{
		const int32 ChunkCount = GetBlastAsset()->GetChunkCount();
		FVector rootCenter(0.f);
		uint32 numRoots = 0;
		ChunkDisplacements.SetNum(ChunkCount);
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
		{
			//Use the uncooked body setups since they are not pre-transformed so it's simpler
			FName ChunkBone = BlastMesh->GetChunkIndexToBoneName()[ChunkIndex];
			int32 BodyIndex = BlastMesh->PhysicsAsset->FindBodyIndex(ChunkBone);
			if (ChunkBone != NAME_None && BlastMesh->PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex))
			{
				const UBodySetup* BodySetup = BlastMesh->PhysicsAsset->SkeletalBodySetups[BodyIndex];
				if (BodySetup != nullptr)
				{
					int32 BoneIndex = BlastMesh->ChunkIndexToBoneIndex[ChunkIndex];

					//Use ref pose to not take into account existing displacement
					ChunkDisplacements[ChunkIndex] = BodySetup->AggGeom.CalcAABB(BlastMesh->GetComponentSpaceInitialBoneTransform(BoneIndex)).GetCenter();

					if (BlastMesh->GetChunkInfo(ChunkIndex).parentChunkIndex == INDEX_NONE)
					{
						rootCenter += ChunkDisplacements[ChunkIndex];
						numRoots++;
					}
				}
			}
		}
		if (numRoots)
		{
			rootCenter /= numRoots;
		}
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ChunkIndex++)
		{
			ChunkDisplacements[ChunkIndex] -= rootCenter;
		}
	}
}

FBox UViewportBlastMeshComponent::GetChunkWorldBounds(int32 ChunkIndex) const
{
	if (BlastFamily.IsValid())
	{
		//Use the uncooked body setups since they are not pre-transformed so it's simpler
		FName ChunkBone = BlastMesh->GetChunkIndexToBoneName()[ChunkIndex];
		int32 BodyIndex = BlastMesh->PhysicsAsset->FindBodyIndex(ChunkBone);
		if (ChunkBone != NAME_None && BlastMesh->PhysicsAsset->SkeletalBodySetups.IsValidIndex(BodyIndex))
		{
			const UBodySetup* BodySetup = BlastMesh->PhysicsAsset->SkeletalBodySetups[BodyIndex];
			if (BodySetup != nullptr)
			{
				int32 BoneIndex = BlastMesh->ChunkIndexToBoneIndex[ChunkIndex];
				return BodySetup->AggGeom.CalcAABB(GetBoneTransform(BoneIndex));
			}
		}
	}
	return FBox();
}

int32 UViewportBlastMeshComponent::GetChunkWorldHit(const FVector& Start, const FVector& End, FVector& ClickedChunkHitLoc, FVector& ClickedChunkHitNorm) const
{
	int32 ClickedChunk = INDEX_NONE;
	if (BlastFamily.IsValid())
	{
		float NearestHitDistance = FLT_MAX;
		
		FHitResult Hit;
		for (uint32 ChunkIndex = 0; ChunkIndex < BlastMesh->GetChunkCount(); ++ChunkIndex)
		{
			if (IsChunkVisible(ChunkIndex))
			{
				int32 BoneIndex = BlastMesh->ChunkIndexToBoneIndex[ChunkIndex];
				auto BodyInstance = GetActorBodyInstance(ChunkIndex);
				auto tr = GetBoneTransform(BoneIndex).Inverse() * BlastMesh->GetComponentSpaceInitialBoneTransform(BoneIndex);
				if (BodyInstance != nullptr && BodyInstance->LineTrace(Hit, tr.TransformPosition(Start), tr.TransformPosition(End), true, false))
				{
					float dist = (Hit.Location - tr.TransformPosition(Start)).SizeSquared();
					if (dist < NearestHitDistance)
					{
						NearestHitDistance = dist;
						ClickedChunk = ChunkIndex;
						ClickedChunkHitLoc = tr.Inverse().TransformPosition(Hit.Location);
						ClickedChunkHitNorm = tr.Inverse().TransformVector(Hit.Normal);
					}
				}
			}
		}
	}
	return ClickedChunk;
}
