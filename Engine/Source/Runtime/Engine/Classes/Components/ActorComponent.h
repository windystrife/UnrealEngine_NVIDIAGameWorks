// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/ScriptMacros.h"
#include "EdGraph/EdGraphPin.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "UObjectAnnotation.h"
#include "ActorComponent.generated.h"

class AActor;
class UActorComponent;
class UAssetUserData;
class ULevel;

#if WITH_EDITOR
/** Annotation for component selection.  This must be in engine isntead of editor for ::IsSelected to work */
extern ENGINE_API FUObjectAnnotationSparseBool GSelectedComponentAnnotation;
#endif

UENUM()
enum class EComponentCreationMethod : uint8
{
	/** A component that is part of a native class. */
	Native,
	/** A component that is created from a template defined in the Components section of the Blueprint. */
	SimpleConstructionScript,	
	/**A dynamically created component, either from the UserConstructionScript or from a Add Component node in a Blueprint event graph. */
	UserConstructionScript,
	/** A component added to a single Actor instance via the Component section of the Actor's details panel. */
	Instance,
};


/** Information about how to update transform*/
enum class EUpdateTransformFlags : int32
{
	None = 0x0,
	SkipPhysicsUpdate = 0x1,	//Don't update the underlying physics
	PropagateFromParent = 0x2	//The update is coming as a result of the parent updating (i.e. not called directly)

};

CONSTEXPR inline EUpdateTransformFlags operator|(EUpdateTransformFlags Left, EUpdateTransformFlags Right)
{
	return static_cast<EUpdateTransformFlags> ( static_cast<int32> (Left) | static_cast<int32> (Right) );
}

CONSTEXPR inline EUpdateTransformFlags operator&(EUpdateTransformFlags Left, EUpdateTransformFlags Right)
{
	return static_cast<EUpdateTransformFlags> (static_cast<int32> (Left) & static_cast<int32> (Right));
}

CONSTEXPR inline bool operator !(EUpdateTransformFlags Value)
{
	return Value == EUpdateTransformFlags::None;
}

CONSTEXPR inline EUpdateTransformFlags operator ~(EUpdateTransformFlags Value)
{
	return static_cast<EUpdateTransformFlags>(~static_cast<int32>(Value));
}


FORCEINLINE EUpdateTransformFlags SkipPhysicsToEnum(bool bSkipPhysics){ return bSkipPhysics ? EUpdateTransformFlags::SkipPhysicsUpdate : EUpdateTransformFlags::None; }


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActorComponentActivatedSignature, UActorComponent*, Component, bool, bReset);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActorComponentDeactivateSignature, UActorComponent*, Component);

DECLARE_MULTICAST_DELEGATE_OneParam(FActorComponentGlobalCreatePhysicsSignature, UActorComponent*);
DECLARE_MULTICAST_DELEGATE_OneParam(FActorComponentGlobalDestroyPhysicsSignature, UActorComponent*);

/**
 * ActorComponent is the base class for components that define reusable behavior that can be added to different types of Actors.
 * ActorComponents that have a transform are known as SceneComponents and those that can be rendered are PrimitiveComponents.
 *
 * @see [ActorComponent](https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/Actors/Components/index.html#actorcomponents)
 * @see USceneComponent
 * @see UPrimitiveComponent
 */
UCLASS(DefaultToInstanced, BlueprintType, abstract, meta=(ShortTooltip="An ActorComponent is a reusable component that can be added to any actor."))
class ENGINE_API UActorComponent : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()
public:

	/** Create component physics state global delegate.*/
	static FActorComponentGlobalCreatePhysicsSignature GlobalCreatePhysicsDelegate;
	/** Destroy component physics state global delegate.*/
	static FActorComponentGlobalDestroyPhysicsSignature GlobalDestroyPhysicsDelegate;

	/**
	 * Default UObject constructor that takes an optional ObjectInitializer.
	 */
	UActorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Main tick function for the Actor */
	UPROPERTY(EditDefaultsOnly, Category="ComponentTick")
	struct FActorComponentTickFunction PrimaryComponentTick;

	/** Array of tags that can be used for grouping and categorizing. Can also be accessed from scripting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Tags)
	TArray<FName> ComponentTags;

protected:

	/** Array of user data stored with the component */
	UPROPERTY()
	TArray<UAssetUserData*> AssetUserData;

	/** 
	 *  Indicates if this ActorComponent is currently registered with a scene. 
	 */
	uint8 bRegistered:1;

	/** If the render state is currently created for this component */
	uint8 bRenderStateCreated:1;

	/** If the physics state is currently created for this component */
	uint8 bPhysicsStateCreated:1;

	/** Is this component currently replicating? Should the network code consider it for replication? Owning Actor must be replicating first! */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category=ComponentReplication,meta=(DisplayName = "Component Replicates"))
	uint8 bReplicates:1;

	/** Is this component safe to ID over the network by name?  */
	UPROPERTY()
	uint8 bNetAddressable:1;

private:
	/** Is this component in need of its whole state being sent to the renderer? */
	uint8 bRenderStateDirty:1;

	/** Is this component's transform in need of sending to the renderer? */
	uint8 bRenderTransformDirty:1;

	/** Is this component's dynamic data in need of sending to the renderer? */
	uint8 bRenderDynamicDataDirty:1;

	/** Used to ensure that any subclass of UActorComponent that overrides PostRename calls up to the Super to make OwnedComponents arrays get updated correctly */
	uint8 bRoutedPostRename:1;

public:

	/** Does this component automatically register with its owner */
	uint8 bAutoRegister:1;

protected:
	uint8 bAllowReregistration:1;

public:
	/** Should this component be ticked in the editor */
	uint8 bTickInEditor:1;

	/** If true, this component never needs a render update. */
	uint8 bNeverNeedsRenderUpdate:1;

	/** Can we tick this concurrently on other threads? */
	uint8 bAllowConcurrentTick:1;

	/** Can this component be destroyed (via K2_DestroyComponent) by any parent */
	uint8 bAllowAnyoneToDestroyMe:1;

#if WITH_EDITORONLY_DATA
	/** True if this component was created by a construction script, and will be destroyed by DestroyConstructedComponents */
	UPROPERTY()
	uint8 bCreatedByConstructionScript_DEPRECATED:1;

	/** True if this component was created as an instance component */
	UPROPERTY()
	uint8 bInstanceComponent_DEPRECATED:1;
#endif

	/** Whether the component is activated at creation or must be explicitly activated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Activation)
	uint8 bAutoActivate:1;

	/** Whether the component is currently active. */
	UPROPERTY(transient, ReplicatedUsing=OnRep_IsActive)
	uint8 bIsActive:1;

	UPROPERTY(EditDefaultsOnly, Category="Variable")
	uint8 bEditableWhenInherited:1;

	/** Cached navigation relevancy flag for collision updates */
	uint8 bNavigationRelevant : 1;

protected:
	/** Whether this component can potentially influence navigation */
	UPROPERTY(EditAnywhere, Category = Collision, AdvancedDisplay)
	uint8 bCanEverAffectNavigation : 1;

public:
	/** If true, we call the virtual InitializeComponent */
	uint8 bWantsInitializeComponent:1;

	/** If true, we call the virtual BeginPlay */
	DEPRECATED(4.14, "bWantsBeginPlay was inconsistently enforced and is now unused. BeginPlay will now always be called for Actor Components.")
	uint8 bWantsBeginPlay:1;

	/** If true, the component will be excluded from non-editor builds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Cooking)
	uint8 bIsEditorOnly:1;

private:
	/** Indicates that OnCreatedComponent has been called, but OnDestroyedComponent has not yet */
	uint8 bHasBeenCreated:1;

	/** Indicates that InitializeComponent has been called, but UninitializeComponent has not yet */
	uint8 bHasBeenInitialized:1;

	/** Indicates that BeginPlay has been called, but EndPlay has not yet */
	uint8 bHasBegunPlay:1;

	/** Indicates the the destruction process has begun for this component to avoid recursion */
	uint8 bIsBeingDestroyed:1;

	/** Whether we've tried to register tick functions. Reset when they are unregistered. */
	uint8 bTickFunctionsRegistered:1;

#if WITH_EDITOR
	/** During undo/redo it isn't safe to cache owner */
	uint8 bCanUseCachedOwner:1;
#endif

	/** True if this component was owned by a net startup actor during level load. */
	uint8 bIsNetStartupComponent : 1;

	/** Tracks whether the component has been added to one of the world's end of frame update lists */
	uint8 MarkedForEndOfFrameUpdateState:2;
	friend struct FMarkComponentEndOfFrameUpdateState;

	friend class FActorComponentInstanceData;
	friend class FActorComponentDetails;

public:
	UPROPERTY()
	EComponentCreationMethod CreationMethod;

private:
	UPROPERTY()
	TArray<FSimpleMemberReference> UCSModifiedProperties;

public:

	uint32 GetMarkedForEndOfFrameUpdateState() const { return MarkedForEndOfFrameUpdateState; }

	void DetermineUCSModifiedProperties();
	void GetUCSModifiedProperties(TSet<const UProperty*>& ModifiedProperties) const;
	void RemoveUCSModifiedProperties(const TArray<UProperty*>& Properties);

	bool IsEditableWhenInherited() const;

	bool HasBeenCreated() const { return bHasBeenCreated; }
	bool HasBeenInitialized() const { return bHasBeenInitialized; }
	bool HasBegunPlay() const { return bHasBegunPlay; }

	/**
	 * Returns whether the component is in the process of being destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category="Components", meta=(DisplayName="Is Component Being Destroyed"))
	bool IsBeingDestroyed() const
	{
		return bIsBeingDestroyed;
	}

	bool IsCreatedByConstructionScript() const;

	UFUNCTION()
	void OnRep_IsActive();

private:
	AActor* GetActorOwnerNoninline() const;

public:
	/** Follow the Outer chain to get the  AActor  that 'Owns' this component */
	UFUNCTION(BlueprintCallable, Category="Components", meta=(Keywords = "Actor Owning Parent"))
	AActor* GetOwner() const;

	virtual UWorld* GetWorld() const override final { return (WorldPrivate ? WorldPrivate : GetWorld_Uncached()); }

	/** See if this component contains the supplied tag */
	UFUNCTION(BlueprintCallable, Category="Components")
	bool ComponentHasTag(FName Tag) const;

	/**
	* Called during saving to determine the load flags to save with the object.
	*
	* @return	true if this object should always be loaded for editor game
	*/
	virtual bool NeedsLoadForEditorGame() const override
	{
		return !IsEditorOnly() && Super::NeedsLoadForEditorGame();
	}

	//~ Begin Trigger/Activation Interface

	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FActorComponentActivatedSignature OnComponentActivated;

	UPROPERTY(BlueprintAssignable, Category = "Components|Activation")
	FActorComponentDeactivateSignature OnComponentDeactivated;

	/**
	 * Activates the SceneComponent
	 * @param bReset - The value to assign to HiddenGame.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation", meta=(UnsafeDuringActorConstruction="true"))
	virtual void Activate(bool bReset=false);
	
	/**
	 * Deactivates the SceneComponent.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation", meta=(UnsafeDuringActorConstruction="true"))
	virtual void Deactivate();

	/**
	 * Sets whether the component is active or not
	 * @param bNewActive - The new active state of the component
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation", meta=(UnsafeDuringActorConstruction="true"))
	virtual void SetActive(bool bNewActive, bool bReset=false);

	/**
	 * Toggles the active state of the component
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation", meta=(UnsafeDuringActorConstruction="true"))
	virtual void ToggleActive();

	/**
	 * Returns whether the component is active or not
	 * @return - The active state of the component.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation", meta=(UnsafeDuringActorConstruction="true"))
	virtual bool IsActive() const { return bIsActive; }

	/**
	 * Sets whether the component should be auto activate or not. Only safe during construction scripts.
	 * @param bNewAutoActivate - The new auto activate state of the component
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Activation")
	virtual void SetAutoActivate(bool bNewAutoActivate);

	/** Sets whether this component can tick when paused. */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetTickableWhenPaused(bool bTickableWhenPaused);

	/** Create any physics engine information for this component */
	void CreatePhysicsState();

	/** Shut down any physics engine structure for this component */
	void DestroyPhysicsState();

	// Networking

	/** This signifies the component can be ID'd by name over the network. This only needs to be called by engine code when constructing blueprint components. */
	void SetNetAddressable();

	/** IsNameStableForNetworking means an object can be referred to its path name (relative to outer) over the network */
	virtual bool IsNameStableForNetworking() const override;

	/** IsSupportedForNetworking means an object can be referenced over the network */
	virtual bool IsSupportedForNetworking() const override;

	/** Enable or disable replication. This is the equivalent of RemoteRole for actors (only a bool is required for components) */
	UFUNCTION(BlueprintCallable, Category="Components")
	void SetIsReplicated(bool ShouldReplicate);

	/** Returns whether replication is enabled or not. */
	FORCEINLINE bool GetIsReplicated() const
	{
		return bReplicates;
	}

	/** Allows a component to replicate other subobject on the actor  */
	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags);

	/** Called on the component right before replication occurs */
	virtual void PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker);

	virtual bool GetComponentClassCanReplicate() const;

	/** Returns whether this component is an editor-only object or not */
	virtual bool IsEditorOnly() const override { return bIsEditorOnly; }

	virtual void MarkAsEditorOnlySubobject() override
	{
		bIsEditorOnly = true;
	}

	/** Returns net role of the owning actor */
	/** Returns true if we are replicating and not authorative */
	bool	IsNetSimulating() const;

	/** Get the network role of the Owner, or ROLE_None if there is no owner. */
	ENetRole GetOwnerRole() const;

	/**
	 * Get the network mode (dedicated server, client, standalone, etc) for this component.
	 * @see IsNetMode()
	 */
	ENetMode GetNetMode() const;

	/**
	* Test whether net mode is the given mode.
	* In optimized non-editor builds this can be more efficient than GetNetMode()
	* because it can check the static build flags without considering PIE.
	*/
	bool IsNetMode(ENetMode Mode) const;

	/** Returns true if this component was owned by a net startup actor during level load. */
	bool IsNetStartupComponent() const { return bIsNetStartupComponent; }

	/** This should only be called by the engine in ULevel::InitializeNetworkActors to initialize bIsNetStartupComponent. */
	void SetIsNetStartupComponent(const bool bInIsNetStartupComponent) { bIsNetStartupComponent = bInIsNetStartupComponent; }

private:

	mutable AActor* OwnerPrivate;

	/** 
	 * Pointer to the world that this component is currently registered with. 
	 * This is only non-NULL when the component is registered.
	 */
	UWorld* WorldPrivate;

	// If WorldPrivate isn't set this will determine the world from outers
	UWorld* GetWorld_Uncached() const;

	/** Private version without inlining that does *not* check Dedicated server build flags (which should already have been done). */
	ENetMode InternalGetNetMode() const;

protected:

	/** "Trigger" related function. Return true if it should activate **/
	virtual bool ShouldActivate() const;

private:
	/** Calls OnUnregister, DestroyRenderState_Concurrent and OnDestroyPhysicsState. */
	void ExecuteUnregisterEvents();

	/** Calls OnRegister, CreateRenderState_Concurrent and OnCreatePhysicsState. */
	void ExecuteRegisterEvents();

	/* Utility function for each of the PostEditChange variations to call for the same behavior */
	void ConsolidatedPostEditChange(const FPropertyChangedEvent& PropertyChangedEvent);
protected:

	friend class FComponentReregisterContextBase;
	friend class FComponentRecreateRenderStateContext;

	/**
	 * Called when a component is registered, after Scene is set, but before CreateRenderState_Concurrent or OnCreatePhysicsState are called.
	 */
	virtual void OnRegister();

	/**
	 * Called when a component is unregistered. Called after DestroyRenderState_Concurrent and OnDestroyPhysicsState are called.
	 */
	virtual void OnUnregister();

	/** Return true if CreateRenderState() should be called */
	virtual bool ShouldCreateRenderState() const 
	{
		return false;
	}

	/** Used to create any rendering thread information for this component
	*
	* **Caution**, this is called concurrently on multiple threads (but never the same component concurrently)
	*/
	virtual void CreateRenderState_Concurrent();

	/** Called to send a transform update for this component to the rendering thread
	*
	* **Caution**, this is called concurrently on multiple threads (but never the same component concurrently)
	*/
	virtual void SendRenderTransform_Concurrent();

	/** Called to send dynamic data for this component to the rendering thread */
	virtual void SendRenderDynamicData_Concurrent();

	/** Used to shut down any rendering thread structure for this component
	*
	* **Caution**, this is called concurrently on multiple threads (but never the same component concurrently)
	*/
	virtual void DestroyRenderState_Concurrent();

	/** Used to create any physics engine information for this component */
	virtual void OnCreatePhysicsState();

	/** Used to shut down and physics engine structure for this component */
	virtual void OnDestroyPhysicsState();

	/** Return true if CreatePhysicsState() should be called.
	    Ideally CreatePhysicsState() should always succeed if this returns true, but this isn't currently the case */
	virtual bool ShouldCreatePhysicsState() const {return false;}

	/** Used to check that DestroyPhysicsState() is working correctly */
	virtual bool HasValidPhysicsState() const { return false; }

	/**
	 * Virtual call chain to register all tick functions
	 * @param bRegister - true to register, false, to unregister
	 */
	virtual void RegisterComponentTickFunctions(bool bRegister);

public:
	/**
	 * Initializes the component.  Occurs at level startup. This is before BeginPlay (Actor or Component).  
	 * All Components in the level will be Initialized on load before any Actor/Component gets BeginPlay
	 * Requires component to be registered, and bWantsInitializeComponent to be true.
	 */
	virtual void InitializeComponent();

	/**
	 * BeginsPlay for the component.  Occurs at level startup. This is before BeginPlay (Actor or Component).  
	 * All Components (that want initialization) in the level will be Initialized on load before any 
	 * Actor/Component gets BeginPlay.
	 * Requires component to be registered and initialized.
	 */
	virtual void BeginPlay();

	/** 
	 * Blueprint implementable event for when the component is beginning play, called before its Owner's BeginPlay on Actor BeginPlay 
	 * or when the component is dynamically created if the Actor has already BegunPlay. 
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Begin Play"))
	void ReceiveBeginPlay();

	/**
	 * Ends gameplay for this component.
	 * Called from AActor::EndPlay only if bHasBegunPlay is true
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/**
	 * Handle this component being Uninitialized.
	 * Called from AActor::EndPlay only if bHasBeenInitialized is true
	 */
	virtual void UninitializeComponent();

	/** Blueprint implementable event for when the component ends play, generally via destruction or its Actor's EndPlay. */
	UFUNCTION(BlueprintImplementableEvent, meta=(Keywords = "delete", DisplayName = "End Play"))
	void ReceiveEndPlay(EEndPlayReason::Type EndPlayReason);
	
	/**
	 * When called, will call the virtual call chain to register all of the tick functions
	 * Do not override this function or make it virtual
	 * @param bRegister - true to register, false, to unregister
	 */
	void RegisterAllComponentTickFunctions(bool bRegister);

	/**
	 * Function called every frame on this ActorComponent. Override this function to implement custom logic to be executed every frame.
	 * Only executes if the component is registered, and also PrimaryComponentTick.bCanEverTick must be set to true.
	 *	
	 * @param DeltaTime - The time since the last tick.
	 * @param TickType - The kind of tick this is, for example, are we paused, or 'simulating' in the editor
	 * @param ThisTickFunction - Internal tick function struct that caused this to run
	 */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);
	/** 
	 * Set up a tick function for a component in the standard way. 
	 * Tick after the actor. Don't tick if the actor is static, or if the actor is a template or if this is a "NeverTick" component.
	 * I tick while paused if only if my owner does.
	 * @param	TickFunction - structure holding the specific tick function
	 * @return  true if this component met the criteria for actually being ticked.
	 */
	bool SetupActorComponentTickFunction(struct FTickFunction* TickFunction);

	/** 
	 * Set this component's tick functions to be enabled or disabled. Only has an effect if the function is registered
	 * 
	 * @param	bEnabled - Whether it should be enabled or not
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	virtual void SetComponentTickEnabled(bool bEnabled);
	/** 
	 * Spawns a task on GameThread that will call SetComponentTickEnabled
	 * 
	 * @param	bEnabled - Whether it should be enabled or not
	 */
	virtual void SetComponentTickEnabledAsync(bool bEnabled);
	/** 
	 * Returns whether this component has tick enabled or not
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool IsComponentTickEnabled() const;

	/** 
	* Sets the tick interval for this component's primary tick function. Does not enable the tick interval. Takes effect on next tick.
	* @param TickInterval	The duration between ticks for this component's primary tick function
	*/
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetComponentTickInterval(float TickInterval);

	/** 
	* Returns whether this component has tick enabled or not
	*/
	UFUNCTION(BlueprintCallable, Category="Utilities")
	float GetComponentTickInterval() const;

	/**
	 * @param InWorld - The world to register the component with.
	 */
	void RegisterComponentWithWorld(UWorld* InWorld);

private:
	
	/** Friend for access to ConditionalTickComponent(). */
	friend struct FActorComponentTickFunction;

public:

	/** Overridable check for a component to indicate to its Owner that it should prevent the Actor from auto destroying when finished */
	virtual bool IsReadyForOwnerToAutoDestroy() const { return true; }

	/**
	 * Returns whether the component's owner is selected.
	 */
	bool IsOwnerSelected() const;

	inline bool IsRenderTransformDirty() const { return bRenderTransformDirty; }
	inline bool IsRenderStateDirty() const { return bRenderStateDirty; }

	/** Invalidate lighting cache with default options. */
	void InvalidateLightingCache()
	{
		InvalidateLightingCacheDetailed(true, false);
	}

	/**
	 * Called when this actor component has moved, allowing it to discard statically cached lighting information.
	 */
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) {}

#if WITH_EDITOR
	/**
	 * Function that gets called from within Map_Check to allow this actor component to check itself
	 * for any potential errors and register them with map check dialog.
	 * @note Derived class implementations should call up to their parents.
	 */
	virtual void CheckForErrors();
#endif

	/**
	 * Uses the bRenderStateDirty/bRenderTransformDirty to perform any necessary work on this component.
	 * Do not call this directly, call MarkRenderStateDirty, MarkRenderDynamicDataDirty, 
	 *
	 * **Caution**, this is called concurrently on multiple threads (but never the same component concurrently)
	 */
	void DoDeferredRenderUpdates_Concurrent();

	/** Recalculate the value of our component to world transform */
	virtual void UpdateComponentToWorld(EUpdateTransformFlags UpdateTransformFlags = EUpdateTransformFlags::None, ETeleportType Teleport = ETeleportType::None){}

	/** Mark the render state as dirty - will be sent to the render thread at the end of the frame. */
	void MarkRenderStateDirty();

	/** Indicate that dynamic data for this component needs to be sent at the end of the frame. */
	void MarkRenderDynamicDataDirty();

	/** Marks the transform as dirty - will be sent to the render thread at the end of the frame*/
	void MarkRenderTransformDirty();

	/** If we belong to a world, mark this for a deferred update, otherwise do it now. */
	void MarkForNeededEndOfFrameUpdate();

	/** If we belong to a world, mark this for a deferred update, otherwise do it now. */
	void MarkForNeededEndOfFrameRecreate();

	/** If we belong to a world, clear the request to do a deferred update. */
	void ClearNeedEndOfFrameUpdate();

	/** return true if this component requires end of frame updates to happen from the game thread. */
	virtual bool RequiresGameThreadEndOfFrameUpdates() const;

	/** return true if this component requires end of frame recreates to happen from the game thread. */
	virtual bool RequiresGameThreadEndOfFrameRecreate() const;

	/** Recreate the render state right away. Generally you always want to call MarkRenderStateDirty instead. 
	*
	* **Caution**, this is called concurrently on multiple threads (but never the same component concurrently)
	*/
	void RecreateRenderState_Concurrent();

	/** Recreate the physics state right way. */
	void RecreatePhysicsState();

	/** @return true if the render 'state' (e.g. scene proxy) is created for this component */
	bool IsRenderStateCreated() const
	{
		return bRenderStateCreated;
	}

	/** @return true if the physics 'state' (e.g. physx bodies) are created for this component */
	bool IsPhysicsStateCreated() const
	{
		return bPhysicsStateCreated;
	}

	/** @name Accessors. */
	class FSceneInterface* GetScene() const;

	/** Return the ULevel that this Component is part of. */
	ULevel* GetComponentLevel() const;

	/** Returns true if this actor is contained by TestLevel. */
	bool ComponentIsInLevel(const class ULevel *TestLevel) const;

	/** See if this component is in the persistent level */
	bool ComponentIsInPersistentLevel(bool bIncludeLevelStreamingPersistent) const;

	/** Called on each component when the Actor's bEnableCollisionChanged flag changes */
	virtual void OnActorEnableCollisionChanged() {}

	/** 
	 * Returns a readable name for this component, including the asset name if applicable 
	 * By default this appends a space plus AdditionalStatObject()
	 */
	virtual FString GetReadableName() const;

	/** Give a readable name for this component, including asset name if applicable */
	virtual UObject const* AdditionalStatObject() const
	{
		return nullptr;
	}

	// Always called immediately before properties are received from the remote.
	virtual void PreNetReceive() override { }
	
	// Always called immediately after properties are received from the remote.
	virtual void PostNetReceive() override { }

	/** Called before we throw away components during RerunConstructionScripts, to cache any data we wish to persist across that operation */
	virtual class FActorComponentInstanceData* GetComponentInstanceData() const;

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack ) override;
	virtual bool CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack ) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual bool Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None ) override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual bool Modify( bool bAlwaysMarkDirty = true ) override;
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty( FPropertyChangedChainEvent& PropertyChangedEvent ) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual bool IsSelectedInEditor() const override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	//~ End IInterface_AssetUserData Interface

	/** See if the owning Actor is currently running the UCS */
	bool IsOwnerRunningUserConstructionScript() const;

	/** See if this component is currently registered */
	FORCEINLINE bool IsRegistered() const { return bRegistered; }

	/** Checked whether the component class allows reregistration */
	FORCEINLINE bool AllowReregistration() const { return bAllowReregistration; }

	/** Register this component, creating any rendering/physics state. Will also adds to outer Actor's Components array, if not already present. */
	void RegisterComponent();

	/** Unregister this component, destroying any rendering/physics state. */
	void UnregisterComponent();

	/** Unregister the component, remove it from its outer Actor's Components array and mark for pending kill. */
	virtual void DestroyComponent(bool bPromoteChildren = false);

	/** Called when a component is created (not loaded) */
	virtual void OnComponentCreated();

	/** 
	 * Called when a component is destroyed
	 * 
	 * @param	bDestroyingHierarchy  - True if the entire component hierarchy is being torn down, allows avoiding expensive operations
	 */
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy);

	/**
	 * Unregister and mark for pending kill a component.  This may not be used to destroy a component that is owned by an actor unless the owning actor is calling the function.
	 */
	UFUNCTION(BlueprintCallable, Category="Components", meta=(Keywords = "Delete", HidePin="Object", DefaultToSelf="Object", DisplayName = "DestroyComponent"))
	void K2_DestroyComponent(UObject* Object);

	/** Unregisters and immediately re-registers component.  Handles bWillReregister properly. */
	void ReregisterComponent();

	/** Changes the ticking group for this component */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	void SetTickGroup(ETickingGroup NewTickGroup);

	/** Make this component tick after PrerequisiteActor */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Make this component tick after PrerequisiteComponent. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Remove tick dependency on PrerequisiteActor. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Remove tick dependency on PrerequisiteComponent. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Event called every frame */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Tick"))
	void ReceiveTick(float DeltaSeconds);
	
	/** 
	 *  Called by owner actor on position shifting
	 *  Component should update all relevant data structures to reflect new actor location
	 *
	 * @param InWorldOffset	 Offset vector the actor shifted by
	 * @param bWorldShift	 Whether this call is part of whole world shifting
	 */
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) {};

	/** Can this component potentially influence navigation */
	bool CanEverAffectNavigation() const;

	/** set value of bCanEverAffectNavigation flag and update navigation octree if needed */
	void SetCanEverAffectNavigation(bool bRelevant);

	/** override to supply actual logic */
	virtual bool IsNavigationRelevant() const { return false; }

protected:
	/** Makes sure navigation system has up to date information regarding component's navigation relevancy 
	 *	and if it can affect navigation at all 
	 *	@param bForceUpdate by default updating navigation system will take place only if the component has
	 *		already been registered. Setting bForceUpdate to true overrides that check */
	void HandleCanEverAffectNavigationChange(bool bForceUpdate = false);

private:

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// this is the old name of the tick function. We just want to avoid mistakes with an attempt to override this
	virtual void Tick( float DeltaTime ) final { check(0); }

#endif

	void ClearNeedEndOfFrameUpdate_Internal();

public:

	/** Prefix used to identify template component instances */
	static const FString ComponentTemplateNameSuffix;
};

//////////////////////////////////////////////////////////////////////////
// UActorComponent inlines

FORCEINLINE bool UActorComponent::CanEverAffectNavigation() const
{
	return bCanEverAffectNavigation;
}

FORCEINLINE_DEBUGGABLE bool UActorComponent::IsNetSimulating() const
{
	return GetIsReplicated() && GetOwnerRole() != ROLE_Authority;
}

FORCEINLINE_DEBUGGABLE ENetMode UActorComponent::GetNetMode() const
{
	// IsRunningDedicatedServer() is a compile-time check in optimized non-editor builds.
	if (IsRunningDedicatedServer())
	{
		return NM_DedicatedServer;
	}

	return InternalGetNetMode();
}

FORCEINLINE_DEBUGGABLE bool UActorComponent::IsNetMode(ENetMode Mode) const
{
#if UE_EDITOR
	// Editor builds are special because of PIE, which can run a dedicated server without the app running with -server.
	return GetNetMode() == Mode;
#else
	// IsRunningDedicatedServer() is a compile-time check in optimized non-editor builds.
	if (Mode == NM_DedicatedServer)
	{
		return IsRunningDedicatedServer();
	}
	else
	{
		return !IsRunningDedicatedServer() && (InternalGetNetMode() == Mode);
	}
#endif // UE_EDITOR
}

FORCEINLINE void UActorComponent::ClearNeedEndOfFrameUpdate()
{
	if (MarkedForEndOfFrameUpdateState != 0)
	{
		ClearNeedEndOfFrameUpdate_Internal();
	}
}

FORCEINLINE_DEBUGGABLE AActor* UActorComponent::GetOwner() const
{
#if WITH_EDITOR
	// During undo/redo the cached owner is unreliable so just used GetTypedOuter
	if (bCanUseCachedOwner)
	{
		checkSlow(OwnerPrivate == GetActorOwnerNoninline()); // verify cached value is correct
		return OwnerPrivate;
	}
	else
	{
		return GetActorOwnerNoninline();
	}
#else
	checkSlow(OwnerPrivate == GetActorOwnerNoninline()); // verify cached value is correct
	return OwnerPrivate;
#endif
}
