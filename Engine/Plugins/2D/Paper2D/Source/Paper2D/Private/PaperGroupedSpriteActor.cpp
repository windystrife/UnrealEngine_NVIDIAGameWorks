// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperGroupedSpriteActor.h"
#include "PaperGroupedSpriteComponent.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// APaperGroupedSpriteActor

APaperGroupedSpriteActor::APaperGroupedSpriteActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderComponent = CreateDefaultSubobject<UPaperGroupedSpriteComponent>(TEXT("RenderComponent"));

	RootComponent = RenderComponent;
}

#if WITH_EDITOR
bool APaperGroupedSpriteActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	RenderComponent->GetReferencedSpriteAssets(Objects);

	return true;
}
#endif

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
