// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneCaptureHandle.h"

class FMovieSceneCaptureProtocolRegistry;
class FSceneViewport;
class IMovieSceneCaptureInterface;

class IMovieSceneCaptureModule : public IModuleInterface
{
public:
	static IMovieSceneCaptureModule& Get()
	{
		static const FName ModuleName(TEXT("MovieSceneCapture"));
		return FModuleManager::LoadModuleChecked< IMovieSceneCaptureModule >(ModuleName);
	}

	virtual IMovieSceneCaptureInterface* InitializeFromCommandLine() = 0;
	virtual IMovieSceneCaptureInterface* CreateMovieSceneCapture(TSharedPtr<FSceneViewport> Viewport) = 0;
	virtual IMovieSceneCaptureInterface* GetFirstActiveMovieSceneCapture() = 0;
	
	virtual IMovieSceneCaptureInterface* RetrieveMovieSceneInterface(FMovieSceneCaptureHandle Handle) = 0;
	virtual void DestroyMovieSceneCapture(FMovieSceneCaptureHandle Handle) = 0;

	virtual void DestroyAllActiveCaptures() = 0;
	virtual FMovieSceneCaptureProtocolRegistry& GetProtocolRegistry() = 0;
};

