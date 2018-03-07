// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceState.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "TextureInstanceView.h"
#include "Streaming/TextureStreamingHelpers.h"

class FStreamingTextureLevelContext;
class ULevel;
class UPrimitiveComponent;
class UTexture2D;

// Can be used either for static primitives or dynamic primitives
class FTextureInstanceState : public FTextureInstanceView
{
public:

	// Will also remove bounds
	bool AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext);
	bool AddComponentFast(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext);

	FORCEINLINE bool HasComponentReferences(const UPrimitiveComponent* Component) const { return ComponentMap.Contains(Component); }
	void RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures);
	bool RemoveComponentReferences(const UPrimitiveComponent* Component);

	void GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components) const;

	void UpdateBounds(const UPrimitiveComponent* Component);
	bool UpdateBounds(int32 BoundIndex);
	bool ConditionalUpdateBounds(int32 BoundIndex);
	void UpdateLastRenderTime(int32 BoundIndex);

	uint32 GetAllocatedSize() const;

	// Generate the compiled elements.
	int32 CompileElements();
	int32 CheckRegistrationAndUnpackBounds(TArray<const UPrimitiveComponent*>& RemovedComponents);

	/** Move around one bound to free the last bound indices. This allows to keep the number of dynamic bounds low. */
	bool MoveBound(int32 SrcBoundIndex, int32 DstBoundIndex);
	void TrimBounds();
	void OffsetBounds(const FVector& Offset);

	FORCEINLINE int32 NumBounds() const { return Bounds4Components.Num(); }
	FORCEINLINE bool HasComponent(int32 BoundIndex) const { return Bounds4Components[BoundIndex] != nullptr; }

private:

	void AddElement(const UPrimitiveComponent* Component, const UTexture2D* Texture, int BoundsIndex, float TexelFactor, bool bForceLoad, int32*& ComponentLink);
	// Returns the next elements using the same component.
	void RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture);

	int32 AddBounds(const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, const UPrimitiveComponent* Component, float LastRenderTime, const FVector4& RangeOrigin, float MinDistance, float MinRange, float MaxRange);
	FORCEINLINE int32 AddBounds(const UPrimitiveComponent* Component);
	void RemoveBounds(int32 Index);

private:

	/** 
	 * Components related to each of the Bounds4 elements. This is stored in another array to allow 
	 * passing Bounds4 to the threaded task without loosing the bound components, allowing incremental update.
	 */
	TArray<const UPrimitiveComponent*> Bounds4Components;

	TArray<int32> FreeBoundIndices;
	TArray<int32> FreeElementIndices;

	TMap<const UPrimitiveComponent*, int32> ComponentMap;

	friend class FTextureLinkIterator;
	friend class FTextureIterator;
};

template <typename TTasks>
class FTextureInstanceStateTaskSync
{
public:

	FTextureInstanceStateTaskSync() : State(new FTextureInstanceState()) {}

	FORCEINLINE void Sync()
	{
		Tasks.SyncResults();
	}

	FORCEINLINE FTextureInstanceState* SyncAndGetState()
	{
		Tasks.SyncResults();
		return State.GetReference();
	}

	// Get State but must be constant as async tasks could be reading data.
	FORCEINLINE const FTextureInstanceState* GetState() const
	{
		return State.GetReference();
	}

	// Used when updating the state, but with no possible reallocation.
	FORCEINLINE FTextureInstanceState* GetStateUnsafe()
	{
		return State.GetReference();
	}

	TTasks& GetTasks() { return Tasks; }
	const TTasks& GetTasks() const { return Tasks; }

private:

	TRefCountPtr<FTextureInstanceState> State;
	TTasks Tasks;
};
