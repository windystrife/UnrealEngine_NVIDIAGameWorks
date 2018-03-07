// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Interface.h"
#include "K2Node_AddPinInterface.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class BLUEPRINTGRAPH_API UK2Node_AddPinInterface : public UInterface
{
	GENERATED_BODY()
};

class BLUEPRINTGRAPH_API IK2Node_AddPinInterface
{
	GENERATED_BODY()

public:
	virtual void AddInputPin() { }
	virtual bool CanAddPin() const { return true; }
};