// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IPortalService
{
public:

	/** Virtual destructor. */
	virtual ~IPortalService() { }

	virtual bool IsAvailable() const = 0;
};
