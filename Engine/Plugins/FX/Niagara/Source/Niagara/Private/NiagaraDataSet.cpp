// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataSet.h"
#include "NiagaraCommon.h"
#include "NiagaraShader.h"
#include "GlobalShader.h"
#include "UpdateTextureShaders.h"

//////////////////////////////////////////////////////////////////////////
void FNiagaraDataSet::SetShaderParams(FNiagaraShader *Shader, FRHICommandList &CommandList)
{
	check(IsInRenderingThread());

	if (Shader->FloatInputBufferParam.IsBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, PrevDataRender().GetGPUBufferFloat()->UAV);
		CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->FloatInputBufferParam.GetBaseIndex(), PrevDataRender().GetGPUBufferFloat()->SRV);
	}
	if (Shader->IntInputBufferParam.IsBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, PrevDataRender().GetGPUBufferInt()->UAV);
		CommandList.SetShaderResourceViewParameter(Shader->GetComputeShader(), Shader->IntInputBufferParam.GetBaseIndex(), PrevDataRender().GetGPUBufferInt()->SRV);
	}
	if (Shader->FloatOutputBufferParam.IsUAVBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, CurrDataRender().GetGPUBufferFloat()->UAV);
		CommandList.SetUAVParameter(Shader->GetComputeShader(), Shader->FloatOutputBufferParam.GetUAVIndex(), CurrDataRender().GetGPUBufferFloat()->UAV);
	}
	if (Shader->IntOutputBufferParam.IsUAVBound())
	{
		CommandList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, CurrDataRender().GetGPUBufferInt()->UAV);
		CommandList.SetUAVParameter(Shader->GetComputeShader(), Shader->IntOutputBufferParam.GetUAVIndex(), CurrDataRender().GetGPUBufferInt()->UAV);
	}

	if (Shader->ComponentBufferSizeWriteParam.IsBound())
	{
		uint32 SafeBufferSize = CurrDataRender().GetFloatStride() / sizeof(float);
		CommandList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->ComponentBufferSizeWriteParam.GetBaseIndex(), Shader->ComponentBufferSizeWriteParam.GetNumBytes(), &SafeBufferSize);
	}

	if (Shader->ComponentBufferSizeReadParam.IsBound())
	{
		uint32 SafeBufferSize = PrevDataRender().GetFloatStride() / sizeof(float);
		CommandList.SetShaderParameter(Shader->GetComputeShader(), 0, Shader->ComponentBufferSizeReadParam.GetBaseIndex(), Shader->ComponentBufferSizeReadParam.GetNumBytes(), &SafeBufferSize);
	}
}



void FNiagaraDataSet::UnsetShaderParams(FNiagaraShader *Shader, FRHICommandList &RHICmdList)
{
	check(IsInRenderingThread());

	if (Shader->FloatOutputBufferParam.IsUAVBound())
	{
#if !PLATFORM_PS4
		Shader->FloatOutputBufferParam.UnsetUAV(RHICmdList, Shader->GetComputeShader());
#endif
		//RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, CurrDataRender().GetGPUBufferFloat()->UAV);
	}

	if (Shader->IntOutputBufferParam.IsUAVBound())
	{
#if !PLATFORM_PS4
		Shader->IntOutputBufferParam.UnsetUAV(RHICmdList, Shader->GetComputeShader());
#endif
		//RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, CurrDataRender().GetGPUBufferInt()->UAV);
	}
}

void FNiagaraDataSet::Dump(FNiagaraDataSet& Other, bool bCurr)
{
	Other.Reset();
	Other.Variables = Variables;
	Other.VariableLayoutMap = VariableLayoutMap;

	int32 IndexRead = CurrBuffer > 0 ? CurrBuffer - 1 : MaxBufferIdx;
	if (bCurr)
	{
		IndexRead = CurrBuffer;
	}

	if (Other.Data[Other.CurrBuffer].GetNumInstancesAllocated() != Data[IndexRead].GetNumInstancesAllocated())
	{
		Other.Finalize();
		Other.Data[Other.CurrBuffer].Allocate(Data[IndexRead].GetNumInstancesAllocated());
	}

	Data[CurrBuffer].CopyTo(Other.Data[Other.CurrBuffer]);
}


void FNiagaraDataSet::Dump(bool bCurr, int32 StartIdx, int32 NumInstances)
{
	TArray<FNiagaraVariable> Vars(Variables);

	FNiagaraDataSetVariableIterator Itr(*this, StartIdx, bCurr);
	Itr.AddVariables(Vars);

	if (NumInstances == INDEX_NONE)
	{
		NumInstances = bCurr ? GetNumInstances() : GetPrevNumInstances();
		NumInstances -= StartIdx;
	}

	int32 NumInstancesDumped = 0;
	TArray<FString> Lines;
	Lines.Reserve(GetNumInstances());
	while (Itr.IsValid() && NumInstancesDumped++ < NumInstances)
	{
		Itr.Get();

		FString Line = TEXT("| ");
		for (FNiagaraVariable& Var : Vars)
		{
			Line += Var.ToString() + TEXT(" | ");
		}
		Lines.Add(Line);
		Itr.Advance();
	}

	static FString Sep;
	if (Sep.Len() == 0)
	{
		for (int32 i = 0; i < 50; ++i)
		{
			Sep.AppendChar(TEXT('='));
		}
	}

	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
	UE_LOG(LogNiagara, Log, TEXT(" Buffer: %d"), CurrBuffer);
 	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
// 	UE_LOG(LogNiagara, Log, TEXT("%s"), *HeaderStr);
// 	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
	for (FString& Str : Lines)
	{
		UE_LOG(LogNiagara, Log, TEXT("%s"), *Str);
	}
	UE_LOG(LogNiagara, Log, TEXT("%s"), *Sep);
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataBuffer::Init(FNiagaraDataSet* InOwner)
{
	Owner = InOwner;
}

void FNiagaraDataBuffer::Allocate(uint32 InNumInstances, ENiagaraSimTarget Target, bool bMaintainExisting)
{
	check(Owner);
	if (Target == ENiagaraSimTarget::CPUSim)
	{
		NumInstancesAllocated = InNumInstances;

		int32 OldFloatStride = FloatStride;
		FloatStride = GetSafeComponentBufferSize(NumInstancesAllocated * sizeof(float));
		FloatData.SetNum(FloatStride * Owner->GetNumFloatComponents(), false);

		int32 OldInt32Stride = Int32Stride;
		Int32Stride = GetSafeComponentBufferSize(NumInstancesAllocated * sizeof(int32));
		Int32Data.SetNum(Int32Stride * Owner->GetNumInt32Components(), false);

		//In some cases we want the existing data in the buffer to be maintained which due to the data layout requires some fix up.
		if (bMaintainExisting)
		{
			if (FloatStride != OldFloatStride && FloatStride > 0 && OldFloatStride > 0)
			{
				for (uint32 CompIdx = Owner->TotalFloatComponents-1; CompIdx > 0; --CompIdx)
				{
					uint8* Src = FloatData.GetData() + OldFloatStride * CompIdx;
					uint8* Dst = FloatData.GetData() + FloatStride * CompIdx;
					FMemory::Memcpy(Dst, Src, OldFloatStride);
				}
			}
			if (Int32Stride != OldInt32Stride && Int32Stride > 0 && OldInt32Stride > 0)
			{
				for (uint32 CompIdx = Owner->TotalInt32Components - 1; CompIdx > 0; --CompIdx)
				{
					uint8* Src = Int32Data.GetData() + OldInt32Stride * CompIdx;
					uint8* Dst = Int32Data.GetData() + Int32Stride * CompIdx;
					FMemory::Memcpy(Dst, Src, OldFloatStride);
				}
			}
		}
	}
}


void FNiagaraDataBuffer::AllocateGPU(uint32 InNumInstances, FRHICommandList &RHICmdList)
{
	if (Owner == 0)
	{
		return;
	}
	check(IsInRenderingThread())
	const uint32 ALLOC_CHUNKSIZE = 4096;

	NumInstancesAllocated = InNumInstances;

	uint32 PaddedNumInstances = ((InNumInstances + NIAGARA_COMPUTE_THREADGROUP_SIZE -1) / NIAGARA_COMPUTE_THREADGROUP_SIZE) * NIAGARA_COMPUTE_THREADGROUP_SIZE;
	FloatStride = PaddedNumInstances * sizeof(float);
	Int32Stride = PaddedNumInstances * sizeof(int32);

	if (NumInstancesAllocated > NumChunksAllocatedForGPU * ALLOC_CHUNKSIZE)
	{
		uint32 PrevChunks = NumChunksAllocatedForGPU;
		NumChunksAllocatedForGPU = ((InNumInstances + ALLOC_CHUNKSIZE - 1) / ALLOC_CHUNKSIZE);
		uint32 NumElementsToAlloc = NumChunksAllocatedForGPU * ALLOC_CHUNKSIZE;

		if (Owner->GetNumFloatComponents())
		{
			if (GPUBufferFloat.Buffer)
			{
				GPUBufferFloat.Release();
			}
			GPUBufferFloat.Initialize(sizeof(float), NumElementsToAlloc * Owner->GetNumFloatComponents(), EPixelFormat::PF_R32_FLOAT);
		}
		if (Owner->GetNumInt32Components())
		{
			if (GPUBufferInt.Buffer)
			{
				GPUBufferInt.Release();
			}
			GPUBufferInt.Initialize(sizeof(int32), NumElementsToAlloc * Owner->GetNumInt32Components(), EPixelFormat::PF_R32_SINT);
		}
	}
}


void FNiagaraDataBuffer::InitGPUFromCPU()
{
	if (Owner->GetNumFloatComponents())
	{
		GPUBufferFloat.Release();
		GPUBufferFloat.Initialize(sizeof(float), FloatStride/sizeof(float) * Owner->GetNumFloatComponents(), EPixelFormat::PF_R32_FLOAT, 0, TEXT("GPUBufferFloat"), &FloatData);
	}
	if (Owner->GetNumInt32Components())
	{
		GPUBufferInt.Release();
		GPUBufferInt.Initialize(sizeof(int32), Int32Stride/sizeof(int32) * Owner->GetNumInt32Components(), EPixelFormat::PF_R32_SINT, 0, TEXT("GPUBufferInt"), &Int32Data);
	}
}

void FNiagaraDataBuffer::SwapInstances(uint32 OldIndex, uint32 NewIndex) 
{
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalFloatComponents; ++CompIdx)
	{
		float* Src = GetInstancePtrFloat(CompIdx, OldIndex);
		float* Dst = GetInstancePtrFloat(CompIdx, NewIndex);
		float Temp = *Dst;
		*Dst = *Src;
		*Src = Temp;
	}
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalInt32Components; ++CompIdx)
	{
		int32* Src = GetInstancePtrInt32(CompIdx, OldIndex);
		int32* Dst = GetInstancePtrInt32(CompIdx, NewIndex);
		int32 Temp = *Dst;
		*Dst = *Src;
		*Src = Temp;
	}
}

void FNiagaraDataBuffer::KillInstance(uint32 InstanceIdx)
{
	check(InstanceIdx < NumInstances);
	--NumInstances;

	for (uint32 CompIdx = 0; CompIdx < Owner->TotalFloatComponents; ++CompIdx)
	{
		float* Src = GetInstancePtrFloat(CompIdx, NumInstances);
		float* Dst = GetInstancePtrFloat(CompIdx, InstanceIdx);
		*Dst = *Src;
	}
	for (uint32 CompIdx = 0; CompIdx < Owner->TotalInt32Components; ++CompIdx)
	{
		int32* Src = GetInstancePtrInt32(CompIdx, NumInstances);
		int32* Dst = GetInstancePtrInt32(CompIdx, InstanceIdx);
		*Dst = *Src;
	}
}

void FNiagaraDataBuffer::CopyTo(FNiagaraDataBuffer& DestBuffer)
{
	DestBuffer.FloatStride = FloatStride;
	DestBuffer.FloatData = FloatData;
	DestBuffer.Int32Stride = Int32Stride;
	DestBuffer.Int32Data = Int32Data;
	DestBuffer.NumInstancesAllocated = NumInstancesAllocated;
	DestBuffer.NumInstances = NumInstances;
}