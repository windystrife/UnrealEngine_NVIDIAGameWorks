// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureInstanceState.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "Streaming/TextureInstanceState.h"
#include "Components/PrimitiveComponent.h"
#include "Streaming/TextureInstanceView.inl"
#include "Engine/World.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture2D.h"
#include "UObject/UObjectHash.h"
#include "RefCounting.h"

int32 FTextureInstanceState::AddBounds(const UPrimitiveComponent* Component)
{
	return AddBounds(Component->Bounds, PackedRelativeBox_Identity, Component, Component->LastRenderTimeOnScreen, Component->Bounds.Origin, 0, 0, FLT_MAX);
}

int32 FTextureInstanceState::AddBounds(const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, const UPrimitiveComponent* InComponent, float LastRenderTime, const FVector4& RangeOrigin, float MinDistance, float MinRange, float MaxRange)
{
	check(InComponent);

	int BoundsIndex = INDEX_NONE;

	if (FreeBoundIndices.Num())
	{
		BoundsIndex = FreeBoundIndices.Pop();
	}
	else
	{
		BoundsIndex = Bounds4.Num() * 4;
		Bounds4.Push(FBounds4());

		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);
		Bounds4Components.Push(nullptr);

		// Since each element contains 4 entries, add the 3 unused ones
		FreeBoundIndices.Push(BoundsIndex + 3);
		FreeBoundIndices.Push(BoundsIndex + 2);
		FreeBoundIndices.Push(BoundsIndex + 1);
	}

	Bounds4[BoundsIndex / 4].Set(BoundsIndex % 4, Bounds, PackedRelativeBox, LastRenderTime, RangeOrigin, MinDistance, MinRange, MaxRange);
	Bounds4Components[BoundsIndex] = InComponent;

	return BoundsIndex;
}

void FTextureInstanceState::RemoveBounds(int32 BoundsIndex)
{
	check(BoundsIndex != INDEX_NONE);
	checkSlow(!FreeBoundIndices.Contains(BoundsIndex));

	// If note all indices were freed
	if (1 + FreeBoundIndices.Num() != Bounds4.Num() * 4)
	{
		FreeBoundIndices.Push(BoundsIndex);
		Bounds4[BoundsIndex / 4].Clear(BoundsIndex % 4);
		Bounds4Components[BoundsIndex] = nullptr;
	}
	else
	{
		Bounds4.Empty();
		Bounds4Components.Empty();
		FreeBoundIndices.Empty();
	}
}

void FTextureInstanceState::AddElement(const UPrimitiveComponent* InComponent, const UTexture2D* InTexture, int InBoundsIndex, float InTexelFactor, bool InForceLoad, int32*& ComponentLink)
{
	check(InComponent && InTexture);

	FTextureDesc* TextureDesc = TextureMap.Find(InTexture);

	// Since textures are processed per component, if there are already elements for this component-texture,
	// they will be in the first entries (as we push to head). If such pair use the same bound, merge the texel factors.
	// The first step is to find if such a duplicate entries exists.

	if (TextureDesc)
	{
		int32 ElementIndex = TextureDesc->HeadLink;
		while (ElementIndex != INDEX_NONE)
		{
			FElement& TextureElement = Elements[ElementIndex];

			if (TextureElement.Component == InComponent)
			{
				if (TextureElement.BoundsIndex == InBoundsIndex)
				{
					if (InTexelFactor >= 0 && TextureElement.TexelFactor >= 0)
					{
						// Abort inserting a new element, and merge the 2 entries together.
						TextureElement.TexelFactor = FMath::Max(TextureElement.TexelFactor, InTexelFactor);
						TextureElement.bForceLoad |= InForceLoad;
						return;
					}
					else if (InTexelFactor < 0 && TextureElement.TexelFactor < 0)
					{
						// Negative are forced resolution.
						TextureElement.TexelFactor = FMath::Min(TextureElement.TexelFactor, InTexelFactor);
						TextureElement.bForceLoad |= InForceLoad;
						return;
					}
				}

				// Check the next bounds for this component.
				ElementIndex = TextureElement.NextTextureLink;
			}
			else
			{
				break;
			}
		}
	}

	int32 ElementIndex = INDEX_NONE;
	if (FreeElementIndices.Num())
	{
		ElementIndex = FreeElementIndices.Pop();
	}
	else
	{
		ElementIndex = Elements.Num();
		Elements.Push(FElement());
	}

	FElement& Element = Elements[ElementIndex];

	Element.Component = InComponent;
	Element.Texture = InTexture;
	Element.BoundsIndex = InBoundsIndex;
	Element.TexelFactor = InTexelFactor;
	Element.bForceLoad = InForceLoad;

	if (TextureDesc)
	{
		FElement& TextureLinkElement = Elements[TextureDesc->HeadLink];

		// The new inserted element as the head element.
		Element.NextTextureLink = TextureDesc->HeadLink;
		TextureLinkElement.PrevTextureLink = ElementIndex;
		TextureDesc->HeadLink = ElementIndex;
	}
	else
	{
		TextureMap.Add(InTexture, FTextureDesc(ElementIndex, InTexture->LODGroup));
	}

	// Simple sanity check to ensure that the component link passed in param is the right one
	checkSlow(ComponentLink == ComponentMap.Find(InComponent));
	if (ComponentLink)
	{
		// The new inserted element as the head element.
		Element.NextComponentLink = *ComponentLink;
		*ComponentLink = ElementIndex;
	}
	else
	{
		ComponentLink = &ComponentMap.Add(InComponent, ElementIndex);
	}

	// Keep the compiled elements up to date if it was built.
	// This will happen when not all components could be inserted in the incremental build.
	if (HasCompiledElements()) 
	{
		CompiledTextureMap.FindOrAdd(Element.Texture).Add(FCompiledElement(Element.BoundsIndex, Element.TexelFactor, Element.bForceLoad));
	}
}

void FTextureInstanceState::RemoveElement(int32 ElementIndex, int32& NextComponentLink, int32& BoundsIndex, const UTexture2D*& Texture)
{
	FElement& Element = Elements[ElementIndex];
	NextComponentLink = Element.NextComponentLink; 
	BoundsIndex = Element.BoundsIndex; 

	// Removed compiled elements. This happens when a static component is not registered after the level became visible.
	if (HasCompiledElements())
	{
		verify(CompiledTextureMap.FindChecked(Element.Texture).RemoveSingleSwap(FCompiledElement(Element.BoundsIndex, Element.TexelFactor, Element.bForceLoad), false) == 1);
	}

	// Unlink textures
	if (Element.Texture)
	{
		if (Element.PrevTextureLink == INDEX_NONE) // If NONE, that means this is the head of the texture list.
		{
			if (Element.NextTextureLink != INDEX_NONE) // Check if there are other entries for this texture.
			{
				 // Replace the head
				TextureMap.Find(Element.Texture)->HeadLink = Element.NextTextureLink;
				Elements[Element.NextTextureLink].PrevTextureLink = INDEX_NONE;
			}
			else // Otherwise, remove the texture entry
			{
				TextureMap.Remove(Element.Texture);
				CompiledTextureMap.Remove(Element.Texture);
				Texture = Element.Texture;
			}
		}
		else // Otherwise, just relink entries.
		{
			Elements[Element.PrevTextureLink].NextTextureLink = Element.NextTextureLink;

			if (Element.NextTextureLink != INDEX_NONE)
			{
				Elements[Element.NextTextureLink].PrevTextureLink = Element.PrevTextureLink;
			}
		}
	}

	// Clear the element and insert in free list.
	if (1 + FreeElementIndices.Num() != Elements.Num())
	{
		FreeElementIndices.Push(ElementIndex);
		Element = FElement();
	}
	else
	{
		Elements.Empty();
		FreeElementIndices.Empty();
	}
}

bool FTextureInstanceState::AddComponent(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext)
{
	TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
	Component->GetStreamingTextureInfoWithNULLRemoval(LevelContext, TextureInstanceInfos);
	// Texture entries are guarantied to be relevant here, except for bounds if the component is not registered.

	if (!Component->IsRegistered())
	{
		// When the components are not registered, every entry must have a valid PackedRelativeBox since the bound is not reliable.
		// it will not be possible to recreate the bounds correctly.
		for (const FStreamingTexturePrimitiveInfo& Info : TextureInstanceInfos)
		{
			if (!Info.PackedRelativeBox)
			{
				return false;
			}
		}
	}

	if (TextureInstanceInfos.Num())
	{
		const UPrimitiveComponent* LODParent = Component->GetLODParentPrimitive();

		// AddElement will handle duplicate texture-bound-component. Here we just have to prevent creating identical bound-component.
		TArray<int32, TInlineAllocator<12> > BoundsIndices;

		int32* ComponentLink = ComponentMap.Find(Component);
		for (int32 TextureIndex = 0; TextureIndex  < TextureInstanceInfos.Num(); ++TextureIndex)
		{
			const FStreamingTexturePrimitiveInfo& Info = TextureInstanceInfos[TextureIndex];

			int32 BoundsIndex = INDEX_NONE;

			// Find an identical bound, or create a new entry.
			{
				for (int32 TestIndex = TextureIndex - 1; TestIndex >= 0; --TestIndex)
				{
					const FStreamingTexturePrimitiveInfo& TestInfo = TextureInstanceInfos[TestIndex];
					if (Info.Bounds == TestInfo.Bounds && Info.PackedRelativeBox == TestInfo.PackedRelativeBox && BoundsIndices[TestIndex] != INDEX_NONE)
					{
						BoundsIndex = BoundsIndices[TestIndex];
						break;
					}
				}

				if (BoundsIndex == INDEX_NONE)
				{
					// In the engine, the MinDistance is computed from the component bound center to the viewpoint.
					// The streaming computes the distance as the distance from viewpoint to the edge of the texture bound box.
					// The implementation also handles MinDistance by bounding the distance to it so that if the viewpoint gets closer the screen size will stop increasing at some point.
					// The fact that the primitive will disappear is not so relevant as this will be handled by the visibility logic, normally streaming one less mip than requested.
					// The important mather is to control the requested mip by limiting the distance, since at close up, the distance becomes very small and all mips are streamer (even after the 1 mip bias).

					float MinDistance = FMath::Max<float>(0, Component->MinDrawDistance - (Info.Bounds.Origin - Component->Bounds.Origin).Size() - Info.Bounds.SphereRadius);
					float MinRange = FMath::Max<float>(0, Component->MinDrawDistance);
					float MaxRange = FLT_MAX;
					if (LODParent)
					{
						// Max distance when HLOD becomes visible.
						MaxRange = LODParent->MinDrawDistance + (Component->Bounds.Origin - LODParent->Bounds.Origin).Size();
					}
					BoundsIndex = AddBounds(Info.Bounds, Info.PackedRelativeBox, Component, Component->LastRenderTimeOnScreen, Component->Bounds.Origin, MinDistance, MinRange, MaxRange);
				}
				BoundsIndices.Push(BoundsIndex);
			}

			// Handle force mip streaming by over scaling the texel factor. 
			AddElement(Component, Info.Texture, BoundsIndex, Info.TexelFactor, Component->bForceMipStreaming, ComponentLink);
		}
		return true;
	}
	return false;
}

bool FTextureInstanceState::AddComponentFast(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext)
{
	// Dynamic components should only be active if registered
	check(Component->IsRegistered());

	// Some components don't have any proxy in game.
	if (Component->SceneProxy)
	{
		TArray<FStreamingTexturePrimitiveInfo> TextureInstanceInfos;
		Component->GetStreamingTextureInfoWithNULLRemoval(LevelContext, TextureInstanceInfos);

		if (TextureInstanceInfos.Num())
		{
			int32 BoundsIndex = AddBounds(Component);
			int32* ComponentLink = ComponentMap.Find(Component);
			for (const FStreamingTexturePrimitiveInfo& Info : TextureInstanceInfos)
			{
				AddElement(Component, Info.Texture, BoundsIndex, Info.TexelFactor, Component->bForceMipStreaming, ComponentLink);
			}

			return true;
		}
	}
	return false;
}

void FTextureInstanceState::RemoveComponent(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures)
{
	TArray<int32, TInlineAllocator<12> > RemovedBoundsIndices;
	int32 ElementIndex = INDEX_NONE;

	ComponentMap.RemoveAndCopyValue(Component, ElementIndex);
	while (ElementIndex != INDEX_NONE)
	{
		int32 BoundsIndex = INDEX_NONE;
		const UTexture2D* Texture = nullptr;

		RemoveElement(ElementIndex, ElementIndex, BoundsIndex, Texture);

		if (BoundsIndex != INDEX_NONE)
		{
			RemovedBoundsIndices.AddUnique(BoundsIndex);
		}

		if (Texture)
		{
			RemovedTextures.AddUnique(Texture);
		}
	};

	for (int32 I = 0; I < RemovedBoundsIndices.Num(); ++I)
	{
		RemoveBounds(RemovedBoundsIndices[I]);
	}
}

bool FTextureInstanceState::RemoveComponentReferences(const UPrimitiveComponent* Component) 
{ 
	// Because the async streaming task could be running, we can't change the async view state. 
	// We limit ourself to clearing the component ptr to avoid invalid access when updating visibility.

	int32* ComponentLink = ComponentMap.Find(Component);
	if (ComponentLink)
	{
		int32 ElementIndex = *ComponentLink;
		while (ElementIndex != INDEX_NONE)
		{
			FElement& Element = Elements[ElementIndex];
			if (Element.BoundsIndex != INDEX_NONE)
			{
				Bounds4Components[Element.BoundsIndex] = nullptr;
			}
			Element.Component = nullptr;

			ElementIndex = Element.NextComponentLink;
		}

		ComponentMap.Remove(Component);
		return true;
	}
	else
	{
		return false;
	}
}

void FTextureInstanceState::GetReferencedComponents(TArray<const UPrimitiveComponent*>& Components) const
{
	for (TMap<const UPrimitiveComponent*, int32>::TConstIterator It(ComponentMap); It; ++It)
	{
		Components.Add(It.Key());
	}
}

void FTextureInstanceState::UpdateBounds(const UPrimitiveComponent* Component)
{
	int32* ComponentLink = ComponentMap.Find(Component);
	if (ComponentLink)
	{
		int32 ElementIndex = *ComponentLink;
		while (ElementIndex != INDEX_NONE)
		{
			const FElement& Element = Elements[ElementIndex];
			if (Element.BoundsIndex != INDEX_NONE)
			{
				Bounds4[Element.BoundsIndex / 4].FullUpdate(Element.BoundsIndex % 4, Component->Bounds, Component->LastRenderTimeOnScreen);
			}
			ElementIndex = Element.NextComponentLink;
		}
	}
}

bool FTextureInstanceState::UpdateBounds(int32 BoundIndex)
{
	const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
	if (Component)
	{
		Bounds4[BoundIndex / 4].FullUpdate(BoundIndex % 4, Component->Bounds, Component->LastRenderTimeOnScreen);
		return true;
	}
	else
	{
		return false;
	}
}

bool FTextureInstanceState::ConditionalUpdateBounds(int32 BoundIndex)
{
	const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
	if (Component)
	{
		if (Component->Mobility != EComponentMobility::Static)
		{
			const FBoxSphereBounds Bounds = Component->Bounds;

			// Check if the bound is coherent as it could be updated while we read it (from async task).
			// We don't have to check the position, as if it was partially updated, this should be ok (interp)
			const float RadiusSquared = FMath::Square<float>(Bounds.SphereRadius);
			const float XSquared = FMath::Square<float>(Bounds.BoxExtent.X);
			const float YSquared = FMath::Square<float>(Bounds.BoxExtent.Y);
			const float ZSquared = FMath::Square<float>(Bounds.BoxExtent.Z);

			if (0.5f * FMath::Min3<float>(XSquared, YSquared, ZSquared) <= RadiusSquared && RadiusSquared <= 2.f * (XSquared + YSquared + ZSquared))
			{
				Bounds4[BoundIndex / 4].FullUpdate(BoundIndex % 4, Bounds, Component->LastRenderTimeOnScreen);
				return true;
			}
		}
		else // Otherwise we assume it is guarantied to be good.
		{
			Bounds4[BoundIndex / 4].FullUpdate(BoundIndex % 4, Component->Bounds, Component->LastRenderTimeOnScreen);
			return true;
		}
	}
	return false;
}


void FTextureInstanceState::UpdateLastRenderTime(int32 BoundIndex)
{
	const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
	if (Component)
	{
		Bounds4[BoundIndex / 4].UpdateLastRenderTime(BoundIndex % 4, Component->LastRenderTimeOnScreen);
	}
}

uint32 FTextureInstanceState::GetAllocatedSize() const
{
	int32 CompiledElementsSize = 0;
	for (TMap<const UTexture2D*, TArray<FCompiledElement> >::TConstIterator It(CompiledTextureMap); It; ++It)
	{
		CompiledElementsSize += It.Value().GetAllocatedSize();
	}

	return Bounds4.GetAllocatedSize() +
		Bounds4Components.GetAllocatedSize() +
		Elements.GetAllocatedSize() +
		FreeBoundIndices.GetAllocatedSize() +
		FreeElementIndices.GetAllocatedSize() +
		TextureMap.GetAllocatedSize() +
		CompiledTextureMap.GetAllocatedSize() + CompiledElementsSize +
		ComponentMap.GetAllocatedSize();
}

int32 FTextureInstanceState::CompileElements()
{
	CompiledTextureMap.Empty();

	// First create an entry for all elements, so that there are no reallocs when inserting each compiled elements.
	for (TMap<const UTexture2D*, FTextureDesc>::TConstIterator TextureIt(TextureMap); TextureIt; ++TextureIt)
	{
		const UTexture2D* Texture = TextureIt.Key();
		CompiledTextureMap.Add(Texture);
	}

	// Then fill in each array.
	for (TMap<const UTexture2D*, TArray<FCompiledElement> >::TIterator It(CompiledTextureMap); It; ++It)
	{
		const UTexture2D* Texture = It.Key();
		TArray<FCompiledElement>& CompiledElemements = It.Value();

		int32 CompiledElementCount = 0;
		for (auto ElementIt = GetElementIterator(Texture); ElementIt; ++ElementIt)
		{
			++CompiledElementCount;
		}
		CompiledElemements.AddUninitialized(CompiledElementCount);

		CompiledElementCount = 0;
		for (auto ElementIt = GetElementIterator(Texture); ElementIt; ++ElementIt)
		{
			CompiledElemements[CompiledElementCount].BoundsIndex = ElementIt.GetBoundsIndex();
			CompiledElemements[CompiledElementCount].TexelFactor = ElementIt.GetTexelFactor();
			CompiledElemements[CompiledElementCount].bForceLoad = ElementIt.GetForceLoad();
			++CompiledElementCount;
		}
	}
	return CompiledTextureMap.Num();
}

int32 FTextureInstanceState::CheckRegistrationAndUnpackBounds(TArray<const UPrimitiveComponent*>& RemovedComponents)
{
	for (int32 BoundIndex = 0; BoundIndex < Bounds4Components.Num(); ++BoundIndex)
	{
		const UPrimitiveComponent* Component = Bounds4Components[BoundIndex];
		if (Component)
		{
			if (Component->IsRegistered() && Component->SceneProxy)
			{
				Bounds4[BoundIndex / 4].UnpackBounds(BoundIndex % 4, Component->Bounds);
			}
			else // Here we can remove the component, as the async task is not yet using this.
			{
				FRemovedTextureArray RemovedTextures; // Here we don't have to process the removed textures as the data was never used.
				RemoveComponent(Component, RemovedTextures);
				RemovedComponents.Add(Component);
			}
		}
	}
	return Bounds4Components.Num();
}

bool FTextureInstanceState::MoveBound(int32 SrcBoundIndex, int32 DstBoundIndex)
{
	check(!HasCompiledElements()); // Defrag is for the dynamic elements which does not support dynamic compiled elements.

	if (Bounds4Components.IsValidIndex(DstBoundIndex) && Bounds4Components.IsValidIndex(SrcBoundIndex) && !Bounds4Components[DstBoundIndex] && Bounds4Components[SrcBoundIndex])
	{
		const UPrimitiveComponent* Component = Bounds4Components[SrcBoundIndex];

		// Update the elements.
		int32* ComponentLink = ComponentMap.Find(Component);
		if (ComponentLink)
		{
			int32 ElementIndex = *ComponentLink;
			while (ElementIndex != INDEX_NONE)
			{
				FElement& Element = Elements[ElementIndex];
				
				// Sanity check to ensure elements and bounds are still linked correctly!
				check(Element.Component == Component);

				if (Element.BoundsIndex == SrcBoundIndex)
				{
					Element.BoundsIndex = DstBoundIndex;
				}
				ElementIndex = Element.NextComponentLink;
			}
		}

		// Update the component ptrs.
		Bounds4Components[DstBoundIndex] = Component;
		Bounds4Components[SrcBoundIndex] = nullptr;

		// Update the free list.
		for (int32& BoundIndex : FreeBoundIndices)
		{
			if (BoundIndex == DstBoundIndex)
			{
				BoundIndex = SrcBoundIndex;
				break;
			}
		}

		UpdateBounds(DstBoundIndex); // Update the bounds using the component.
		Bounds4[SrcBoundIndex / 4].Clear(SrcBoundIndex % 4);	

		return true;
	}
	else
	{
		return false;
	}
}

void FTextureInstanceState::TrimBounds()
{
	const int32 DefragThreshold = 8; // Must be a multiple of 4
	check(NumBounds4() * 4 == NumBounds());

	bool bUpdateFreeBoundIndices = false;

	// Here we check the bound components from low indices to high indices
	// because there are more chance that the lower range indices fail.
	// (since the incremental update move null component to the end)
	bool bDefragRangeIsFree = true;
	while (bDefragRangeIsFree)
	{
		const int32 LowerBoundThreshold = NumBounds() - DefragThreshold;
		if (Bounds4Components.IsValidIndex(LowerBoundThreshold))
		{
			for (int BoundIndex = LowerBoundThreshold; BoundIndex < NumBounds(); ++BoundIndex)
			{
				if (Bounds4Components[BoundIndex])
				{
					bDefragRangeIsFree = false;
					break;
				}
			}

			if (bDefragRangeIsFree)
			{
				Bounds4.RemoveAt(Bounds4.Num() - DefragThreshold / 4, DefragThreshold / 4, false);
				Bounds4Components.RemoveAt(Bounds4Components.Num() - DefragThreshold, DefragThreshold, false);
				bUpdateFreeBoundIndices = true;
			}
		}
		else
		{
			bDefragRangeIsFree = false;
		}
	} 

	if (bUpdateFreeBoundIndices)
	{
		// The bounds are cleared outside the range loop to prevent parsing elements multiple times.
		for (int Index = 0; Index < FreeBoundIndices.Num(); ++Index)
		{
			if (FreeBoundIndices[Index] >= NumBounds())
			{
				FreeBoundIndices.RemoveAtSwap(Index);
				--Index;
			}
		}
		check(NumBounds4() * 4 == NumBounds());
	}
}

void FTextureInstanceState::OffsetBounds(const FVector& Offset)
{
	for (int32 BoundIndex = 0; BoundIndex < Bounds4Components.Num(); ++BoundIndex)
	{
		if (Bounds4Components[BoundIndex])
		{
			Bounds4[BoundIndex / 4].OffsetBounds(BoundIndex % 4, Offset);
		}
	}
}

