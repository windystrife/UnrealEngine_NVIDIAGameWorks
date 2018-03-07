// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_LatentOnlineCall.generated.h"

class FBlueprintActionDatabaseRegistrar;

// This node is a latent online subsystem call (handles scanning all UOnlineBlueprintCallProxyBase classes for static factory calls)
UCLASS()
class ONLINEBLUEPRINTSUPPORT_API UK2Node_LatentOnlineCall : public UK2Node_BaseAsyncTask
{
	GENERATED_UCLASS_BODY()
	
	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface
};
