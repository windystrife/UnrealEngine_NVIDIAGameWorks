// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleMoviePlayer.h"
#include "ModuleInterface.h"
#include "MoviePlayer.h"
#include "AppleMovieStreamer.h"

TSharedPtr<FAVPlayerMovieStreamer> AppleMovieStreamer;

class FAppleMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FAVPlayerMovieStreamer *Streamer = new FAVPlayerMovieStreamer;
		AppleMovieStreamer = MakeShareable(Streamer);
		GetMoviePlayer()->RegisterMovieStreamer(AppleMovieStreamer);
	}

	virtual void ShutdownModule() override
	{
		AppleMovieStreamer.Reset();
	}
};

IMPLEMENT_MODULE( FAppleMoviePlayerModule, AppleMoviePlayer )
