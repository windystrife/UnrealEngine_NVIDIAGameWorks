// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImgMediaPrivate.h"

#include "Modules/ModuleManager.h"

#include "ImgMediaPlayer.h"
#include "IImgMediaModule.h"


DEFINE_LOG_CATEGORY(LogImgMedia);


/**
 * Implements the AVFMedia module.
 */
class FImgMediaModule
	: public IImgMediaModule
{
public:

	/** Default constructor. */
	FImgMediaModule() { }

public:

	//~ IImgMediaModule interface

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		return MakeShared<FImgMediaPlayer, ESPMode::ThreadSafe>(EventSink);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FImgMediaModule, ImgMedia);
