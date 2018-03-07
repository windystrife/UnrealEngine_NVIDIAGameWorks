// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_Switch.h"
#include "NodeDependingOnEnumInterface.h"
#include "K2Node_SwitchEnum.generated.h"

class FBlueprintActionDatabaseRegistrar;

UCLASS(MinimalAPI)
class UK2Node_SwitchEnum : public UK2Node_Switch, public INodeDependingOnEnumInterface
{
	GENERATED_UCLASS_BODY()

	/** Name of the enum being switched on */
	UPROPERTY()
	UEnum* Enum;

	/** List of the current entries in the enum */
	UPROPERTY()
	TArray<FName> EnumEntries;

	/** List of the current entries in the enum */
	UPROPERTY()
	TArray<FText> EnumFriendlyNames;

	// INodeDependingOnEnumInterface
	virtual class UEnum* GetEnum() const override { return Enum; }
	virtual bool ShouldBeReconstructedAfterEnumChanged() const override {return true;}
	// End of INodeDependingOnEnumInterface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool CanEverRemoveExecutionPin() const override { return false; }
	virtual bool CanUserEditPinAdvancedViewFlag() const override { return true; }
	virtual void PreloadRequiredAssets() override;
	// End of UK2Node interface

	// UK2Node_Switch Interface
	virtual FString GetUniquePinName() override;
	virtual FEdGraphPinType GetPinType() const override;
	virtual void AddPinToSwitchNode() override;
	virtual void RemovePinFromSwitchNode(UEdGraphPin* TargetPin) override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	// End of UK2Node_Switch Interface

	/** Bind the switch to a named enum */
	void SetEnum(UEnum* InEnum);

protected:
	/** Helper method to set-up pins */
	virtual void CreateCasePins() override;
	/** Helper method to set-up correct selection pin */
	virtual void CreateSelectionPin() override;
	
	/** Don't support removing pins from an enum */
	virtual void RemovePin(UEdGraphPin* TargetPin) override {}

private:
	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};
