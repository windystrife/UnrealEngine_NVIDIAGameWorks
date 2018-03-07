// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MapPakDownloaderModule.h"
#include "MapPakDownloader.h"
#include "MapPakDownloaderLog.h"


class FMapPakDownloaderModule : public IMapPakDownloaderModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr<FMapPakDownloader> GetDownloader() override;

private: 

	TSharedPtr<FMapPakDownloader> MapPakDownloader;

};

IMPLEMENT_MODULE(FMapPakDownloaderModule, MapPakDownloader)


void FMapPakDownloaderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	TSharedPtr<FMapPakDownloader> Downloader = MakeShareable(new FMapPakDownloader());

	if (!Downloader->Init())
	{
		ShutdownModule();
	}
	MapPakDownloader = Downloader;
}


void FMapPakDownloaderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

TSharedPtr<FMapPakDownloader> FMapPakDownloaderModule::GetDownloader()
{
	return MapPakDownloader;
}


DEFINE_LOG_CATEGORY(LogMapPakDownloader);



