// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Interface.h"
#include "NodeMappingProviderInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API UNodeMappingProviderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** A producer of animated values */
class ENGINE_API INodeMappingProviderInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Returns nodes that needs for them to map */
	virtual void GetMappableNodeData(TArray<FName>& OutNames, TArray<FTransform>& OutTransforms) const = 0;
	virtual void Setup() = 0;
};