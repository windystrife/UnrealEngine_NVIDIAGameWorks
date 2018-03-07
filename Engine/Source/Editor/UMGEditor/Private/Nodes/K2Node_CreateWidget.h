// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_CreateWidget.generated.h"

class UEdGraph;

UCLASS()
class UMGEDITOR_API UK2Node_CreateWidget : public UK2Node_ConstructObjectFromClass
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UEdGraphNode Interface.

	//~ Begin UK2Node Interface
	virtual FText GetMenuCategory() const override;
	virtual FName GetCornerIcon() const override;
	//~ End UK2Node Interface.

	/** Get the owning player pin */
	UEdGraphPin* GetOwningPlayerPin() const;

protected:
	/** Gets the default node title when no class is selected */
	virtual FText GetBaseNodeTitle() const override;
	/** Gets the node title when a class has been selected. */
	virtual FText GetNodeTitleFormat() const override;
	/** Gets base class to use for the 'class' pin.  UObject by default. */
	virtual UClass* GetClassPinBaseClass() const override;
	/**  */
	virtual bool IsSpawnVarPin(UEdGraphPin* Pin) const override;
};
