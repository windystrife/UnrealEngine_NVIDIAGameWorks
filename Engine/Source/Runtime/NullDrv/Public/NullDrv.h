// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "RHI.h"
#include "NullRHI.h"

/** Implements the NullDrv module as a dynamic RHI providing module. */
class FNullDynamicRHIModule
	: public IDynamicRHIModule
{
public:

	// IDynamicRHIModule

	virtual bool SupportsDynamicReloading() override { return false; }
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::Num) override
	{
		return new FNullDynamicRHI();
	}
};
