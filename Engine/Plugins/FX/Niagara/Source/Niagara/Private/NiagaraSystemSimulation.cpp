// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemSimulation.h"
#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraTypes.h"
#include "NiagaraEvents.h"
#include "NiagaraSettings.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraConstants.h"
#include "NiagaraStats.h"
#include "ParallelFor.h"
#include "NiagaraComponent.h"
#include "NiagaraWorldManager.h"

DECLARE_CYCLE_STAT(TEXT("System Simulation (Batched)"), STAT_NiagaraSystemSim, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Pre Simulate (Batched)"), STAT_NiagaraSystemSim_PreSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Update (Batched)"), STAT_NiagaraSystemSim_Update, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Spawn (Batched)"), STAT_NiagaraSystemSim_Spawn, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Transfer Parameters (Batched)"), STAT_NiagaraSystemSim_TransferParameters, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Post Simulate (Batched)"), STAT_NiagaraSystemSim_PostSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Mark Component Dirty"), STAT_NiagaraSystemSim_MarkComponentDirty, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("System Simulation (Solo)"), STAT_NiagaraSystemSimSolo, STATGROUP_Niagara);


static int32 GbDumpSystemData = 0;
static FAutoConsoleVariableRef CVarNiagaraDumpSystemData(
	TEXT("fx.DumpSystemData"),
	GbDumpSystemData,
	TEXT("If > 0, results of system simulations will be dumped to the log. \n"),
	ECVF_Default
);

static int32 GbParallelSystemPreTick = 1;
static FAutoConsoleVariableRef CVarNiagaraParallelSystemPreTick(
	TEXT("fx.ParallelSystemPreTick"),
	GbParallelSystemPreTick,
	TEXT("If > 0, system pre tick is parallelized. \n"),
	ECVF_Default
);

static int32 GbParallelSystemPostTick = 1;
static FAutoConsoleVariableRef CVarNiagaraParallelSystemPostTick(
	TEXT("fx.ParallelSystemPostTick"),
	GbParallelSystemPostTick,
	TEXT("If > 0, system post tick is parallelized. \n"),
	ECVF_Default
);

//TODO: Experiment with parallel param transfer.
//static int32 GbParallelSystemParamTransfer = 1;
//static FAutoConsoleVariableRef CVarNiagaraParallelSystemParamTransfer(
//	TEXT("fx.ParallelSystemParamTransfer"),
//	GbParallelSystemParamTransfer,
//	TEXT("If > 0, system param transfer is parallelized. \n"),
//	ECVF_Default
//);

//////////////////////////////////////////////////////////////////////////

bool FNiagaraSystemSimulation::Init(UNiagaraSystem* InSystem, UWorld* InWorld)
{
	System = InSystem;
	World = InWorld;

	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(InWorld);
	check(WorldMan);

	bCanExecute = System->GetSystemSpawnScript()->IsValid() && System->GetSystemUpdateScript()->IsValid();
	bCanExecuteSolo = System->GetSystemSpawnScript(true)->IsValid() && System->GetSystemUpdateScript(true)->IsValid();
	const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();

	if (bCanExecute)
	{
		DataSet.Reset();
		DataSet.AddVariables(System->GetSystemSpawnScript()->Attributes);
		DataSet.AddVariables(System->GetSystemUpdateScript()->Attributes);
		DataSet.Finalize();

		SpawnParameterDataSet.Reset();
		FNiagaraParameters* EngineParamsSpawn = System->GetSystemSpawnScript()->DataSetToParameters.Find(TEXT("Engine"));
		if (EngineParamsSpawn != nullptr)
		{
			SpawnParameterDataSet.AddVariables(EngineParamsSpawn->Parameters);
		}
		SpawnParameterDataSet.Finalize();
		UpdateParameterDataSet.Reset();
		FNiagaraParameters* EngineParamsUpdate = System->GetSystemUpdateScript()->DataSetToParameters.Find(TEXT("Engine"));
		if (EngineParamsUpdate != nullptr)
		{
			UpdateParameterDataSet.AddVariables(EngineParamsUpdate->Parameters);
		}
		UpdateParameterDataSet.Finalize();

		SpawnExecContext.Init(System->GetSystemSpawnScript(false), ENiagaraSimTarget::CPUSim);
		UpdateExecContext.Init(System->GetSystemUpdateScript(false), ENiagaraSimTarget::CPUSim);

		//Bind parameter collections.
		for (UNiagaraParameterCollection* Collection : System->GetSystemSpawnScript(false)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&SpawnExecContext.Parameters);
		}
		for (UNiagaraParameterCollection* Collection : System->GetSystemUpdateScript(false)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&UpdateExecContext.Parameters);
		}

		SystemEnabledAccessor.Create(&DataSet, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("System.Enabled")));
		SystemExecutionStateAccessor.Create(&DataSet, FNiagaraVariable(EnumPtr, TEXT("System.ExecutionState")));
		EmitterEnabledAccessors.Reset();
		EmitterSpawnInfoAccessors.Reset();
		EmitterExecutionStateAccessors.Reset();
		EmitterSpawnInfoAccessors.SetNum(System->GetNumEmitters());

		for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
		{
			FNiagaraEmitterHandle& EmitterHandle = System->GetEmitterHandle(EmitterIdx);
			UNiagaraEmitter* Emitter = EmitterHandle.GetInstance();
			FString EmitterName = Emitter->GetUniqueEmitterName();
			check(Emitter);
			EmitterEnabledAccessors.Emplace(DataSet, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), *(EmitterName + TEXT(".Enabled"))));
			EmitterExecutionStateAccessors.Emplace(DataSet, FNiagaraVariable(EnumPtr, *(EmitterName + TEXT(".ExecutionState"))));
			const TArray<FNiagaraEmitterSpawnAttributes>& EmitterSpawnAttrNames = System->GetEmitterSpawnAttributes();
			for (const FNiagaraEmitterSpawnAttributes& SpawnAttrs : EmitterSpawnAttrNames)
			{
				for (FName AttrName : SpawnAttrs.SpawnAttributes)
				{
					EmitterSpawnInfoAccessors[EmitterIdx].Emplace(DataSet, FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), AttrName));
				}
			}
		}
	}

	if (bCanExecuteSolo)
	{
		//Init solo simulation.
		DataSetSolo.Reset();
		DataSetSolo.AddVariables(System->GetSystemSpawnScript(true)->Attributes);
		DataSetSolo.AddVariables(System->GetSystemUpdateScript(true)->Attributes);
		DataSetSolo.Finalize();

		SpawnExecContextSolo.Init(System->GetSystemSpawnScript(true), ENiagaraSimTarget::CPUSim);
		UpdateExecContextSolo.Init(System->GetSystemUpdateScript(true), ENiagaraSimTarget::CPUSim);

		SoloSystemEnabledAccessor.Create(&DataSetSolo, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("System.Enabled")));
		SoloSystemExecutionStateAccessor.Create(&DataSetSolo, FNiagaraVariable(EnumPtr, TEXT("System.ExecutionState")));
		SoloEmitterEnabledAccessors.Reset();
		SoloEmitterExecutionStateAccessors.Reset();
		SoloEmitterSpawnInfoAccessors.Reset();
		SoloEmitterSpawnInfoAccessors.SetNum(System->GetNumEmitters());

		for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
		{
			FNiagaraEmitterHandle& EmitterHandle = System->GetEmitterHandle(EmitterIdx);
			UNiagaraEmitter* Emitter = EmitterHandle.GetInstance();
			FString EmitterName = Emitter->GetUniqueEmitterName();
			check(Emitter);
			SoloEmitterEnabledAccessors.Emplace(DataSetSolo, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), *(EmitterName + TEXT(".Enabled"))));
			SoloEmitterExecutionStateAccessors.Emplace(DataSetSolo, FNiagaraVariable(EnumPtr, *(EmitterName + TEXT(".ExecutionState"))));
			
			for (FName AttrName : System->GetEmitterSpawnAttributes()[EmitterIdx].SpawnAttributes)
			{
				SoloEmitterSpawnInfoAccessors[EmitterIdx].Emplace(DataSetSolo, FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraSpawnInfo::StaticStruct()), AttrName));
			}
		}
	}

	return true;
}

void FNiagaraSystemSimulation::Destroy()
{
	while (SystemInstances.Num())
	{
		SystemInstances.Last()->Deactivate(true);
	}
	while (PendingSystemInstances.Num())
	{
		PendingSystemInstances.Last()->Deactivate(true);
	}
	while (SoloSystemInstances.Num())
	{
		SoloSystemInstances.Last()->Deactivate(true);
	}

	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World);
	check(WorldMan);
	//Unbind parameter collections from the batched exec contexts.
	for (UNiagaraParameterCollection* Collection : System->GetSystemSpawnScript(false)->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&SpawnExecContext.Parameters);
	}
	for (UNiagaraParameterCollection* Collection : System->GetSystemUpdateScript(false)->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&UpdateExecContext.Parameters);
	}
}

void FNiagaraSystemSimulation::TickSoloDataSet()
{
	//Flip buffers and shrink the solo data set allocation.
	if (bCanExecuteSolo)
	{
		DataSetSolo.Tick();
		DataSetSolo.Allocate(SoloSystemInstances.Num());
		DataSetSolo.SetNumInstances(SoloSystemInstances.Num());
	}
}

void FNiagaraSystemSimulation::Tick(float DeltaSeconds)
{
	if (!System->IsValid())
	{
		// TODO: evaluate whether or not we should have removed this from the world manager instead?
		return;
	}

	TickSoloDataSet();

	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim);

	int32 OrigNum = SystemInstances.Num();
	int32 SpawnNum = PendingSystemInstances.Num();
	int32 NewNum = OrigNum + SpawnNum;
	
	SystemInstances.Reserve(NewNum);
	for (FNiagaraSystemInstance* Inst : PendingSystemInstances)
	{
		Inst->SetPendingSpawn(false);
		Inst->SystemInstanceIndex = SystemInstances.Add(Inst);
	}
	PendingSystemInstances.Reset();

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_PreSimulate);

		auto PreTick = [&](int32 SystemIndex)
		{
			FNiagaraSystemInstance* Inst = SystemInstances[SystemIndex];
			Inst->PreSimulateTick(DeltaSeconds);

			check(!Inst->GetParameters().bLayoutDirty);
			if (Inst->GetParameters().bParametersDirty && bCanExecute)
			{
				SpawnParameterToDataSetBinding.ParameterStoreToDataSet(Inst->GetParameters(), SpawnParameterDataSet, SystemIndex);
				UpdateParameterToDataSetBinding.ParameterStoreToDataSet(Inst->GetParameters(), UpdateParameterDataSet, SystemIndex);
			}
			//TODO: Find good way to check that we're not using any instance parameter data interfaces in the system scripts here.
			//In that case we need to solo and will never get here.
		};

		SpawnParameterDataSet.Allocate(NewNum);
		UpdateParameterDataSet.Allocate(NewNum);

		if (GbParallelSystemPreTick)
		{
			ParallelFor(SystemInstances.Num(), PreTick);
		}
		else
		{
			//Tick instances and pull out any per instance parameters we need.
			for (int32 i = 0; i < SystemInstances.Num(); ++i)
			{
				PreTick(i);
			}
		}

		SpawnParameterDataSet.Tick();
		UpdateParameterDataSet.Tick();
	}

	if (bCanExecute && NewNum > 0)
	{
		InitBindings(SystemInstances[0]);

		DataSet.Tick();
		DataSet.Allocate(NewNum);
		
		//Setup the few real constants like delta time.
		float InvDt = 1.0f / DeltaSeconds;
		SpawnExecContext.Parameters.SetParameterValue(DeltaSeconds, SYS_PARAM_ENGINE_DELTA_TIME);
		SpawnExecContext.Parameters.SetParameterValue(InvDt, SYS_PARAM_ENGINE_INV_DELTA_TIME);

		UpdateExecContext.Parameters.SetParameterValue(DeltaSeconds, SYS_PARAM_ENGINE_DELTA_TIME);
		UpdateExecContext.Parameters.SetParameterValue(InvDt, SYS_PARAM_ENGINE_INV_DELTA_TIME);

		TArray<FNiagaraDataSetExecutionInfo> DataSetExecInfos;
		DataSetExecInfos.Emplace(&DataSet, 0, false, true);

		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_Update);
			DataSet.SetNumInstances(OrigNum);
			//Run update.
			UpdateExecContext.Tick(nullptr);//We can't require a specfic instance here as these are for all instances.
			DataSetExecInfos.SetNum(1, false);
			DataSetExecInfos[0].StartInstance = 0;
			DataSetExecInfos.Emplace(&UpdateParameterDataSet, 0, false, false);
			UpdateExecContext.Execute(OrigNum, DataSetExecInfos);

			if (GbDumpSystemData)
			{
				UE_LOG(LogNiagara, Log, TEXT("=== Updated %d Systems ==="), OrigNum);
				DataSet.Dump(true, 0, OrigNum);
				UpdateParameterDataSet.Dump(true, 0, OrigNum);
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_Spawn);
			DataSet.SetNumInstances(NewNum);
			//Run Spawn
			SpawnExecContext.Tick(nullptr);//We can't require a specific instance here as these are for all instances.
			DataSetExecInfos.SetNum(1, false);
			DataSetExecInfos[0].StartInstance = OrigNum;
			DataSetExecInfos.Emplace(&SpawnParameterDataSet, OrigNum, false, false);
			SpawnExecContext.Execute(SpawnNum, DataSetExecInfos);

			if (GbDumpSystemData)
			{
				UE_LOG(LogNiagara, Log, TEXT("=== Spwaned %d Systems ==="), SpawnNum);
				DataSet.Dump(true, OrigNum, SpawnNum);
				SpawnParameterDataSet.Dump(true, OrigNum, SpawnNum);
			}
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_TransferParameters);
			SystemEnabledAccessor.InitForAccess(true);
			SystemExecutionStateAccessor.InitForAccess(true);
			for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
			{
				EmitterEnabledAccessors[EmitterIdx].InitForAccess(true);
				EmitterExecutionStateAccessors[EmitterIdx].InitForAccess(true);
				for (int32 SpawnInfoIdx = 0; SpawnInfoIdx < EmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
				{
					EmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].InitForAccess(true);
				}				
			}

			int32 SystemIndex = 0;
			while (SystemIndex < SystemInstances.Num())
			{
				ENiagaraExecutionState ExecutionState = (ENiagaraExecutionState)SystemExecutionStateAccessor.GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Active);
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
				SystemInst->SetExecutionState(ExecutionState);
				bool bSystemEnabled = SystemEnabledAccessor.GetSafe(SystemIndex, true);
				//Kill any instances that are flagged as disabled.
				if (!bSystemEnabled || ExecutionState == ENiagaraExecutionState::Dead)
				{
					RemoveInstance(SystemInst);
					SystemInst->Disable();
				}
				else
				{
					//Now pull data out of the simulation and drive the emitters with it.
					TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = SystemInst->GetEmitters();
					for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
					{
						FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();

						FNiagaraBool bEmitterEnabled = EmitterEnabledAccessors[EmitterIdx].GetSafe(SystemIndex, true);
						EmitterInst.SetEnabled(bEmitterEnabled);//TODO: Alter this to instruct emitter to dump existing particles optionally.

						TArray<FNiagaraSpawnInfo>& EmitterInstSpawnInfos = EmitterInst.GetSpawnInfo();
						for (int32 SpawnInfoIdx=0; SpawnInfoIdx < EmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
						{
							if (SpawnInfoIdx < EmitterInstSpawnInfos.Num())
							{
								EmitterInstSpawnInfos[SpawnInfoIdx] = EmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].Get(SystemIndex);
							}
							else
							{
								ensure(SpawnInfoIdx < EmitterInstSpawnInfos.Num());
							}
						}

						ENiagaraExecutionState State = (ENiagaraExecutionState)EmitterExecutionStateAccessors[EmitterIdx].GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Active);
						EmitterInst.SetExecutionState(State);

						//TODO: Any other fixed function stuff like this?

						FNiagaraScriptExecutionContext& SpawnContext = EmitterInst.GetSpawnExecutionContext();
						DataSetToEmitterSpawnParameters[EmitterIdx].DataSetToParameterStore(SpawnContext.Parameters, DataSet, SystemIndex);

						FNiagaraScriptExecutionContext& UpdateContext = EmitterInst.GetUpdateExecutionContext();
						DataSetToEmitterUpdateParameters[EmitterIdx].DataSetToParameterStore(UpdateContext.Parameters, DataSet, SystemIndex);

						TArray<FNiagaraScriptExecutionContext>& EventContexts = EmitterInst.GetEventExecutionContexts();
						for (int32 EventIdx = 0; EventIdx < EventContexts.Num(); ++EventIdx)
						{
							FNiagaraScriptExecutionContext& EventContext = EventContexts[EventIdx];
							DataSetToEmitterEventParameters[EmitterIdx][EventIdx].DataSetToParameterStore(EventContext.Parameters, DataSet, SystemIndex);
						}
					}

					//System is still enabled. 
					++SystemIndex;
				}
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_PostSimulate);

		if (GbParallelSystemPostTick)
		{
			ParallelFor(SystemInstances.Num(),
				[&](int32 SystemIndex)
			{
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
				SystemInst->PostSimulateTick(DeltaSeconds);
			});
		}
		else
		{
			//Now actually tick emitters.
			for (int32 SystemIndex = 0; SystemIndex < SystemInstances.Num(); ++SystemIndex)
			{
				FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
				SystemInst->PostSimulateTick(DeltaSeconds);
			}
		}
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSim_MarkComponentDirty);
		//This is not a small amount of the update time. 
		//Annoyingly these can't be done in parallel it seems.
		//TODO: Find some way to parallelize this. Especially UpdateComponentToWorld();
		int32 SystemIndex = 0;
		while (SystemIndex < SystemInstances.Num())
		{
			FNiagaraSystemInstance* SystemInst = SystemInstances[SystemIndex];
			++SystemIndex;
			if (SystemIndex < SystemInstances.Num())
			{
				FPlatformMisc::Prefetch(SystemInstances[SystemIndex]->GetComponent());
			}
			SystemInst->GetComponent()->UpdateComponentToWorld();
			SystemInst->GetComponent()->MarkRenderDynamicDataDirty();
		}
	}
}

void FNiagaraSystemSimulation::RemoveInstance(FNiagaraSystemInstance* Instance)
{
	if (Instance->SystemInstanceIndex == INDEX_NONE)
	{
		return;
	}

	if (Instance->IsSolo())
	{
		if (SoloSystemInstances.IsValidIndex(Instance->SystemInstanceIndex))
		{
			if (GbDumpSystemData)
			{
				UE_LOG(LogNiagara, Log, TEXT("=== Removing System Solo %d ==="), Instance->SystemInstanceIndex);
				DataSetSolo.Dump(true, Instance->SystemInstanceIndex, 1);
			}

			int32 NumInstances = DataSetSolo.GetNumInstances();
			check(SoloSystemInstances.Num() == NumInstances);

			int32 SystemIndex = Instance->SystemInstanceIndex;
			check(Instance == SoloSystemInstances[SystemIndex]);
			check(SoloSystemInstances.IsValidIndex(SystemIndex));
			DataSetSolo.KillInstance(SystemIndex);
			SoloSystemInstances.RemoveAtSwap(SystemIndex);
			Instance->SystemInstanceIndex = INDEX_NONE;
			DataSetSolo.SetNumInstances(SoloSystemInstances.Num());

			if (SoloSystemInstances.IsValidIndex(SystemIndex))
			{
				SoloSystemInstances[SystemIndex]->SystemInstanceIndex = SystemIndex;
			}
		}
	}
	else if (Instance->IsPendingSpawn())
	{
		int32 SystemIndex = Instance->SystemInstanceIndex;
		check(Instance == PendingSystemInstances[SystemIndex]);
		PendingSystemInstances.RemoveAtSwap(SystemIndex);
		Instance->SystemInstanceIndex = INDEX_NONE;
		Instance->SetPendingSpawn(false);
		if (PendingSystemInstances.IsValidIndex(SystemIndex))
		{
			PendingSystemInstances[SystemIndex]->SystemInstanceIndex = SystemIndex;
		}
	}
	else if (SystemInstances.IsValidIndex(Instance->SystemInstanceIndex))
	{
		if (GbDumpSystemData)
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Removing System %d ==="), Instance->SystemInstanceIndex);
			DataSet.Dump(true, Instance->SystemInstanceIndex, 1);
		}

		int32 NumInstances = DataSet.GetNumInstances();
		check(SystemInstances.Num() == NumInstances);

		int32 SystemIndex = Instance->SystemInstanceIndex;
		check(Instance == SystemInstances[SystemIndex]);
		check(SystemInstances.IsValidIndex(SystemIndex));
		DataSet.KillInstance(SystemIndex);
		SystemInstances.RemoveAtSwap(SystemIndex);
		Instance->SystemInstanceIndex = INDEX_NONE;

		if (SystemInstances.IsValidIndex(SystemIndex))
		{
			SystemInstances[SystemIndex]->SystemInstanceIndex = SystemIndex;
		}
	}
}

void FNiagaraSystemSimulation::AddInstance(FNiagaraSystemInstance* Instance)
{
	Instance->SetPendingSpawn(true);
	Instance->SystemInstanceIndex = PendingSystemInstances.Add(Instance);
}

void FNiagaraSystemSimulation::ResetSolo(FNiagaraSystemInstance* Instance)
{
	Instance->SetPendingSpawn(true);
	Instance->SystemInstanceIndex = INDEX_NONE;
	Instance->SetExecutionState(ENiagaraExecutionState::Active);
}

void FNiagaraSystemSimulation::TickSolo(FNiagaraSystemInstance* SystemInst)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSimSolo);
	check(System->IsValid());

	if (!System->GetSystemSpawnScript(true)->IsValid() || !System->GetSystemUpdateScript(true)->IsValid())
	{
		return;
	}

	FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World);
	check(WorldMan);

	int32 SystemIndex = SystemInst->SystemInstanceIndex;

	TArray<FNiagaraDataSetExecutionInfo> DataSetExecInfos;
	DataSetExecInfos.Emplace(&DataSetSolo, SystemIndex, false, true);

	if (SystemInst->SystemInstanceIndex == INDEX_NONE || SystemInst->IsPendingSpawn())
	{
		SystemInst->SetPendingSpawn(false);

		//Spawn/respawn this system this frame rather than updating.
		if (SystemIndex == INDEX_NONE)
		{
			SystemIndex = SoloSystemInstances.Num();
			SystemInst->SystemInstanceIndex = SystemIndex;
			verify(SoloSystemInstances.Add(SystemInst) == SystemIndex);
			DataSetSolo.Allocate(SoloSystemInstances.Num(), ENiagaraSimTarget::CPUSim, true);
			DataSetSolo.SetNumInstances(SoloSystemInstances.Num());
			DataSetExecInfos[0].StartInstance = SystemIndex;
		}

		check(SoloSystemInstances.IsValidIndex(SystemIndex));
		//////////////////////////////////////////////////////////////////////////
		//TODO: This can be optimized as all the offests are the same. Can skip all the searching involved with a full bind().
		//Unbind parameter collections from the batched exec contexts.
		for (UNiagaraParameterCollection* Collection : System->GetSystemSpawnScript(true)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&SpawnExecContextSolo.Parameters);
		}
		SystemInst->GetParameters().Bind(&SpawnExecContextSolo.Parameters);
		SpawnExecContextSolo.Tick(SystemInst);
		//////////////////////////////////////////////////////////////////////////
		
		SpawnExecContextSolo.Execute(1, DataSetExecInfos);

		//////////////////////////////////////////////////////////////////////////
		//TODO: This can be optimized as all the offests are the same. Can skip all the searching involved with a full bind().
		SystemInst->GetParameters().Unbind(&SpawnExecContextSolo.Parameters);
		for (UNiagaraParameterCollection* Collection : System->GetSystemSpawnScript(true)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&SpawnExecContextSolo.Parameters);
		}
		//////////////////////////////////////////////////////////////////////////
		
		DataSetSolo.SetNumInstances(SoloSystemInstances.Num());

		if (GbDumpSystemData)
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Reset/spawn System Solo %d ==="), SystemIndex);
			DataSetSolo.Dump(true, SystemIndex, 1);
		}
	}
	else
	{
		check(SoloSystemInstances.IsValidIndex(SystemIndex));
		//////////////////////////////////////////////////////////////////////////
		//TODO: This can be optimized as all the offests are the same. Can skip all the searching involved with a full bind().
		for (UNiagaraParameterCollection* Collection : System->GetSystemUpdateScript(true)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&SpawnExecContextSolo.Parameters);
		}
		SystemInst->GetParameters().Bind(&UpdateExecContextSolo.Parameters);
		UpdateExecContextSolo.Tick(SystemInst);
		//////////////////////////////////////////////////////////////////////////

		UpdateExecContextSolo.Execute(1, DataSetExecInfos);

		//////////////////////////////////////////////////////////////////////////
		//TODO: This can be optimized as all the offests are the same. Can skip all the searching involved with a full bind().
		SystemInst->GetParameters().Unbind(&UpdateExecContextSolo.Parameters);
		for (UNiagaraParameterCollection* Collection : System->GetSystemUpdateScript(true)->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&UpdateExecContextSolo.Parameters);
		}
		//////////////////////////////////////////////////////////////////////////

		DataSetSolo.SetNumInstances(SoloSystemInstances.Num());

		if (GbDumpSystemData)
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Updated System Solo %d ==="), SystemIndex);
			DataSetSolo.Dump(true, SystemIndex, 1);
		}
	}

	InitBindings(SystemInst);

	//Kill instance if it's marked for delete.
	SoloSystemEnabledAccessor.InitForAccess(true);
	SoloSystemExecutionStateAccessor.InitForAccess(true);
	for (int32 EmitterIdx = 0; EmitterIdx < System->GetNumEmitters(); ++EmitterIdx)
	{
		SoloEmitterEnabledAccessors[EmitterIdx].InitForAccess(true);
		SoloEmitterExecutionStateAccessors[EmitterIdx].InitForAccess(true);
		for (int32 SpawnInfoIdx = 0; SpawnInfoIdx < SoloEmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
		{
			SoloEmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].InitForAccess(true);
		}
	}		

	ensure(SystemIndex != INDEX_NONE);
	bool bSystemEnabled = SoloSystemEnabledAccessor.GetSafe(SystemIndex, true);
	ENiagaraExecutionState SoloExecutionState = (ENiagaraExecutionState)SoloSystemExecutionStateAccessor.GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Active);
	SystemInst->SetExecutionState(SoloExecutionState);

	if (!bSystemEnabled || SoloExecutionState == ENiagaraExecutionState::Dead)
	{
		RemoveInstance(SystemInst);
		SystemInst->Disable();
	}
	else
	{
		//Now pull data out of the simulation and drive the emitters with it.
		TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = SystemInst->GetEmitters();
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
		{
			FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();

			FNiagaraBool bEmitterEnabled = SoloEmitterEnabledAccessors[EmitterIdx].GetSafe(SystemIndex, true);
			EmitterInst.SetEnabled(bEmitterEnabled);//TODO: Alter this to instruct emitter to dump existing particles optionally.

			ENiagaraExecutionState State = (ENiagaraExecutionState)SoloEmitterExecutionStateAccessors[EmitterIdx].GetSafe(SystemIndex, (int32)ENiagaraExecutionState::Active);
			EmitterInst.SetExecutionState(State);

			for (int32 SpawnInfoIdx = 0; SpawnInfoIdx < SoloEmitterSpawnInfoAccessors[EmitterIdx].Num(); ++SpawnInfoIdx)
			{
				EmitterInst.GetSpawnInfo()[SpawnInfoIdx] = SoloEmitterSpawnInfoAccessors[EmitterIdx][SpawnInfoIdx].Get(SystemIndex);
			}
			
			//TODO: Any other fixed function stuff like this?

			FNiagaraScriptExecutionContext& SpawnContext = EmitterInst.GetSpawnExecutionContext();
			DataSetToEmitterSpawnParameters[EmitterIdx].DataSetToParameterStore(SpawnContext.Parameters, DataSetSolo, SystemIndex);

			FNiagaraScriptExecutionContext& UpdateContext = EmitterInst.GetUpdateExecutionContext();
			DataSetToEmitterUpdateParameters[EmitterIdx].DataSetToParameterStore(UpdateContext.Parameters, DataSetSolo, SystemIndex);

			TArray<FNiagaraScriptExecutionContext>& EventContexts = EmitterInst.GetEventExecutionContexts();
			for (int32 EventIdx = 0; EventIdx < EventContexts.Num(); ++EventIdx)
			{
				FNiagaraScriptExecutionContext& EventContext = EventContexts[EventIdx];
				DataSetToEmitterEventParameters[EmitterIdx][EventIdx].DataSetToParameterStore(EventContext.Parameters, DataSetSolo, SystemIndex);
			}
		}
	}
}

void FNiagaraSystemSimulation::InitBindings(FNiagaraSystemInstance* SystemInst)
{
	//Have to init here as we need an actual parameter store to pull the layout info from.
	//TODO: Pull the layout stuff out of each data set and store. So much duplicated data.
	//This assumes that all layouts for all emitters is the same. Which it should be.
	//Ideally we can store all this layout info in the systm/emitter assets so we can just generate this in Init()
	if (DataSetToEmitterSpawnParameters.Num() == 0)
	{
		SpawnParameterToDataSetBinding.Init(SpawnParameterDataSet, SystemInst->GetInstanceParameters());
		UpdateParameterToDataSetBinding.Init(UpdateParameterDataSet, SystemInst->GetInstanceParameters());

		TArray<TSharedRef<FNiagaraEmitterInstance>>& Emitters = SystemInst->GetEmitters();
		check(DataSetToEmitterUpdateParameters.Num() == 0);
		check(DataSetToEmitterEventParameters.Num() == 0);
		DataSetToEmitterSpawnParameters.SetNum(Emitters.Num());
		DataSetToEmitterUpdateParameters.SetNum(Emitters.Num());
		DataSetToEmitterEventParameters.SetNum(Emitters.Num());
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); ++EmitterIdx)
		{
			FNiagaraEmitterInstance& EmitterInst = Emitters[EmitterIdx].Get();
			FNiagaraScriptExecutionContext& SpawnContext = EmitterInst.GetSpawnExecutionContext();
			DataSetToEmitterSpawnParameters[EmitterIdx].Init(DataSet, SpawnContext.Parameters);

			FNiagaraScriptExecutionContext& UpdateContext = EmitterInst.GetUpdateExecutionContext();
			DataSetToEmitterUpdateParameters[EmitterIdx].Init(DataSet, UpdateContext.Parameters);

			TArray<FNiagaraScriptExecutionContext>& EventContexts = EmitterInst.GetEventExecutionContexts();
			DataSetToEmitterEventParameters[EmitterIdx].SetNum(EventContexts.Num());
			for (int32 EventIdx = 0; EventIdx < EventContexts.Num(); ++EventIdx)
			{
				FNiagaraScriptExecutionContext& EventContext = EventContexts[EventIdx];
				DataSetToEmitterEventParameters[EmitterIdx][EventIdx].Init(DataSet, EventContext.Parameters);
			}
		}
	}
}



