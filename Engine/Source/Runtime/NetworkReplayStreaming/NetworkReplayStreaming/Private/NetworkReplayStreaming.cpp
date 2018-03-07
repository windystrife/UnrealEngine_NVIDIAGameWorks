// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NetworkReplayStreaming.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_MODULE( FNetworkReplayStreaming, NetworkReplayStreaming );

INetworkReplayStreamingFactory& FNetworkReplayStreaming::GetFactory(const TCHAR* FactoryNameOverride)
{
	FString FactoryName = TEXT( "NullNetworkReplayStreaming" );

	if (FactoryNameOverride == nullptr)
	{
		GConfig->GetString( TEXT( "NetworkReplayStreaming" ), TEXT( "DefaultFactoryName" ), FactoryName, GEngineIni );
	}
	else
	{
		FactoryName = FactoryNameOverride;
	}

	FString CmdlineFactoryName;
	if (FParse::Value(FCommandLine::Get(), TEXT("-REPLAYSTREAMER="), CmdlineFactoryName))
	{
		FactoryName = CmdlineFactoryName;
	}

	// See if we need to forcefully fallback to the null streamer
	if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
	{
		FModuleManager::Get().LoadModule( *FactoryName );
	
		if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
		{
			FactoryName = TEXT( "NullNetworkReplayStreaming" );
		}
	}

	return FModuleManager::Get().LoadModuleChecked< INetworkReplayStreamingFactory >( *FactoryName );
}
