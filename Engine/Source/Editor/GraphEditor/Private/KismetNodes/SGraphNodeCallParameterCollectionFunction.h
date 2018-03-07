// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KismetNodes/SGraphNodeK2Default.h"

class SGraphPin;

class SGraphNodeCallParameterCollectionFunction : public SGraphNodeK2Default
{
protected:

	// SGraphNode interface
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	// End of SGraphNode interface
};
