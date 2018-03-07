// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsMoviePlayer.h"
#include "MoviePlayer.h"
#include "WindowsMovieStreamer.h"
#include "Modules/ModuleManager.h"

#include "AllowWindowsPlatformTypes.h"
#include <mfapi.h>
#include "HideWindowsPlatformTypes.h"


TSharedPtr<FMediaFoundationMovieStreamer> MovieStreamer;

class FWindowsMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		bool bLoadSuccessful = true;
		// now attempt to load the delay loaded DLLs
		if (!LoadMediaLibrary(TEXT("shlwapi.dll")))
		{
			bLoadSuccessful = false;
		}
		if (!LoadMediaLibrary(TEXT("mf.dll")))
		{
			bLoadSuccessful = false;
		}
		if (!LoadMediaLibrary(TEXT("mfplat.dll")))
		{
			bLoadSuccessful = false;
		}
		if (!LoadMediaLibrary(TEXT("mfplay.dll")))
		{
			bLoadSuccessful = false;
		}

		if( bLoadSuccessful )
		{
			HRESULT Hr = MFStartup(MF_VERSION);
			check(SUCCEEDED(Hr));

			MovieStreamer = MakeShareable(new FMediaFoundationMovieStreamer);
			GetMoviePlayer()->RegisterMovieStreamer(MovieStreamer);
		}
	}

	virtual void ShutdownModule() override
	{
		if( MovieStreamer.IsValid() )
		{
			MovieStreamer.Reset();

			MFShutdown();
		}
	}

	bool LoadMediaLibrary(const FString& Library)
	{
		if (LoadLibraryW(*Library) == NULL)
		{
			uint32 ErrorCode = GetLastError();
			if (ErrorCode == ERROR_MOD_NOT_FOUND)
			{
				UE_LOG(LogWindowsMoviePlayer, Log, TEXT("Could not load %s. Library not found."), *Library, ErrorCode);
			}
			else
			{
				UE_LOG(LogWindowsMoviePlayer, Warning, TEXT("Could not load %s. Error=%d"), *Library, ErrorCode);
			}
			
			return false;
		}

		return true;
	}
};

IMPLEMENT_MODULE( FWindowsMoviePlayerModule, WindowsMoviePlayer )
