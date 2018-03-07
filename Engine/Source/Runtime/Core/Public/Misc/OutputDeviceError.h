// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"

// Error device.
class CORE_API FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

