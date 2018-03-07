// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class UMovieSceneCapture;

class IMovieSceneCaptureDialogModule : public IModuleInterface
{
public:
	static IMovieSceneCaptureDialogModule& Get()
	{
		static const FName ModuleName(TEXT("MovieSceneCaptureDialog"));
		return FModuleManager::LoadModuleChecked<IMovieSceneCaptureDialogModule>(ModuleName);
	}
	virtual void OpenDialog(const TSharedRef<class FTabManager>& TabManager, UMovieSceneCapture* CaptureObject) = 0;

	/** Get the world we're currently recording from, if an in process record is happening */
	virtual UWorld* GetCurrentlyRecordingWorld() = 0;
};

