// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivate.h"

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#include "MediaPlayer.h"


DEFINE_LOG_CATEGORY(LogMediaAssets);


/**
 * Implements the MediaAssets module.
 */
class FMediaAssetsModule
	: public FSelfRegisteringExec
	, public IModuleInterface
{
public:

	//~ FSelfRegisteringExec interface

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("MEDIA")))
		{
			FString MovieCmd = FParse::Token(Cmd, 0);

			if (MovieCmd.Contains(TEXT("PLAY")))
			{
				for (TObjectIterator<UMediaPlayer> It; It; ++It)
				{
					UMediaPlayer* MediaPlayer = *It;
					MediaPlayer->Play();
				}
			}
			else if (MovieCmd.Contains(TEXT("PAUSE")))
			{
				for (TObjectIterator<UMediaPlayer> It; It; ++It)
				{
					UMediaPlayer* MediaPlayer = *It;
					MediaPlayer->Pause();
				}
			}

			return true;
		}

		return false;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FMediaAssetsModule, MediaAssets);
