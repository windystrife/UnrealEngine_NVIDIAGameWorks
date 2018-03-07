// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperFlipbookActor.h"
#include "PaperFlipbookComponent.h"
#include "PaperFlipbook.h"

//////////////////////////////////////////////////////////////////////////
// APaperFlipbookActor

APaperFlipbookActor::APaperFlipbookActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderComponent = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("RenderComponent"));

	RootComponent = RenderComponent;
}

#if WITH_EDITOR
bool APaperFlipbookActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (UPaperFlipbook* FlipbookAsset = RenderComponent->GetFlipbook())
	{
		Objects.Add(FlipbookAsset);
	}
	return true;
}
#endif
