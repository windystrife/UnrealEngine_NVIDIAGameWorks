// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Engine/World.h"

class FNiagaraWorldManager;

/**
* Niagara module interface
*/
class INiagaraModule : public IModuleInterface
{
public:
	DECLARE_DELEGATE_RetVal(void, FOnProcessQueue);

	virtual void StartupModule()override;
	virtual void ShutdownModule()override;

	FDelegateHandle NIAGARA_API SetOnProcessShaderCompilationQueue(FOnProcessQueue InOnProcessQueue);
	void NIAGARA_API ResetOnProcessShaderCompilationQueue(FDelegateHandle DelegateHandle);
	void ProcessShaderCompilationQueue();


	FNiagaraWorldManager* GetWorldManager(UWorld* World);

	void DestroyAllSystemSimulations(class UNiagaraSystem* System);

	// Callback function registered with global world delegates to instantiate world manager when a game world is created
	void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);

	// Callback function registered with global world delegates to cleanup world manager contents
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	// Callback function registered with global world delegates to cleanup world manager when a game world is destroyed
	void OnPreWorldFinishDestroy(UWorld* World);

	void TickWorld(UWorld* World, ELevelTick TickType, float DeltaSeconds);
private:
	TMap<class UWorld*, class FNiagaraWorldManager*> WorldManagers;
	FOnProcessQueue OnProcessQueue;
};

