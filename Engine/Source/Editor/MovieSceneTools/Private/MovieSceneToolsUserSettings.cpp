// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsUserSettings.h"
#include "UObject/UnrealType.h"


UMovieSceneUserThumbnailSettings::UMovieSceneUserThumbnailSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	ThumbnailSize = FIntPoint(128, 72);
	bDrawThumbnails = true;
	Quality = EThumbnailQuality::Normal;
}

void UMovieSceneUserThumbnailSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneUserThumbnailSettings, Quality))
	{
		BroadcastRedrawThumbnails();
	}
	
	ThumbnailSize.X = FMath::Clamp(ThumbnailSize.X, 1, 1024);
	ThumbnailSize.Y = FMath::Clamp(ThumbnailSize.Y, 1, 1024);

	SaveConfig();
}

UMovieSceneUserImportFBXSettings::UMovieSceneUserImportFBXSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bForceFrontXAxis = false;
	bCreateCameras = true;
}
