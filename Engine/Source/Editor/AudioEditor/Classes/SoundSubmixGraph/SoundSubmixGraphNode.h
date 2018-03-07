// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNode.h"
#include "SoundSubmixGraphNode.generated.h"

class USoundSubmix;

UCLASS(MinimalAPI)
class USoundSubmixGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** The SoundSubmix this represents */
	UPROPERTY(VisibleAnywhere, instanced, Category=Sound)
	USoundSubmix* SoundSubmix;

	/** Get the Pin that connects to all children */
	UEdGraphPin* GetChildPin() const { return ChildPin; }

	/** Get the Pin that connects to its parent */
	UEdGraphPin* GetParentPin() const { return ParentPin; }
	
	/** Check whether the children of this node match the SoundSubmix it is representing */
	bool CheckRepresentsSoundSubmix();

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void AllocateDefaultPins() override;
	virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override;
	//~ End UEdGraphNode Interface.

private:
	/** Pin that connects to all children */
	UEdGraphPin* ChildPin;
	
	/** Pin that connects to its parent */
	UEdGraphPin* ParentPin;
};
