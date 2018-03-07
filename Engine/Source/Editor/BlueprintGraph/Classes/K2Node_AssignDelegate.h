// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.generated.h"

class UEdGraph;

/**
 * Modeled after FEdGraphSchemaAction_K2AssignDelegate for the newer Blueprint
 * menu system. Acts simply as a UK2Node_AddDelegate with an attached custom-
 * event node (spawned in PostPlacedNewNode).
 */
UCLASS(MinimalAPI)
class UK2Node_AssignDelegate : public UK2Node_AddDelegate
{
	GENERATED_UCLASS_BODY()

public:
	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void PostPlacedNewNode() override;
	// End of UEdGraphNode interface

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedListTitle;
};
