// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MultiGate.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;
class UEdGraphPin;

UCLASS(MinimalAPI)
class UK2Node_MultiGate : public UK2Node_ExecutionSequence
{
	GENERATED_UCLASS_BODY()

	/** Reference to the integer that contains */
	UPROPERTY(transient)
	class UK2Node_TemporaryVariable* DataNode;

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UK2Node Interface

	/** Getting pin access */
	BLUEPRINTGRAPH_API UEdGraphPin* GetResetPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* GetIsRandomPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* GetLoopPin() const;
	BLUEPRINTGRAPH_API UEdGraphPin* GetStartIndexPin() const;
	BLUEPRINTGRAPH_API void GetOutPins(TArray<UEdGraphPin*>& OutPins) const;

	/** Gets the name and class of the MarkBit function from the KismetNodeHelperLibrary */
	BLUEPRINTGRAPH_API void GetMarkBitFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the HasUnmarkedBit function from the KismetNodeHelperLibrary */
	BLUEPRINTGRAPH_API void GetHasUnmarkedBitFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the MarkFirstUnmarkedBit function from the KismetNodeHelperLibrary */
	BLUEPRINTGRAPH_API void GetUnmarkedBitFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the Greater_IntInt function from the KismetMathLibrary */
	BLUEPRINTGRAPH_API void GetConditionalFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the EqualEqual_IntInt function from the KismetMathLibrary */
	BLUEPRINTGRAPH_API void GetEqualityFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the NotEqual_BoolBool function from the KismetMathLibrary */
	BLUEPRINTGRAPH_API void GetBoolNotEqualFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the PrintString function */
	BLUEPRINTGRAPH_API void GetPrintStringFunction(FName& FunctionName, UClass** FunctionClass);
	/** Gets the name and class of the ClearAllBits function from the KismetNodeHelperLibrary */
	BLUEPRINTGRAPH_API void GetClearAllBitsFunction(FName& FunctionName, UClass** FunctionClass);

private:
	// Returns the exec output pin name for a given 0-based index
 	virtual FString GetPinNameGivenIndex(int32 Index) const override;
};

