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
#include "NiagaraParameterStore.h"

/** 
Storage class containing actual runtime buffers to be used by the VM and the GPU. 
Is not the actual source for any parameter data, rather just the final place it's gathered from various other places ready for execution.
*/
class FNiagaraScriptExecutionParameterStore : public FNiagaraParameterStore
{
public:
	FNiagaraScriptExecutionParameterStore();
	FNiagaraScriptExecutionParameterStore(const FNiagaraParameterStore& Other);
	FNiagaraScriptExecutionParameterStore& operator=(const FNiagaraParameterStore& Other);

	//TODO: This function can probably go away entirely when we replace the FNiagaraParameters and DataInterface info in the script with an FNiagaraParameterStore.
	//Special care with prev params and internal params will have to be taken there.
	void Init(UNiagaraScript* Script);

	void CopyCurrToPrev();

	bool AddParameter(const FNiagaraVariable& Param, bool bInitInterfaces=true)
	{
		return FNiagaraParameterStore::AddParameter(Param, bInitInterfaces);
	}

	bool RemoveParameter(FNiagaraVariable& Param)
	{
		check(0);//Not allowed to remove parameters from an execution store as it will adjust the table layout mess up the 
		return false;
	}

	void RenameParameter(FNiagaraVariable& Param, FName NewName)
	{
		check(0);//Can't rename parameters for an execution store.
	}

	uint32 GetExternalParameterSize() { return ParameterSize; }
private:

	/** Size of the parameter data not including prev frame values or internal constants. Allows copying into previous parameter values for interpolated spawn scripts. */
	int32 ParameterSize;
};

struct FNiagaraDataSetExecutionInfo
{
	FNiagaraDataSetExecutionInfo()
		: DataSet(nullptr)
		, StartInstance(0)
		, bAllocate(false)
		, bUpdateInstanceCount(false)
	{
	}

	FNiagaraDataSetExecutionInfo(FNiagaraDataSet* InDataSet, int32 InStartInstance, bool bInAllocate, bool bInUpdateInstanceCount)
		: DataSet(InDataSet)
		, StartInstance(InStartInstance)
		, bAllocate(bInAllocate)
		, bUpdateInstanceCount(bInUpdateInstanceCount)
	{}

	FNiagaraDataSet* DataSet;
	int32 StartInstance;
	bool bAllocate;
	bool bUpdateInstanceCount;
};

struct FNiagaraScriptExecutionContext
{
	UNiagaraScript* Script;

	/** Table of external function delegates called from the VM. */
	TArray<FVMExternalFunction> FunctionTable;

	/** Table of instance data for data interfaces that require it. */
	TArray<void*> DataInterfaceInstDataTable;

	/** Parameter store. Contains all data interfaces and a parameter buffer that can be used directly by the VM or GPU. */
	FNiagaraScriptExecutionParameterStore Parameters;

	TArray<FDataSetMeta> DataSetMetaTable;

	
	static uint32 TickCounter;

	FNiagaraScriptExecutionContext();

	bool Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget);
	bool Tick(class FNiagaraSystemInstance* Instance);
	void PostTick();

	bool Execute(uint32 NumInstances, TArray<FNiagaraDataSetExecutionInfo>& DataSetInfos);

	const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const { return Parameters.GetDataInterfaces(); }

	void DirtyDataInterfaces();

	bool CanExecute()const;
};
