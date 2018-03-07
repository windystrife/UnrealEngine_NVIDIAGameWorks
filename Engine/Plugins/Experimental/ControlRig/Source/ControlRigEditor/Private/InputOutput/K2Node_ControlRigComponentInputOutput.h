// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_ControlRig.h"
#include "K2Node_ControlRigComponentInputOutput.generated.h"

class UEdGraph;
class UControlRig;

/** Provides functionality for I/O on an animation component's animation ControlRig */
UCLASS(Abstract)
class UK2Node_ControlRigComponentInputOutput : public UK2Node_ControlRig
{
	GENERATED_UCLASS_BODY()

public:
	/** Name of the ControlRig component pin */
	FString ControlRigComponentPinName;

	/** The type of the ControlRig whose input/outputs we want to access. If the supplied ControlRig does not match, no action will be taken */
	UPROPERTY(EditAnywhere, Category = "ControlRig")
	TSubclassOf<UControlRig> ControlRigType;

public:
	// UObject interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;

	// UK2Node Interface.
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;

protected:
	// UK2Node_ControlRig Interface
	virtual const UClass* GetControlRigClassImpl() const override;

	/** Check whether this node is embedded in an actor (this changes its expansion behavior and default pins) */
	bool IsInActor() const;
};
