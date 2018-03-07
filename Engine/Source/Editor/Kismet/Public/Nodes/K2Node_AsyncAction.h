// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_AsyncAction.generated.h"

class FBlueprintActionDatabaseRegistrar;

/** !!! The proxy object should have RF_StrongRefOnFrame flag. !!! */

UCLASS()
class KISMET_API UK2Node_AsyncAction : public UK2Node_BaseAsyncTask
{
	GENERATED_UCLASS_BODY()
	
	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface
};
