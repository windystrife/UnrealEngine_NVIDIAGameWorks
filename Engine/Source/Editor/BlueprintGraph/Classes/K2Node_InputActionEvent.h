// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "K2Node_Event.h"
#include "K2Node_InputActionEvent.generated.h"

class UDynamicBlueprintBinding;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_InputActionEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FName InputActionName;

	UPROPERTY()
	TEnumAsByte<EInputEvent> InputKeyEvent;

	UPROPERTY()
	uint32 bConsumeInput:1;

	UPROPERTY()
	uint32 bExecuteWhenPaused:1;

	UPROPERTY()
	uint32 bOverrideParentBinding:1;

	//~ Begin UK2Node Interface
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	//~ End UK2Node Interface
};
