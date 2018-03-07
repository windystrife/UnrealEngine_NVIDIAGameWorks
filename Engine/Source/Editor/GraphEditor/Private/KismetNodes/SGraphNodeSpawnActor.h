// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetNodes/SGraphNodeK2Default.h"

class SGraphNodeSpawnActor : public SGraphNodeK2Default
{
public:

	// SGraphNode interface
	virtual void CreatePinWidgets() override;
	// End of SGraphNode interface
};
