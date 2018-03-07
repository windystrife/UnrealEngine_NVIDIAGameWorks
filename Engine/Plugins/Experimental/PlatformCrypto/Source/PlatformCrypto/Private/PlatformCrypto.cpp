// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IPlatformCrypto.h"

#include "PlatformCryptoIncludes.h"

class FPlatformCrypto : public IPlatformCrypto
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FPlatformCrypto, PlatformCrypto )



void FPlatformCrypto::StartupModule()
{
}


void FPlatformCrypto::ShutdownModule()
{
}

TUniquePtr<FEncryptionContext> IPlatformCrypto::CreateContext()
{
	return MakeUnique<FEncryptionContext>();
}
