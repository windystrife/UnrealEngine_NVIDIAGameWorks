// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "NiagaraScript.h"
#include "NiagaraCollision.h"
#include "NiagaraEmitter.generated.h"

class UMaterial;
class UNiagaraEmitter;
class UNiagaraEventReceiverEmitterAction;

//TODO: Event action that spawns other whole Systems?
//One that calls a BP exposed delegate?

USTRUCT()
struct FNiagaraEventReceiverProperties
{
	GENERATED_BODY()

	FNiagaraEventReceiverProperties()
	: Name(NAME_None)
	, SourceEventGenerator(NAME_None)
	, SourceEmitter(NAME_None)
	{

	}

	FNiagaraEventReceiverProperties(FName InName, FName InEventGenerator, FName InSourceEmitter)
		: Name(InName)
		, SourceEventGenerator(InEventGenerator)
		, SourceEmitter(InSourceEmitter)
	{

	}

	/** The name of this receiver. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName Name;

	/** The name of the EventGenerator to bind to. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName SourceEventGenerator;

	/** The name of the emitter from which the Event Generator is taken. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	FName SourceEmitter;

	UPROPERTY(EditAnywhere, Instanced, Category = "Event Receiver")
	TArray<UNiagaraEventReceiverEmitterAction*> EmitterActions;
};

USTRUCT()
struct FNiagaraEventGeneratorProperties
{
	GENERATED_BODY()

	FNiagaraEventGeneratorProperties()
	: MaxEventsPerFrame(64)
	{

	}

	FNiagaraEventGeneratorProperties(FNiagaraDataSetProperties &Props, FName InEventGenerator, FName InSourceEmitter)
		: ID(Props.ID.Name)
		, SourceEmitter(InSourceEmitter)
		, SetProps(Props)		
	{

	}

	/** Max Number of Events that can be generated per frame. */
	UPROPERTY(EditAnywhere, Category = "Event Receiver")
	int32 MaxEventsPerFrame; //TODO - More complex allocation so that we can grow dynamically if more space is needed ?

	FName ID;
	FName SourceEmitter;

	UPROPERTY()
	FNiagaraDataSetProperties SetProps;
};


UENUM()
enum class EScriptExecutionMode : uint8
{
	/** The event script is run on every existing particle in the emitter.*/
	EveryParticle = 0,
	/** The event script is run only on the particles that were spawned in response to the current event in the emitter.*/
	SpawnedParticles,
	/** The event script is run only on the particle whose int32 ParticleIndex is specified in the event payload.*/
	SingleParticle
};

UENUM()
enum class EScriptCompileIndices : uint8
{
	SpawnScript = 0,
	UpdateScript,
	EventScript
};

USTRUCT()
struct FNiagaraEmitterScriptProperties
{
	FNiagaraEmitterScriptProperties() : Script(nullptr)
	{

	}

	GENERATED_BODY()
	
 	UPROPERTY()
 	UNiagaraScript *Script;

	UPROPERTY()
	TArray<FNiagaraEventReceiverProperties> EventReceivers;

	UPROPERTY()
	TArray<FNiagaraEventGeneratorProperties> EventGenerators;

	NIAGARA_API void InitDataSetAccess();
};

USTRUCT()
struct FNiagaraEventScriptProperties : public FNiagaraEmitterScriptProperties
{
	GENERATED_BODY()
			
	FNiagaraEventScriptProperties() : FNiagaraEmitterScriptProperties()
	{
		ExecutionMode = EScriptExecutionMode::EveryParticle;
		SpawnNumber = 0;
		MaxEventsPerFrame = 0;
	}
	
	/** Controls which particles have the event script run on them.*/
	UPROPERTY(EditAnywhere, Category="Event Handler Options")
	EScriptExecutionMode ExecutionMode;

	/** Controls whether or not particles are spawned as a result of handling the event. Only valid for EScriptExecutionMode::SpawnedParticles*/
	UPROPERTY(EditAnywhere, Category="Event Handler Options")
	uint32 SpawnNumber;

	/** Controls how many events are consumed by this event handler. If there are more events generated than this value, they will be ignored.*/
	UPROPERTY(EditAnywhere, Category="Event Handler Options")
	uint32 MaxEventsPerFrame;

	/** Id of the Emitter Handle that generated the event. If all zeroes, the event generator is assumed to be this emitter.*/
	UPROPERTY(EditAnywhere, Category="Event Handler Options")
	FGuid SourceEmitterID;

	/** The name of the event generated. This will be "Collision" for collision events and the Event Name field on the DataSetWrite node in the module graph for others.*/
	UPROPERTY(EditAnywhere, Category="Event Handler Options")
	FName SourceEventName;

};

/** Represents timed burst of particles in an emitter. */
USTRUCT()
struct FNiagaraEmitterBurst
{
	GENERATED_BODY()

	/** The base time of the burst absolute emitter time. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	float Time;

	/** 
	 * A range around the base time which can be used to randomize burst timing.  The resulting range is 
	 * from Time - (TimeRange / 2) to Time + (TimeRange / 2).
	 */
	UPROPERTY(EditAnywhere, Category = "Burst")
	float TimeRange;

	/** The minimum number of particles to spawn. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMinimum;

	/** The maximum number of particles to spawn. */
	UPROPERTY(EditAnywhere, Category = "Burst")
	uint32 SpawnMaximum;
};

/** 
 *	UNiagaraEmitter stores the attributes of an FNiagaraEmitterInstance
 *	that need to be serialized and are used for its initialization 
 */
UCLASS(MinimalAPI)
class UNiagaraEmitter : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	//Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	void Serialize(FArchive& Ar)override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//End UObject Interface

	UPROPERTY(EditAnywhere, Category = "Emitter")
	bool bLocalSpace;

	//UPROPERTY(EditAnywhere, Category = "Emitter")
	//float StartTime;
	//UPROPERTY(EditAnywhere, Category = "Emitter")
	//float EndTime;
	//UPROPERTY(EditAnywhere, Category = "Emitter")
	//int32 NumLoops;
	UPROPERTY(EditAnywhere, Category = "Emitter")
	ENiagaraCollisionMode CollisionMode;
	
	UPROPERTY(EditAnywhere, Instanced, Category = "Render")
	TArray<class UNiagaraRendererProperties*> RendererProperties;

	UPROPERTY()
	FNiagaraEmitterScriptProperties UpdateScriptProps;

	UPROPERTY()
	FNiagaraEmitterScriptProperties SpawnScriptProps;

	UPROPERTY(EditAnywhere, Category="Event Handler")
	TArray<FNiagaraEventScriptProperties> EventHandlerScriptProps;

	/** When enabled, this will spawn using interpolated parameter values and perform a partial update at spawn time. This adds significant additional cost for spawning but will produce much smoother spawning for high spawn rates, erratic frame rates and fast moving emitters. */
	UPROPERTY(EditAnywhere, Category = "Emitter")
	uint32 bInterpolatedSpawning : 1;

	UPROPERTY(EditAnywhere, Category = "Emitter")
	ENiagaraSimTarget SimTarget;

#if WITH_EDITORONLY_DATA
	/** Adjusted every time that we compile this emitter. Lets us know that we might differ from any cached versions.*/
	UPROPERTY()
	FGuid ChangeId;

	/** 'Source' data/graphs for the scripts used by this emitter. */
	UPROPERTY()
	class UNiagaraScriptSourceBase*	GraphSource;

	void NIAGARA_API CompileScripts(TArray<ENiagaraScriptCompileStatus>& OutScriptStatuses, TArray<FString>& OutGraphLevelErrorMessages, TArray<FString>& PathNames, TArray<UNiagaraScript*>& Scripts);
	ENiagaraScriptCompileStatus NIAGARA_API CompileScript(EScriptCompileIndices InScriptToCompile, FString& OutGraphLevelErrorMessages, int32 InSubScriptIdx = 0);

	UNiagaraEmitter* MakeRecursiveDeepCopy(UObject* DestOuter) const;
	UNiagaraEmitter* MakeRecursiveDeepCopy(UObject* DestOuter, TMap<const UObject*, UObject*>& ExistingConversions) const;

	void NIAGARA_API GetScripts(TArray<UNiagaraScript*>& OutScripts);

	/** Data used by the editor to maintain UI state etc.. */
	UPROPERTY()
	UObject* EditorData;
#endif

	bool IsValid()const;

	bool UsesScript(const UNiagaraScript* Script)const;
	//bool UsesDataInterface(UNiagaraDataInterface* Interface);
	bool UsesCollection(const class UNiagaraParameterCollection* Collection)const;

	FString NIAGARA_API GetUniqueEmitterName()const;

	/** Converts an emitter paramter "Emitter.XXXX" into it's real parameter name. */
	FNiagaraVariable GetEmitterParameter(const FNiagaraVariable& EmitterVar)const;
};


