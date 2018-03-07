// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineBaseTypes.h"
#include "K2Node_Event.h"
#include "K2Node_InputTouchEvent.generated.h"

class UDynamicBlueprintBinding;

UCLASS(MinimalAPI)
class UK2Node_InputTouchEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

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
