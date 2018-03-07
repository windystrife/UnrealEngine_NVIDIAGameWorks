// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "NodeDependingOnEnumInterface.generated.h"

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class BLUEPRINTGRAPH_API UNodeDependingOnEnumInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class BLUEPRINTGRAPH_API INodeDependingOnEnumInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual class UEnum* GetEnum() const PURE_VIRTUAL(INodeDependingOnEnumInterface::GetEnum,return NULL;);

	virtual bool ShouldBeReconstructedAfterEnumChanged() const {return false;}
};
