// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteActor.h"
#include "Engine/CollisionProfile.h"
#include "PaperSpriteComponent.h"
#include "PaperSprite.h"

//////////////////////////////////////////////////////////////////////////
// APaperSpriteActor

APaperSpriteActor::APaperSpriteActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("RenderComponent"));
	RenderComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RenderComponent->Mobility = EComponentMobility::Static;

	RootComponent = RenderComponent;
}

#if WITH_EDITOR
bool APaperSpriteActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (UPaperSprite* SourceSprite = RenderComponent->GetSprite())
	{
		Objects.Add(SourceSprite);
	}
	return true;
}
#endif
