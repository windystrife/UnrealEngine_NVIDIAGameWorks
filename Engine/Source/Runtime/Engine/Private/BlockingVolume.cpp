// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/BlockingVolume.h"
#include "Components/BrushComponent.h"

static FName InvisibleWall_NAME(TEXT("InvisibleWall"));

ABlockingVolume::ABlockingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->SetCanEverAffectNavigation(true);
	GetBrushComponent()->SetCollisionProfileName(InvisibleWall_NAME);
}

#if WITH_EDITOR

static FName InvisibleWallDynamic_NAME(TEXT("InvisibleWallDynamic"));

void ABlockingVolume::LoadedFromAnotherClass(const FName& OldClassName)
{
	Super::LoadedFromAnotherClass(OldClassName);

	if(GetLinkerUE4Version() < VER_UE4_REMOVE_DYNAMIC_VOLUME_CLASSES)
	{
		static FName DynamicBlockingVolume_NAME(TEXT("DynamicBlockingVolume"));

		if(OldClassName == DynamicBlockingVolume_NAME)
		{
			GetBrushComponent()->Mobility = EComponentMobility::Movable;

			if(GetBrushComponent()->GetCollisionProfileName() == InvisibleWall_NAME)
			{
				GetBrushComponent()->SetCollisionProfileName(InvisibleWallDynamic_NAME);
			}
		}
	}
}

void ABlockingVolume::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	// Get 'deepest' property name we changed.
	const FName TailPropName = PropertyChangedEvent.PropertyChain.GetTail()->GetValue()->GetFName();
	static FName Mobility_NAME(TEXT("Mobility"));
	if( TailPropName == Mobility_NAME )
	{
		// If the collision profile is one of the 'default' ones for a BlockingVolume, make sure it is the correct one
		// If user has changed it to something else, don't touch it
		FName CurrentProfileName = GetBrushComponent()->GetCollisionProfileName();
		if(	CurrentProfileName == InvisibleWall_NAME ||
			CurrentProfileName == InvisibleWallDynamic_NAME )
		{
			if(GetBrushComponent()->Mobility == EComponentMobility::Movable)
			{
				GetBrushComponent()->SetCollisionProfileName(InvisibleWallDynamic_NAME);
			}
			else
			{
				GetBrushComponent()->SetCollisionProfileName(InvisibleWall_NAME);
			}
		}
	}
}

#endif
