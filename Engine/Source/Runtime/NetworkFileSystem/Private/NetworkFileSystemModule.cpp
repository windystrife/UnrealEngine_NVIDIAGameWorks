// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "INetworkFileSystemModule.h"
#include "NetworkFileSystemLog.h"
#include "NetworkFileServer.h"
#include "NetworkFileServerHttp.h"
#include "Interfaces/ITargetPlatformManagerModule.h"


DEFINE_LOG_CATEGORY(LogFileServer);


/**
 * Implements the NetworkFileSystem module.
 */
class FNetworkFileSystemModule
	: public INetworkFileSystemModule
{
public:

	// INetworkFileSystemModule interface

	virtual INetworkFileServer* CreateNetworkFileServer( bool bLoadTargetPlatforms, int32 Port, FNetworkFileDelegateContainer NetworkFileDelegateContainer, const ENetworkFileServerProtocol Protocol ) const override
	{
		TArray<ITargetPlatform*> ActiveTargetPlatforms;
		if (bLoadTargetPlatforms)
		{
			ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();

			// if we didn't specify a target platform then use the entire target platform list (they could all be possible!)
			FString Platforms;
			if (FParse::Value(FCommandLine::Get(), TEXT("TARGETPLATFORM="), Platforms))
			{
				ActiveTargetPlatforms =  TPM.GetActiveTargetPlatforms();
			}
			else
			{
				ActiveTargetPlatforms = TPM.GetTargetPlatforms();
			}
		}

		switch ( Protocol )
		{
#if ENABLE_HTTP_FOR_NFS
		case NFSP_Http: 
			return new FNetworkFileServerHttp(Port, NetworkFileDelegateContainer, ActiveTargetPlatforms);
#endif
		case NFSP_Tcp:
			return new FNetworkFileServer(Port, NetworkFileDelegateContainer, ActiveTargetPlatforms);
		}
 
		return NULL;
	}
};


IMPLEMENT_MODULE(FNetworkFileSystemModule, NetworkFileSystem);
