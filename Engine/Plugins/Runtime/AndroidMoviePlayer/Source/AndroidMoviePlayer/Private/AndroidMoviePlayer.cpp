// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidMoviePlayer.h"
#include "AndroidMovieStreamer.h"

#include "MoviePlayer.h"

TSharedPtr<FAndroidMediaPlayerStreamer> AndroidMovieStreamer;

class FAndroidMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		if (IsSupported())
		{
			FAndroidMediaPlayerStreamer* Streamer = new FAndroidMediaPlayerStreamer;
			AndroidMovieStreamer = MakeShareable(Streamer);
			GetMoviePlayer()->RegisterMovieStreamer(AndroidMovieStreamer);
		}
	}

	virtual void ShutdownModule() override
	{
		if (IsSupported())
		{
			AndroidMovieStreamer->Cleanup();
		}
	}

private:

	bool IsSupported()
	{
		return FAndroidMisc::GetAndroidBuildVersion() >= 14;
	}
};

IMPLEMENT_MODULE( FAndroidMoviePlayerModule, AndroidMoviePlayer )
