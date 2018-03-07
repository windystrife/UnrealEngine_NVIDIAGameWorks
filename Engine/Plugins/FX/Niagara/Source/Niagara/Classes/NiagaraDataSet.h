// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "DynamicRHIResourceArray.h"
#include "RHI.h"
#include "VectorVM.h"
#include "RenderingThread.h"


/** Helper class defining the layout and location of an FNiagaraVariable in an FNiagaraDataBuffer-> */
struct FNiagaraVariableLayoutInfo
{
	/** Start index for the float components in the main buffer. */
	uint32 FloatComponentStart;
	/** Start index for the int32 components in the main buffer. */
	uint32 Int32ComponentStart;

	uint32 GetNumFloatComponents()const { return LayoutInfo.FloatComponentByteOffsets.Num(); }
	uint32 GetNumInt32Components()const { return LayoutInfo.Int32ComponentByteOffsets.Num(); }

	/** This variable's type layout info. */
	FNiagaraTypeLayoutInfo LayoutInfo;
};

class FNiagaraDataSet;

/** Buffer containing one frame of Niagara simulation data. */
class FNiagaraDataBuffer
{
public:
	FNiagaraDataBuffer() : Owner(nullptr)
	{
		Reset();
	}
	void Init(FNiagaraDataSet* InOwner);
	void Allocate(uint32 NumInstances, ENiagaraSimTarget Target = ENiagaraSimTarget::CPUSim, bool bMaintainExisting=false);
	void AllocateGPU(uint32 InNumInstances, FRHICommandList &RHICmdList);
	void InitGPUFromCPU();
	void SwapInstances(uint32 OldIndex, uint32 NewIndex);
	void KillInstance(uint32 InstanceIdx);
	void CopyTo(FNiagaraDataBuffer& DestBuffer);

	/** Returns a ptr to the start of the buffer for the passed component idx and base type. */
	//template<typename T>
	//FORCEINLINE uint8* GetComponentPtr(uint32 ComponentIdx);
	/** Returns a ptr to specific instance data in the buffer for the passed component idx and base type. */

	FORCEINLINE uint8* GetComponentPtrFloat(uint32 ComponentIdx)
	{
		return FloatData.GetData() + FloatStride * ComponentIdx;
	}

	FORCEINLINE uint8* GetComponentPtrInt32(uint32 ComponentIdx)
	{
		return Int32Data.GetData() + Int32Stride * ComponentIdx;
	}

	FORCEINLINE float* GetInstancePtrFloat(uint32 ComponentIdx, uint32 InstanceIdx)
	{
		return (float*)GetComponentPtrFloat(ComponentIdx) + InstanceIdx;
	}

	FORCEINLINE int32* GetInstancePtrInt32(uint32 ComponentIdx, uint32 InstanceIdx)
	{
		return (int32*)GetComponentPtrInt32(ComponentIdx) + InstanceIdx;
	}

	uint32 GetNumInstances()const { return NumInstances; }
	uint32 GetNumInstancesAllocated()const { return NumInstancesAllocated; }

	void SetNumInstances(uint32 InNumInstances) 
	{ 
		NumInstances = InNumInstances;
	}

	FORCEINLINE void Reset()
	{
		FloatData.Empty();
		Int32Data.Empty();
		FloatStride = 0;
		Int32Stride = 0;
		NumInstances = 0;
		NumInstancesAllocated = 0;
		NumChunksAllocatedForGPU = 0;
	}

	FORCEINLINE uint32 GetSizeBytes()const
	{
		return FloatData.Num() + Int32Data.Num();
	}

	const FRWBuffer *GetGPUBufferFloat() const
	{
		return &GPUBufferFloat;
	}

	const FRWBuffer *GetGPUBufferInt() const
	{
		return &GPUBufferInt;
	}

	int32 GetSafeComponentBufferSize() const
	{
		return GetSafeComponentBufferSize(GetNumInstancesAllocated());
	}

	uint32 GetFloatStride() const { return FloatStride; }
	uint32 GetInt32Stride() const { return Int32Stride; }

private:

	FORCEINLINE int32 GetSafeComponentBufferSize(int32 RequiredSize) const
	{
		//Round up to VECTOR_WIDTH_BYTES.
		//Both aligns the component buffers to the vector width but also ensures their ops cannot stomp over one another.		
		return RequiredSize + VECTOR_WIDTH_BYTES - (RequiredSize % VECTOR_WIDTH_BYTES);
	}

	/** Back ptr to our owning data set. Used to access layout info for the buffer. */
	FNiagaraDataSet* Owner;

	/** Float components of simulation data. */
	TResourceArray<uint8> FloatData;
	/** Int32 components of simulation data. */
	TResourceArray<uint8> Int32Data;

	/** Stride between components in the float buffer. */
	uint32 FloatStride;
	/** Stride between components in the int32 buffer. */
	uint32 Int32Stride;

	uint32 NumChunksAllocatedForGPU;

	/** Number of instances in data. */
	uint32 NumInstances;
	/** Number of instances the buffer has been allocated for. */
	uint32 NumInstancesAllocated;


	FRWBuffer GPUBufferFloat;
	FRWBuffer GPUBufferInt;
};

//////////////////////////////////////////////////////////////////////////

/**
General storage class for all per instance simulation data in Niagara.
*/
class FNiagaraDataSet
{
	friend FNiagaraDataBuffer;
public:
	FNiagaraDataSet()
	{
		Reset();
	}
	
	FNiagaraDataSet(FNiagaraDataSetID InID)
		: ID(InID)
	{
		Reset();
	}

	void Reset()
	{
		Variables.Empty();
		VariableLayoutMap.Empty();
		Data[0].Reset();
		Data[1].Reset();
		CurrBuffer = 0;
		CurrRenderBuffer = 0;
		bFinalized = false;
		TotalFloatComponents = 0;
		TotalInt32Components = 0;
		MaxBufferIdx = 1;
	}

	void AddVariable(FNiagaraVariable& Variable)
	{
		check(!bFinalized);
		Variables.AddUnique(Variable);
	}

	void AddVariables(const TArray<FNiagaraVariable>& Vars)
	{
		check(!bFinalized);
		for (const FNiagaraVariable& Var : Vars)
		{
			Variables.AddUnique(Var);
		}
	}

	/** Finalize the addition of variables and other setup before this data set can be used. */
	FORCEINLINE void Finalize()
	{
		check(!bFinalized);
		bFinalized = true;
		BuildLayout();
	}

	/** Removes a specific instance from the current frame's data buffer. */
	FORCEINLINE void KillInstance(uint32 InstanceIdx)
	{
		check(bFinalized);
		CurrData().KillInstance(InstanceIdx);
	}

	FORCEINLINE void SwapInstances(uint32 OldIndex, uint32 NewIndex)
	{
		check(bFinalized);
		PrevData().SwapInstances(OldIndex, NewIndex);
	}

	/** Appends the passed variable to the set of input and output registers ready for consumption by the VectorVM. */
	bool AppendToRegisterTable(const FNiagaraVariable& VarInfo, uint8** InputRegisters, int32& NumInputRegisters, uint8** OutputRegisters, int32& NumOutputRegisters, int32 StartInstance, bool bNoOutputRegisters = false)
	{
		check(bFinalized);
		if (const FNiagaraVariableLayoutInfo* VariableLayout = VariableLayoutMap.Find(VarInfo))
		{
			uint32 NumComponents = VariableLayout->LayoutInfo.GetNumComponents();

			for (uint32 CompIdx = 0; CompIdx < VariableLayout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = VariableLayout->FloatComponentStart + CompIdx;
				uint32 CompRegisterOffset = VariableLayout->LayoutInfo.FloatComponentRegisterOffsets[CompIdx];
				InputRegisters[NumInputRegisters + CompRegisterOffset] = (uint8*)PrevData().GetInstancePtrFloat(CompBufferOffset, StartInstance);
				OutputRegisters[NumOutputRegisters + CompRegisterOffset] = bNoOutputRegisters ? nullptr : (uint8*)CurrData().GetInstancePtrFloat(CompBufferOffset, StartInstance);
			}
			for (uint32 CompIdx = 0; CompIdx < VariableLayout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = VariableLayout->Int32ComponentStart + CompIdx;
				uint32 CompRegisterOffset = VariableLayout->LayoutInfo.Int32ComponentRegisterOffsets[CompIdx];
				InputRegisters[NumInputRegisters + CompRegisterOffset] = (uint8*)PrevData().GetInstancePtrInt32(CompBufferOffset, StartInstance);
				OutputRegisters[NumOutputRegisters + CompRegisterOffset] = bNoOutputRegisters ? nullptr : (uint8*)CurrData().GetInstancePtrInt32(CompBufferOffset, StartInstance);
			}
			NumInputRegisters += NumComponents;
			NumOutputRegisters += NumComponents;

			return true;
		}
		return false;
	}

	void SetShaderParams(class FNiagaraShader *Shader, FRHICommandList &CommandList);
	void UnsetShaderParams(class FNiagaraShader *Shader, FRHICommandList &CommandList);
	FORCEINLINE void Allocate(uint32 NumInstances, ENiagaraSimTarget Target = ENiagaraSimTarget::CPUSim, bool bMaintainExisting=false)
	{
		check(bFinalized);
		CurrData().Allocate(NumInstances, Target, bMaintainExisting);
	}

	void InitGPUFromCPU()
	{
		ensure(IsInGameThread());
		PrevData().InitGPUFromCPU();
	}

	FORCEINLINE void Tick(ENiagaraSimTarget SimTarget = ENiagaraSimTarget::CPUSim)
	{
		SwapBuffers(SimTarget);
	}
	FORCEINLINE void TickRenderThread(ENiagaraSimTarget SimTarget = ENiagaraSimTarget::CPUSim)
	{
		SwapBuffersRenderThread(SimTarget);
	}



	// called before rendering to make sure we access the correct buffer
	FORCEINLINE void ValidateBufferIndices()
	{
		CurrRenderBuffer = CurrBuffer;
	}

	FORCEINLINE void CopyPrevToCur()
	{
		PrevData().CopyTo(CurrData());
	}

	FORCEINLINE FNiagaraDataSetID GetID()const { return ID; }
	FORCEINLINE void SetID(FNiagaraDataSetID InID) { ID = InID; }

	FORCEINLINE FNiagaraDataBuffer& CurrData() 
	{
		//check(IsInGameThread());
		return Data[CurrBuffer]; 
	}
	FORCEINLINE FNiagaraDataBuffer& PrevData() 
	{ 
		//check(IsInGameThread());
		return Data[CurrBuffer > 0 ? CurrBuffer-1 : MaxBufferIdx];
	}
	FORCEINLINE const FNiagaraDataBuffer& CurrData()const 
	{ 
		//check(IsInGameThread());
		return Data[CurrBuffer];
	}
	FORCEINLINE const FNiagaraDataBuffer& PrevData()const 
	{ 
		//check(IsInGameThread());
		return Data[CurrBuffer > 0 ? CurrBuffer - 1 : MaxBufferIdx];
	}

	FORCEINLINE FNiagaraDataBuffer& CurrDataRender() 
	{ 
		check(!IsInGameThread());
		return Data[CurrRenderBuffer];
	}
	FORCEINLINE FNiagaraDataBuffer& PrevDataRender() 
	{ 
		check(!IsInGameThread());
		return Data[CurrRenderBuffer > 0 ? CurrRenderBuffer - 1 : MaxBufferIdx];
	}
	FORCEINLINE const FNiagaraDataBuffer& CurrDataRender() const 
	{ 
		check(!IsInGameThread());
		return Data[CurrRenderBuffer];
	}
	FORCEINLINE const FNiagaraDataBuffer& PrevDataRender() const 
	{
		check(!IsInGameThread());
		return Data[CurrRenderBuffer > 0 ? CurrRenderBuffer - 1 : MaxBufferIdx];
	}

	FORCEINLINE uint32 GetNumInstances()const { return CurrData().GetNumInstances(); }
	FORCEINLINE uint32 GetNumInstancesAllocated()const { return CurrData().GetNumInstancesAllocated(); }
	FORCEINLINE void SetNumInstances(uint32 InNumInstances) { CurrData().SetNumInstances(InNumInstances); }

	FORCEINLINE void ResetNumInstances()
	{
		CurrData().SetNumInstances(0);
		PrevData().SetNumInstances(0);
	}

	FORCEINLINE void ResetBuffers()
	{
		CurrData().Reset();
		PrevData().Reset();
	}

	FORCEINLINE uint32 GetPrevNumInstances()const { return PrevData().GetNumInstances(); }

	FORCEINLINE uint32 GetNumVariables()const { return Variables.Num(); }

	FORCEINLINE uint32 GetSizeBytes()const
	{
		return Data[0].GetSizeBytes() + Data[1].GetSizeBytes();
	}

	FORCEINLINE bool HasVariable(const FNiagaraVariable& Var)const
	{
		return Variables.Contains(Var);
	}

	FORCEINLINE const FNiagaraVariableLayoutInfo* GetVariableLayout(const FNiagaraVariable& Var)const
	{
		return VariableLayoutMap.Find(Var);
	}

	// get the float and int component offsets of a variable; if the variable doesn't exist, returns -1
	//
	FORCEINLINE const bool GetVariableComponentOffsets(const FNiagaraVariable& Var, int32 &FloatStart, int32 &IntStart) const
	{
		const FNiagaraVariableLayoutInfo *Info = GetVariableLayout(Var);
		if (Info)
		{
			FloatStart = Info->FloatComponentStart;
			IntStart = Info->Int32ComponentStart;
			return true;
		}

		FloatStart = -1;
		IntStart = -1;
		return false;
	}

	void Dump(bool bCurr, int32 StartIdx = 0, int32 NumInstances = INDEX_NONE);
	void Dump(FNiagaraDataSet& Other, bool bCurr);
	FORCEINLINE const TArray<FNiagaraVariable> &GetVariables() { return Variables; }

	// called before dispatch from NiagaraEmitterInstanceBatcher
	FRWBuffer &SetupDataSetIndices() 
	{ 
		check(IsInRenderingThread());
		if (DataSetIndices.Buffer)
		{
			DataSetIndices.Release();
		}
		DataSetIndices.Initialize(sizeof(int32), 64 /*Context->NumDataSets*/, EPixelFormat::PF_R32_SINT);	// always allocate for up to 64 data sets
		return DataSetIndices; 
	}

	FRWBuffer &GetDataSetIndices()
	{
		return DataSetIndices;
	}

private:

	FORCEINLINE void SwapBuffers(ENiagaraSimTarget SimTarget)
	{
		if (SimTarget == ENiagaraSimTarget::CPUSim)
		{
			MaxBufferIdx = 2;
			CurrBuffer = CurrBuffer < 2 ? CurrBuffer+1 : 0;
		}
		else
		{
			MaxBufferIdx = 1;
			CurrBuffer = CurrBuffer == 0 ? 1 : 0;
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(SwapDataSetBuffersGPU,
			uint32, CurrBuffer, CurrBuffer,
			uint32 &, CurrRenderBuffer, CurrRenderBuffer,
			{
				CurrRenderBuffer = CurrBuffer;
			});
	}

	FORCEINLINE void SwapBuffersRenderThread(ENiagaraSimTarget SimTarget)
	{
		check(IsInRenderingThread());
		if (SimTarget == ENiagaraSimTarget::CPUSim)
		{
			MaxBufferIdx = 2;
			CurrBuffer = CurrBuffer < 2 ? CurrBuffer + 1 : 0;
		}
		else
		{
			MaxBufferIdx = 1;
			CurrBuffer = CurrBuffer == 0 ? 1 : 0;
		}

		CurrRenderBuffer = CurrBuffer;
	}


	void BuildLayout()
	{
		VariableLayoutMap.Empty();
		TotalFloatComponents = 0;
		TotalInt32Components = 0;

		for (FNiagaraVariable& Var : Variables)
		{
			FNiagaraVariableLayoutInfo& VarInfo = VariableLayoutMap.Add(Var);
			FNiagaraTypeLayoutInfo::GenerateLayoutInfo(VarInfo.LayoutInfo, Var.GetType().GetScriptStruct());
			VarInfo.FloatComponentStart = TotalFloatComponents;
			VarInfo.Int32ComponentStart = TotalInt32Components;
			TotalFloatComponents += VarInfo.GetNumFloatComponents();
			TotalInt32Components += VarInfo.GetNumInt32Components();
		}
		Data[0].Init(this);
		Data[1].Init(this);
		Data[2].Init(this);
	}

	FORCEINLINE uint32 GetNumFloatComponents() { return TotalFloatComponents; }
	FORCEINLINE uint32 GetNumInt32Components() { return TotalInt32Components; }
		
	/** Unique ID for this data set. Used to allow referencing from other emitters and Systems. */
	FNiagaraDataSetID ID;
	/** Variables in the data set. */
	TArray<FNiagaraVariable> Variables;
	/** Map from the variable to some extra data describing it's layout in the data set. */
	TMap<FNiagaraVariable, FNiagaraVariableLayoutInfo> VariableLayoutMap;
	/** Total number of components of each type in the data set. */
	uint32 TotalFloatComponents;
	uint32 TotalInt32Components;
	/** Index of current state data. */
	uint32 CurrBuffer;
	uint32 CurrRenderBuffer;
	uint32 MaxBufferIdx;

	/** Once finalized, the data layout etc is built and no more variables can be added. */
	bool bFinalized;

	FNiagaraDataBuffer Data[3];
	FRWBuffer DataSetIndices; 
};

/**
General iterator for getting and setting data in and FNiagaraDataSet.
*/
struct FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessorBase()
		: DataSet(nullptr)
		, DataBuffer(nullptr)
		, VarLayout(nullptr)
	{}

	FNiagaraDataSetAccessorBase(FNiagaraDataSet* InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: DataSet(InDataSet)
		, DataBuffer(bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData())
	{
		VarLayout = DataSet->GetVariableLayout(InVar);
	}

	void Create(FNiagaraDataSet* InDataSet, FNiagaraVariable InVar)
	{
		DataSet = InDataSet;
		VarLayout = DataSet->GetVariableLayout(InVar);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
	}

	FORCEINLINE bool IsValid()const { return VarLayout != nullptr && DataBuffer; }
protected:

	FNiagaraDataSet* DataSet;
	FNiagaraDataBuffer* DataBuffer;
	const FNiagaraVariableLayoutInfo* VarLayout;
};

template<typename T>
struct FNiagaraDataSetAccessor : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<T>() {}
	FNiagaraDataSetAccessor(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(T) == InVar.GetType().GetSize());
		checkf(false, TEXT("You must provide a fast runtime specialization for this type."));// Allow this slow generic version?
	}

	FORCEINLINE T operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE T GetSafe(int32 Index, T Default)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE T Get(int32 Index)const
	{
		T Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, T& OutValue)const
	{
		uint8* ValuePtr = (uint8*)&OutValue;

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumFloatComponents(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			float* Src = DataBuffer->GetInstancePtrFloat(CompBufferOffset, Index);
			float* Dst = (float*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumInt32Components(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			int32* Src = DataBuffer->GetInstancePtrInt32(CompBufferOffset, Index);
			int32* Dst = (int32*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}
	}

	FORCEINLINE void Set(int32 Index, const T& InValue)
	{
		uint8* ValuePtr = (uint8*)&InValue;

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumFloatComponents(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			float* Dst = DataBuffer->GetInstancePtrFloat(CompBufferOffset, Index);
			float* Src = (float*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}

		for (uint32 CompIdx = 0; CompIdx < VarLayout->GetNumInt32Components(); ++CompIdx)
		{
			uint32 CompBufferOffset = VarLayout->FloatComponentStart + CompIdx;
			int32* Dst = DataBuffer->GetInstancePtrInt32(CompBufferOffset, Index);
			int32* Src = (int32*)(ValuePtr + VarLayout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
			*Dst = *Src;
		}
	}
};

template<>
struct FNiagaraDataSetAccessor<FNiagaraBool> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FNiagaraBool>() {}
	FNiagaraDataSetAccessor<FNiagaraBool>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FNiagaraBool) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE FNiagaraBool operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FNiagaraBool Get(int32 Index)const
	{
		FNiagaraBool Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE FNiagaraBool GetSafe(int32 Index, bool Default = true)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE void Get(int32 Index, FNiagaraBool& OutValue)const
	{
		OutValue.Value = Base[Index];
	}

	FORCEINLINE void Set(int32 Index, const FNiagaraBool& InValue)
	{
		Base[Index] = InValue.Value;
	}

private:

	int32* Base;
};

template<>
struct FNiagaraDataSetAccessor<int32> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<int32>() {}
	FNiagaraDataSetAccessor<int32>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(int32) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}
	
	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
			if (DataBuffer->GetNumInstances() != 0)
			{
				check(Base != nullptr);
			}
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE int32 operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE int32 Get(int32 Index)const
	{
		int32 Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE int32 GetSafe(int32 Index, int32 Default = 0)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE void Get(int32 Index, int32& OutValue)const
	{
		check(Base != nullptr);
		OutValue = Base[Index];
	}

	FORCEINLINE void Set(int32 Index, const int32& InValue)
	{
		check(Base != nullptr);
		Base[Index] = InValue;
	}

private:

	int32* Base;
};

template<>
struct FNiagaraDataSetAccessor<float> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<float>() {}
	FNiagaraDataSetAccessor<float>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(float) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			Base = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
		}
		else
		{
			Base = nullptr;
		}
	}

	FORCEINLINE float operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE float GetSafe(int32 Index, float Default = 0.0f)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE float Get(int32 Index)const
	{
		float Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, float& OutValue)const
	{
		OutValue = Base[Index];
	}

	FORCEINLINE void Set(int32 Index, const float& InValue)
	{
		Base[Index] = InValue;
	}

private:
	float* Base;
};

template<>
struct FNiagaraDataSetAccessor<FVector2D> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector2D>() {}
	FNiagaraDataSetAccessor<FVector2D>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector2D) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
		}
	}

	FORCEINLINE FVector2D operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector2D GetSafe(int32 Index, FVector2D Default = FVector2D::ZeroVector)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector2D Get(int32 Index)const
	{
		FVector2D Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector2D& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector2D& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
	}

private:

	float* XBase;
	float* YBase;
};

template<>
struct FNiagaraDataSetAccessor<FVector> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector>() {}
	FNiagaraDataSetAccessor<FVector>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			ZBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
			ZBase = nullptr;
		}
	}

	FORCEINLINE FVector operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector GetSafe(int32 Index, FVector Default = FVector::ZeroVector)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector Get(int32 Index)const
	{
		FVector Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
		OutValue.Z = ZBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
		ZBase[Index] = InValue.Z;
	}

private:

	float* XBase;
	float* YBase;
	float* ZBase;
};

template<>
struct FNiagaraDataSetAccessor<FVector4> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FVector4>() {}
	FNiagaraDataSetAccessor<FVector4>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FVector4) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout)
		{
			XBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			YBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			ZBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
			WBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 3);
		}
		else
		{
			XBase = nullptr;
			YBase = nullptr;
			ZBase = nullptr;
			WBase = nullptr;
		}
	}

	FORCEINLINE FVector4 operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FVector4 GetSafe(int32 Index, const FVector4& Default = FVector4(0.0f,0.0f,0.0f,0.0f))const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FVector4 Get(int32 Index)const
	{
		FVector4 Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FVector4& OutValue)const
	{
		OutValue.X = XBase[Index];
		OutValue.Y = YBase[Index];
		OutValue.Z = ZBase[Index];
		OutValue.W = WBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FVector4& InValue)
	{
		XBase[Index] = InValue.X;
		YBase[Index] = InValue.Y;
		ZBase[Index] = InValue.Z;
		WBase[Index] = InValue.W;
	}

private:

	float* XBase;
	float* YBase;
	float* ZBase;
	float* WBase;
};

template<>
struct FNiagaraDataSetAccessor<FLinearColor> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FLinearColor>() {}
	FNiagaraDataSetAccessor<FLinearColor>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer=true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FLinearColor) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout)
		{
			RBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			GBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
			BBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 2);
			ABase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 3);
		}
		else
		{
			RBase = nullptr;
			GBase = nullptr;
			BBase = nullptr;
			ABase = nullptr;
		}
	}

	FORCEINLINE FLinearColor operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FLinearColor GetSafe(int32 Index, FLinearColor Default = FLinearColor::White)const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FLinearColor Get(int32 Index)const
	{
		FLinearColor Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FLinearColor& OutValue)const
	{
		OutValue.R = RBase[Index];
		OutValue.G = GBase[Index];
		OutValue.B = BBase[Index];
		OutValue.A = ABase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FLinearColor& InValue)
	{
		RBase[Index] = InValue.R;
		GBase[Index] = InValue.G;
		BBase[Index] = InValue.B;
		ABase[Index] = InValue.A;
	}

private:

	float* RBase;
	float* GBase;
	float* BBase;
	float* ABase;
};

template<>
struct FNiagaraDataSetAccessor<FNiagaraSpawnInfo> : public FNiagaraDataSetAccessorBase
{
	FNiagaraDataSetAccessor<FNiagaraSpawnInfo>() {}
	FNiagaraDataSetAccessor<FNiagaraSpawnInfo>(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessorBase(&InDataSet, InVar, bCurrBuffer)
	{
		check(sizeof(FNiagaraSpawnInfo) == InVar.GetType().GetSize());
		InitForAccess(bCurrBuffer);
	}

	void InitForAccess(bool bCurrBuffer = true)
	{
		DataBuffer = bCurrBuffer ? &DataSet->CurrData() : &DataSet->PrevData();
		if (VarLayout != nullptr)
		{
			CountBase = (int32*)DataBuffer->GetComponentPtrInt32(VarLayout->Int32ComponentStart);
			InterpStartDtBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart);
			IntervalDtBase = (float*)DataBuffer->GetComponentPtrFloat(VarLayout->FloatComponentStart + 1);
		}
		else
		{
			CountBase = nullptr;
			InterpStartDtBase = nullptr;
			IntervalDtBase = nullptr;
		}
	}

	FORCEINLINE FNiagaraSpawnInfo operator[](int32 Index)const
	{
		return Get(Index);
	}

	FORCEINLINE FNiagaraSpawnInfo GetSafe(int32 Index, FNiagaraSpawnInfo Default = FNiagaraSpawnInfo())const
	{
		if (IsValid())
		{
			return Get(Index);
		}

		return Default;
	}

	FORCEINLINE FNiagaraSpawnInfo Get(int32 Index)const
	{
		FNiagaraSpawnInfo Ret;
		Get(Index, Ret);
		return Ret;
	}

	FORCEINLINE void Get(int32 Index, FNiagaraSpawnInfo& OutValue)const
	{
		OutValue.Count = CountBase[Index];
		OutValue.InterpStartDt = InterpStartDtBase[Index];
		OutValue.IntervalDt = IntervalDtBase[Index];
	}

	FORCEINLINE void Set(int32 Index, const FNiagaraSpawnInfo& InValue)
	{
		CountBase[Index] = InValue.Count;
		InterpStartDtBase[Index] = InValue.InterpStartDt;
		IntervalDtBase[Index] = InValue.IntervalDt;
	}

private:

	int32* CountBase;
	float* InterpStartDtBase;
	float* IntervalDtBase;
};

//Provide iterator to keep iterator access patterns still in use.
template<typename T>
struct FNiagaraDataSetIterator : public FNiagaraDataSetAccessor<T>
{
	FNiagaraDataSetIterator<T>() {}
	FNiagaraDataSetIterator(FNiagaraDataSet& InDataSet, FNiagaraVariable InVar, uint32 StartIndex = 0, bool bCurrBuffer = true)
		: FNiagaraDataSetAccessor<T>(InDataSet, InVar, bCurrBuffer)
		, CurrIdx(StartIndex)
	{}

	FORCEINLINE T operator*() { return Get(); }
	FORCEINLINE T Get()const
	{
		T Ret;
		Get(Ret);
		return Ret;
	}
	FORCEINLINE void Get(T& OutValue)const { FNiagaraDataSetAccessor<T>::Get(CurrIdx, OutValue); }

	FORCEINLINE void Set(const T& InValue)
	{
		FNiagaraDataSetAccessor<T>::Set(CurrIdx, InValue);
	}

	FORCEINLINE void Advance() { ++CurrIdx; }
	FORCEINLINE bool IsValid()const
	{
		return FNiagaraDataSetAccessorBase::VarLayout != nullptr && CurrIdx < FNiagaraDataSetAccessorBase::DataBuffer->GetNumInstances();
	}

	uint32 GetCurrIndex()const { return CurrIdx; }

private:
	uint32 CurrIdx;
};

/**
Iterator that will pull or push data between a DataSet and some FNiagaraVariables it contains.
Super slow. Don't use at runtime.
*/
struct FNiagaraDataSetVariableIterator
{
	FNiagaraDataSetVariableIterator(FNiagaraDataSet& InDataSet, uint32 StartIdx = 0, bool bCurrBuffer = true)
		: DataSet(InDataSet)
		, DataBuffer(bCurrBuffer ? DataSet.CurrData() : DataSet.PrevData())
		, CurrIdx(StartIdx)
	{
	}

	void Get()
	{
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable* Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo* Layout = VarLayouts[VarIdx];
			check(Var && Layout);
			uint8* ValuePtr = Var->GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				float* Src = DataBuffer.GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Dst = (float*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->Int32ComponentStart + CompIdx;
				int32* Src = DataBuffer.GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Dst = (int32*)(ValuePtr + Layout->LayoutInfo.Int32ComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}

	void Set()
	{
		for (int32 VarIdx = 0; VarIdx < Variables.Num(); ++VarIdx)
		{
			FNiagaraVariable* Var = Variables[VarIdx];
			const FNiagaraVariableLayoutInfo* Layout = VarLayouts[VarIdx];

			check(Var && Layout);
			uint8* ValuePtr = Var->GetData();

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				float* Dst = DataBuffer.GetInstancePtrFloat(CompBufferOffset, CurrIdx);
				float* Src = (float*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}

			for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
			{
				uint32 CompBufferOffset = Layout->FloatComponentStart + CompIdx;
				int32* Dst = DataBuffer.GetInstancePtrInt32(CompBufferOffset, CurrIdx);
				int32* Src = (int32*)(ValuePtr + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx]);
				*Dst = *Src;
			}
		}
	}

	void Advance() { ++CurrIdx; }

	bool IsValid()const
	{
		return CurrIdx < DataBuffer.GetNumInstances();
	}

	uint32 GetCurrIndex()const { return CurrIdx; }

	void AddVariable(FNiagaraVariable* InVar)
	{
		check(InVar);
		Variables.AddUnique(InVar);
		VarLayouts.AddUnique(DataSet.GetVariableLayout(*InVar));
		InVar->AllocateData();
	}

	void AddVariables(TArray<FNiagaraVariable>& Vars)
	{
		for (FNiagaraVariable& Var : Vars)
		{
			AddVariable(&Var);
		}
	}
private:

	FNiagaraDataSet& DataSet;
	FNiagaraDataBuffer& DataBuffer;
	TArray<FNiagaraVariable*> Variables;
	TArray<const FNiagaraVariableLayoutInfo*> VarLayouts;

	uint32 CurrIdx;
};