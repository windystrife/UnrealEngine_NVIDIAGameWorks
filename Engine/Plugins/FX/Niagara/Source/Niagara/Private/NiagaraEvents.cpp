// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEvents.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraStats.h"


DECLARE_DWORD_COUNTER_STAT(TEXT("Num Death Events"), STAT_NiagaraNumDeathEvents, STATGROUP_Niagara);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Spawn Events"), STAT_NiagaraNumSpawnEvents, STATGROUP_Niagara);


TMap<FName, TMap<FName, PerEmitterEventDataSetMap>> FNiagaraEventDataSetMgr::EmitterEventDataSets;


void UNiagaraEventReceiverEmitterAction_SpawnParticles::PerformAction(FNiagaraEmitterInstance& OwningSim, const FNiagaraEventReceiverProperties& OwningEventReceiver)
{
	//Find the generator set we're bound to and spawn NumParticles particles for every event it generated last frame.
// 	FNiagaraDataSet* GeneratorSet = OwningSim.GetParentSystemInstance()->GetDataSet(FNiagaraDataSetID(OwningEventReceiver.SourceEventGenerator, ENiagaraDataSetType::Event), OwningEventReceiver.SourceEmitter);
// 	if (GeneratorSet)
// 	{
// 		OwningSim.SpawnBurst(GeneratorSet->GetPrevNumInstances()*NumParticles);
// 	}
}
//////////////////////////////////////////////////////////////////////////
