// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LevelTextureManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "Streaming/LevelTextureManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void FLevelTextureManager::Remove(FRemovedTextureArray* RemovedTextures)
{ 
	TArray<const UPrimitiveComponent*> ReferencedComponents;
	StaticInstances.GetReferencedComponents(ReferencedComponents);
	ReferencedComponents.Append(UnprocessedStaticComponents);
	ReferencedComponents.Append(PendingInsertionStaticPrimitives);
	for (const UPrimitiveComponent* Component : ReferencedComponents)
	{
		if (Component)
		{
			check(Component->IsValidLowLevelFast()); // Check that this component was not already destroyed.
			check(Component->bAttachedToStreamingManagerAsStatic);  // Check that is correctly tracked

			// A component can only be referenced in one level, so if it was here, we can clear the flag
			Component->bAttachedToStreamingManagerAsStatic = false;
		}
	}

	// Mark all static textures for removal.
	if (RemovedTextures)
	{
		for (auto It = StaticInstances.GetTextureIterator(); It; ++It)
		{
			RemovedTextures->Push(*It);
		}
	}

	BuildStep = EStaticBuildStep::BuildTextureLookUpMap;
	StaticActorsWithNonStaticPrimitives.Empty();
	UnprocessedStaticActors.Empty();
	UnprocessedStaticComponents.Empty();
	PendingInsertionStaticPrimitives.Empty();
	TextureGuidToLevelIndex.Empty();
	bIsInitialized = false;
}

float FLevelTextureManager::GetWorldTime() const
{
	if (!GIsEditor && Level)
	{
		UWorld* World = Level->GetWorld();

		// When paused, updating the world time sometimes break visibility logic.
		if (World && !World->IsPaused())
		{
			return World->GetTimeSeconds();
		}
	}
	return 0;
}

void FLevelTextureManager::IncrementalBuild(FStreamingTextureLevelContext& LevelContext, bool bForceCompletion, int64& NumStepsLeft)
{
	check(Level);

	switch (BuildStep)
	{
	case EStaticBuildStep::BuildTextureLookUpMap:
	{
		// Build the map to convert from a guid to the level index. 
		TextureGuidToLevelIndex.Reserve(Level->StreamingTextureGuids.Num());
		for (int32 TextureIndex = 0; TextureIndex < Level->StreamingTextureGuids.Num(); ++TextureIndex)
		{
			TextureGuidToLevelIndex.Add(Level->StreamingTextureGuids[TextureIndex], TextureIndex);
		}
		NumStepsLeft -= Level->StreamingTextureGuids.Num();
		BuildStep = EStaticBuildStep::GetActors;

		// Update the level context with the texture guid map. This is required in case the incremental build runs more steps.
		LevelContext = FStreamingTextureLevelContext(EMaterialQualityLevel::Num, Level, &TextureGuidToLevelIndex);
		break;
	}
	case EStaticBuildStep::GetActors:
	{
		// Those must be cleared at this point.
		check(UnprocessedStaticActors.Num() == 0 && UnprocessedStaticComponents.Num() == 0 && PendingInsertionStaticPrimitives.Num() == 0);

		// Find all static actors
		UnprocessedStaticActors.Empty(Level->Actors.Num());
		for (const AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->IsRootComponentStatic())
			{
				UnprocessedStaticActors.Push(Actor);
			}
		}

		NumStepsLeft -= (int64)FMath::Max<int32>(Level->Actors.Num() / 16, 1); // div 16 because this is light weight.
		BuildStep = EStaticBuildStep::GetComponents;
		break;
	}
	case EStaticBuildStep::GetComponents:
	{
		while ((bForceCompletion || NumStepsLeft > 0) && UnprocessedStaticActors.Num())
		{
			const AActor* StaticActor = UnprocessedStaticActors.Pop(false);
			check(StaticActor);

			// Check if the actor is still static, as the mobility could change.
			// @todo : need a better framework to handle mobility switch (maybe prevent streamer from processing the level while updating)
			if (StaticActor->IsRootComponentStatic())
			{
				TInlineComponentArray<UPrimitiveComponent*> Primitives;
				StaticActor->GetComponents<UPrimitiveComponent>(Primitives);

				bool bHasNonStaticPrimitives = false;
				for (UPrimitiveComponent* Primitive : Primitives)
				{
					check(Primitive);
					if (Primitive->Mobility == EComponentMobility::Static)
					{
						// If the level is visible, then the component must be fully valid at this point.
						if (!Level->bIsVisible || (Primitive->IsRegistered() && Primitive->SceneProxy))
						{
							UnprocessedStaticComponents.Push(Primitive);
							Primitive->bAttachedToStreamingManagerAsStatic = true;
						}
					}
					else
					{
						bHasNonStaticPrimitives = true;
					}
				}

				// Mark this actor so that its non static components are processed in the final stage.
				if (bHasNonStaticPrimitives)
				{	
					StaticActorsWithNonStaticPrimitives.Push(StaticActor);
				}

				NumStepsLeft -= (int64)FMath::Max<int32>(Primitives.Num() / 16, 1); // div 16 because this is light weight.
			}
		}

		if (!UnprocessedStaticActors.Num())
		{
			UnprocessedStaticActors.Empty(); // Free the memory.
			BuildStep = EStaticBuildStep::ProcessComponents;
		}
		break;
	}
	case EStaticBuildStep::ProcessComponents:
	{
		while ((bForceCompletion || NumStepsLeft > 0) && UnprocessedStaticComponents.Num())
		{
			const UPrimitiveComponent* Primitive = UnprocessedStaticComponents.Pop(false);
			check(Primitive && Primitive->bAttachedToStreamingManagerAsStatic);

			if (Primitive->Mobility == EComponentMobility::Static)
			{
				// Try to insert the component, this will fail if some texture entry has not PackedRelativeBounds
				// or if there are no references to streaming textures.
				if (!StaticInstances.Add(Primitive, LevelContext))
				{
					if (Level->bIsVisible)
					{
						Primitive->bAttachedToStreamingManagerAsStatic = false;
					}
					else // If the level is not yet visible, retry later (fix for PackedRelativeBounds or partially initialized components)
					{
						PendingInsertionStaticPrimitives.Add(Primitive);
					}
				}
			}
			else // Otherwise, check if the root component is still static, to ensure the component gets processed.
			{
				Primitive->bAttachedToStreamingManagerAsStatic = false;

				const AActor* Owner = Primitive->GetOwner();
				if (Owner && Owner->IsRootComponentStatic())
				{
					StaticActorsWithNonStaticPrimitives.AddUnique(Owner);
				}
				// Otherwise, if the root is not static anymore, the actor will get processed when the level becomes visible
			}
			--NumStepsLeft;
		}

		if (!UnprocessedStaticComponents.Num())
		{
			UnprocessedStaticComponents.Empty(); // Free the memory.
			BuildStep = EStaticBuildStep::NormalizeLightmapTexelFactors;
		}
		break;
	}
	case EStaticBuildStep::NormalizeLightmapTexelFactors:
		// Unfortunately, PendingInsertionStaticPrimtivComponents won't be taken into account here.
		StaticInstances.NormalizeLightmapTexelFactor();
		BuildStep = EStaticBuildStep::CompileElements;
		break;
	case EStaticBuildStep::CompileElements:
		// Compile elements (to optimize runtime) for what is there.
		// PendingInsertionStaticPrimitives will be added after.
		NumStepsLeft -= (int64)StaticInstances.CompileElements();
		BuildStep = EStaticBuildStep::WaitForRegistration;
		break;
	case EStaticBuildStep::WaitForRegistration:
		if (Level->bIsVisible)
		{
			TArray<const UPrimitiveComponent*> RemovedComponents;

			// Remove unregistered component and resolve the bounds using the packed relative boxes.
			NumStepsLeft -= (int64)StaticInstances.CheckRegistrationAndUnpackBounds(RemovedComponents);

			for (const UPrimitiveComponent* Component : RemovedComponents)
			{	// Those component are released, not referenced anymore.
				check(Component);				
				Component->bAttachedToStreamingManagerAsStatic = false;
			}

			NumStepsLeft -= (int64)PendingInsertionStaticPrimitives.Num();

			// Insert the component we could not preprocess.
			while (PendingInsertionStaticPrimitives.Num())
			{
				const UPrimitiveComponent* Primitive = PendingInsertionStaticPrimitives.Pop(false);
				Primitive->bAttachedToStreamingManagerAsStatic = false;

				// Since the level is visible, all static primitive should be registered with scene proxies (otherwise nothing rendered)
				if (Primitive->IsRegistered() && Primitive->SceneProxy)
				{
					if (StaticInstances.Add(Primitive, LevelContext))
					{
						Primitive->bAttachedToStreamingManagerAsStatic = true;
					}
				}
			}
			PendingInsertionStaticPrimitives.Empty(); // Free the memory.
			TextureGuidToLevelIndex.Empty();
			BuildStep = EStaticBuildStep::Done;
		}
		break;
	default:
		break;
	}
}

bool FLevelTextureManager::NeedsIncrementalBuild(int32 NumStepsLeftForIncrementalBuild) const
{
	check(Level);

	if (BuildStep == EStaticBuildStep::Done)
	{
		return false;
	}
	else if (Level->bIsVisible)
	{
		return true; // If visible, continue until done.
	}
	else // Otherwise, continue while there are incremental build steps available or we are waiting for visibility.
	{
		return BuildStep != EStaticBuildStep::WaitForRegistration && NumStepsLeftForIncrementalBuild > 0;
	}
}

void FLevelTextureManager::IncrementalUpdate(
	FDynamicTextureInstanceManager& DynamicComponentManager, 
	FRemovedTextureArray& RemovedTextures, 
	int64& NumStepsLeftForIncrementalBuild, 
	float Percentage, 
	bool bUseDynamicStreaming) 
{
	QUICK_SCOPE_CYCLE_COUNTER(FStaticComponentTextureManager_IncrementalUpdate);

	check(Level);

	if (NeedsIncrementalBuild(NumStepsLeftForIncrementalBuild))
	{
		FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, Level, &TextureGuidToLevelIndex);
		do
		{
			IncrementalBuild(LevelContext, Level->bIsVisible, NumStepsLeftForIncrementalBuild);
		}
		while (NeedsIncrementalBuild(NumStepsLeftForIncrementalBuild));
	}

	if (BuildStep == EStaticBuildStep::Done)
	{
		if (Level->bIsVisible && !bIsInitialized)
		{
			FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, Level);
			if (bUseDynamicStreaming)
			{
				// Handle dynamic component for static actors
				for (const AActor* Actor : StaticActorsWithNonStaticPrimitives)
				{
					if (Actor && Actor->IsRootComponentStatic()) // If static, it gets processed in the next loop
					{
						TInlineComponentArray<UPrimitiveComponent*> Primitives;
						Actor->GetComponents<UPrimitiveComponent>(Primitives);
						for (const UPrimitiveComponent* Primitive : Primitives)
						{
							check(Primitive);
							if (!Primitive->bHandledByStreamingManagerAsDynamic && Primitive->Mobility != EComponentMobility::Static)
							{
								DynamicComponentManager.Add(Primitive, LevelContext);
							}
						}
					}
				}

				// Flag all dynamic components so that they get processed.
				for (const AActor* Actor : Level->Actors)
				{
					// In the preprocessing step, we only handle static actors, to allow dynamic actors to update before insertion.
					if (Actor && !Actor->IsRootComponentStatic())
					{
						TInlineComponentArray<UPrimitiveComponent*> Primitives;
						Actor->GetComponents<UPrimitiveComponent>(Primitives);
						for (UPrimitiveComponent* Primitive : Primitives)
						{
							check(Primitive);
							// If the flag is already set, then this primitive is already handled when it's proxy get created.
							if (!Primitive->bHandledByStreamingManagerAsDynamic)
							{
								DynamicComponentManager.Add(Primitive, LevelContext);
							}
						}
					}
				}
			}
			bIsInitialized = true;
		}
		else if (!Level->bIsVisible && bIsInitialized)
		{
			// Mark all static textures for removal.
			for (auto It = StaticInstances.GetTextureIterator(); It; ++It)
			{
				RemovedTextures.Push(*It);
			}

			bIsInitialized = false;
		}

		// If the level is visible, update the bounds.
		if (Level->bIsVisible)
		{
			StaticInstances.Refresh(Percentage);
		}
	}
}

void FLevelTextureManager::NotifyLevelOffset(const FVector& Offset)
{
	if (BuildStep == EStaticBuildStep::Done)
	{
		// offset static primitives bounds
		StaticInstances.OffsetBounds(Offset);
	}
}

uint32 FLevelTextureManager::GetAllocatedSize() const
{
	return StaticInstances.GetAllocatedSize() + 
		StaticActorsWithNonStaticPrimitives.GetAllocatedSize() + 
		UnprocessedStaticActors.GetAllocatedSize() + 
		UnprocessedStaticComponents.GetAllocatedSize() + 
		PendingInsertionStaticPrimitives.GetAllocatedSize();
}
