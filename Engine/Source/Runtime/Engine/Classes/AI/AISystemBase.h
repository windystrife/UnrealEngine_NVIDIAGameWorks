// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/SoftObjectPath.h"
#include "Modules/ModuleInterface.h"
#include "AISystemBase.generated.h"

UCLASS(abstract, config = Engine, defaultconfig)
class ENGINE_API UAISystemBase : public UObject
{
	GENERATED_UCLASS_BODY()

	virtual ~UAISystemBase(){}
		
	/** 
	 * Called when world initializes all actors and prepares them to start gameplay
	 * @param bTimeGotReset whether the WorldSettings's TimeSeconds are reset to zero
	 */
	virtual void InitializeActorsForPlay(bool bTimeGotReset) PURE_VIRTUAL(UAISystemBase::InitializeActorsForPlay, );

	/**
	 * Event called on world origin location changes
	 *
	 * @param	OldOriginLocation			Previous world origin location
	 * @param	NewOriginLocation			New world origin location
	 */
	virtual void WorldOriginLocationChanged(FIntVector OldOriginLocation, FIntVector NewOriginLocation) PURE_VIRTUAL(UAISystemBase::WorldOriginLocationChanged, );

	/**
	 * Called by UWorld::CleanupWorld.
	 * @param bSessionEnded whether to notify the viewport that the game session has ended
	 * @param NewWorld Optional new world that will be loaded after this world is cleaned up. Specify a new world to prevent it and it's sublevels from being GCed during map transitions.
	 */
	virtual void CleanupWorld(bool bSessionEnded = true, bool bCleanupResources = true, UWorld* NewWorld = NULL) PURE_VIRTUAL(UAISystemBase::CleanupWorld, );

	/** 
	 *	Called by UWorld::BeginPlay to indicate the gameplay has started
	 */
	virtual void StartPlay();

private:
	UPROPERTY(globalconfig, noclear, meta = (MetaClass = "AISystem", DisplayName = "AISystem Class"))
	FSoftClassPath AISystemClassName;

	UPROPERTY(globalconfig, noclear, meta = (MetaClass = "AISystem", DisplayName = "AISystem Module"))
	FName AISystemModuleName;

	UPROPERTY(globalconfig, noclear)
	bool bInstantiateAISystemOnClient;
	
public:
	static FSoftClassPath GetAISystemClassName();
	static FName GetAISystemModuleName();
	static bool ShouldInstantiateInNetMode(ENetMode NetMode);
};

class IAISystemModule : public IModuleInterface
{
public:
	virtual UAISystemBase* CreateAISystemInstance(UWorld* World) = 0;
};
