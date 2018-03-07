// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FTypeContainer;
class IPortalService;

class IPortalServiceProvider
	: public IModuleInterface
{
public:

	/**
	 * Get a service instance of the specified type. 
	 *
	 * @param ServiceName The name of the service type to get.
	 * @param Dependencies Optional service dependencies.
	 * @return The service instance, or nullptr if unavailable.
	 */
	virtual TSharedPtr<IPortalService> GetService(const FString& ServiceName, const TSharedRef<FTypeContainer>& Dependencies) = 0;
};
