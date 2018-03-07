// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshLightingInfo.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "Model.h"
#include "Lightmass/LightmappedSurfaceCollection.h"

#define LOCTEXT_NAMESPACE "Editor.StatsViewer"

UStaticMeshLightingInfo::UStaticMeshLightingInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UStaticMeshLightingInfo::UpdateNames()
{
	if ( LevelName.Len() == 0 || StaticMeshActor.IsValid() )
	{
		AActor* Actor = Cast<AActor>(StaticMeshActor.Get());
		ULightmappedSurfaceCollection* SurfaceCollection = Cast<ULightmappedSurfaceCollection>(StaticMeshActor.Get());

		if (SurfaceCollection && SurfaceCollection->SourceModel)
		{
			LevelName = SurfaceCollection->SourceModel->GetOutermost()->GetName();
		}
		else if (Actor)
		{
			LevelName = Actor->GetLevel()->GetOutermost()->GetName();
		}
		else if (StaticMeshActor.Get())
		{
			LevelName = StaticMeshActor.Get()->GetOutermost()->GetName();
		}
		else
		{
			LevelName = TEXT("<None>");
		}

		const int32 NameIndex = LevelName.Find( TEXT("/") );
		if ( NameIndex != INDEX_NONE )
		{
			LevelName = LevelName.RightChop( NameIndex + 1 );
		}
	}

	if( bTextureMapping )
	{
		TextureMapping = LOCTEXT("LightingUsesTextureMapping", "Texture" ).ToString();
	}
	else
	{
		TextureMapping = LOCTEXT("LightingUsesVertexMapping", "Vertex" ).ToString();
	}
}

#undef LOCTEXT_NAMESPACE
