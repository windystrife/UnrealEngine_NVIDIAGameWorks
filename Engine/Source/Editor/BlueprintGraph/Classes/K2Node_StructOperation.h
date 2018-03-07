// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node.h"
#include "K2Node_Variable.h"
#include "K2Node_StructOperation.generated.h"

UCLASS(MinimalAPI, abstract)
class UK2Node_StructOperation : public UK2Node_Variable
{
	GENERATED_UCLASS_BODY()

	/** Class that this variable is defined in.  */
	UPROPERTY()
	UScriptStruct* StructType;

	//~ Begin UEdGraphNode Interface
	virtual FString GetPinMetaData(FString InPinName, FName InKey) override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	//virtual bool DrawNodeAsVariable() const override { return true; }
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput = nullptr) const override;
	virtual FString GetFindReferenceSearchString() const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	//~ End UK2Node Interface

protected:
	// Updater for subclasses that allow hiding pins
	struct FStructOperationOptionalPinManager : public FOptionalPinManager
	{
		//~ Begin FOptionalPinsUpdater Interface
		virtual void GetRecordDefaults(UProperty* TestProperty, FOptionalPinFromProperty& Record) const override
		{
			Record.bCanToggleVisibility = true;
			UStruct* OwnerStruct = TestProperty ? TestProperty->GetOwnerStruct() : nullptr;
			Record.bShowPin = OwnerStruct ? !OwnerStruct->HasMetaData(TEXT("HiddenByDefault")) : true;
		}

		virtual void CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const override;
		// End of FOptionalPinsUpdater interfac
	};
#if WITH_EDITOR
	static bool DoRenamedPinsMatch(const UEdGraphPin* NewPin, const UEdGraphPin* OldPin, bool bStructInVaraiablesOut);
#endif
};

