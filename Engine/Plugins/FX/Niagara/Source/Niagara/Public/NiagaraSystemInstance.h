// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "NiagaraParameterBindingInstance.h"
#include "NiagaraDataInterfaceBindingInstance.h"
#include "UniquePtr.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"

class FNiagaraWorldManager;
class UNiagaraComponent;

class NIAGARA_API FNiagaraSystemInstance 
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnInitialized);
	
#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnReset);
	DECLARE_MULTICAST_DELEGATE(FOnDestroyed);
#endif

public:
	/** Creates a new niagara System instance with the supplied component. */
	explicit FNiagaraSystemInstance(UNiagaraComponent* InComponent);

	/** Cleanup*/
	virtual ~FNiagaraSystemInstance();

	/** Initializes this System instance to simulate the supplied System. */
	void Init(class FNiagaraSystemSimulation* InSystemSimulation, bool bForceReset = false, bool bInForceSolo=false);

	void Activate(bool bReset = false);
	void Deactivate(bool bImmediate = false);

	//void RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance);
	void BindParameters();
	void UnbindParameters();

	FORCEINLINE FNiagaraParameterStore& GetInstanceParameters() { return InstanceParameters; }
	
	FNiagaraWorldManager* GetWorldManager()const;

	/** Defines modes for resetting the System instance. */
	enum class EResetMode
	{
		/** Defers resetting the System instance and simulations until the next tick. */
		DeferredReset,
		/** Resets the System instance and simulations immediately. */
		ImmediateReset,
		/** same as above, but reinitializes instead of fast resetting */
		DeferredReInit,
		ImmediateReInit
	};
	
	/** Requests the the simulation be reset on the next tick. */
	void Reset(EResetMode Mode);

	void ComponentTick(float DeltaSeconds);
	void PreSimulateTick(float DeltaSeconds);
	void PostSimulateTick(float DeltaSeconds);
	void HandleResets();

	void Enable();
	void Disable();
	
	ENiagaraExecutionState GetExecutionState() { return ExecutionState; }
	void SetExecutionState(ENiagaraExecutionState InState);

	/** Gets the simulation for the supplied emitter handle. */
	TSharedPtr<FNiagaraEmitterInstance> GetSimulationForHandle(const FNiagaraEmitterHandle& EmitterHandle);

	UNiagaraSystem* GetSystem()const;
	FORCEINLINE UNiagaraComponent *GetComponent() { return Component; }
	FORCEINLINE TArray<TSharedRef<FNiagaraEmitterInstance> > &GetEmitters()	{ return Emitters; }
	FORCEINLINE FBox &GetSystemBounds()	{ return SystemBounds;  }

	FORCEINLINE bool IsSolo()const { return bSolo; }

	//TEMPORARY. We wont have a single set of parameters when we're executing system scripts.
	//System params will be pulled in from a data set.
	FORCEINLINE FNiagaraParameterStore& GetParameters() { return InstanceParameters; }

	/** Gets a data set either from another emitter or one owned by the System itself. */
	FNiagaraDataSet* GetDataSet(FNiagaraDataSetID SetID, FName EmitterName = NAME_None);

	/** Gets a multicast delegate which is called whenever this instance is initialized with an System asset. */
	FOnInitialized& OnInitialized();

#if WITH_EDITOR
	/** Gets a multicast delegate which is called whenever this instance is reset due to external changes in the source System asset. */
	FOnReset& OnReset();

	FOnDestroyed& OnDestroyed();
#endif

	FName GetIDName() { return IDName; }

	/** Queue up the data sources to have PrepareForSimulation called on them next tick.*/
	void ReinitializeDataInterfaces();

	/** Returns the instance data for a particular interface for this System. */
	FORCEINLINE void* FindDataInterfaceInstanceData(UNiagaraDataInterface* Interface) 
	{
		if (int32* InstDataOffset = DataInterfaceInstanceDataOffsets.Find(Interface))
		{
			return &DataInterfaceInstanceData[*InstDataOffset];
		}
		return nullptr;
	}

	void DestroyDataInterfaceInstanceData();

	bool UsesEmitter(const UNiagaraEmitter* Emitter)const;
	bool UsesScript(const UNiagaraScript* Script)const;
	//bool UsesDataInterface(UNiagaraDataInterface* Interface);
	bool UsesCollection(const UNiagaraParameterCollection* Collection)const;

	FORCEINLINE bool IsPendingSpawn()const { return bPendingSpawn; }
	FORCEINLINE void SetPendingSpawn(bool bInValue) { bPendingSpawn = bInValue; }

	FORCEINLINE float GetAge()const { return Age; }
	
	FORCEINLINE FNiagaraSystemSimulation* GetSystemSimulation()const
	{
		check(SystemSimulation);
		return SystemSimulation; 
	}

	/** Index of this instance in the system simulation. */
	int32 SystemInstanceIndex;

private:
	/** Builds the emitter simulations. */
	void InitEmitters();

	/** Resets the System, emitter simulations, and renderers to initial conditions. */
	void ReInitInternal();
	/** Resets for restart, assumes no change in emitter setup */
	void ResetInternal();

	/** Updates the renders for the simulations. Gathers both the EmitterRenderers that were previously set as well as the ones that we  create within.*/
	void UpdateRenderModules(ERHIFeatureLevel::Type InFeatureLevel, TArray<NiagaraRenderer*>& OutNewRenderers, TArray<NiagaraRenderer*>& OutOldRenderers);

	/** Updates the scene proxy for the System with the specified EmitterRenderer array. Note that this is pushed onto the rendering thread behind the scenes.*/
	void UpdateProxy(TArray<NiagaraRenderer*>& InRenderers);

	/** Call PrepareForSImulation on each data source from the simulations and determine which need per-tick updates.*/
	void InitDataInterfaces();
	
	/** Perform per-tick updates on data interfaces that need it.*/
	void TickDataInterfaces(float DeltaSeconds);

	void TickInstanceParameters(float DeltaSeconds);

	void BindParameterCollections(FNiagaraScriptExecutionContext& ExecContext);
	
	UNiagaraComponent* Component;
	class FNiagaraSystemSimulation *SystemSimulation;
	FBox SystemBounds;

	/** The age of the System instance. */
	float Age;

	TMap<FNiagaraDataSetID, FNiagaraDataSet> ExternalEvents;

	TArray< TSharedRef<FNiagaraEmitterInstance> > Emitters;

	FOnInitialized OnInitializedDelegate;

#if WITH_EDITOR
	FOnReset OnResetDelegate;
	FOnDestroyed OnDestroyedDelegate;
#endif

	FGuid ID;
	FName IDName;
	
	/** Per instance data for any data interfaces requiring it. */
	TArray<uint8> DataInterfaceInstanceData;

	/** Map of data interfaces to their instance data. */
	TMap<TWeakObjectPtr<UNiagaraDataInterface>, int32> DataInterfaceInstanceDataOffsets;

	/** Per system instance parameters. These can be fed by the component and are placed into a dataset for execution for the system scripts. */
	FNiagaraParameterStore InstanceParameters;
	
	FNiagaraParameterDirectBinding<FVector> OwnerPositionParam;
	FNiagaraParameterDirectBinding<FVector> OwnerVelocityParam;
	FNiagaraParameterDirectBinding<FVector> OwnerXAxisParam;
	FNiagaraParameterDirectBinding<FVector> OwnerYAxisParam;
	FNiagaraParameterDirectBinding<FVector> OwnerZAxisParam;
	FNiagaraParameterDirectBinding<FMatrix> OwnerTransformParam;
	FNiagaraParameterDirectBinding<FMatrix> OwnerInverseParam;
	FNiagaraParameterDirectBinding<FMatrix> OwnerTransposeParam;
	FNiagaraParameterDirectBinding<FMatrix> OwnerInverseTransposeParam;
	FNiagaraParameterDirectBinding<float> OwnerDeltaSecondsParam;
	FNiagaraParameterDirectBinding<float> OwnerInverseDeltaSecondsParam;
	FNiagaraParameterDirectBinding<float> OwnerMinDistanceToCameraParam;
	FNiagaraParameterDirectBinding<int32> SystemNumEmittersParam;
	FNiagaraParameterDirectBinding<int32> SystemNumEmittersAliveParam;

	TArray<FNiagaraParameterDirectBinding<int32>> ParameterNumParticleBindings;

	/** Indicates whether this instance must update itself rather than being batched up as most instances are. */
	uint32 bSolo : 1;
	uint32 bForceSolo : 1;

	uint32 bPendingSpawn : 1;

	uint32 bActive : 1;

	/** Flag to ensure the System instance is only reset once per frame. */
	uint32 bResetPending : 1;
	uint32 bReInitPending : 1;

	/** Disable ticking and rendering if there was some serious error. */
	uint32 bError : 1;

	/** Notifier that data interfaces need reinitialization next tick.*/
	uint32 bDataInterfacesNeedInit : 1;

	/* System tick state */
	ENiagaraExecutionState ExecutionState;
};
