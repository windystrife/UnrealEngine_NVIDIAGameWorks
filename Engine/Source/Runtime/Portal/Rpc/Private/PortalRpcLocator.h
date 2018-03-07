// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPortalRpcLocator;

class FPortalRpcLocatorFactory
{
public:
	static TSharedRef<IPortalRpcLocator> Create();
};
