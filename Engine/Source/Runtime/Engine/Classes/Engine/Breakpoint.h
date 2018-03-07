// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Breakpoint.generated.h"

class UEdGraphNode;

UCLASS()
class ENGINE_API UBreakpoint
	: public UObject
{
	GENERATED_UCLASS_BODY()

private:
	// Is the breakpoint currently enabled?
	UPROPERTY(transient)
	uint32 bEnabled:1;

	// Node that the breakpoint is placed on
	UPROPERTY()
	class UEdGraphNode* Node;

	// Is this breakpoint auto-generated, and should be removed when next hit?
	UPROPERTY()
	uint32 bStepOnce:1;

	UPROPERTY()
	uint32 bStepOnce_WasPreviouslyDisabled:1;

	UPROPERTY()
	uint32 bStepOnce_RemoveAfterHit:1;

public:

	/** Get the target node for the breakpoint */
	UEdGraphNode* GetLocation() const
	{
		return Node;
	}

	/** @returns true if the breakpoint should be enabled when debugging */
	bool IsEnabled() const
	{
		return bEnabled;
	}

	/** @returns true if the user wanted the breakpoint enabled?  (single stepping, etc... could result in the breakpoint being temporarily enabled) */
	bool IsEnabledByUser() const
	{
		return bEnabled && !(bStepOnce && bStepOnce_WasPreviouslyDisabled);
	}

	/** Gets a string that describes the location */
	FText GetLocationDescription() const;

	friend class FKismetDebugUtilities;
};
