// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstanceBatcher.h: Queueing and batching for Niagara simulation;
use to reduce per-simulation overhead by batching together simulations using
the same VectorVM byte code / compute shader code
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "RendererInterface.h"
#include "NiagaraParameters.h"
#include "NiagaraEmitter.h"
#include "Tickable.h"
#include "ModuleManager.h"
#include "ModuleManager.h"

struct FNiagaraScriptExecutionContext;

struct FNiagaraComputeExecutionContext
{
	FNiagaraComputeExecutionContext()
		: MainDataSet(nullptr)
		, SpawnRateInstances(0)
		, BurstInstances(0)
		, EventSpawnTotal(0)
		, RTUpdateScript(0)
		, RTSpawnScript(0)
	{
		TickCounter++;
	}
	class FNiagaraDataSet *MainDataSet;
	TArray<FNiagaraDataSet*>UpdateEventWriteDataSets;
	TArray<FNiagaraEventScriptProperties> EventHandlerScriptProps;
	TArray<FNiagaraDataSet*> EventSets;
	uint32 SpawnRateInstances;
	uint32 BurstInstances;

	TArray<int32> EventSpawnCounts;
	uint32 EventSpawnTotal;

	class FNiagaraScript *RTUpdateScript;
	class FNiagaraScript *RTSpawnScript;
	TArray<uint8, TAlignedHeapAllocator<16>> UpdateParams;		// RT side copy of the parameter data
	TArray<FNiagaraScriptDataInterfaceInfo> UpdateInterfaces;
	TArray<uint8, TAlignedHeapAllocator<16>> SpawnParams;		// RT side copy of the parameter data
	static uint32 TickCounter;
};


class NiagaraEmitterInstanceBatcher : public FTickableGameObject, public FComputeDispatcher
{
public:
	NiagaraEmitterInstanceBatcher()
		: CurQueueIndex(0)
	{
		IRendererModule *RendererModule = FModuleManager::GetModulePtr<IRendererModule>("Renderer");
		if (RendererModule)
		{
			RendererModule->RegisterPostOpaqueComputeDispatcher(this);
		}
	}

	~NiagaraEmitterInstanceBatcher()
	{
		IRendererModule *RendererModule = FModuleManager::GetModulePtr<IRendererModule>("Renderer");
		if (RendererModule)
		{
			RendererModule->UnRegisterPostOpaqueComputeDispatcher(this);
		}
	}

	static NiagaraEmitterInstanceBatcher *Get()
	{
		if (BatcherSingleton == nullptr)
		{
			BatcherSingleton = new NiagaraEmitterInstanceBatcher();
		}
		return BatcherSingleton;
	}

	void Queue(FNiagaraComputeExecutionContext *InContext);

	virtual bool IsTickable() const
	{
		return true;
	}

	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(NiagaraEmitterInstanceBatcher, STATGROUP_Tickables);
	}


	virtual void Tick(float DeltaTime) override
	{
		BuildBatches();
	}

	// TODO: process queue, build batches from context with the same script
	//  also need to figure out how to handle multiple sets of parameters across a batch
	//	for now this executes every single sim in the queue individually, which is terrible 
	//	in terms of overhead
	void BuildBatches()
	{
	}

	// from FComputeDispatcher; called once per frame by the render thread, swaps buffers and works down 
	// the queue submitted by the game thread; means we're one frame behind with this; we need a mechanism to determine execution order here.
	virtual void Execute(FRHICommandList &RHICmdList)
	{
		CurQueueIndex ^= 0x1;
		ExecuteAll(RHICmdList);
	}

	void ExecuteAll(FRHICommandList &RHICmdList);
	void TickSingle(const FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList) const;

	void SetPrevDataStrideParams(const FNiagaraDataSet *Set, FNiagaraShader *Shader, FRHICommandList &RHICmdList) const;

	void SetupEventUAVs(const FNiagaraComputeExecutionContext *Context, uint32 NumInstances, FRHICommandList &RHICmdList) const;
	void UnsetEventUAVs(const FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList) const;
	void SetupDataInterfaceBuffers(const TArray<FNiagaraScriptDataInterfaceInfo> &DIInfos, FNiagaraShader *Shader, FRHICommandList &RHICmdList) const;

	void Run(FNiagaraDataSet *DataSet, const uint32 StartInstance, const uint32 NumInstances, class FNiagaraShader *Shader, const TArray<uint8, TAlignedHeapAllocator<16>>  &Params, FRHICommandList &RhiCmdList, bool bCopyBeforeStart = false) const;
	void RunEventHandlers(const FNiagaraComputeExecutionContext *Context, uint32 NumInstancesAfterSim, uint32 NumInstancesAfterSpawn, uint32 NumInstancesAfterNonEventSpawn, FRHICommandList &RhiCmdList) const;
	void ResolveDatasetWrites(uint32 *OutArray, const FNiagaraComputeExecutionContext *Context) const;
private:
	static NiagaraEmitterInstanceBatcher* BatcherSingleton;

	uint32 CurQueueIndex;
	TArray<FNiagaraComputeExecutionContext*> SimulationQueue[2];
};
