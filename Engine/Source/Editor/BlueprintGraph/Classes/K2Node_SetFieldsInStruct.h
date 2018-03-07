// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SetFieldsInStruct.generated.h"

// Pure kismet node that creates a struct with specified values for each member
UCLASS(MinimalAPI)
class UK2Node_SetFieldsInStruct : public UK2Node_MakeStruct
{
	GENERATED_UCLASS_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual bool CanSplitPin(const UEdGraphPin* Pin) const override;
	//~ End  UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool IsNodePure() const override { return false; }
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End K2Node Interface

	BLUEPRINTGRAPH_API static bool ShowCustomPinActions(const UEdGraphPin* InGraphPin, bool bIgnorePinsNum);

	enum EPinsToRemove
	{
		GivenPin,
		AllOtherPins
	};

	BLUEPRINTGRAPH_API void RemoveFieldPins(UEdGraphPin* InGraphPin, EPinsToRemove Selection);
	BLUEPRINTGRAPH_API bool AllPinsAreShown() const;
	BLUEPRINTGRAPH_API void RestoreAllPins();

protected:
	struct FSetFieldsInStructPinManager : public FMakeStructPinManager
	{
	public:
		FSetFieldsInStructPinManager(const uint8* InSampleStructMemory) : FMakeStructPinManager(InSampleStructMemory) 
		{}

		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override;
	};

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};
