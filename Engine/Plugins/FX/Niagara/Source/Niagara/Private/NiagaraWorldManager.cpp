// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraWorldManager.h"
#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraTypes.h"
#include "NiagaraEvents.h"
#include "NiagaraSettings.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystemInstance.h"
#include "Scalability.h"
#include "ConfigCacheIni.h"


FNiagaraWorldManager* FNiagaraWorldManager::Get(UWorld* World)
{
	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	return NiagaraModule.GetWorldManager(World);
}

void FNiagaraWorldManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ParameterCollections);
}

UNiagaraParameterCollectionInstance* FNiagaraWorldManager::GetParameterCollection(UNiagaraParameterCollection* Collection)
{
	UNiagaraParameterCollectionInstance** OverrideInst = ParameterCollections.Find(Collection);
	if (!OverrideInst)
	{
		OverrideInst = &ParameterCollections.Add(Collection);
		*OverrideInst = CastChecked<UNiagaraParameterCollectionInstance>(StaticDuplicateObject(Collection->GetDefaultInstance(), World));
	}

	check(OverrideInst && *OverrideInst);
	return *OverrideInst;
}

void FNiagaraWorldManager::SetParameterCollection(UNiagaraParameterCollectionInstance* NewInstance)
{
	check(NewInstance);
	if (NewInstance)
	{
		UNiagaraParameterCollection* Collection = NewInstance->GetParent();
		UNiagaraParameterCollectionInstance** OverrideInst = ParameterCollections.Find(Collection);
		if (!OverrideInst)
		{
			OverrideInst = &ParameterCollections.Add(Collection);
		}
		else
		{
			if (*OverrideInst && NewInstance)
			{
				//Need to transfer existing bindings from old instance to new one.
				FNiagaraParameterStore& ExistingStore = (*OverrideInst)->GetParameterStore();
				FNiagaraParameterStore& NewStore = NewInstance->GetParameterStore();

				ExistingStore.TransferBindings(NewStore);
			}
		}

		*OverrideInst = NewInstance;
	}
}

FNiagaraSystemSimulation* FNiagaraWorldManager::GetSystemSimulation(UNiagaraSystem* System)
{
	FNiagaraSystemSimulation* Sim = SystemSimulations.Find(System);
	if (Sim)
	{
		return Sim;
	}
	
	Sim = &SystemSimulations.Add(System);
	Sim->Init(System, World);
	return Sim;
}

void FNiagaraWorldManager::DestroySystemSimulation(UNiagaraSystem* System)
{
	FNiagaraSystemSimulation* Sim = SystemSimulations.Find(System);
	if (Sim)
	{
		Sim->Destroy();
		SystemSimulations.Remove(System);
	}	
}

void FNiagaraWorldManager::Tick(float DeltaSeconds)
{
/*
Some experimental tinkering with links to effects quality.
Going off this idea tbh

	int32 CurrentEffectsQuality = Scalability::GetEffectsQualityDirect(true);
	if (CachedEffectsQuality != CurrentEffectsQuality)
	{
		CachedEffectsQuality = CurrentEffectsQuality;
		FString SectionName = FString::Printf(TEXT("%s@%d"), TEXT("EffectsQuality"), CurrentEffectsQuality);
		if (FConfigFile* File = GConfig->FindConfigFileWithBaseName(TEXT("Niagara")))
		{
			FString ScalabilityCollectionString;			
			if (File->GetString(*SectionName, TEXT("ScalabilityCollection"), ScalabilityCollectionString))
			{
				//NewLandscape_Material = LoadObject<UMaterialInterface>(NULL, *NewLandsfgcapeMaterialName, NULL, LOAD_NoWarn);
				UNiagaraParameterCollectionInstance* ScalabilityCollection = LoadObject<UNiagaraParameterCollectionInstance>(NULL, *ScalabilityCollectionString, NULL, LOAD_NoWarn);
				if (ScalabilityCollection)
				{
					SetParameterCollection(ScalabilityCollection);
				}
			}
		}
	}
*/

	//Tick our collections to push any changes to bound stores.
	for (TPair<UNiagaraParameterCollection*, UNiagaraParameterCollectionInstance*> CollectionInstPair : ParameterCollections)
	{
		check(CollectionInstPair.Value);
		CollectionInstPair.Value->Tick();
	}

	//Now tick all system instances. 
	for (TPair<UNiagaraSystem*, FNiagaraSystemSimulation>& SystemSim : SystemSimulations)
	{
		SystemSim.Value.Tick(DeltaSeconds);
	}
}