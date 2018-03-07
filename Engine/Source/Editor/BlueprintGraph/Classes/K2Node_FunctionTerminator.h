// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_FunctionTerminator.generated.h"

UCLASS(abstract, MinimalAPI)
class UK2Node_FunctionTerminator : public UK2Node_EditablePinBase
{
	GENERATED_UCLASS_BODY()

	/** 
	 * The source class that defines the signature, if it is getting that from elsewhere (e.g. interface, base class etc). 
	 * If NULL, this is a newly created function.
	 */
	UPROPERTY()
	TSubclassOf<class UObject>  SignatureClass;

	/** The name of the signature function. */
	UPROPERTY()
	FName SignatureName;


	//~ Begin UEdGraphNode Interface
	virtual bool CanDuplicateNode() const override { return false; }
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FString CreateUniquePinName(FString SourcePinName) const override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	//~ End UK2Node Interface

	//~ Begin UK2Node_EditablePinBase Interface
	virtual bool CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage) override;
	//~ End UK2Node_EditablePinBase Interface

	/** Promotes the node from being a part of an interface override to a full function that allows for parameter and result pin additions */
	virtual void PromoteFromInterfaceOverride(bool bIsPrimaryTerminator = true);
};

