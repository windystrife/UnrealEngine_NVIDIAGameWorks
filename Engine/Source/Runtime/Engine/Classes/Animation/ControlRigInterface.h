// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "UObject/Interface.h"
#include "ControlRigInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API UControlRigInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** A producer of animated values */
class ENGINE_API IControlRigInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/**
	 * Perform any per tick setup work
	 */
	virtual void PreEvaluate() = 0;

	/**
	 * Perform any work that this animation controller needs to do per-tick
	 */
	virtual void Evaluate() = 0;

	/**
	 * Perform any per tick finalization work
	 */
	virtual void PostEvaluate() = 0;
};
