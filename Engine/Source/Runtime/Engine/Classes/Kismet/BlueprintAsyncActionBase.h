// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "BlueprintAsyncActionBase.generated.h"

UCLASS()
class ENGINE_API UBlueprintAsyncActionBase : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Called to trigger the action once the delegates have been bound */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true"))
	virtual void Activate();

	/** Call when the action is completely done, this makes the action free to delete */
	virtual void SetReadyToDestroy();
};
