// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "NiagaraParameterCollection.h"
#include "GCObject.h"
#include "NiagaraDataSet.h"
#include "NiagaraScriptExecutionContext.h"

class UWorld;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;

//TODO: Pull all the layout information here, in the data set and in parameter stores out into a single layout structure that's shared between all instances of it.
//Currently there's tons of extra data and work done setting these up.
struct FNiagaraParameterStoreToDataSetBinding
{
	//Array of floats offsets
	struct FDataOffsets
	{
		/** Offset of this value in the parameter store. */
		int32 ParameterOffset;
		/** Offset of this value in the data set's components. */
		int32 DataSetComponentOffset;
		FDataOffsets(int32 InParamOffset, int32 InDataSetComponentOffset) : ParameterOffset(InParamOffset), DataSetComponentOffset(InDataSetComponentOffset) {}
	};
	TArray<FDataOffsets> FloatOffsets;
	TArray<FDataOffsets> Int32Offsets;

	void Empty()
	{
		FloatOffsets.Empty();
		Int32Offsets.Empty();
	}

	void Init(FNiagaraDataSet& DataSet, const FNiagaraParameterStore& ParameterStore)
	{
		//For now, until I get time to refactor all the layout info into something more coherent we'll init like this and just have to assume the correct layout sets and stores are used later.
		//Can check it but it'd be v slow.

		for (const FNiagaraVariable& Var : DataSet.GetVariables())
		{
			const FNiagaraVariableLayoutInfo* Layout = DataSet.GetVariableLayout(Var);
			const int32* ParameterOffsetPtr = ParameterStore.ParameterOffsets.Find(Var);
			if (ParameterOffsetPtr && Layout)
			{
				int32 ParameterOffset = *ParameterOffsetPtr;
				for (uint32 CompIdx = 0; CompIdx < Layout->GetNumFloatComponents(); ++CompIdx)
				{
					int32 ParamOffset = ParameterOffset + Layout->LayoutInfo.FloatComponentByteOffsets[CompIdx];
					int32 DataSetOffset = Layout->FloatComponentStart + Layout->LayoutInfo.FloatComponentRegisterOffsets[CompIdx];
					FloatOffsets.Emplace(ParamOffset, DataSetOffset);
				}
				for (uint32 CompIdx = 0; CompIdx < Layout->GetNumInt32Components(); ++CompIdx)
				{
					int32 ParamOffset = ParameterOffset + Layout->LayoutInfo.Int32ComponentByteOffsets[CompIdx];
					int32 DataSetOffset = Layout->Int32ComponentStart + Layout->LayoutInfo.Int32ComponentRegisterOffsets[CompIdx];
					Int32Offsets.Emplace(ParamOffset, DataSetOffset);
				}
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void DataSetToParameterStore(FNiagaraParameterStore& ParameterStore, FNiagaraDataSet& DataSet, int32 DataSetInstanceIndex)
	{
		FNiagaraDataBuffer& CurrBuffer = DataSet.CurrData();
		uint8* ParameterData = ParameterStore.GetParameterDataArray().GetData();

		for (const FDataOffsets& DataOffsets : FloatOffsets)
		{
			float* ParamPtr = (float*)(ParameterData + DataOffsets.ParameterOffset);
			float* DataSetPtr = CurrBuffer.GetInstancePtrFloat(DataOffsets.DataSetComponentOffset, DataSetInstanceIndex);
			*ParamPtr = *DataSetPtr;
		}
		for (const FDataOffsets& DataOffsets : Int32Offsets)
		{
			int32* ParamPtr = (int32*)(ParameterData + DataOffsets.ParameterOffset);
			int32* DataSetPtr = CurrBuffer.GetInstancePtrInt32(DataOffsets.DataSetComponentOffset, DataSetInstanceIndex);
			*ParamPtr = *DataSetPtr;
		}
	}

	FORCEINLINE_DEBUGGABLE void ParameterStoreToDataSet(FNiagaraParameterStore& ParameterStore, FNiagaraDataSet& DataSet, int32 DataSetInstanceIndex)
	{
		FNiagaraDataBuffer& CurrBuffer = DataSet.CurrData();
		uint8* ParameterData = ParameterStore.GetParameterDataArray().GetData();

		for (const FDataOffsets& DataOffsets : FloatOffsets)
		{
			float* ParamPtr = (float*)(ParameterData + DataOffsets.ParameterOffset);
			float* DataSetPtr = CurrBuffer.GetInstancePtrFloat(DataOffsets.DataSetComponentOffset, DataSetInstanceIndex);
			*DataSetPtr = *ParamPtr;
		}
		for (const FDataOffsets& DataOffsets : Int32Offsets)
		{
			int32* ParamPtr = (int32*)(ParameterData + DataOffsets.ParameterOffset);
			int32* DataSetPtr = CurrBuffer.GetInstancePtrInt32(DataOffsets.DataSetComponentOffset, DataSetInstanceIndex);
			*DataSetPtr = *ParamPtr;
		}
	}
};

/** Simulation performing all system and emitter scripts for a instances of a UNiagaraSystem in a world. */
class FNiagaraSystemSimulation
{
public:
	bool Init(UNiagaraSystem* InSystem, UWorld* InWorld);
	void Destroy();
	void Tick(float DeltaSeconds);

	void RemoveInstance(FNiagaraSystemInstance* Instance);
	void AddInstance(FNiagaraSystemInstance* Instance);

	void ResetSolo(FNiagaraSystemInstance* Instance);
	void TickSolo(FNiagaraSystemInstance* Instance);
	void RemoveSolo(FNiagaraSystemInstance* Instance);

	void TickSoloDataSet();

	FORCEINLINE UNiagaraSystem* GetSystem()const { return System; }
protected:

	/** System of instances being simulated. No need for GC knowledge as all simulations will be cleaned up by the world manager if the system is invalid. */
	UNiagaraSystem* System;

	/** World this system simulation belongs to. */
	UWorld* World;

	/** Main dataset containing system instance attribute data. */
	FNiagaraDataSet DataSet;

	/**
	Dataset for system instances doing solo simulation i.e. not batched like most systems.
	This can be required if strict ordering is needed or the system/emitter scripts use a data interface overridden by the component.
	*/
	FNiagaraDataSet DataSetSolo;

	/**
	As there's a 1 to 1 relationship between system instance and their execution in this simulation we must pull all that instances parameters into a dataset for simulation.
	In some cases this might be a big waste of memory as there'll be duplicated data from a parameter store that's shared across all instances.
	Though all these parameters can be unique per instance so for now lets just do the simple thing and improve later.
	*/
	FNiagaraDataSet SpawnParameterDataSet;
	FNiagaraDataSet UpdateParameterDataSet;

	FNiagaraScriptExecutionContext SpawnExecContext;
	FNiagaraScriptExecutionContext UpdateExecContext;

	FNiagaraScriptExecutionContext SpawnExecContextSolo;
	FNiagaraScriptExecutionContext UpdateExecContextSolo;

	/** Bindings that pull per component parameters into the spawn parameter dataset. */
	FNiagaraParameterStoreToDataSetBinding SpawnParameterToDataSetBinding;
	/** Bindings that pull per component parameters into the update parameter dataset. */
	FNiagaraParameterStoreToDataSetBinding UpdateParameterToDataSetBinding;
	
	/** Binding to push system attributes into each emitter spawn parameters. */
	TArray<FNiagaraParameterStoreToDataSetBinding> DataSetToEmitterSpawnParameters;
	/** Binding to push system attributes into each emitter update parameters. */
	TArray<FNiagaraParameterStoreToDataSetBinding> DataSetToEmitterUpdateParameters;
	/** Binding to push system attributes into each emitter event parameters. */
	TArray<TArray<FNiagaraParameterStoreToDataSetBinding>> DataSetToEmitterEventParameters;

	/** System instances that have been spawned and are now simulating. */
	TArray<FNiagaraSystemInstance*> SystemInstances;
	/** System instances that are pending to be spawned. */
	TArray<FNiagaraSystemInstance*> PendingSystemInstances;

	TArray<FNiagaraSystemInstance*> SoloSystemInstances;

	void InitBindings(FNiagaraSystemInstance* SystemInst);

	FNiagaraDataSetAccessor<FNiagaraBool> SystemEnabledAccessor;
	FNiagaraDataSetAccessor<int32> SystemExecutionStateAccessor;
	TArray<FNiagaraDataSetAccessor<FNiagaraBool>> EmitterEnabledAccessors;
	TArray<TArray<FNiagaraDataSetAccessor<FNiagaraSpawnInfo>>> EmitterSpawnInfoAccessors;
	TArray<FNiagaraDataSetAccessor<int32>> EmitterExecutionStateAccessors;

	//Annoying duplicates required because these access the solo data set. When I rework the where the layout data for parameters and data sets live then these can go away.
	FNiagaraDataSetAccessor<FNiagaraBool> SoloSystemEnabledAccessor;
	FNiagaraDataSetAccessor<int32> SoloSystemExecutionStateAccessor;
	TArray<FNiagaraDataSetAccessor<FNiagaraBool>> SoloEmitterEnabledAccessors;
	TArray<TArray<FNiagaraDataSetAccessor<FNiagaraSpawnInfo>>> SoloEmitterSpawnInfoAccessors;
	TArray<FNiagaraDataSetAccessor<int32>> SoloEmitterExecutionStateAccessors;

	uint32 bCanExecute : 1;
	uint32 bCanExecuteSolo : 1;
};
