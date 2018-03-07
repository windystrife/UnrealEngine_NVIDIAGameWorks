// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComposurePipelineBaseActor.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


AComposurePipelineBaseActor::AComposurePipelineBaseActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
}

void AComposurePipelineBaseActor::RerunConstructionScripts()
{
#if WITH_EDITOR
	if (GEditor && GEditor->bIsSimulatingInEditor)
	{
		// Don't reconstruct blueprints if simulating so that keyframe in sequencer doesn't clobber the
		// pipeline state.
		return;
	}
#endif

	Super::RerunConstructionScripts();
}
