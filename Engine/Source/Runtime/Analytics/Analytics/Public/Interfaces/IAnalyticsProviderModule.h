// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "AnalyticsProviderConfigurationDelegate.h"

class IAnalyticsProvider;

/** Generic interface for an analytics provider. Other modules can define more and register them with this module. */
class IAnalyticsProviderModule : public IModuleInterface
{
public:
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const = 0;
};
