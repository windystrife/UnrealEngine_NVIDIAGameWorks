// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Advertising.h"
#include "Interfaces/IAdvertisingProvider.h"

IMPLEMENT_MODULE( FAdvertising, Advertising );

DEFINE_LOG_CATEGORY_STATIC( LogAdvertising, Display, All );

FAdvertising::FAdvertising()
{
}

FAdvertising::~FAdvertising()
{
}

IAdvertisingProvider * FAdvertising::GetAdvertisingProvider( const FName& ProviderName )
{
	// Check if we can successfully load the module.
	if ( ProviderName != NAME_None )
	{
		IAdvertisingProvider * Module = FModuleManager::Get().LoadModulePtr<IAdvertisingProvider>(ProviderName);
		if ( Module != NULL )
		{
			UE_LOG(LogAdvertising, Log, TEXT("Creating Advertising provider %s"), *ProviderName.ToString());
			return Module;
		}
		else
		{
			UE_LOG(LogAdvertising, Warning, TEXT("Failed to find Advertising provider named %s."), *ProviderName.ToString());
		}
	}
	else
	{
		UE_LOG(LogAdvertising, Warning, TEXT("GetAdvertisingProvider called with a module name of None."));
	}
	return NULL;
}

void FAdvertising::StartupModule()
{
}

void FAdvertising::ShutdownModule()
{
}
