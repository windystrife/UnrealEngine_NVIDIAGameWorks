// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "VisualLoggerDebugSnapshotInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UVisualLoggerDebugSnapshotInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ENGINE_API IVisualLoggerDebugSnapshotInterface
{
	GENERATED_IINTERFACE_BODY()
public:
	virtual void GrabDebugSnapshot(struct FVisualLogEntry* Snapshot) const {}
};

