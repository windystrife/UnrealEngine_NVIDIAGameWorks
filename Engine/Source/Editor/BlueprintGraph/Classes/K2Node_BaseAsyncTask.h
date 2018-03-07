// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "K2Node_BaseAsyncTask.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;
class UEdGraphSchema_K2;
class UK2Node_CustomEvent;
class UK2Node_TemporaryVariable;

/** struct to remap pins for Async Tasks.
 * a single K2 node is shared by many proxy classes.
 * This allows redirecting pins by name per proxy class.
 * Add entries similar to this one in Engine.ini:
 * +K2AsyncTaskPinRedirects=(ProxyClassName="AbilityTask_PlayMontageAndWait", OldPinName="OnComplete", NewPinName="OnBlendOut")
 */

struct FAsyncTaskPinRedirectMapInfo
{
	TMap<FString, TArray<UClass*> > OldPinToProxyClassMap;
};

/** !!! The proxy object should have RF_StrongRefOnFrame flag. !!! */

UCLASS(Abstract)
class BLUEPRINTGRAPH_API UK2Node_BaseAsyncTask : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	/** Whether or not two pins match for purposes of reconnection after reconstruction.  This allows pins that may have had their names changed via reconstruction to be matched to their old values on a node-by-node basis, if needed*/
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	// End of UK2Node interface

protected:
	// Returns the factory function (checked)
	UFunction* GetFactoryFunction() const;

protected:
	// The name of the function to call to create a proxy object
	UPROPERTY()
	FName ProxyFactoryFunctionName;

	// The class containing the proxy object functions
	UPROPERTY()
	UClass* ProxyFactoryClass;

	// The type of proxy object that will be created
	UPROPERTY()
	UClass* ProxyClass;

	// The name of the 'go' function on the proxy object that will be called after delegates are in place, can be NAME_None
	UPROPERTY()
	FName ProxyActivateFunctionName;

	struct BLUEPRINTGRAPH_API FBaseAsyncTaskHelper
	{
		struct FOutputPinAndLocalVariable
		{
			UEdGraphPin* OutputPin;
			UK2Node_TemporaryVariable* TempVar;

			FOutputPinAndLocalVariable(UEdGraphPin* Pin, UK2Node_TemporaryVariable* Var) : OutputPin(Pin), TempVar(Var) {}
		};

		static bool ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction, const UEdGraphSchema_K2* Schema);
		static bool CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);
		static bool CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema);
		static bool HandleDelegateImplementation(
			UMulticastDelegateProperty* CurrentProperty, const TArray<FBaseAsyncTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs,
			UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin,
			UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

		static const FString& GetAsyncTaskProxyName();
	};

	// Pin Redirector support
	static TMap<FString, FAsyncTaskPinRedirectMapInfo> AsyncTaskPinRedirectMap;
	static bool bAsyncTaskPinRedirectMapInitialized;
};
