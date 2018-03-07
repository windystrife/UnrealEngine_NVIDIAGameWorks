// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstance.h: Niagara emitter simulation class
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEvents.generated.h"

struct FNiagaraEventReceiverProperties;

#define NIAGARA_BUILTIN_EVENTNAME_COLLISION FName("NiagaraSystem_Collision")
#define NIAGARA_BUILTIN_EVENTNAME_SPAWN FName("Spawn")
#define NIAGARA_BUILTIN_EVENTNAME_DEATH FName("Death")

/**
 *	Type struct for collision event payloads; collision event data set is based on this
 *  TODO: figure out how we can pipe attributes from the colliding particle in here
 */
USTRUCT()
struct FNiagaraCollisionEventPayload
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector CollisionPos;
	UPROPERTY()
	FVector CollisionNormal;
	UPROPERTY()
	FVector CollisionVelocity;
	UPROPERTY()
	int32 ParticleIndex;
	UPROPERTY()
	int32 PhysicalMaterialIndex;
};


struct FNiagaraEmitterInstance;

/**
Base class for actions that an event receiver will perform at the emitter level.
*/
UCLASS(abstract)
class UNiagaraEventReceiverEmitterAction : public UObject
{
	GENERATED_BODY()
public:
	virtual void PerformAction(FNiagaraEmitterInstance& OwningSim, const FNiagaraEventReceiverProperties& OwningEventReceiver){}
};

UCLASS(EditInlineNew)
class UNiagaraEventReceiverEmitterAction_SpawnParticles : public UNiagaraEventReceiverEmitterAction
{
	GENERATED_BODY()

public:

	/** Number of particles to spawn per event received. */
	UPROPERTY(EditAnywhere, Category = "Spawn")
	uint32 NumParticles;

	virtual void PerformAction(FNiagaraEmitterInstance& OwningSim, const FNiagaraEventReceiverProperties& OwningEventReceiver)override;
};



typedef TMap<FName, FNiagaraDataSet*> PerEmitterEventDataSetMap;
typedef TMap<FName, PerEmitterEventDataSetMap> PerSystemInstanceDataSetMap;

class FNiagaraEventDataSetMgr
{
public:
	FNiagaraEventDataSetMgr()
	{
	}

	// call this to allocate a data set for a specific emitter and event
	// if the data set already exists, returns the existing one
	static FNiagaraDataSet *CreateEventDataSet(FName OwnerSystemInstanceName, FName EmitterName, FName EventName)
	{
		PerSystemInstanceDataSetMap *SystemInstanceMap = EmitterEventDataSets.Find(OwnerSystemInstanceName);
		if (!SystemInstanceMap)
		{
			SystemInstanceMap = &EmitterEventDataSets.Add(OwnerSystemInstanceName);
		}

		// find the emitter's map
		PerEmitterEventDataSetMap *Map = SystemInstanceMap->Find(EmitterName);
		if (!Map)
		{
			Map = &SystemInstanceMap->Add(EmitterName);
		}

		// TODO: find a better way of multiple events trying to write to the same data set; 
		// for example, if two analytical collision primitives want to send collision events, they need to push to the same data set

		FNiagaraDataSet **DataSetPtr = Map->Find(EventName);
		FNiagaraDataSet *OutSet = nullptr;
		if (DataSetPtr == nullptr)
		{
			OutSet = new FNiagaraDataSet();
			Map->Add(EventName) = OutSet;
		}
		else
		{
			OutSet = *DataSetPtr;
		}
		return OutSet;
	}

	// deletes and cleans up all event data sets for an emitter
	//  should be called when the emitter is destroyed
	static void Reset(FName OwnerSystemInstanceName, FName EmitterName)
	{
		PerSystemInstanceDataSetMap *SystemInstanceMap = EmitterEventDataSets.Find(OwnerSystemInstanceName);
		if (SystemInstanceMap)
		{
			PerEmitterEventDataSetMap *Map = SystemInstanceMap->Find(EmitterName);
			if (Map)
			{
				for (TPair<FName, FNiagaraDataSet *> Pair : *Map)
				{
					delete Pair.Value;
				}
				Map->Empty();
				SystemInstanceMap->Remove(EmitterName);
			}
			if (SystemInstanceMap->Num() == 0)
			{
				EmitterEventDataSets.Remove(OwnerSystemInstanceName);
			}
		}
	}

	/*
	static void AddEventSet(FName EmitterName, FName EventName, const FNiagaraDataSet *DataSet)
	{
		// find the emitter's map
		PerEmitterEventDataSetMap *Map = EmitterEventDataSets.Find(EmitterName);
		if(!Map)
		{
			Map = &EmitterEventDataSets.Add(EmitterName);
		}

		// TODO: find a better way of multiple events trying to write to the same data set; 
		// for example, if two analytical collision primitives want to send collision events, they need to push to the same data set
		ensure(Map->Find(EventName) == nullptr);

		if (Map->Find(EventName) == nullptr)
		{
			Map->Add(EventName) = DataSet;
		}
	}
	*/

	static PerEmitterEventDataSetMap * GetEmitterMap(FName OwnerSystemInstanceName, FName EmitterName)
	{
		TMap<FName, PerEmitterEventDataSetMap> *SystemMap = EmitterEventDataSets.Find(OwnerSystemInstanceName);
		if (SystemMap)
		{
			return SystemMap->Find(EmitterName);
		}

		return nullptr;
	}

	static FNiagaraDataSet * GetEventDataSet(FName OwnerSystemInstanceName, FName EmitterName, FName EventName)
	{
		PerEmitterEventDataSetMap *SetMap = GetEmitterMap(OwnerSystemInstanceName, EmitterName);
		if (!SetMap)
		{
			return nullptr;
		}

		FNiagaraDataSet **FoundSet = SetMap->Find(EventName);
		if (!FoundSet)
		{
			return nullptr;
		}

		return *FoundSet;
	}

private:
	static TMap<FName, TMap<FName, PerEmitterEventDataSetMap>> EmitterEventDataSets;
};