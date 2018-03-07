// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrowdManagerBase.generated.h"

/** Base class for Crowd Managers. If you want to create a custom crowd manager
 *	implement a class extending this one and set UNavigationSystem::CrowdManagerClass
 *	to point at your class */
UCLASS(Abstract, Transient)
class ENGINE_API UCrowdManagerBase : public UObject
{
	GENERATED_BODY()
public:
	virtual void Tick(float DeltaTime) PURE_VIRTUAL(UCrowdManagerBase::Tick, );
};

