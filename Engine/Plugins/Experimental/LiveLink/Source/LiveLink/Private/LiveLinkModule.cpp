// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ILiveLinkModule.h"

#include "Features/IModularFeatures.h"

#include "LiveLinkClient.h"

/**
 * Implements the Messaging module.
 */

#define LOCTEXT_NAMESPACE "LiveLinkModule"

class FLiveLinkModule
	: public ILiveLinkModule
{
public:
	FLiveLinkClient LiveLinkClient;

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(FLiveLinkClient::ModularFeatureName, &LiveLinkClient);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(FLiveLinkClient::ModularFeatureName, &LiveLinkClient);
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};

IMPLEMENT_MODULE(FLiveLinkModule, LiveLink);

#undef LOCTEXT_NAMESPACE