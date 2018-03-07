// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AIPerceptionListenerInterface.generated.h"

class UAIPerceptionComponent;

UINTERFACE()
class AIMODULE_API UAIPerceptionListenerInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AIMODULE_API IAIPerceptionListenerInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual UAIPerceptionComponent* GetPerceptionComponent() { return nullptr; }
};

