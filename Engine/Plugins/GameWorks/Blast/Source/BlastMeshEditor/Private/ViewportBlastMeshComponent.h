#pragma once

#include "BlastMeshComponent.h"
#include "ViewportBlastMeshComponent.generated.h"


UCLASS()
class BLASTMESHEDITOR_API UViewportBlastMeshComponent : public UBlastMeshComponent
{
	GENERATED_BODY()

public:
	UViewportBlastMeshComponent(const FObjectInitializer& ObjectInitializer);

	FBox GetChunkWorldBounds(int32 ChunkIndex) const;
	int32 GetChunkWorldHit(const FVector& Start, const FVector& End, FVector& ClickedChunkHitLoc, FVector& ClickedChunkHitNorm) const;
	void SetChunkLocation(int32 ChunkIndex, FVector newLocation);
	void SetChunkLocationWorldspace(int32 ChunkIndex, FVector newLocation);
	void ForceBoneTransformUpdate();
	void InitAllActors();
	void BuildChunkDisplacements();

	TArray <FVector> ChunkDisplacements;
};
