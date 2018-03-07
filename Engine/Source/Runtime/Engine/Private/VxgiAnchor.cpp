// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Engine/VxgiAnchor.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/BillboardComponent.h"

AVxgiAnchor::AVxgiAnchor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnabled(true)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("VxgiAnchorComponent0"));
	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture;
			FName ID_VxgiAnchor;
			FText NAME_VxgiAnchor;

			FConstructorStatics()
				: SpriteTexture(TEXT("/Engine/EditorResources/EmptyActor"))
				, ID_VxgiAnchor(TEXT("VxgiAnchor"))
				, NAME_VxgiAnchor(NSLOCTEXT( "SpriteCategory", "VXGI", "VxgiAnchor" ))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (GetSpriteComponent())
		{
			GetSpriteComponent()->Sprite = ConstructorStatics.SpriteTexture.Get();
			GetSpriteComponent()->SpriteInfo.Category = ConstructorStatics.ID_VxgiAnchor;
			GetSpriteComponent()->SpriteInfo.DisplayName = ConstructorStatics.NAME_VxgiAnchor;
			GetSpriteComponent()->EditorScale = 0.5f;
			GetSpriteComponent()->AttachToComponent(SceneComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
#endif // WITH_EDITORONLY_DATA
}