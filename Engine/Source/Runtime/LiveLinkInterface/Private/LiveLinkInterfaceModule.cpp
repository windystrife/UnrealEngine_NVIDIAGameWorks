// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ILiveLinkInterfaceModule.h"
#include "ILiveLinkClient.h"

FName ILiveLinkClient::ModularFeatureName(TEXT("ModularFeature_LiveLinkClient"));

class FLiveLinkInterfaceModule : public ILiveLinkInterfaceModule
{
public:
	
	// IModuleInterface interface
	virtual void StartupModule() override
	{
		
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:

};

IMPLEMENT_MODULE(FLiveLinkInterfaceModule, LiveLinkInterface);
