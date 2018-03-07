// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/TargetPoint.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Engine/Texture2D.h"

ATargetPoint::ATargetPoint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));

	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> TargetIconSpawnObject;
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> TargetIconObject;
			FName ID_TargetPoint;
			FText NAME_TargetPoint;
			FConstructorStatics()
				: TargetIconSpawnObject(TEXT("/Engine/EditorMaterials/TargetIconSpawn"))
				, TargetIconObject(TEXT("/Engine/EditorMaterials/TargetIcon"))
				, ID_TargetPoint(TEXT("TargetPoint"))
				, NAME_TargetPoint(NSLOCTEXT("SpriteCategory", "TargetPoint", "Target Points"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.TargetIconObject.Get();
			SpriteComponent->RelativeScale3D = FVector(0.35f, 0.35f, 0.35f);
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.ID_TargetPoint;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_TargetPoint;
			SpriteComponent->bIsScreenSizeScaled = true;

			SpriteComponent->SetupAttachment(SceneComponent);
		}

		if (ArrowComponent)
		{
			ArrowComponent->ArrowColor = FColor(150, 200, 255);

			ArrowComponent->ArrowSize = 0.5f;
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_TargetPoint;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_TargetPoint;
			ArrowComponent->SetupAttachment(SpriteComponent);
			ArrowComponent->bIsScreenSizeScaled = true;
		}
	}
#endif // WITH_EDITORONLY_DATA

	bHidden = true;
	bCanBeDamaged = false;
}

#if WITH_EDITORONLY_DATA
/** Returns SpriteComponent subobject **/
UBillboardComponent* ATargetPoint::GetSpriteComponent() const { return SpriteComponent; }
/** Returns ArrowComponent subobject **/
UArrowComponent* ATargetPoint::GetArrowComponent() const { return ArrowComponent; }
#endif
