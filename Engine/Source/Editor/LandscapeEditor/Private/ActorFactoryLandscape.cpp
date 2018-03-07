// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Classes/ActorFactoryLandscape.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeProxy.h"
#include "LandscapeEditorObject.h"
#include "Classes/LandscapePlaceholder.h"

#define LOCTEXT_NAMESPACE "Landscape"

UActorFactoryLandscape::UActorFactoryLandscape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("Landscape", "Landscape");
	NewActorClass = ALandscapeProxy::StaticClass();
}

AActor* UActorFactoryLandscape::SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName Name)
{
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);

	FEdModeLandscape* EdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

	EdMode->UISettings->NewLandscape_Location = Transform.GetLocation();
	EdMode->UISettings->NewLandscape_Rotation = Transform.GetRotation().Rotator();

	EdMode->SetCurrentTool("NewLandscape");

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = InLevel;
	SpawnInfo.ObjectFlags = InObjectFlags;
	SpawnInfo.Name = Name;
	return InLevel->OwningWorld->SpawnActor(ALandscapePlaceholder::StaticClass(), &Transform, SpawnInfo);
}

ALandscapePlaceholder::ALandscapePlaceholder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> TerrainTexture;
		FConstructorStatics()
			: TerrainTexture(TEXT("/Engine/EditorResources/S_Terrain"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent0"));
	RootComponent = SceneComponent;
	RootComponent->Mobility = EComponentMobility::Static;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->Sprite = ConstructorStatics.TerrainTexture.Get();
		SpriteComponent->RelativeScale3D = FVector(0.5f, 0.5f, 0.5f);
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->RelativeLocation = FVector(0, 0, 100);
		SpriteComponent->bAbsoluteScale = true;
	}
#endif
}

bool ALandscapePlaceholder::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest /*= false*/, bool bNoCheck /*= false*/)
{
	bool bResult = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);

	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);

	FEdModeLandscape* EdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);

	EdMode->UISettings->NewLandscape_Location = GetActorLocation();
	EdMode->UISettings->NewLandscape_Rotation = GetActorRotation();

	EdMode->SetCurrentTool("NewLandscape");

	return bResult;
}

void ALandscapePlaceholder::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!HasAnyFlags(RF_Transient))
	{
		Destroy();
	}
}

#undef LOCTEXT_NAMESPACE
