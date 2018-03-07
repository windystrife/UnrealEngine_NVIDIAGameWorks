// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "BlueprintNodeSignature.h"
#include "K2Node.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_DynamicCast.generated.h"

UCLASS(MinimalAPI)
class UK2Node_DynamicCast : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** The type that the input should try to be cast to */
	UPROPERTY()
	TSubclassOf<class UObject>  TargetType;

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual void PostReconstructNode() override;
	virtual void PostPlacedNewNode() override;
	//~ End UEdGraphNode Interface

	//~ Begin UK2Node Interface
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual FText GetMenuCategory() const override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	virtual bool IsNodePure() const override { return bIsPureCast; }
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UK2Node Interface

	/** Get the 'valid cast' exec pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetValidCastPin() const;

	/** Get the 'invalid cast' exec pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetInvalidCastPin() const;

	/** Get the cast result pin */
	BLUEPRINTGRAPH_API UEdGraphPin* GetCastResultPin() const;

	/** Get the input object to be casted pin */
	BLUEPRINTGRAPH_API virtual UEdGraphPin* GetCastSourcePin() const;

	/** Get the boolean output pin that signifies a successful/failed cast. */
	BLUEPRINTGRAPH_API virtual UEdGraphPin* GetBoolSuccessPin() const;

	/**
	 * Will change the node's purity, and reallocate pins accordingly (adding/
	 * removing exec pins).
	 * 
	 * @param  bNewPurity  The new value for bIsPureCast.
	 */
	BLUEPRINTGRAPH_API void SetPurity(bool bNewPurity);

protected:
	/** Flips the node's purity (adding/removing exec pins as needed). */
	void TogglePurity();

	/** Update exec pins when converting from impure to pure. */
	bool ReconnectPureExecPins(TArray<UEdGraphPin*>& OldPins);
	
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;

	UPROPERTY()
	bool bIsPureCast;
};

