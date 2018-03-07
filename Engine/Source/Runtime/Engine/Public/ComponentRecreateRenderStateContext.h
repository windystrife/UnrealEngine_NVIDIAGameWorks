// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Components/ActorComponent.h"

/** Destroys render state for a component and then recreates it when this object is destroyed */
class FComponentRecreateRenderStateContext
{
private:
	/** Pointer to component we are recreating render state for */
	UActorComponent* Component;
public:
	FComponentRecreateRenderStateContext(UActorComponent* InComponent)
	{
		check(InComponent);
		checkf(!InComponent->IsUnreachable(), TEXT("%s"), *InComponent->GetFullName());

		if (InComponent->IsRegistered() && InComponent->IsRenderStateCreated())
		{
			InComponent->DestroyRenderState_Concurrent();
			Component = InComponent;
		}
		else
		{
			Component = nullptr;
		}
	}

	~FComponentRecreateRenderStateContext()
	{
		if (Component && !Component->IsRenderStateCreated() && Component->IsRegistered())
		{
			Component->CreateRenderState_Concurrent();
		}
	}
};

/** Destroys render states for all components and then recreates them when this object is destroyed */
class FGlobalComponentRecreateRenderStateContext
{
public:
	/** 
	* Initialization constructor. 
	*/
	ENGINE_API FGlobalComponentRecreateRenderStateContext();

	/** Destructor */
	ENGINE_API ~FGlobalComponentRecreateRenderStateContext();

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentRecreateRenderStateContext> ComponentContexts;
};
