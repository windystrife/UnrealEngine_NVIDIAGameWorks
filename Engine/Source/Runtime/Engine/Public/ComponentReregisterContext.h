// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "RenderingThread.h"
#include "UObject/UObjectIterator.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "AI/Navigation/NavigationSystem.h"
#include "ContentStreaming.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogActorComponent, Log, All);

/**
 * Base class for Component Reregister objects, provides helper functions for performing the UnRegister
 * and ReRegister
 */
class FComponentReregisterContextBase
{
protected:
	//Unregisters the Component and returns the world it was registered to.
	UWorld* UnRegister(UActorComponent* InComponent)
	{
		UWorld* World = nullptr;

		check(InComponent);
		checkf(!InComponent->IsUnreachable(), TEXT("%s"), *InComponent->GetFullName());

		if(InComponent->IsRegistered() && InComponent->GetWorld())
		{
			// Save the world and set the component's world to NULL to prevent a nested FComponentReregisterContext from reregistering this component.
			World = InComponent->GetWorld();
			FNavigationLockContext NavUpdateLock(World);

			// Will set bRegistered to false
			InComponent->ExecuteUnregisterEvents();

			InComponent->WorldPrivate = nullptr;
		}
		return World;
	}

	//Reregisters the given component on the given scene
	void ReRegister(UActorComponent* InComponent, UWorld* InWorld)
	{
		check(InComponent);
		
		if( !InComponent->IsPendingKill() )
		{
			// Set scene pointer back
			check(InWorld != NULL); // If Component is set, World should be too (see logic in constructor)

			if( InComponent->IsRegistered() )
			{
				// The component has been registered already but external code is
				// going to expect the reregister to happen now. So unregister and
				// re-register.
				UE_LOG(LogActorComponent, Log, TEXT("~FComponentReregisterContext: (%s) Component already registered."), *InComponent->GetPathName());
				InComponent->ExecuteUnregisterEvents();
			}

			InComponent->WorldPrivate = InWorld;
			FNavigationLockContext NavUpdateLock(InWorld);

			// Will set bRegistered to true
			InComponent->ExecuteRegisterEvents();
		}
	}
};

/**
 * Unregisters a component for the lifetime of this class.
 *
 * Typically used by constructing the class on the stack:
 * {
 *		FComponentReregisterContext ReregisterContext(this);
 *		// The component is unregistered with the world here as ReregisterContext is constructed.
 *		...
 * }	// The component is registered with the world here as ReregisterContext is destructed.
 */

class FComponentReregisterContext : public FComponentReregisterContextBase
{
private:
	/** Pointer to component we are unregistering */
	TWeakObjectPtr<UActorComponent> Component;
	/** Cache pointer to world from which we were removed */
	TWeakObjectPtr<UWorld> World;
public:
	FComponentReregisterContext(UActorComponent* InComponent)
		: World(nullptr)
	{
		World = UnRegister(InComponent);
		// If we didn't get a scene back NULL the component so we dont try to
		// process it on destruction
		Component = World.IsValid() ? InComponent : nullptr;
	}

	~FComponentReregisterContext()
	{
		if( Component.IsValid() )
		{
			ReRegister(Component.Get(), World.Get());
		}
	}
};


/* Pairing of UActorComponent and its UWorld. Used only by FMultiComponentReregisterContext
 * for tracking purposes 
 */
struct FMultiComponentReregisterPair
{
	/** Pointer to component we are unregistering */
	UActorComponent* Component;
	/** Cache pointer to world from which we were removed */
	UWorld* World;

	FMultiComponentReregisterPair(UActorComponent* _Component, UWorld* _World) : Component(_Component), World(_World) {}
};


/**
 * Unregisters multiple components for the lifetime of this class.
 *
 * Typically used by constructing the class on the stack:
 * {
 *		FMultiComponentReregisterContext ReregisterContext(arrayOfComponents);
 *		// The components are unregistered with the world here as ReregisterContext is constructed.
 *		...
 * }	// The components are registered with the world here as ReregisterContext is destructed.
 */

class FMultiComponentReregisterContext : public FComponentReregisterContextBase
{
private:
	/** Component pairs that need to be re registered */
	TArray<FMultiComponentReregisterPair> ComponentsPair;
	
public:
	FMultiComponentReregisterContext(const TArray<UActorComponent*>& InComponents)
	{
		// Unregister each component and cache resulting scene
		for (UActorComponent* Component : InComponents)
		{
			UWorld* World = UnRegister(Component);
			if(World)
			{
				ComponentsPair.Push( FMultiComponentReregisterPair(Component, World) );
			}
		}
	}

	~FMultiComponentReregisterContext()
	{
		//Re-register each valid component pair that we unregistered in our constructor
		for(auto Iter = ComponentsPair.CreateIterator(); Iter; ++Iter)
		{
			FMultiComponentReregisterPair& pair = (*Iter);
			if(pair.Component)
			{
				ReRegister(pair.Component, pair.World);
			}
		}
	}
};

/** Removes all components from their scenes for the lifetime of the class. */
class FGlobalComponentReregisterContext
{
public:
	/** 
	* Initialization constructor. 
	*/
	ENGINE_API FGlobalComponentReregisterContext();
	
	/** 
	* Initialization constructor. 
	*
	* @param ExcludeComponents - Component types to exclude when reregistering 
	*/
	FGlobalComponentReregisterContext(const TArray<UClass*>& ExcludeComponents);

	/** Destructor */
	ENGINE_API ~FGlobalComponentReregisterContext();

	/** Indicates that a FGlobalComponentReregisterContext is currently active */
	static int32 ActiveGlobalReregisterContextCount;

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
};

/** Removes all components of the templated type from their scenes for the lifetime of the class. */
template<class ComponentType>
class TComponentReregisterContext
{
public:
	/** Initialization constructor. */
	TComponentReregisterContext()
	{
		// wait until resources are released
		FlushRenderingCommands();

		// Reregister all components of the templated type.
		for(TObjectIterator<ComponentType> ComponentIt;ComponentIt;++ComponentIt)
		{
			new(ComponentContexts) FComponentReregisterContext(*ComponentIt);
		}
	}

private:
	/** The recreate contexts for the individual components. */
	TIndirectArray<FComponentReregisterContext> ComponentContexts;
};

