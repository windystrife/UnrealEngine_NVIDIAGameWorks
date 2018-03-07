// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class IStepExecutor;
class FDriverConfiguration;

class FStepExecutorFactory
{
public:

	static TSharedRef<IStepExecutor, ESPMode::ThreadSafe> Create(
		const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration);

};
