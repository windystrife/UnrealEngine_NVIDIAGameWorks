// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "EdGraph/EdGraph.h"
#include "Textures/SlateIcon.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "BlueprintNodeSignature.h"
#include "K2Node.h"

#include "K2Node_InputKey.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_InputKey : public UK2Node
{
	GENERATED_UCLASS_BODY()

	// The key that is bound
	UPROPERTY(EditAnywhere, Category="Input")
	FKey InputKey;

	// Prevents actors with lower priority from handling this input
	UPROPERTY(EditAnywhere, Category="Input")
	uint32 bConsumeInput:1;

	// Should the binding execute even when the game is paused
	UPROPERTY(EditAnywhere, Category="Input")
	uint32 bExecuteWhenPaused:1;

	// Should any bindings to this event in parent classes be removed
	UPROPERTY(EditAnywhere, Category="Input")
	uint32 bOverrideParentBinding:1;

	// Does this binding require the control key on PC or the command key on Mac to be held
	UPROPERTY(EditAnywhere, Category="Modifier")
	uint32 bControl:1;

	// Does this binding require the alt key to be held
	UPROPERTY(EditAnywhere, Category="Modifier")
	uint32 bAlt:1;

	// Does this binding require the shift key to be held
	UPROPERTY(EditAnywhere, Category="Modifier")
	uint32 bShift:1;

	// Does this binding require the windows key on PC or the control key on Mac to be held
	UPROPERTY(EditAnywhere, Category="Modifier")
	uint32 bCommand:1;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UK2Node Interface.
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FBlueprintNodeSignature GetSignature() const override;
	//~ End UK2Node Interface

	//~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	//~ End UEdGraphNode Interface.

	FText GetModifierText() const;

	FName GetModifierName() const;

	FText GetKeyText() const;

	/** Get the 'pressed' input pin */
	UEdGraphPin* GetPressedPin() const;

	/** Get the 'released' input pin */
	UEdGraphPin* GetReleasedPin() const;

private:
	/** Constructing FText strings can be costly, so we cache the node's title/tooltip */
	FNodeTextCache CachedTooltip;
	FNodeTextCache CachedNodeTitle;
};
