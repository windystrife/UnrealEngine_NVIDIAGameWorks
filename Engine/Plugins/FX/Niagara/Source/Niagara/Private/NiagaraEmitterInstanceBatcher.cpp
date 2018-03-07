// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraScriptExecutionContext.h"
#include "RHI.h"
#include "NiagaraStats.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "ClearQuad.h"

DECLARE_CYCLE_STAT(TEXT("Batching"), STAT_NiagaraBatching, STATGROUP_Niagara);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Niagara GPU Sim"), Stat_GPU_NiagaraSim, STATGROUP_GPU);


NiagaraEmitterInstanceBatcher* NiagaraEmitterInstanceBatcher::BatcherSingleton = nullptr;
uint32 FNiagaraComputeExecutionContext::TickCounter = 0;


void NiagaraEmitterInstanceBatcher::Queue(FNiagaraComputeExecutionContext *InContext)
{
	//SimulationQueue[CurQueueIndex]->Add(InContext);

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(QueueNiagaraDispatch,
		TArray<FNiagaraComputeExecutionContext*>*, Queue, &SimulationQueue[0],
		uint32, QueueIndex, CurQueueIndex,
		FNiagaraComputeExecutionContext*, ExecContext, InContext,
		{
			Queue[QueueIndex].Add(ExecContext);
		});
}


void NiagaraEmitterInstanceBatcher::ExecuteAll(FRHICommandList &RHICmdList)
{
	TArray<FNiagaraComputeExecutionContext*> &WorkQueue = SimulationQueue[CurQueueIndex ^ 0x1];
	for (FNiagaraComputeExecutionContext *Context : WorkQueue)
	{
		//ExecuteSingle(Context, RHICmdList);
		TickSingle(Context, RHICmdList);
	}
	WorkQueue.Empty();
}

void NiagaraEmitterInstanceBatcher::TickSingle(const FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraTick);

	check(IsInRenderingThread());
	Context->MainDataSet->TickRenderThread(ENiagaraSimTarget::GPUComputeSim);

	FNiagaraShader *UpdateShader = Context->RTUpdateScript->GetShader();
	FNiagaraShader *SpawnShader = Context->RTSpawnScript->GetShader();
	if (!UpdateShader || !SpawnShader)
	{
		return;
	}

	uint32 PrevNumInstances = Context->MainDataSet->PrevDataRender().GetNumInstances();

	uint32 EventSpawnTotal = 0;
	for (int32 i = 0; i < Context->EventHandlerScriptProps.Num(); i++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = Context->EventHandlerScriptProps[i];
		if (EventHandlerProps.ExecutionMode == EScriptExecutionMode::SpawnedParticles && Context->EventSets[i])
		{
			uint32 NumEventsToProcess = Context->EventSets[i]->PrevDataRender().GetNumInstances();
			uint32 EventSpawnNum = NumEventsToProcess * EventHandlerProps.SpawnNumber;
			EventSpawnTotal += EventSpawnNum;
		}
	}


	// allocate for additional instances spawned and set the new number in the data set, if the new number is greater (meaning if we're spawning in this run)
	// TODO: interpolated spawning
	//
	uint32 NewNumInstances = Context->SpawnRateInstances + Context->BurstInstances + EventSpawnTotal + PrevNumInstances;
	if (NewNumInstances > PrevNumInstances)
	{
		Context->MainDataSet->CurrDataRender().AllocateGPU(NewNumInstances, RHICmdList);
		Context->MainDataSet->CurrDataRender().SetNumInstances(NewNumInstances);
	}
	// if we're not spawning, we need to make sure that the current buffer alloc size and number of instances matches the last one
	// we may have spawned in the last tick, so the buffers may be different sizes
	//
	else if (Context->MainDataSet->CurrDataRender().GetNumInstances() < Context->MainDataSet->PrevDataRender().GetNumInstances())
	{
		Context->MainDataSet->CurrDataRender().AllocateGPU(PrevNumInstances, RHICmdList);
		Context->MainDataSet->CurrDataRender().SetNumInstances(PrevNumInstances);
	}

	//UE_LOG(LogNiagara, Log, TEXT("Pre sim instances %i"), PrevNumInstances);

	// simulation run
	RHICmdList.SetComputeShader(UpdateShader->GetComputeShader());
	SetupDataInterfaceBuffers(Context->UpdateInterfaces, UpdateShader, RHICmdList);
	SetupEventUAVs(Context, PrevNumInstances, RHICmdList);
	Run(Context->MainDataSet, 0, PrevNumInstances, UpdateShader, Context->UpdateParams, RHICmdList);
	UnsetEventUAVs(Context, RHICmdList);

	// resolve data set writes
	// grabs the number of instances written from the index set during the simulation run
	//
	uint32 NumInstancesAfterSim[64];
	NumInstancesAfterSim[0] = PrevNumInstances;
	ResolveDatasetWrites(NumInstancesAfterSim, Context);		// this is going to cause a flush; need to place it differently
	Context->MainDataSet->CurrDataRender().SetNumInstances(NumInstancesAfterSim[0]);

	// TODO: hack - only updating event set 0 on update scripts now; need to match them to their indices and update them all
	if (Context->UpdateEventWriteDataSets.Num())
	{
		Context->UpdateEventWriteDataSets[0]->CurrDataRender().SetNumInstances(NumInstancesAfterSim[1]);
	}

	UE_LOG(LogNiagara, Log, TEXT("After sim instances %i"), NumInstancesAfterSim[0]);

	// spawn run
	// bursts and regular spawn rate happen at once here
	RHICmdList.SetComputeShader(SpawnShader->GetComputeShader());
	uint32 SpawnInstances = Context->SpawnRateInstances + Context->BurstInstances + EventSpawnTotal;
	uint32 NumInstancesAfterSpawn = NumInstancesAfterSim[0] + SpawnInstances;
	uint32 NumInstancesAfterNonEventSpawn = NumInstancesAfterSim[0] + Context->SpawnRateInstances + Context->BurstInstances;
	if (SpawnInstances)
	{
		Run(Context->MainDataSet, NumInstancesAfterSim[0], Context->SpawnRateInstances + Context->BurstInstances + EventSpawnTotal, SpawnShader, Context->SpawnParams, RHICmdList);
		Context->MainDataSet->CurrDataRender().SetNumInstances(NumInstancesAfterSpawn);

		//uncomment this to check number of spawn instances versus ones actually processed by the shader
		uint32 NumInstancesSpawned[64];
		ResolveDatasetWrites(NumInstancesSpawned, Context);
		ensure(NumInstancesSpawned[0] == SpawnInstances);
		UE_LOG(LogNiagara, Log, TEXT("Spawned %i to %i, ran spawn script on %i"), Context->SpawnRateInstances, NumInstancesAfterSpawn, NumInstancesSpawned[0]);

		ensure(NumInstancesAfterNonEventSpawn + EventSpawnTotal == NumInstancesAfterSpawn);
		ensure(NumInstancesAfterNonEventSpawn + EventSpawnTotal == Context->MainDataSet->CurrDataRender().GetNumInstances());
	}



	RunEventHandlers(Context, NumInstancesAfterSim[0], NumInstancesAfterSpawn, NumInstancesAfterNonEventSpawn, RHICmdList);

	// the VF grabs PrevDataRender for drawing, so need to transition
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Context->MainDataSet->PrevDataRender().GetGPUBufferFloat()->UAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, Context->MainDataSet->PrevDataRender().GetGPUBufferInt()->UAV);
}


void NiagaraEmitterInstanceBatcher::ResolveDatasetWrites(uint32 *OutArray, const FNiagaraComputeExecutionContext *Context) const
{
	if (Context->MainDataSet->GetDataSetIndices().NumBytes > 0)
	{
		int32 *DataSetInstanceIndexBuffer = static_cast<int32*>(RHILockVertexBuffer(Context->MainDataSet->GetDataSetIndices().Buffer, 0, 64 * sizeof(int32), EResourceLockMode::RLM_ReadOnly));
		for (uint32 SetIdx = 0; SetIdx < 64; SetIdx++)
		{
			OutArray[SetIdx] = DataSetInstanceIndexBuffer[SetIdx];
		}
		RHIUnlockVertexBuffer(Context->MainDataSet->GetDataSetIndices().Buffer);
	}
}

void NiagaraEmitterInstanceBatcher::SetupDataInterfaceBuffers(const TArray<FNiagaraScriptDataInterfaceInfo> &DIInfos, FNiagaraShader *Shader, FRHICommandList &RHICmdList) const
{
	// set up data interface buffers, as defined by the DIs during compilation
	//
	uint32 InterfaceIndex = 0;
	for (const FNiagaraScriptDataInterfaceInfo &InterfaceInfo : DIInfos)
	{
		TArray<FNiagaraDataInterfaceBufferData> &BufferDatas = InterfaceInfo.DataInterface->GetBufferDataArray();
		for (FNiagaraDataInterfaceBufferData &BufferData : BufferDatas)
		{
			FShaderResourceParameter *Param = Shader->FindDIBufferParam(InterfaceIndex, BufferData.UniformName);
			if (Param)
			{
				RHICmdList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Param->GetBaseIndex(), BufferData.Buffer.SRV);
			}
		}
		InterfaceIndex++;
	}
}

void NiagaraEmitterInstanceBatcher::Run(FNiagaraDataSet *DataSet, uint32 StartInstance, const uint32 NumInstances, FNiagaraShader *Shader, const TArray<uint8, TAlignedHeapAllocator<16>>  &Params, FRHICommandList &RHICmdList, bool bCopyBeforeStart) const
{
	// Recreate a clear data set index buffer for the simulation shader to write number of written instances to
	//
	FRWBuffer &InstanceIdxBuf = DataSet->SetupDataSetIndices();
	ClearUAV(RHICmdList, InstanceIdxBuf, 0);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EComputeToCompute, InstanceIdxBuf.UAV);

	// set the shader and data set params 
	//
	//RHICmdList.SetComputeShader(Shader->GetComputeShader());
	DataSet->SetShaderParams(Shader, RHICmdList);

	// set the index buffer uav
	//
	if (Shader->OutputIndexBufferParam.IsBound())
	{
		RHICmdList.SetUAVParameter(Shader->GetComputeShader(), Shader->OutputIndexBufferParam.GetUAVIndex(), InstanceIdxBuf.UAV);
	}

	// set the execution parameters
	//
	if (Shader->EmitterTickCounterParam.IsBound())
	{
		RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->EmitterTickCounterParam.GetBaseIndex(), Shader->EmitterTickCounterParam.GetNumBytes(), &FNiagaraComputeExecutionContext::TickCounter);
	}
	uint32 Copy = bCopyBeforeStart ? 1 : 0;
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->CopyInstancesBeforeStartParam.GetBaseIndex(), Shader->CopyInstancesBeforeStartParam.GetNumBytes(), &Copy);

	// Now figure out how many instances each thread needs to simulate and how many thread groups we're running. We do this based on the total number of 
	// instances we need to run and how many threads per thread group there are as well as the max. number of thread groups (defined in NiagaraCommon.h)
	//
	uint32 NumInstancesPerThread = 0;
	uint32 AdjustedNumInstances = NumInstances;
	uint32 SimulateStartInstance = StartInstance;

	// if the caller requests to copy all instances before StartInstance, set up the params accordingly
	// spawn events need to do this, for example
	if (bCopyBeforeStart)
	{
		AdjustedNumInstances += StartInstance;
		StartInstance = 0;
	}

	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->SimulateStartInstanceParam.GetBaseIndex(), Shader->SimulateStartInstanceParam.GetNumBytes(), &SimulateStartInstance);
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->StartInstanceParam.GetBaseIndex(), Shader->StartInstanceParam.GetNumBytes(), &StartInstance);

	NumInstancesPerThread = FMath::DivideAndRoundUp(AdjustedNumInstances, NIAGARA_COMPUTE_THREADGROUP_SIZE);
	uint32 NumThreadGroups = 1;
	if (AdjustedNumInstances > NIAGARA_COMPUTE_THREADGROUP_SIZE)
	{
		NumThreadGroups = FMath::Min(NIAGARA_MAX_COMPUTE_THREADGROUPS, FMath::DivideAndRoundUp(AdjustedNumInstances, NIAGARA_COMPUTE_THREADGROUP_SIZE));
		NumInstancesPerThread = FMath::DivideAndRoundUp(NumInstancesPerThread, NumThreadGroups);
	}

	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->NumInstancesPerThreadParam.GetBaseIndex(), Shader->NumInstancesPerThreadParam.GetNumBytes(), &NumInstancesPerThread);
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->NumInstancesParam.GetBaseIndex(), Shader->NumInstancesParam.GetNumBytes(), &AdjustedNumInstances);
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->NumThreadGroupsParam.GetBaseIndex(), Shader->NumThreadGroupsParam.GetNumBytes(), &NumThreadGroups);

	if (StartInstance > 0)
	{
		//UE_LOG(LogNiagara, Log, TEXT("Startinstance %i (%i)    NumInstances %i (%i)   %i/Thread, %i groups"), StartInstance, SimulateStartInstance, NumInstances, AdjustedNumInstances, NumInstancesPerThread, NumThreadGroups);
	}


	// setup script parameters
	FRHIUniformBufferLayout CBufferLayout(TEXT("Niagara Compute Sim CBuffer"));
	CBufferLayout.ConstantBufferSize = Params.Num();
	if (CBufferLayout.ConstantBufferSize)
	{
		CBufferLayout.ResourceOffset = 0;
		check(CBufferLayout.Resources.Num() == 0);
		const uint8* ParamData = Params.GetData();
		FUniformBufferRHIRef CBuffer = RHICreateUniformBuffer(ParamData, CBufferLayout, EUniformBufferUsage::UniformBuffer_MultiFrame);
		RHICmdList.SetShaderUniformBuffer(Shader->GetComputeShader(), Shader->EmitterConstantBufferParam.GetBaseIndex(), CBuffer);
	}

	// Dispatch, if anything needs to be done
	//
	if (NumInstancesPerThread)
	{
		SCOPED_DRAW_EVENT(RHICmdList, NiagaraGPUSimulationCS);
		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_NiagaraSim);
		DispatchComputeShader(RHICmdList, Shader, NumThreadGroups, 1, 1);
	}

	// Unset UAV parameters and transition resources (TODO: resource transition should be moved to the renderer)
	// 
	DataSet->UnsetShaderParams(Shader, RHICmdList);
	Shader->OutputIndexBufferParam.UnsetUAV(RHICmdList, Shader->GetComputeShader());
}


void NiagaraEmitterInstanceBatcher::RunEventHandlers(const FNiagaraComputeExecutionContext *Context, uint32 NumInstancesAfterSim, uint32 NumInstancesAfterSpawn, uint32 NumInstancesAfterNonEventSpawn, FRHICommandList &RHICmdList) const
{
	// Event handler run
	//
	for (int32 EventScriptIdx = 0; EventScriptIdx < Context->EventHandlerScriptProps.Num(); EventScriptIdx++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = Context->EventHandlerScriptProps[EventScriptIdx];
		FNiagaraDataSet *EventSet = Context->EventSets[EventScriptIdx];
		if (EventSet)
		{
			int32 NumEvents = EventSet->PrevDataRender().GetNumInstances();

			// handle all-particle events
			if (NumEvents && EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::EveryParticle && EventSet)
			{
				FNiagaraShader *EventHandlerShader = EventHandlerProps.Script->GetRenderThreadScript()->GetShader();
				if (EventHandlerShader)
				{
					SetupDataInterfaceBuffers(EventHandlerProps.Script->DataInterfaceInfo, EventHandlerShader, RHICmdList);

					RHICmdList.SetShaderParameter(EventHandlerShader->GetComputeShader(), 0, EventHandlerShader->NumParticlesPerEventParam.GetBaseIndex(), sizeof(int32), &NumInstancesAfterSpawn);
					RHICmdList.SetShaderParameter(EventHandlerShader->GetComputeShader(), 0, EventHandlerShader->NumEventsPerParticleParam.GetBaseIndex(), sizeof(int32), &NumEvents);

					// If we have events, Swap buffers, to make sure we don't overwrite previous script results
					Context->MainDataSet->TickRenderThread(ENiagaraSimTarget::GPUComputeSim);
					Context->MainDataSet->CurrDataRender().AllocateGPU(NumInstancesAfterSpawn, RHICmdList);
					Context->MainDataSet->CurrDataRender().SetNumInstances(NumInstancesAfterSpawn);

					SetPrevDataStrideParams(EventSet, EventHandlerShader, RHICmdList);

					// set up event data set buffers
					FShaderResourceParameter &FloatParam = EventHandlerShader->EventFloatSRVParams[0];
					FShaderResourceParameter &IntParam = EventHandlerShader->EventIntSRVParams[0];
					RHICmdList.SetShaderResourceViewParameter(EventHandlerShader->GetComputeShader(), FloatParam.GetBaseIndex(), EventSet->PrevDataRender().GetGPUBufferFloat()->SRV);
					RHICmdList.SetShaderResourceViewParameter(EventHandlerShader->GetComputeShader(), IntParam.GetBaseIndex(), EventSet->PrevDataRender().GetGPUBufferInt()->SRV);

					TArray<uint8, TAlignedHeapAllocator<16>>  BlankParams;
					Run(Context->MainDataSet, 0, NumInstancesAfterNonEventSpawn, EventHandlerShader, BlankParams, RHICmdList);
				}
			}

			// handle spawn events
			if (EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::SpawnedParticles && EventSet)
			{
				uint32 NumEventsToProcess = EventSet->PrevDataRender().GetNumInstances();
				uint32 EventSpawnNum = NumEventsToProcess * EventHandlerProps.SpawnNumber;

				//int32 SpawnNum = Context->EventSpawnCounts[EventScriptIdx];
				FNiagaraShader *EventHandlerShader = EventHandlerProps.Script->GetRenderThreadScript()->GetShader();
				if (NumEvents && EventSpawnNum && EventHandlerShader)
				{
					//SetupDataInterfaceBuffers(EventHandlerProps.Script->DataInterfaceInfo, EventHandlerShader, RHICmdList);
					int32 OneEvent = 1;
					int32 ParticlesPerEvent = EventSpawnNum / NumEvents;
					RHICmdList.SetShaderParameter(EventHandlerShader->GetComputeShader(), 0, EventHandlerShader->NumEventsPerParticleParam.GetBaseIndex(), sizeof(int32), &OneEvent);
					RHICmdList.SetShaderParameter(EventHandlerShader->GetComputeShader(), 0, EventHandlerShader->NumParticlesPerEventParam.GetBaseIndex(), sizeof(int32), &ParticlesPerEvent);

					// If we have events, Swap buffers
					Context->MainDataSet->TickRenderThread(ENiagaraSimTarget::GPUComputeSim);
					Context->MainDataSet->CurrDataRender().AllocateGPU(NumInstancesAfterSpawn, RHICmdList);
					Context->MainDataSet->CurrDataRender().SetNumInstances(NumInstancesAfterSpawn);

					// set up event data set buffers
					FShaderResourceParameter &FloatParam = EventHandlerShader->EventFloatSRVParams[0];
					FShaderResourceParameter &IntParam = EventHandlerShader->EventIntSRVParams[0];
					RHICmdList.SetShaderResourceViewParameter(EventHandlerShader->GetComputeShader(), FloatParam.GetBaseIndex(), EventSet->PrevDataRender().GetGPUBufferFloat()->SRV);
					RHICmdList.SetShaderResourceViewParameter(EventHandlerShader->GetComputeShader(), IntParam.GetBaseIndex(), EventSet->PrevDataRender().GetGPUBufferInt()->SRV);

					SetPrevDataStrideParams(EventSet, EventHandlerShader, RHICmdList);

					// we assume event spawns are at the end of the buffer
					check(NumInstancesAfterNonEventSpawn + EventSpawnNum == Context->MainDataSet->CurrDataRender().GetNumInstances());

					TArray<uint8, TAlignedHeapAllocator<16>>  BlankParams;
					Run(Context->MainDataSet, NumInstancesAfterNonEventSpawn, EventSpawnNum, EventHandlerShader, BlankParams, RHICmdList, true);
				}
			}
		}
	}
}

void NiagaraEmitterInstanceBatcher::SetPrevDataStrideParams(const FNiagaraDataSet *Set, FNiagaraShader *Shader, FRHICommandList &RHICmdList) const
{
	int32 FloatStride = Set->PrevDataRender().GetFloatStride() / sizeof(float);
	int32 IntStride = Set->PrevDataRender().GetInt32Stride() / sizeof(int32);
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->EventReadFloatStrideParams[0].GetBaseIndex(), sizeof(int32), &FloatStride);
	RHICmdList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->EventReadIntStrideParams[0].GetBaseIndex(), sizeof(int32), &IntStride);
}


void NiagaraEmitterInstanceBatcher::SetupEventUAVs(const FNiagaraComputeExecutionContext *Context, uint32 NewNumInstances, FRHICommandList &RHICmdList) const
{
	FNiagaraShader *UpdateShader = Context->RTUpdateScript->GetShader();

	uint32 SetIndex = 0;
	for (FNiagaraDataSet *Set : Context->UpdateEventWriteDataSets)
	{
		if (NewNumInstances)
		{
			Set->CurrDataRender().AllocateGPU(NewNumInstances, RHICmdList);
			Set->CurrDataRender().SetNumInstances(NewNumInstances);
			FRWShaderParameter &FloatParam = UpdateShader->EventFloatUAVParams[SetIndex];
			FRWShaderParameter &IntParam = UpdateShader->EventIntUAVParams[SetIndex];
			if (FloatParam.IsUAVBound())
			{
				RHICmdList.SetUAVParameter(UpdateShader->GetComputeShader(), FloatParam.GetUAVIndex(), Set->CurrDataRender().GetGPUBufferFloat()->UAV);
			}
			if (IntParam.IsUAVBound())
			{
				RHICmdList.SetUAVParameter(UpdateShader->GetComputeShader(), IntParam.GetUAVIndex(), Set->CurrDataRender().GetGPUBufferInt()->UAV);
			}

			uint32 FloatStride = Set->CurrDataRender().GetFloatStride() / sizeof(float);
			uint32 IntStride = Set->CurrDataRender().GetInt32Stride() / sizeof(int32);
			RHICmdList.SetShaderParameter(UpdateShader->GetComputeShader(), 0, UpdateShader->EventWriteFloatStrideParams[SetIndex].GetBaseIndex(), sizeof(int32), &FloatStride);
			RHICmdList.SetShaderParameter(UpdateShader->GetComputeShader(), 0, UpdateShader->EventWriteIntStrideParams[SetIndex].GetBaseIndex(), sizeof(int32), &IntStride);
		}

		SetIndex++;
	}


}


void NiagaraEmitterInstanceBatcher::UnsetEventUAVs(const FNiagaraComputeExecutionContext *Context, FRHICommandList &RHICmdList) const
{
	FNiagaraShader *UpdateShader = Context->RTUpdateScript->GetShader();

	for (int32 SetIndex = 0; SetIndex < Context->UpdateEventWriteDataSets.Num(); SetIndex++)
	{
		FRWShaderParameter &FloatParam = UpdateShader->EventFloatUAVParams[SetIndex];
		FRWShaderParameter &IntParam = UpdateShader->EventIntUAVParams[SetIndex];
		FloatParam.UnsetUAV(RHICmdList, UpdateShader->GetComputeShader());
		IntParam.UnsetUAV(RHICmdList, UpdateShader->GetComputeShader());
	}
}