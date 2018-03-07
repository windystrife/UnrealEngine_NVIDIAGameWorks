// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "NiagaraActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

ANiagaraActor::ANiagaraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraComponent0"));

	RootComponent = NiagaraComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject;
			FName ID_Systems;
			FText NAME_Systems;
			FConstructorStatics()
				: SpriteTextureObject(TEXT("/Engine/EditorResources/S_Emitter"))
				, ID_Systems(TEXT("Systems"))
				, NAME_Systems(NSLOCTEXT("SpriteCategory", "Systems", "Systems"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.SpriteTextureObject.Get();
			SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
			SpriteComponent->bHiddenInGame = true;
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_Systems;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Systems;
			SpriteComponent->SetupAttachment(NiagaraComponent);
			SpriteComponent->bReceivesDecals = false;
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(0, 255, 128);

			ArrowComponent->ArrowSize = 1.5f;
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Systems;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Systems;
			ArrowComponent->SetupAttachment(NiagaraComponent);
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
bool ANiagaraActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (UNiagaraSystem* System = NiagaraComponent->GetAsset())
	{
		Objects.Add(System);
	}

	return true;
}
#endif // WITH_EDITOR

