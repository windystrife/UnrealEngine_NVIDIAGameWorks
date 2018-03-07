// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "IMovieSceneCaptureProtocol.h"
#include "Slate/SceneViewport.h"


bool IMovieSceneCaptureProtocol::CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const
{
	return bOverwriteExisting || IFileManager::Get().FileSize(InFilename) == -1;
}

FCaptureProtocolInitSettings FCaptureProtocolInitSettings::FromSlateViewport(TSharedRef<FSceneViewport> InSceneViewport, UObject* InProtocolSettings)
{
	FCaptureProtocolInitSettings Settings;
	Settings.SceneViewport = InSceneViewport;
	Settings.DesiredSize = InSceneViewport->GetSize();
	Settings.ProtocolSettings = InProtocolSettings;
	return Settings;
}
