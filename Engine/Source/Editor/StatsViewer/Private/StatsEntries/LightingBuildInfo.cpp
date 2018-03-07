// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LightingBuildInfo.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "Model.h"
#include "Lightmass/LightmappedSurfaceCollection.h"

ULightingBuildInfo::ULightingBuildInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULightingBuildInfo::Set( TWeakObjectPtr<UObject> InObject, double InLightingTime, float InUnmappedTexelsPercentage, int32 InUnmappedTexelsMemory, int32 InTotalTexelMemory )
{
	Object = InObject;
	LightingTime = (float)InLightingTime;
	UnmappedTexelsPercentage = InUnmappedTexelsPercentage;
	UnmappedTexelsMemory = (float)InUnmappedTexelsMemory / 1024.0f;
	TotalTexelMemory = (float)InTotalTexelMemory / 1024.0f;

	UpdateNames();
}

void ULightingBuildInfo::UpdateNames()
{
	if( Object.IsValid() )
	{
		AActor* Actor = Cast<AActor>(Object.Get());
		ULightmappedSurfaceCollection* SurfaceCollection = Cast<ULightmappedSurfaceCollection>(Object.Get());

		if (SurfaceCollection && SurfaceCollection->SourceModel)
		{
			LevelName = SurfaceCollection->SourceModel->GetOutermost()->GetName();
		}
		else if (Actor)
		{
			LevelName = Actor->GetLevel()->GetOutermost()->GetName();
		}
		else
		{
			LevelName = Object->GetOutermost()->GetName();
		}

		const int32 NameIndex = LevelName.Find( TEXT("/") );
		if ( NameIndex != INDEX_NONE )
		{
			LevelName = LevelName.RightChop( NameIndex + 1 );
		}
	}
}
