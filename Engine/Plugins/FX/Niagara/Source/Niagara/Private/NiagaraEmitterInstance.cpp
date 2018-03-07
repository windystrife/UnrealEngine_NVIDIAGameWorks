// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterInstance.h"
#include "Materials/Material.h"
#include "VectorVM.h"
#include "NiagaraStats.h"
#include "NiagaraConstants.h"
#include "NiagaraRenderer.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraDataInterface.h"
#include "NiagaraEmitterInstanceBatcher.h"
#include "NiagaraScriptExecutionContext.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraWorldManager.h"

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Custom Events"), STAT_NiagaraNumCustomEvents, STATGROUP_Niagara);

//DECLARE_CYCLE_STAT(TEXT("Tick"), STAT_NiagaraTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Simulate"), STAT_NiagaraSimulate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn"), STAT_NiagaraSpawn, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Spawn"), STAT_NiagaraEvents, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Kill"), STAT_NiagaraKill, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Event Handling"), STAT_NiagaraEventHandle, STATGROUP_Niagara);

static int32 GbDumpParticleData = 0;
static FAutoConsoleVariableRef CVarNiagaraDumpParticleData(
	TEXT("fx.DumpParticleData"),
	GbDumpParticleData,
	TEXT("If > 0 current frame particle data will be dumped after simulation. \n"),
	ECVF_Default
	);

//////////////////////////////////////////////////////////////////////////

FNiagaraEmitterInstance::FNiagaraEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance)
: bError(false)
, CPUTimeMS(0.0f)
, ExecutionState(ENiagaraExecutionState::Active)
, CachedBounds(ForceInit)
, ParentSystemInstance(InParentSystemInstance)
{
	bDumpAfterEvent = false;
}

FNiagaraEmitterInstance::~FNiagaraEmitterInstance()
{
	//UE_LOG(LogNiagara, Warning, TEXT("~Simulator %p"), this);
	ClearRenderer();
	CachedBounds.Init();
	UnbindParameters();
}

void FNiagaraEmitterInstance::ClearRenderer()
{
	for (int32 i = 0; i < EmitterRenderer.Num(); i++)
	{
		if (EmitterRenderer[i])
		{
			//UE_LOG(LogNiagara, Warning, TEXT("ClearRenderer %p"), EmitterRenderer);
			// This queues up the renderer for deletion on the render thread..
			EmitterRenderer[i]->Release();
			EmitterRenderer[i] = nullptr;
		}
	}
}

void FNiagaraEmitterInstance::Init(int32 InEmitterIdx, FName InSystemInstanceName)
{
	EmitterIdx = InEmitterIdx;
	OwnerSystemInstanceName = InSystemInstanceName;
	Data = FNiagaraDataSet(FNiagaraDataSetID(GetEmitterHandle().GetIdName(), ENiagaraDataSetType::ParticleData));

	//Init the spawn infos to the correct number for this system.
	const TArray<FNiagaraEmitterSpawnAttributes>& EmitterSpawnInfoAttrs = ParentSystemInstance->GetSystem()->GetEmitterSpawnAttributes();
	if (EmitterSpawnInfoAttrs.IsValidIndex(EmitterIdx))
	{
		SpawnInfos.SetNum(EmitterSpawnInfoAttrs[EmitterIdx].SpawnAttributes.Num());
	}
}

void FNiagaraEmitterInstance::ResetSimulation()
{
	Data.ResetNumInstances();
	Age = 0;
	Loops = 0;
	CollisionBatch.Reset();
	bError = false;

	const UNiagaraEmitter* PinnedProps = GetEmitterHandle().GetInstance();

	SetExecutionState(ENiagaraExecutionState::Active);

	if (!PinnedProps)
	{
		UE_LOG(LogNiagara, Error, TEXT("Unknown Error creating Niagara Simulation. Properties were null."));
		bError = true;
		return;
	}

	//Check for various failure conditions and bail.
	if (!PinnedProps->UpdateScriptProps.Script || !PinnedProps->SpawnScriptProps.Script)
	{
		//TODO - Arbitrary named scripts. Would need some base functionality for Spawn/Udpate to be called that can be overriden in BPs for emitters with custom scripts.
		UE_LOG(LogNiagara, Error, TEXT("Emitter cannot be enabled because it's doesn't have both an update and spawn script."), *PinnedProps->GetFullName());
		bError = true;
		return;
	}
	if (PinnedProps->UpdateScriptProps.Script->ByteCode.Num() == 0 && PinnedProps->SpawnScriptProps.Script->ByteCode.Num() == 0 && PinnedProps->SimTarget==ENiagaraSimTarget::CPUSim)
	{
		UE_LOG(LogNiagara, Error, TEXT("Emitter cannot be enabled because it's spawn or update script was not compiled correctly. %s"), *PinnedProps->GetFullName());
		bError = true;
		return;
	}

	if (PinnedProps->SpawnScriptProps.Script->DataUsage.bReadsAttriubteData)
	{
		UE_LOG(LogNiagara, Error, TEXT("%s reads attribute data and so cannot be used as a spawn script. The data being read would be invalid."), *PinnedProps->SpawnScriptProps.Script->GetName());
		bError = true;
		return;
	}
	if (PinnedProps->UpdateScriptProps.Script->Attributes.Num() == 0 || PinnedProps->SpawnScriptProps.Script->Attributes.Num() == 0)
	{
		UE_LOG(LogNiagara, Error, TEXT("This emitter cannot be enabled because it's spawn or update script doesn't have any attriubtes.."));
		bError = true;
		return;
	}
}

void FNiagaraEmitterInstance::DirtyDataInterfaces()
{
	// Make sure that our function tables need to be regenerated...
	SpawnExecContext.DirtyDataInterfaces();
	UpdateExecContext.DirtyDataInterfaces();
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		EventContext.DirtyDataInterfaces();
	}
}

void FNiagaraEmitterInstance::ReInitSimulation()
{
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	bIsEnabled = EmitterHandle.GetIsEnabled();

	ResetSimulation();

	Data.Reset();
	DataSetMap.Empty();

	const UNiagaraEmitter* PinnedProps = EmitterHandle.GetInstance();

	//Add the particle data to the data set map.
	//Currently just used for the tick loop but will also allow access directly to the particle data from other emitters.
	DataSetMap.Add(Data.GetID(), &Data);
	//Warn the user if there are any attributes used in the update script that are not initialized in the spawn script.
	//TODO: We need some window in the System editor and possibly the graph editor for warnings and errors.

	const bool bVerboseAttributeLogging = false;

	if (bVerboseAttributeLogging)
	{
		for (FNiagaraVariable& Attr : PinnedProps->UpdateScriptProps.Script->Attributes)
		{
			int32 FoundIdx;
			if (!PinnedProps->SpawnScriptProps.Script->Attributes.Find(Attr, FoundIdx))
			{
				UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the Update script for %s but it is not initialised in the Spawn script!"), *Attr.GetName().ToString(), *EmitterHandle.GetName().ToString());
			}
			for (int32 i = 0; i < PinnedProps->EventHandlerScriptProps.Num(); i++)
			{
				if (PinnedProps->EventHandlerScriptProps[i].Script && !PinnedProps->EventHandlerScriptProps[i].Script->Attributes.Find(Attr, FoundIdx))
				{
					UE_LOG(LogNiagara, Warning, TEXT("Attribute %s is used in the event handler script for %s but it is not initialised in the Spawn script!"), *Attr.GetName().ToString(), *EmitterHandle.GetName().ToString());
				}
			}
		}
	}
	Data.AddVariables(PinnedProps->UpdateScriptProps.Script->Attributes);
	Data.AddVariables(PinnedProps->SpawnScriptProps.Script->Attributes);
	Data.Finalize();

	CollisionBatch.Init(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName());

	UpdateScriptEventDataSets.Empty();
	for (const FNiagaraEventGeneratorProperties &GeneratorProps : PinnedProps->UpdateScriptProps.EventGenerators)
	{
		FNiagaraDataSet *Set = FNiagaraEventDataSetMgr::CreateEventDataSet(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName(), GeneratorProps.SetProps.ID.Name);
		Set->Reset();
		Set->AddVariables(GeneratorProps.SetProps.Variables);
		Set->Finalize();
		UpdateScriptEventDataSets.Add(Set);
	}

	SpawnScriptEventDataSets.Empty();
	for (const FNiagaraEventGeneratorProperties &GeneratorProps : PinnedProps->SpawnScriptProps.EventGenerators)
	{
		FNiagaraDataSet *Set = FNiagaraEventDataSetMgr::CreateEventDataSet(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName(), GeneratorProps.SetProps.ID.Name);
		Set->Reset();
		Set->AddVariables(GeneratorProps.SetProps.Variables);
		Set->Finalize();
		SpawnScriptEventDataSets.Add(Set);
	}

	SpawnExecContext.Init(PinnedProps->SpawnScriptProps.Script, PinnedProps->SimTarget);
	UpdateExecContext.Init(PinnedProps->UpdateScriptProps.Script, PinnedProps->SimTarget);

	EventExecContexts.SetNum(PinnedProps->EventHandlerScriptProps.Num());
	int32 NumEvents = PinnedProps->EventHandlerScriptProps.Num();
	for (int32 i = 0; i < NumEvents; i++)
	{
		UNiagaraScript* EventScript = PinnedProps->EventHandlerScriptProps[i].Script;
		if (EventScript)
		{
			if (EventScript->ByteCode.Num() == 0)
			{
				UE_LOG(LogNiagara, Error, TEXT("%s has an event handler script hat didn't compile correctly."), *GetEmitterHandle().GetName().ToString());
				bError = true;
				return;
			}
		}

		//This is cpu explicitly? Are we doing event handlers on GPU?
		EventExecContexts[i].Init(EventScript, ENiagaraSimTarget::CPUSim);
	}

	UNiagaraEmitter* Emitter = GetEmitterHandle().GetInstance();

	//Setup direct bindings for setting parameter values.
	SpawnIntervalBinding.Init(SpawnExecContext.Parameters, Emitter->GetEmitterParameter(SYS_PARAM_EMITTER_SPAWN_INTERVAL));
	InterpSpawnStartBinding.Init(SpawnExecContext.Parameters, Emitter->GetEmitterParameter(SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT));

	FNiagaraVariable EmitterAgeParam = Emitter->GetEmitterParameter(SYS_PARAM_EMITTER_AGE);
	SpawnEmitterAgeBinding.Init(SpawnExecContext.Parameters, EmitterAgeParam);
	UpdateEmitterAgeBinding.Init(UpdateExecContext.Parameters, EmitterAgeParam);
	EventEmitterAgeBindings.SetNum(NumEvents);
	for (int32 i = 0; i < NumEvents; i++)
	{
		EventEmitterAgeBindings[i].Init(EventExecContexts[i].Parameters, EmitterAgeParam);
	}

	SpawnExecCountBinding.Init(SpawnExecContext.Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	UpdateExecCountBinding.Init(UpdateExecContext.Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	EventExecCountBindings.SetNum(NumEvents);
	for (int32 i = 0; i < NumEvents; i++)
	{
		EventExecCountBindings[i].Init(EventExecContexts[i].Parameters, SYS_PARAM_ENGINE_EXEC_COUNT);
	}

	if (PinnedProps->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		//Just ensure we've generated the singleton here on the GT as it throws a wobbler if we do this later in parallel.
		NiagaraEmitterInstanceBatcher::Get();
	}
}

//Unsure on usage of this atm. Possibly useful in future.
// void FNiagaraEmitterInstance::RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance)
// {
// 	OldInstance->GetParameterStore().Unbind(&SpawnExecContext.Parameters);
// 	NewInstance->GetParameterStore().Bind(&SpawnExecContext.Parameters);
// 
// 	OldInstance->GetParameterStore().Unbind(&UpdateExecContext.Parameters);
// 	NewInstance->GetParameterStore().Bind(&UpdateExecContext.Parameters);
// 
// 	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
// 	{
// 		OldInstance->GetParameterStore().Unbind(&EventContext.Parameters);
// 		NewInstance->GetParameterStore().Bind(&EventContext.Parameters);
// 	}
// }

void FNiagaraEmitterInstance::UnbindParameters()
{
	FNiagaraWorldManager* WorldMan = ParentSystemInstance->GetWorldManager();
	check(WorldMan);

	//Unbind our parameter collections.
	for (UNiagaraParameterCollection* Collection : SpawnExecContext.Script->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&SpawnExecContext.Parameters);
	}
	for (UNiagaraParameterCollection* Collection : UpdateExecContext.Script->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&UpdateExecContext.Parameters);
	}

	for (int32 EventIdx = 0; EventIdx < EventExecContexts.Num(); ++EventIdx)
	{
		for (UNiagaraParameterCollection* Collection : EventExecContexts[EventIdx].Script->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Unbind(&EventExecContexts[EventIdx].Parameters);
		}
	}

	FNiagaraParameterStore& SystemParams = ParentSystemInstance->GetParameters();
	SystemParams.Unbind(&SpawnExecContext.Parameters);
	SystemParams.Unbind(&UpdateExecContext.Parameters);
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		SystemParams.Unbind(&EventContext.Parameters);
	}
}

void FNiagaraEmitterInstance::BindParameters()
{
	FNiagaraWorldManager* WorldMan = ParentSystemInstance->GetWorldManager();
	check(WorldMan);

	for (UNiagaraParameterCollection* Collection : SpawnExecContext.Script->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&SpawnExecContext.Parameters);
	}
	for (UNiagaraParameterCollection* Collection : UpdateExecContext.Script->ParameterCollections)
	{
		WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&UpdateExecContext.Parameters);
	}

	for (int32 EventIdx = 0; EventIdx < EventExecContexts.Num(); ++EventIdx)
	{
		for (UNiagaraParameterCollection* Collection : EventExecContexts[EventIdx].Script->ParameterCollections)
		{
			WorldMan->GetParameterCollection(Collection)->GetParameterStore().Bind(&EventExecContexts[EventIdx].Parameters);
		}
	}

	//Now bind parameters from the component and system.
	FNiagaraParameterStore& InstanceParams = ParentSystemInstance->GetParameters();
	InstanceParams.Bind(&SpawnExecContext.Parameters);
	InstanceParams.Bind(&UpdateExecContext.Parameters);
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		InstanceParams.Bind(&EventContext.Parameters);
	}
}

void FNiagaraEmitterInstance::PostResetSimulation()
{
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	if (!bError)
	{
		check(ParentSystemInstance);
		const UNiagaraEmitter* Props = EmitterHandle.GetInstance();

		//Go through all our receivers and grab their generator sets so that the source emitters can do any init work they need to do.
		for (const FNiagaraEventReceiverProperties& Receiver : Props->SpawnScriptProps.EventReceivers)
		{
			//FNiagaraDataSet* ReceiverSet = ParentSystemInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
			const FNiagaraDataSet* ReceiverSet = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), Receiver.SourceEmitter, Receiver.SourceEventGenerator);

		}

		for (const FNiagaraEventReceiverProperties& Receiver : Props->UpdateScriptProps.EventReceivers)
		{
			//FNiagaraDataSet* ReceiverSet = ParentSystemInstance->GetDataSet(FNiagaraDataSetID(Receiver.SourceEventGenerator, ENiagaraDataSetType::Event), Receiver.SourceEmitter);
			const FNiagaraDataSet* ReceiverSet = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), Receiver.SourceEmitter, Receiver.SourceEventGenerator);
		}

		// add the collision event set
		// TODO: add spawn and death event data sets (if enabled)
		if (Props->CollisionMode != ENiagaraCollisionMode::None)
		{
			CollisionBatch.Init(ParentSystemInstance->GetIDName(), EmitterHandle.GetIdName());	// creates and sets up the data set for the events
		}
	}
}

FNiagaraDataSet* FNiagaraEmitterInstance::GetDataSet(FNiagaraDataSetID SetID)
{
	FNiagaraDataSet** SetPtr = DataSetMap.Find(SetID);
	FNiagaraDataSet* Ret = NULL;
	if (SetPtr)
	{
		Ret = *SetPtr;
	}
	else
	{
		// TODO: keep track of data sets generated by the scripts (event writers) and find here
	}

	return Ret;
}

const FNiagaraEmitterHandle& FNiagaraEmitterInstance::GetEmitterHandle() const
{
	return ParentSystemInstance->GetSystem()->GetEmitterHandles()[EmitterIdx];
}

float FNiagaraEmitterInstance::GetTotalCPUTime()
{
	float Total = CPUTimeMS;
	for (int32 i = 0; i < EmitterRenderer.Num(); i++)
	{
		if (EmitterRenderer[i])
		{
			Total += EmitterRenderer[i]->GetCPUTimeMS();

		}
	}

	return Total;
}

int FNiagaraEmitterInstance::GetTotalBytesUsed()
{
	int32 BytesUsed = Data.GetSizeBytes();
	/*
	for (FNiagaraDataSet& Set : DataSets)
	{
		BytesUsed += Set.GetSizeBytes();
	}
	*/
	return BytesUsed;
}


/** Look for dead particles and move from the end of the list to the dead location, compacting in the process
  * Also calculates bounds; Kill will be removed from this once we do conditional write
  */
void FNiagaraEmitterInstance::PostProcessParticles()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraKill);
	int32 OrigNumParticles = Data.GetNumInstances();
	int32 CurNumParticles = OrigNumParticles;

	CachedBounds.Init();

	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	if (CurNumParticles == 0 || EmitterHandle.GetInstance()->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		return;
	}

	FNiagaraDataSetIterator<FVector> PosItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
	FNiagaraDataSetIterator<FVector2D> SizeItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Size")));
	FNiagaraDataSetIterator<FVector> MeshScaleItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));

	FVector MaxSize(EForceInit::ForceInitToZero);

	while (PosItr.IsValid())
	{
		// Only increment the iterators if we don't kill a particle because
		// we swap with the last particle and it too may have aged out, so we want
		// to keep looping on the same index as long as it gets swapped out for a 
		// dead particle.

		FVector Position;
		PosItr.Get(Position);

		// Some graphs have a tendency to divide by zero. This ContainsNaN has been added prophylactically
		// to keep us safe during GDC. It should be removed as soon as we feel safe that scripts are appropriately warned.
		if (Position.ContainsNaN() == false)
		{
			CachedBounds += Position;

			// We advance the scale or size depending of if we use either.
			if (MeshScaleItr.IsValid())
			{
				MaxSize = MaxSize.ComponentMax(*MeshScaleItr);
				MeshScaleItr.Advance();
			}
			else if (SizeItr.IsValid())
			{
				MaxSize = MaxSize.ComponentMax(FVector((*SizeItr).GetMax()));
				SizeItr.Advance();
			}
			// Now we advance our main iterator since we've safely handled this particle.
			PosItr.Advance();
		}
		else
		{
			//Must always advance otherwise we'll have inf loop if we have nans in the position
			if (MeshScaleItr.IsValid())
			{
				MeshScaleItr.Advance();
			}
			else if (SizeItr.IsValid())
			{
				SizeItr.Advance();
			}
			PosItr.Advance();
		}
	}

	CachedBounds = CachedBounds.ExpandBy(MaxSize);
}

/** 
  * PreTick - handles killing dead particles, emitter death, and buffer swaps
  */
void FNiagaraEmitterInstance::PreTick()
{
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	const UNiagaraEmitter* PinnedProps = EmitterHandle.GetInstance();

	if (!PinnedProps || !bIsEnabled || bError || ExecutionState == ENiagaraExecutionState::Dead)
	{
		return;
	}

	bool bOk = true;
	bOk &= SpawnExecContext.Tick(ParentSystemInstance);
	bOk &= UpdateExecContext.Tick(ParentSystemInstance);
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		bOk &= EventContext.Tick(ParentSystemInstance);
	}

	if (!bOk)
	{
		ResetSimulation();
		bError = true;
		return;
	}

	if (Data.GetNumInstances() == 0)
	{
		return;
	}

	check(Data.GetNumVariables() > 0);
	check(PinnedProps->SpawnScriptProps.Script);
	check(PinnedProps->UpdateScriptProps.Script);

	// generate events from collisions
	if (PinnedProps->CollisionMode == ENiagaraCollisionMode::SceneGeometry)
	{
		CollisionBatch.GenerateEventsFromResults(this);
	}


	//Swap all data set buffers before doing the main tick on any simulation.
	if (PinnedProps->SimTarget == ENiagaraSimTarget::CPUSim)
	{
		for (TPair<FNiagaraDataSetID, FNiagaraDataSet*> SetPair : DataSetMap)
		{
			SetPair.Value->Tick(PinnedProps->SimTarget);
		}
		CollisionBatch.Tick(PinnedProps->SimTarget);

		for (FNiagaraDataSet* Set : UpdateScriptEventDataSets)
		{
			Set->Tick(PinnedProps->SimTarget);
		}

		for (FNiagaraDataSet* Set : SpawnScriptEventDataSets)
		{
			Set->Tick(PinnedProps->SimTarget);
		}
	}
}


void FNiagaraEmitterInstance::Tick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraTick);
	SimpleTimer TickTime;

	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	const UNiagaraEmitter* PinnedProps = EmitterHandle.GetInstance();
	if (!PinnedProps || !bIsEnabled || bError || ExecutionState == ENiagaraExecutionState::Dead)
	{
		return;
	}

	Age += DeltaSeconds;

	check(Data.GetNumVariables() > 0);
	check(PinnedProps->SpawnScriptProps.Script);
	check(PinnedProps->UpdateScriptProps.Script);
	
	//TickEvents(DeltaSeconds);

	// add system constants
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraConstants);
		float InvDT = 1.0f / DeltaSeconds;

		//TODO: Create a binding helper object for these to avoid the search.
		SpawnEmitterAgeBinding.SetValue(Age);
		UpdateEmitterAgeBinding.SetValue(Age);
		for (FNiagaraParameterDirectBinding<float>& Binding : EventEmitterAgeBindings)
		{
			Binding.SetValue(Age);
		}
	}
	
	// Calculate number of new particles from regular spawning 
	int32 OrigNumParticles = Data.GetPrevNumInstances();

	uint32 SpawnTotal = 0;
	for (FNiagaraSpawnInfo& Info : SpawnInfos)
	{
		SpawnTotal += Info.Count;
	}

	// Calculate number of new particles from all event related spawns
	TArray<TArray<int32>> EventSpawnCounts;
	EventSpawnCounts.AddDefaulted(PinnedProps->EventHandlerScriptProps.Num());
	TArray<int32> EventHandlerSpawnCounts;
	EventHandlerSpawnCounts.AddDefaulted(PinnedProps->EventHandlerScriptProps.Num());
	uint32 EventSpawnTotal = 0;
	TArray<FNiagaraDataSet*> EventSet;
	EventSet.AddZeroed(PinnedProps->EventHandlerScriptProps.Num());
	TArray<FGuid> SourceEmitterGuid;
	SourceEmitterGuid.AddDefaulted(PinnedProps->EventHandlerScriptProps.Num());
	TArray<FName> SourceEmitterName;
	SourceEmitterName.AddDefaulted(PinnedProps->EventHandlerScriptProps.Num());
	TArray<bool> bPerformEventSpawning;
	bPerformEventSpawning.AddDefaulted(PinnedProps->EventHandlerScriptProps.Num());

	for (int32 i = 0; i < PinnedProps->EventHandlerScriptProps.Num(); i++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = PinnedProps->EventHandlerScriptProps[i];
		SourceEmitterGuid[i] = EventHandlerProps.SourceEmitterID;
		SourceEmitterName[i] = SourceEmitterGuid[i].IsValid() ? *SourceEmitterGuid[i].ToString() : EmitterHandle.GetIdName();
		EventSet[i] = FNiagaraEventDataSetMgr::GetEventDataSet(ParentSystemInstance->GetIDName(), SourceEmitterName[i], EventHandlerProps.SourceEventName);
		bPerformEventSpawning[i] = (ExecutionState == ENiagaraExecutionState::Active && EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::SpawnedParticles);
		if (bPerformEventSpawning[i])
		{
			uint32 EventSpawnNum = CalculateEventSpawnCount(EventHandlerProps, EventSpawnCounts[i], EventSet[i]);
			EventSpawnTotal += EventSpawnNum;
			EventHandlerSpawnCounts[i] = EventSpawnNum;
		}
	}


	/* GPU simulation -  we just create an FNiagaraComputeExecutionContext, queue it, and let the batcher take care of the rest
	 */
	if (PinnedProps->SimTarget == ENiagaraSimTarget::GPUComputeSim)	
	{
		FNiagaraComputeExecutionContext *ComputeContext = new FNiagaraComputeExecutionContext();
		ComputeContext->MainDataSet = &Data;
		ComputeContext->RTSpawnScript = PinnedProps->SpawnScriptProps.Script->GetRenderThreadScript();
		ComputeContext->RTUpdateScript = PinnedProps->UpdateScriptProps.Script->GetRenderThreadScript();
		ComputeContext->SpawnRateInstances = SpawnTotal;
		ComputeContext->BurstInstances = 0;
		ComputeContext->EventSpawnTotal = EventSpawnTotal;

		ComputeContext->UpdateInterfaces = PinnedProps->UpdateScriptProps.Script->DataInterfaceInfo;

		// copy over the constants for the render thread
		//
		int32 Size = UpdateExecContext.Parameters.GetExternalParameterSize();
		if (Size)
		{
			ComputeContext->UpdateParams.AddUninitialized(FMath::DivideAndRoundUp(Size, 16) * 16);
			FMemory::Memcpy(ComputeContext->UpdateParams.GetData(), UpdateExecContext.Parameters.GetParameterData(0), Size);
		}
		Size = SpawnExecContext.Parameters.GetExternalParameterSize();
		if (Size)
		{
			ComputeContext->SpawnParams.AddUninitialized(FMath::DivideAndRoundUp(Size, 16) * 16);
			FMemory::Memcpy(ComputeContext->SpawnParams.GetData(), SpawnExecContext.Parameters.GetParameterData(0), Size);
		}

		// push event data sets to the context
		for (FNiagaraDataSet *Set : UpdateScriptEventDataSets)
		{
			ComputeContext->UpdateEventWriteDataSets.Add(Set);
		}

		ComputeContext->EventHandlerScriptProps = PinnedProps->EventHandlerScriptProps;
		ComputeContext->EventSets = EventSet;
		ComputeContext->EventSpawnCounts = EventHandlerSpawnCounts;
		NiagaraEmitterInstanceBatcher::Get()->Queue(ComputeContext);

		CachedBounds.Init();
		CachedBounds = CachedBounds.ExpandBy(FVector(20.0, 20.0, 20.0));	// temp until GPU sims update bounds
		return;
	}

	int32 AllocationSize = OrigNumParticles + SpawnTotal + EventSpawnTotal;
	//Allocate space for prev frames particles and any new one's we're going to spawn.
	Data.Allocate(AllocationSize, PinnedProps->SimTarget);



	TArray<FNiagaraDataSetExecutionInfo> DataSetExecInfos;
	DataSetExecInfos.Emplace(&Data, 0, false, true);

	// Simulate existing particles forward by DeltaSeconds.
	if ((ExecutionState != ENiagaraExecutionState::Dead && ExecutionState != ENiagaraExecutionState::Paused) /*&& OrigNumParticles > 0*/)
	{
		/*
		if (bDumpAfterEvent)
		{
			Data.Dump(false);
			bDumpAfterEvent = false;
		}
		*/

		Data.SetNumInstances(OrigNumParticles);
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSimulate);

		UpdateExecCountBinding.SetValue(OrigNumParticles);
		DataSetExecInfos.SetNum(1, false);
		DataSetExecInfos[0].StartInstance = 0;
		for (FNiagaraDataSet* EventDataSet : UpdateScriptEventDataSets)
		{
			DataSetExecInfos.Emplace(EventDataSet, 0, true, false);
		}
		UpdateExecContext.Execute(OrigNumParticles, DataSetExecInfos);

		if (GbDumpParticleData)
		{
			UE_LOG(LogNiagara, Log, TEXT("=== Updated %d Particles ==="), OrigNumParticles);
			Data.Dump(true, 0, OrigNumParticles);
		}
	}

	uint32 EventSpawnStart = Data.GetNumInstances();

	//Init new particles with the spawn script.
	if (ExecutionState==ENiagaraExecutionState::Active && SpawnTotal + EventSpawnTotal > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSpawn);

		//Handle main spawn rate spawning
		auto SpawnParticles = [&](int32 Num, FString DumpLabel)
		{
			if (Num > 0)
			{
				int32 OrigNum = Data.GetNumInstances();
				Data.SetNumInstances(OrigNum + Num);

				SpawnExecCountBinding.SetValue(Num);
				DataSetExecInfos.SetNum(1, false);
				DataSetExecInfos[0].StartInstance = OrigNum;
				for (FNiagaraDataSet* EventDataSet : SpawnScriptEventDataSets)
				{
					DataSetExecInfos.Emplace(EventDataSet, OrigNum, true, false);
				}
				SpawnExecContext.Execute(Num, DataSetExecInfos);

				if (GbDumpParticleData)
				{
					UE_LOG(LogNiagara, Log, TEXT("=== Spawned %d Particles ==="), Num);
					Data.Dump(true, OrigNum, Num);
				}
			}
		};

		//Perform all our regular spawning that's driven by our emitter script.
		for (FNiagaraSpawnInfo& Info : SpawnInfos)
		{
			SpawnIntervalBinding.SetValue(Info.IntervalDt);
			InterpSpawnStartBinding.SetValue(Info.InterpStartDt);

			SpawnParticles(Info.Count, TEXT("Regular Spawn"));
		};

		EventSpawnStart = Data.GetNumInstances();

		for (int32 EventScriptIdx = 0; EventScriptIdx < PinnedProps->EventHandlerScriptProps.Num(); EventScriptIdx++)
		{
			//Spawn particles coming from events.
			for (int32 i = 0; i < EventSpawnCounts[EventScriptIdx].Num(); i++)
			{
				int32 EventNumToSpawn = EventSpawnCounts[EventScriptIdx][i];

				//Event spawns are instantaneous at the middle of the frame?
				SpawnIntervalBinding.SetValue(0.0f);
				InterpSpawnStartBinding.SetValue(DeltaSeconds * 0.5f);

				SpawnParticles(EventNumToSpawn, TEXT("Event Spawn"));
			}
		}
	}

	// handle event based spawning
	for (int32 EventScriptIdx = 0;  EventScriptIdx < PinnedProps->EventHandlerScriptProps.Num(); EventScriptIdx++)
	{
		const FNiagaraEventScriptProperties &EventHandlerProps = PinnedProps->EventHandlerScriptProps[EventScriptIdx];

		if (bPerformEventSpawning[EventScriptIdx] && EventSet[EventScriptIdx] && EventSpawnCounts[EventScriptIdx].Num())
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);
			for (int32 i = 0; i < EventSpawnCounts[EventScriptIdx].Num(); i++)
			{
				int32 EventNumToSpawn = EventSpawnCounts[EventScriptIdx][i];
				ensure(EventNumToSpawn + EventSpawnStart < Data.GetNumInstances());
				EventExecCountBindings[EventScriptIdx].SetValue(EventNumToSpawn);

				DataSetExecInfos.SetNum(1, false);
				DataSetExecInfos[0].StartInstance = EventSpawnStart;
				DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
				SpawnExecContext.Execute(EventNumToSpawn, DataSetExecInfos);

				EventSpawnStart += EventNumToSpawn;
			}
		}


		// handle all-particle events
		if (EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::EveryParticle && EventSet[EventScriptIdx])
		{
			if (EventSet[EventScriptIdx]->GetPrevNumInstances())
			{
				SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);

				for (uint32 i = 0; i < EventSet[EventScriptIdx]->GetPrevNumInstances(); i++)
				{
					// If we have events, Swap buffers, to make sure we don't overwrite previous script results
					// and copy prev to cur, because the event script isn't likely to write all attributes
					// TODO: copy only event script's unused attributes
					Data.Tick();
					Data.CopyPrevToCur();

					EventExecCountBindings[EventScriptIdx].SetValue(Data.GetPrevNumInstances());
					DataSetExecInfos.SetNum(1, false);
					DataSetExecInfos[0].StartInstance = 0;
					DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
					SpawnExecContext.Execute(Data.GetPrevNumInstances(), DataSetExecInfos);

					// TODO: Should we check to see if we wrote all particles here?
				}
			}
		}


		// handle single-particle events
		// TODO: we'll need a way to either skip execution of the VM if an index comes back as invalid, or we'll have to pre-process
		// event/particle arrays; this is currently a very naive (and comparatively slow) implementation, until full indexed reads work
		if (EventHandlerProps.Script && EventHandlerProps.ExecutionMode == EScriptExecutionMode::SingleParticle && EventSet[EventScriptIdx])
		{
			SCOPE_CYCLE_COUNTER(STAT_NiagaraEventHandle);
			FNiagaraVariable IndexVar(FNiagaraTypeDefinition::GetIntDef(), "ParticleIndex");
			FNiagaraDataSetIterator<int32> IndexItr(*EventSet[EventScriptIdx], IndexVar, 0, false);
			if (IndexItr.IsValid())
			{
				EventExecCountBindings[EventScriptIdx].SetValue(1);

				for (uint32 i = 0; i < EventSet[EventScriptIdx]->GetPrevNumInstances(); i++)
				{
					int32 Index = *IndexItr;
					IndexItr.Advance();
					DataSetExecInfos.SetNum(1, false);
					DataSetExecInfos[0].StartInstance = Index;
					DataSetExecInfos.Emplace(EventSet[EventScriptIdx], i, false, false);
					SpawnExecContext.Execute(1, DataSetExecInfos);

					// TODO: Should we check to see if we wrote all particles here?
				}
			}
		}
	}


	// kick off collision tests from this emitter
	if (PinnedProps->CollisionMode == ENiagaraCollisionMode::SceneGeometry)
	{
		CollisionBatch.KickoffNewBatch(this, DeltaSeconds);
	}

	PostProcessParticles();

	SpawnExecContext.PostTick();
	UpdateExecContext.PostTick();
	for (FNiagaraScriptExecutionContext& EventContext : EventExecContexts)
	{
		EventContext.PostTick();
	}

	CPUTimeMS = TickTime.GetElapsedMilliseconds();

	INC_DWORD_STAT_BY(STAT_NiagaraNumParticles, Data.GetNumInstances());
}


/** Calculate total number of spawned particles from events; these all come from event handler script with the SpawnedParticles execution mode
 *  We get the counts ahead of event processing time so we only have to allocate new particles once
 *  TODO: augment for multiple spawning event scripts
 */
uint32 FNiagaraEmitterInstance::CalculateEventSpawnCount(const FNiagaraEventScriptProperties &EventHandlerProps, TArray<int32> &EventSpawnCounts, FNiagaraDataSet *EventSet)
{
	uint32 EventSpawnTotal = 0;
	int32 NumEventsToProcess = 0;

	if (EventSet)
	{
		NumEventsToProcess = EventSet->GetPrevNumInstances();
		if(EventHandlerProps.MaxEventsPerFrame > 0)
		{
			NumEventsToProcess = FMath::Min<int32>(EventSet->GetPrevNumInstances(), EventHandlerProps.MaxEventsPerFrame);
		}

		for (int32 i = 0; i < NumEventsToProcess; i++)
		{
			if (ExecutionState == ENiagaraExecutionState::Active)
			{
				EventSpawnCounts.Add(EventHandlerProps.SpawnNumber);
				EventSpawnTotal += EventHandlerProps.SpawnNumber;
			}
		}
	}

	return EventSpawnTotal;
}

void FNiagaraEmitterInstance::SetExecutionState(ENiagaraExecutionState InState)
{
	if (InState != ExecutionState)
	{
		const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Emitter \"%s\" change state: %s to %s"), *GetEmitterHandle().GetName().ToString(), *EnumPtr->GetNameStringByValue((int64)ExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState));
	}

	if (InState == ENiagaraExecutionState::Active && ExecutionState == ENiagaraExecutionState::Inactive)
	{
		UE_LOG(LogNiagara, Log, TEXT("Emitter \"%s\" change state N O O O O O "), *GetEmitterHandle().GetName().ToString());
	}
	ExecutionState = InState;
}


#if WITH_EDITORONLY_DATA

bool FNiagaraEmitterInstance::CheckAttributesForRenderer(int32 Index)
{
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	if (Index > EmitterRenderer.Num())
	{
		return false;
	}

	bool bOk = true;
	if (EmitterRenderer[Index])
	{
		const TArray<FNiagaraVariable>& RequiredAttrs = EmitterRenderer[Index]->GetRequiredAttributes();

		for (FNiagaraVariable Attr : RequiredAttrs)
		{
			// TODO .. should we always be namespaced?
			FString AttrName = Attr.GetName().ToString();
			if (AttrName.RemoveFromStart(TEXT("Particles.")))
			{
				Attr.SetName(*AttrName);
			}

			if (!Data.HasVariable(Attr))
			{
				bOk = false;
				UE_LOG(LogNiagara, Error, TEXT("Cannot render %s because it does not define attribute %s %s."), *EmitterHandle.GetName().ToString(), *Attr.GetType().GetNameText().ToString() , *Attr.GetName().ToString());
			}
		}

		EmitterRenderer[Index]->SetEnabled(bOk);
	}
	return bOk;
}

#endif

/** Replace the current System renderer with a new one of Type.
Don't forget to call RenderModuleUpdate on the SceneProxy after calling this! 
 */
void FNiagaraEmitterInstance::UpdateEmitterRenderer(ERHIFeatureLevel::Type FeatureLevel, TArray<NiagaraRenderer*>& ToBeAddedList, TArray<NiagaraRenderer*>& ToBeRemovedList)
{
	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	const UNiagaraEmitter* EmitterProperties = EmitterHandle.GetInstance();

	if (EmitterProperties != nullptr)
	{
		// Add all the old to be purged..
		for (int32 SubIdx = 0; SubIdx < EmitterRenderer.Num(); SubIdx++)
		{
			if (EmitterRenderer[SubIdx] != nullptr)
			{
				ToBeRemovedList.Add(EmitterRenderer[SubIdx]);
				EmitterRenderer[SubIdx] = nullptr;
			}
		}

		if (bIsEnabled && !bError)
		{
			EmitterRenderer.Empty();
			EmitterRenderer.AddZeroed(EmitterProperties->RendererProperties.Num());
			for (int32 SubIdx = 0; SubIdx < EmitterProperties->RendererProperties.Num(); SubIdx++)
			{
				UMaterialInterface *Material = nullptr;

				TArray<UMaterialInterface*> UsedMats;
				if (EmitterProperties->RendererProperties[SubIdx] != nullptr)
				{
					EmitterProperties->RendererProperties[SubIdx]->GetUsedMaterials(UsedMats);
					if (UsedMats.Num() != 0)
					{
						Material = UsedMats[0];
					}
				}

				if (Material == nullptr)
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				if (EmitterProperties->RendererProperties[SubIdx] != nullptr)
				{
					EmitterRenderer[SubIdx] = EmitterProperties->RendererProperties[SubIdx]->CreateEmitterRenderer(FeatureLevel);
					EmitterRenderer[SubIdx]->SetMaterial(Material, FeatureLevel);
					EmitterRenderer[SubIdx]->SetLocalSpace(EmitterProperties->bLocalSpace);
					ToBeAddedList.Add(EmitterRenderer[SubIdx]);

					//UE_LOG(LogNiagara, Warning, TEXT("CreateRenderer %p"), EmitterRenderer);
#if WITH_EDITORONLY_DATA
					CheckAttributesForRenderer(SubIdx);
#endif
				}
				else
				{
					EmitterRenderer[SubIdx] = nullptr;
				}
			}
		}
	}
}
