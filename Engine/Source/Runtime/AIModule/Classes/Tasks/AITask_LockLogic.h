// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Tasks/AITask.h"
#include "AITask_LockLogic.generated.h"

/** Locks AI logic until removed by external trigger */
UCLASS()
class AIMODULE_API UAITask_LockLogic : public UAITask
{
	GENERATED_UCLASS_BODY()
};
