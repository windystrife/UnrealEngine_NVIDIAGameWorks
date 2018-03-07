// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptExecutionContext.h"
#include "NiagaraStats.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"

DECLARE_CYCLE_STAT(TEXT("Register Setup"), STAT_NiagaraSimRegisterSetup, STATGROUP_Niagara);


//////////////////////////////////////////////////////////////////////////
FNiagaraScriptExecutionParameterStore::FNiagaraScriptExecutionParameterStore()
	: FNiagaraParameterStore()
{

}

FNiagaraScriptExecutionParameterStore::FNiagaraScriptExecutionParameterStore(const FNiagaraParameterStore& Other)
{
	*this = Other;
}

FNiagaraScriptExecutionParameterStore& FNiagaraScriptExecutionParameterStore::operator=(const FNiagaraParameterStore& Other)
{
	FNiagaraParameterStore::operator=(Other);
	return *this;
}

void FNiagaraScriptExecutionParameterStore::Init(UNiagaraScript* Script)
{
	//TEMPORARTY:
	//We should replace the storage on the script with an FNiagaraParameterStore also so we can just copy that over here. Though that is an even bigger refactor job so this is a convenient place to break that work up.

	bParametersDirty = true;
	bInterfacesDirty = true;
	Empty();

	//Here we add the current frame parameters.
	for (FNiagaraVariable& Param : Script->Parameters.Parameters)
	{
		AddParameter(Param, false);
	}
	
	//Add previous frame values if we're interpolated spawn.
	bool bIsInterpolatedSpawn = Script->IsInterpolatedParticleSpawnScript();
	if (bIsInterpolatedSpawn)
	{
		for (FNiagaraVariable& Param : Script->Parameters.Parameters)
		{
			FNiagaraVariable PrevParam(Param.GetType(), FName(*(TEXT("PREV__") + Param.GetName().ToString())));
			AddParameter(PrevParam, false);
		}
	}
	
	ParameterSize = ParameterData.Num();
	if (bIsInterpolatedSpawn)
	{
		CopyCurrToPrev();
	}

	//Internal constants
	for (FNiagaraVariable& InternalVar : Script->InternalParameters.Parameters)
	{
		AddParameter(InternalVar, false);
	}

	for (FNiagaraScriptDataInterfaceInfo& Info : Script->DataInterfaceInfo)
	{
		FNiagaraVariable Var(FNiagaraTypeDefinition(Info.DataInterface->GetClass()), Info.Name);
		AddParameter(Var, false);
		SetDataInterface(Info.DataInterface, IndexOf(Var));
	}
}

void FNiagaraScriptExecutionParameterStore::CopyCurrToPrev()
{
	int32 ParamStart = ParameterSize / 2;
	checkSlow(FMath::Frac((float)ParameterSize / 2) == 0.0f);

	FMemory::Memcpy(GetParameterData_Internal(ParamStart), GetParameterData(0), ParamStart);
}

FNiagaraScriptExecutionContext::FNiagaraScriptExecutionContext()
	: Script(nullptr)
{

}

uint32 FNiagaraScriptExecutionContext::TickCounter = 0;

bool FNiagaraScriptExecutionContext::Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget)
{
	Script = InScript;

	Parameters.Init(Script);

	return true;//TODO: Error cases?
}

bool FNiagaraScriptExecutionContext::Tick(FNiagaraSystemInstance* ParentSystemInstance)
{
	if (Script)//TODO: Remove. Script can only be null for system instances that currently don't have their script exec context set up correctly.
	{
		//Bind data interfaces if needed.
		if (Parameters.bInterfacesDirty)
		{
			FunctionTable.Empty();
			const TArray<UNiagaraDataInterface*> DataInterfaces = GetDataInterfaces();
			// We must make sure that the data interfaces match up between the original script values and our overrides...
			if (Script->DataInterfaceInfo.Num() != DataInterfaces.Num())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Mismatch between Niagara Exectuion Context data interfaces and those in it's script!"));
				return false;
			}

			//Fill the instance data table.
			if (ParentSystemInstance)
			{
				DataInterfaceInstDataTable.SetNumZeroed(Script->NumUserPtrs);
				for (int32 i = 0; i < Script->DataInterfaceInfo.Num(); i++)
				{
					int32 UserPtrIdx = Script->DataInterfaceInfo[i].UserPtrIdx;
					if (UserPtrIdx != INDEX_NONE)
					{
						UNiagaraDataInterface* Interface = DataInterfaces[i];
						void* InstData = ParentSystemInstance->FindDataInterfaceInstanceData(Interface);
						DataInterfaceInstDataTable[UserPtrIdx] = InstData;
					}
				}
			}
			else
			{
				check(Script->NumUserPtrs == 0);//Can't have user ptrs if we have no parent instance.
			}

			bool bSuccessfullyMapped = true;
			for (FVMExternalFunctionBindingInfo& BindingInfo : Script->CalledVMExternalFunctions)
			{
				for (int32 i = 0; i < Script->DataInterfaceInfo.Num(); i++)
				{
					FNiagaraScriptDataInterfaceInfo& ScriptInfo = Script->DataInterfaceInfo[i];
					UNiagaraDataInterface* ExternalInterface = DataInterfaces[i];

					if (ScriptInfo.Name == BindingInfo.OwnerName)
					{
						void* InstData = ScriptInfo.UserPtrIdx == INDEX_NONE ? nullptr : DataInterfaceInstDataTable[ScriptInfo.UserPtrIdx];
						FVMExternalFunction Func = ExternalInterface->GetVMExternalFunction(BindingInfo, InstData);
						if (Func.IsBound())
						{
							FunctionTable.Add(Func);
						}
						else
						{
							UE_LOG(LogNiagara, Error, TEXT("Could not Get VMExternalFunction '%s'.. emitter will not run!"), *BindingInfo.Name.ToString());
							bSuccessfullyMapped = false;
						}
					}
				}
			}

			if (!bSuccessfullyMapped)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Error building data interface function table!"));
				FunctionTable.Empty();
				return false;
			}
		}
	}

	Parameters.Tick();

	return true;
}

void FNiagaraScriptExecutionContext::PostTick()
{
	//If we're for interpolated spawn, copy over the previous frame's parameters into the Prev parameters.
	if (Script && Script->IsInterpolatedParticleSpawnScript())
	{
		Parameters.CopyCurrToPrev();
	}
}

bool FNiagaraScriptExecutionContext::Execute(uint32 NumInstances, TArray<FNiagaraDataSetExecutionInfo>& DataSetInfos)
{
	if (NumInstances == 0)
	{
		return true;
	}

	++TickCounter;//Should this be per execution?

	int32 NumInputRegisters = 0;
	int32 NumOutputRegisters = 0;
	uint8* InputRegisters[VectorVM::MaxInputRegisters];
	uint8* OutputRegisters[VectorVM::MaxOutputRegisters];

	DataSetMetaTable.Reset();

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimRegisterSetup);
		for (FNiagaraDataSetExecutionInfo& DataSetInfo : DataSetInfos)
		{
			check(DataSetInfo.DataSet);
			FDataSetMeta SetMeta(DataSetInfo.DataSet->GetSizeBytes(), &InputRegisters[NumInputRegisters], NumInputRegisters);
			DataSetMetaTable.Add(SetMeta);
			if (DataSetInfo.bAllocate)
			{
				DataSetInfo.DataSet->Allocate(NumInstances);
				DataSetInfo.DataSet->SetNumInstances(NumInstances);
			}
			for (const FNiagaraVariable &Var : DataSetInfo.DataSet->GetVariables())
			{
				DataSetInfo.DataSet->AppendToRegisterTable(Var, InputRegisters, NumInputRegisters, OutputRegisters, NumOutputRegisters, DataSetInfo.StartInstance);
			}
		}
	}

	VectorVM::Exec(
		Script->ByteCode.GetData(),
		InputRegisters,
		NumInputRegisters,
		OutputRegisters,
		NumOutputRegisters,
		Parameters.GetParameterDataArray().GetData(),
		DataSetMetaTable,
		FunctionTable.GetData(),
		DataInterfaceInstDataTable.GetData(),
		NumInstances
#if STATS
#if WITH_EDITORONLY_DATA
		, Script->GetStatScopeIDs()
#else
		, TArray<TStatId>()
#endif
#endif
	);

	// Tell the datasets we wrote how many instances were actually written.
	for (int Idx = 0; Idx < DataSetInfos.Num(); Idx++)
	{
		FNiagaraDataSetExecutionInfo& Info = DataSetInfos[Idx];
		if (Info.bUpdateInstanceCount)
		{
			Info.DataSet->SetNumInstances(Info.StartInstance + DataSetMetaTable[Idx].DataSetAccessIndex);
		}
	}

#if WITH_EDITORONLY_DATA
	if (Script->GetDebuggerInfo().bRequestDebugFrame)
	{
		DataSetInfos[0].DataSet->Dump(Script->GetDebuggerInfo().DebugFrame, true);
		Script->GetDebuggerInfo().bRequestDebugFrame = false;
		Script->GetDebuggerInfo().DebugFrameLastWriteId = TickCounter;
	}
#endif
	return true;//TODO: Error cases?
}

void FNiagaraScriptExecutionContext::DirtyDataInterfaces()
{
	Parameters.bInterfacesDirty = true;
}

bool FNiagaraScriptExecutionContext::CanExecute()const
{
	return Script && Script->ByteCode.Num() > 0;
}