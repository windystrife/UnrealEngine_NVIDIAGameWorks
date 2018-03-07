// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "NiagaraParameterCollection.h"
#include "GCObject.h"
#include "NiagaraDataSet.h"
#include "NiagaraScriptExecutionContext.h"
#include "NiagaraSystemSimulation.h"

class UWorld;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;

/**
* Manager class for any data relating to a particular world.
*/
class FNiagaraWorldManager : public FGCObject
{
public:
	
	FNiagaraWorldManager(UWorld* InWorld)
		: World(InWorld)
		, CachedEffectsQuality(INDEX_NONE)
	{}

	static FNiagaraWorldManager* Get(UWorld* World);

	//~ GCObject Interface
	void AddReferencedObjects(FReferenceCollector& Collector);
	//~ GCObject Interface
	
	UNiagaraParameterCollectionInstance* GetParameterCollection(UNiagaraParameterCollection* Collection);
	void SetParameterCollection(UNiagaraParameterCollectionInstance* NewInstance);
	FNiagaraSystemSimulation* GetSystemSimulation(UNiagaraSystem* System);
	void DestroySystemSimulation(UNiagaraSystem* System);

	void Tick(float DeltaSeconds);
private:
	UWorld* World;

	TMap<UNiagaraParameterCollection*, UNiagaraParameterCollectionInstance*> ParameterCollections;

	TMap<UNiagaraSystem*, FNiagaraSystemSimulation> SystemSimulations;

	int32 CachedEffectsQuality;
};

