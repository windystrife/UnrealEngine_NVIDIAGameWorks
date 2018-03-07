// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraParameterStore.h"
#include "NiagaraSystemInstance.h"

#include "NiagaraComponent.generated.h"

class FMeshElementCollector;
class NiagaraRenderer;
class UNiagaraSystem;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;

/**
* UNiagaraComponent is the primitive component for a Niagara System.
* @see ANiagaraActor
* @see UNiagaraSystem
*/
UCLASS(ClassGroup = (Rendering, Common), hidecategories = (Object, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent))
class NIAGARA_API UNiagaraComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
		
	/** Defines modes for updating the component's age. */
	enum class EAgeUpdateMode
	{
		/** Update the age using the delta time supplied to the tick function. */
		TickDeltaTime,
		/** Update the age by seeking to the DesiredAge. */
		DesiredAge
	};

#if WITH_EDITORONLY_DATA
	DECLARE_MULTICAST_DELEGATE(FOnSystemInstanceChanged);
#endif

private:
	UPROPERTY(EditAnywhere, Category="Niagara", meta = (DisplayName = "Niagara System Asset"))
	UNiagaraSystem* Asset;

	/** Initial values for parameter overrides. 
	TODO: This should be a minimal set of explicitly override parameters similar to how parameter collection instances override their collections store. 
	Should expose anything in the "User" namespace.
	*/
 	UPROPERTY(EditAnywhere, Category = Parameters)
 	FNiagaraParameterStore InitialParameters;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TMap<FName, bool> EditorOverridesValue;

	FOnSystemInstanceChanged OnSystemInstanceChangedDelegate;
#endif

	/**
	When true, this component's system will be force to update via a slower "solo" path rather than the more optimal batched path with other instances of the same system.
	*/
	UPROPERTY(EditAnywhere, Category = Parameters)
	uint32 bForceSolo : 1;

	TUniquePtr<FNiagaraSystemInstance> SystemInstance;

	/** Defines the mode use when updating the System age. */
	EAgeUpdateMode AgeUpdateMode;
	
	/** The desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	float DesiredAge;

	/** The delta time used when seeking to the desired age.  This is only relevant when using the DesiredAge age update mode. */
	float SeekDelta;

	//~ Begin UActorComponent Interface.
protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	virtual void BeginDestroy() override;
public:

	virtual void Activate(bool bReset = false)override;
	virtual void Deactivate()override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual const UObject* AdditionalStatObject() const override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~ End UActorComponent Interface.

	//~ Begin UPrimitiveComponent Interface
	virtual int32 GetNumMaterials() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent Interface

	class FNiagaraSystemSimulation* GetSystemSimulation();

	void DestroyInstance();

	void SetAsset(UNiagaraSystem* InAsset);
	UNiagaraSystem* GetAsset() const { return Asset; }

	void SetForceSolo(bool bInForceSolo) { bForceSolo = bInForceSolo; }
	bool GetForceSolo()const { return bForceSolo; }

	EAgeUpdateMode GetAgeUpdateMode() const;

	/** Sets the age update mode for the System instance. */
	void SetAgeUpdateMode(EAgeUpdateMode InAgeUpdateMode);

	/** Gets the desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	float GetDesiredAge() const;

	/** Sets the desired age of the System instance.  This is only relevant when using the DesiredAge age update mode. */
	void SetDesiredAge(float InDesiredAge);

	/** Gets the delta value which is used when seeking from the current age, to the desired age.  This is only relevant
	when using the DesiredAge age update mode. */
	float GetSeekDelta() const;

	/** Sets the delta value which is used when seeking from the current age, to the desired age.  This is only relevant
	when using the DesiredAge age update mode. */
	void SetSeekDelta(float InSeekDelta);

	FNiagaraSystemInstance* GetSystemInstance() const;
	
	void OnSystemDisabled();

	/** Returns true if this component forces it's instances to run in "Solo" mode. A sub optimal path required in some situations. */
	bool ForcesSolo()const;

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector4)"))
	void SetNiagaraVariableVec4(const FString& InVariableName, const FVector4& InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector3)"))
	void SetNiagaraVariableVec3(const FString& InVariableName, FVector InValue);

	/** Sets a Niagara Vector3 parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Vector2)"))
	void SetNiagaraVariableVec2(const FString& InVariableName, FVector2D InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Float)"))
	void SetNiagaraVariableFloat(const FString& InVariableName, float InValue);

	/** Sets a Niagara float parameter by name, overriding locally if necessary.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Niagara Variable (Bool)"))
	void SetNiagaraVariableBool(const FString& InVariableName, bool InValue);

	/** Debug accessors for getting positions in blueprints. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Positions"))
	TArray<FVector> GetNiagaraParticlePositions_DebugOnly(const FString& InEmitterName);
	
	/** Debug accessors for getting a float attribute array in blueprints. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Float Attrib"))
	TArray<float> GetNiagaraParticleValues_DebugOnly(const FString& InEmitterName, const FString& InValueName);
	
	/** Debug accessors for getting a FVector attribute array in blueprints. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Niagara Emitter Vec3 Attrib"))
	TArray<FVector> GetNiagaraParticleValueVec3_DebugOnly(const FString& InEmitterName, const FString& InValueName);
	/** Resets the System to it's initial pre-simulated state. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Reset System"))
	void ResetSystem();

	/** Called on when an external object wishes to force this System to reinitialize itself from the System data.*/
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Reinitialize System"))
	void ReinitializeSystem();

	/** Gets whether or not rendering is enabled for this component. */
	bool GetRenderingEnabled() const;

	/** Sets whether or not rendering is enabled for this component. */
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Rendering Enabled"))
	void SetRenderingEnabled(bool bInRenderingEnabled);

	//~ Begin UObject Interface.
	virtual void PostLoad();
#if WITH_EDITOR
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Compare local overrides with the source System. Remove any that have mismatched types or no longer exist on the System. Returns whether or not any changes occurred.*/
	virtual bool SynchronizeWithSourceSystem();

	bool IsParameterValueOverriddenLocally(const FName& InParamName);
	void SetParameterValueOverriddenLocally(const FName& InParamName, bool bInOverridden);
	
	FOnSystemInstanceChanged& OnSystemInstanceChanged() { return OnSystemInstanceChangedDelegate; }
#endif

	FNiagaraParameterStore& GetInitialParameters() { return InitialParameters; }

	//~ End UObject Interface.
	
	UPROPERTY()
	bool bRenderingEnabled;
};






/**
* Scene proxy for drawing niagara particle simulations.
*/
class FNiagaraSceneProxy : public FPrimitiveSceneProxy
{
public:

	FNiagaraSceneProxy(const UNiagaraComponent* InComponent);
	~FNiagaraSceneProxy();

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(struct FNiagaraDynamicDataBase* NewDynamicData);
	TArray<class NiagaraRenderer*>& GetEmitterRenderers() { return EmitterRenderers; }
	void AddEmitterRenderer(NiagaraRenderer* Renderer);
	void UpdateEmitterRenderers(TArray<NiagaraRenderer*>& InRenderers);

	/** Gets whether or not this scene proxy should be rendered. */
	bool GetRenderingEnabled() const;

	/** Sets whether or not this scene proxy should be rendered. */
	void SetRenderingEnabled(bool bInRenderingEnabled);

private:
	void ReleaseRenderThreadResources();

	//~ Begin FPrimitiveSceneProxy Interface.
	virtual void CreateRenderThreadResources() override;

	//virtual void OnActorPositionChanged() override;
	virtual void OnTransformChanged() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	/*
	virtual bool CanBeOccluded() const override
	{
	return !MaterialRelevance.bDisableDepthTest;
	}
	*/

	/** Callback from the renderer to gather simple lights that this proxy wants renderered. */
	virtual void GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const override;


	virtual uint32 GetMemoryFootprint() const override;

	uint32 GetAllocatedSize() const;

private:
	//class NiagaraRenderer* EmitterRenderer;
	TArray<class NiagaraRenderer*>EmitterRenderers;

	bool bRenderingEnabled;
};
