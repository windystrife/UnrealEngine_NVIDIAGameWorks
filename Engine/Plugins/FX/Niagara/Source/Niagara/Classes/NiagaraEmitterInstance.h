// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstance.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEvents.h"
#include "NiagaraCollision.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptExecutionContext.h"

class FNiagaraSystemInstance;
class NiagaraRenderer;
struct FNiagaraEmitterHandle;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;

/**
* A Niagara particle simulation.
*/
struct FNiagaraEmitterInstance
{

private:
	struct FNiagaraBurstInstance
	{
		float Time;
		uint32 NumberToSpawn;
	};

public:
	explicit FNiagaraEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance);
	bool bDumpAfterEvent;
	virtual ~FNiagaraEmitterInstance();

	void Init(int32 InEmitterIdx, FName SystemInstanceName);
	void ResetSimulation();
	void ReInitSimulation();
	void DirtyDataInterfaces();

	/** Replaces the binding for a single parameter colleciton instance. If for example the component begins to override the global instance. */
	//void RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance);
	void BindParameters();
	void UnbindParameters();
	
	/** Called after all emitters in an System have been initialized, allows emitters to access information from one another. */
	void PostResetSimulation();

	void PreTick();
	void Tick(float DeltaSeconds);

	uint32 CalculateEventSpawnCount(const FNiagaraEventScriptProperties &EventHandlerProps, TArray<int32> &EventSpawnCounts, FNiagaraDataSet *EventSet);
	void PostProcessParticles();

	FBox GetBounds() const { return CachedBounds; }

	FNiagaraDataSet &GetData()	{ return Data; }

	int32 GetEmitterRendererNum() const {
		return EmitterRenderer.Num();
	}

	NiagaraRenderer *GetEmitterRenderer(int32 Index)	{ return EmitterRenderer[Index]; }
	
	bool IsEnabled()const 	{ return bIsEnabled;  }
	
	/** Set whether or not this simulation is enabled*/
	void SetEnabled(bool bEnabled) { bIsEnabled = bEnabled; }

	/** Sets the error state. */
	void SetError(bool bInError) { bError = bInError; }
		
	/** Create a new NiagaraRenderer. The old renderer is not immediately deleted, but instead put in the ToBeRemoved list.*/
	void NIAGARA_API UpdateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, TArray<NiagaraRenderer*>& ToBeAddedList, TArray<NiagaraRenderer*>& ToBeRemovedList);

	int32 GetNumParticles()	{ return Data.GetNumInstances(); }

	NIAGARA_API const FNiagaraEmitterHandle& GetEmitterHandle() const;

	FNiagaraSystemInstance* GetParentSystemInstance()	{ return ParentSystemInstance; }

	float NIAGARA_API GetTotalCPUTime();
	int	NIAGARA_API GetTotalBytesUsed();
	
	ENiagaraExecutionState NIAGARA_API GetExecutionState()	{ return ExecutionState; }
	void NIAGARA_API SetExecutionState(ENiagaraExecutionState InState);

	FNiagaraDataSet* GetDataSet(FNiagaraDataSetID SetID);

	/** Tell the renderer thread that we're done with the Niagara renderer on this simulation.*/
	void ClearRenderer();

	FBox GetCachedBounds() { return CachedBounds; }

	FNiagaraScriptExecutionContext& GetSpawnExecutionContext() { return SpawnExecContext; }
	FNiagaraScriptExecutionContext& GetUpdateExecutionContext() { return UpdateExecContext; }
	TArray<FNiagaraScriptExecutionContext>& GetEventExecutionContexts() { return EventExecContexts; }

	TArray<FNiagaraSpawnInfo>& GetSpawnInfo() { return SpawnInfos; }
private:

	/** The index of our emitter in our parent system instance. */
	int32 EmitterIdx;

	/* The age of the emitter*/
	float Age;
	/* how many loops this emitter has gone through */
	uint32 Loops;
	/* If false, don't tick or render*/
	bool bIsEnabled;
	
	/* Some error has occurred so stop ticking or rendering. Better to just kill the emitter? */
	bool bError;

	/* Seconds taken to process everything (including rendering) */
	float CPUTimeMS;
	/* Emitter tick state */
	ENiagaraExecutionState ExecutionState;
	/* Emitter bounds */
	FBox CachedBounds;

	/** Array of all spawn info driven by our owning emitter script. */
	TArray<FNiagaraSpawnInfo> SpawnInfos;

	FNiagaraScriptExecutionContext SpawnExecContext;
	FNiagaraScriptExecutionContext UpdateExecContext;
	TArray<FNiagaraScriptExecutionContext> EventExecContexts;

	FNiagaraParameterDirectBinding<float> SpawnIntervalBinding;
	FNiagaraParameterDirectBinding<float> InterpSpawnStartBinding;

	FNiagaraParameterDirectBinding<float> SpawnEmitterAgeBinding;
	FNiagaraParameterDirectBinding<float> UpdateEmitterAgeBinding;
	TArray<FNiagaraParameterDirectBinding<float>> EventEmitterAgeBindings;

	FNiagaraParameterDirectBinding<int32> SpawnExecCountBinding;
	FNiagaraParameterDirectBinding<int32> UpdateExecCountBinding;
	TArray<FNiagaraParameterDirectBinding<int32>> EventExecCountBindings;

	/** particle simulation data */
	FNiagaraDataSet Data;
	/** The cached GetComponentTransform() transform. */
	FTransform CachedComponentToWorld;

	TArray<NiagaraRenderer*> EmitterRenderer;
	FNiagaraSystemInstance *ParentSystemInstance;

	TArray<FNiagaraDataSet*> UpdateScriptEventDataSets;
	TArray<FNiagaraDataSet*> SpawnScriptEventDataSets;
	TMap<FNiagaraDataSetID, FNiagaraDataSet*> DataSetMap;
	
	FNiagaraCollisionBatch CollisionBatch;

	FName OwnerSystemInstanceName;

#if WITH_EDITORONLY_DATA
	bool CheckAttributesForRenderer(int32 Index);
#endif
};
