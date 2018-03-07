// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "ComponentInstanceDataCache.h"
#include "Components/ChildActorComponent.h"
#include "RenderCommandFence.h"
#include "Misc/ITransaction.h"
#include "Engine/Level.h"

#include "Actor.generated.h"

class AActor;
class AController;
class AMatineeActor;
class APawn;
class APlayerController;
class UActorChannel;
class UChildActorComponent;
class UNetDriver;
class UPrimitiveComponent;
struct FAttachedActorInfo;
struct FNetViewer;
struct FNetworkObjectInfo;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogActor, Log, Warning);
 

// Delegate signatures
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams( FTakeAnyDamageSignature, AActor*, DamagedActor, float, Damage, const class UDamageType*, DamageType, class AController*, InstigatedBy, AActor*, DamageCauser );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_NineParams( FTakePointDamageSignature, AActor*, DamagedActor, float, Damage, class AController*, InstigatedBy, FVector, HitLocation, class UPrimitiveComponent*, FHitComponent, FName, BoneName, FVector, ShotFromDirection, const class UDamageType*, DamageType, AActor*, DamageCauser );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorBeginOverlapSignature, AActor*, OverlappedActor, AActor*, OtherActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorEndOverlapSignature, AActor*, OverlappedActor, AActor*, OtherActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams( FActorHitSignature, AActor*, SelfActor, AActor*, OtherActor, FVector, NormalImpulse, const FHitResult&, Hit );

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorBeginCursorOverSignature, AActor*, TouchedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FActorEndCursorOverSignature, AActor*, TouchedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorOnClickedSignature, AActor*, TouchedActor , FKey, ButtonPressed );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorOnReleasedSignature, AActor*, TouchedActor , FKey, ButtonReleased );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorOnInputTouchBeginSignature, ETouchIndex::Type, FingerIndex, AActor*, TouchedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorOnInputTouchEndSignature, ETouchIndex::Type, FingerIndex, AActor*, TouchedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorBeginTouchOverSignature, ETouchIndex::Type, FingerIndex, AActor*, TouchedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FActorEndTouchOverSignature, ETouchIndex::Type, FingerIndex, AActor*, TouchedActor );

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActorDestroyedSignature, AActor*, DestroyedActor );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActorEndPlaySignature, AActor*, Actor , EEndPlayReason::Type, EndPlayReason);

DECLARE_DELEGATE_SixParams(FMakeNoiseDelegate, AActor*, float /*Loudness*/, class APawn*, const FVector&, float /*MaxRange*/, FName /*Tag*/);

#if !UE_BUILD_SHIPPING
DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnProcessEvent, AActor*, UFunction*, void*);
#endif

DECLARE_CYCLE_STAT_EXTERN(TEXT("GetComponentsTime"),STAT_GetComponentsTime,STATGROUP_Engine,ENGINE_API);

#if WITH_EDITOR
/** Annotation for actor selection.  This must be in engine instead of editor for ::IsSelected to work */
extern ENGINE_API FUObjectAnnotationSparseBool GSelectedActorAnnotation;
#endif

/**
 * Actor is the base class for an Object that can be placed or spawned in a level.
 * Actors may contain a collection of ActorComponents, which can be used to control how actors move, how they are rendered, etc.
 * The other main function of an Actor is the replication of properties and function calls across the network during play.
 * 
 * @see https://docs.unrealengine.com/latest/INT/Programming/UnrealArchitecture/Actors/
 * @see UActorComponent
 */
UCLASS(BlueprintType, Blueprintable, config=Engine, meta=(ShortTooltip="An Actor is an object that can be placed or spawned in the world."))
class ENGINE_API AActor : public UObject
{
	/**
	* The functions of interest to initialization order for an Actor is roughly as follows:
	* PostLoad/PostActorCreated - Do any setup of the actor required for construction. PostLoad for serialized actors, PostActorCreated for spawned.  
	* AActor::OnConstruction - The construction of the actor, this is where Blueprint actors have their components created and blueprint variables are initialized
	* AActor::PreInitializeComponents - Called before InitializeComponent is called on the actor's components
	* UActorComponent::InitializeComponent - Each component in the actor's components array gets an initialize call (if bWantsInitializeComponent is true for that component)
	* AActor::PostInitializeComponents - Called after the actor's components have been initialized
	* AActor::BeginPlay - Called when the level is started
	*/

	GENERATED_BODY()
public:

	/**
	 * Default constructor for AActor
	 */
	AActor();

	/**
	 * Constructor for AActor that takes an ObjectInitializer
	 */
	AActor(const FObjectInitializer& ObjectInitializer);

private:
	/** Called from the constructor to initialize the class to its default settings */
	void InitializeDefaults();

public:
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Primary Actor tick function, which calls TickActor().
	 * Tick functions can be configured to control whether ticking is enabled, at what time during a frame the update occurs, and to set up tick dependencies.
	 * @see https://docs.unrealengine.com/latest/INT/API/Runtime/Engine/Engine/FTickFunction/
	 * @see AddTickPrerequisiteActor(), AddTickPrerequisiteComponent()
	 */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	struct FActorTickFunction PrimaryActorTick;

	/** Allow each actor to run at a different time speed. The DeltaTime for a frame is multiplied by the global TimeDilation (in WorldSettings) and this CustomTimeDilation for this actor's tick.  */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category="Misc")
	float CustomTimeDilation;	

public:
	/**
	 * Allows us to only see this Actor in the Editor, and not in the actual game.
	 * @see SetActorHiddenInGame()
	 */
	UPROPERTY(Interp, EditAnywhere, Category=Rendering, BlueprintReadOnly, replicated, meta=(DisplayName = "Actor Hidden In Game", SequencerTrackClass = "MovieSceneVisibilityTrack"))
	uint8 bHidden:1;

	/** If true, when the actor is spawned it will be sent to the client but receive no further replication updates from the server afterwards. */
	UPROPERTY()
	uint8 bNetTemporary:1;

	/** If true, this actor was loaded directly from the map, and for networking purposes can be addressed by its full path name */
	UPROPERTY()
	uint8 bNetStartup:1;

	/** If true, this actor is only relevant to its owner. If this flag is changed during play, all non-owner channels would need to be explicitly closed. */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadOnly)
	uint8 bOnlyRelevantToOwner:1;

	/** Always relevant for network (overrides bOnlyRelevantToOwner). */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	uint8 bAlwaysRelevant:1;    

	/**
	 * If true, replicate movement/location related properties.
	 * Actor must also be set to replicate.
	 * @see SetReplicates()
	 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Replication/
	 */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicateMovement, Category=Replication, EditDefaultsOnly)
	uint8 bReplicateMovement:1;    

	/** Called on client when updated bReplicateMovement value is received for this actor. */
	UFUNCTION()
	virtual void OnRep_ReplicateMovement();

	/**
	 * If true, this actor is no longer replicated to new clients, and is "torn off" (becomes a ROLE_Authority) on clients to which it was being replicated.
	 * @see TornOff()
	 */
	UPROPERTY(replicated)
	uint8 bTearOff:1;    

	/** Networking - Server - TearOff this actor to stop replication to clients. Will set bTearOff to true. */
	UFUNCTION(BlueprintCallable, Category = Replication)
	virtual void TearOff();

	/**
	 * Whether we have already exchanged Role/RemoteRole on the client, as when removing then re-adding a streaming level.
	 * Causes all initialization to be performed again even though the actor may not have actually been reloaded.
	 */
	UPROPERTY(transient)
	uint8 bExchangedRoles:1;

	/** Is this actor still pending a full net update due to clients that weren't able to replicate the actor at the time of LastNetUpdateTime */
	DEPRECATED(4.16, "bPendingNetUpdate has been deprecated. Please use bPendingNetUpdate that is now on FNetworkObjectInfo (obtained by calling GetNetworkObjectInfo)")
	uint8 bPendingNetUpdate:1;

	/** This actor will be loaded on network clients during map load */
	UPROPERTY(Category=Replication, EditDefaultsOnly)
	uint8 bNetLoadOnClient:1;

	/** If actor has valid Owner, call Owner's IsNetRelevantFor and GetNetPriority */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	uint8 bNetUseOwnerRelevancy:1;

	/** If true, all input on the stack below this actor will not be considered */
	UPROPERTY(EditDefaultsOnly, Category=Input)
	uint8 bBlockInput:1;

	/** True if this actor is currently running user construction script (used to defer component registration) */
	uint8 bRunningUserConstructionScript:1;

	/**
	 * Whether we allow this Actor to tick before it receives the BeginPlay event.
	 * Normally we don't tick actors until after BeginPlay; this setting allows this behavior to be overridden.
	 * This Actor must be able to tick for this setting to be relevant.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Tick")
	uint8 bAllowTickBeforeBeginPlay:1;

private:
	/** Whether FinishSpawning has been called for this Actor.  If it has not, the Actor is in a mal-formed state */
	uint8 bHasFinishedSpawning:1;

	/** Whether we've tried to register tick functions. Reset when they are unregistered. */
	uint8 bTickFunctionsRegistered : 1;

	/** Whether we've deferred the RegisterAllComponents() call at spawn time. Reset when RegisterAllComponents() is called. */
	uint8 bHasDeferredComponentRegistration : 1;

	/**
	 * Enables any collision on this actor.
	 * @see SetActorEnableCollision(), GetActorEnableCollision()
	 */
	UPROPERTY()
	uint8 bActorEnableCollision:1;

	/** Flag indicating we have checked initial simulating physics state to sync networked proxies to the server. */
	uint8 bNetCheckedInitialPhysicsState:1;

protected:
	/**
	 * If true, this actor will replicate to remote machines
	 * @see SetReplicates()
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Replication")
	uint8 bReplicates:1;

	/** This function should only be used in the constructor of classes that need to set the RemoteRole for backwards compatibility purposes */
	void SetRemoteRoleForBackwardsCompat(const ENetRole InRemoteRole) { RemoteRole = InRemoteRole; }

	/**
	 * Does this actor have an owner responsible for replication? (APlayerController typically)
	 *
	 * @return true if this actor can call RPCs or false if no such owner chain exists
	 */
	virtual bool HasNetOwner() const;

	UFUNCTION()
	virtual void OnRep_Owner();

private:
	/**
	 * Describes how much control the remote machine has over the actor.
	 */
	UPROPERTY(Replicated, transient)
	TEnumAsByte<enum ENetRole> RemoteRole;

	/**
	 * Owner of this Actor, used primarily for replication (bNetUseOwnerRelevancy & bOnlyRelevantToOwner) and visibility (PrimitiveComponent bOwnerNoSee and bOnlyOwnerSee)
	 * @see SetOwner(), GetOwner()
	 */
	UPROPERTY(ReplicatedUsing=OnRep_Owner)
	AActor* Owner;

public:

	/** Used to specify the net driver to replicate on (NAME_None || NAME_GameNetDriver is the default net driver) */
	UPROPERTY()
	FName NetDriverName;

	/**
	 * Set whether this actor replicates to network clients. When this actor is spawned on the server it will be sent to clients as well.
	 * Properties flagged for replication will update on clients if they change on the server.
	 * Internally changes the RemoteRole property and handles the cases where the actor needs to be added to the network actor list.
	 * @param bInReplicates Whether this Actor replicates to network clients.
	 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Networking/Replication/
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Replication")
	void SetReplicates(bool bInReplicates);

	/**
	* Set whether this actor's movement replicates to network clients.
	* @param bInReplicateMovement Whether this Actor's movement replicates to clients.
	*/
	UFUNCTION(BlueprintCallable, Category = "Replication")
	virtual void SetReplicateMovement(bool bInReplicateMovement);

	/** Sets whether or not this Actor is an autonomous proxy, which is an actor on a network client that is controlled by a user on that client. */
	void SetAutonomousProxy(const bool bInAutonomousProxy, const bool bAllowForcePropertyCompare=true);
	
	/** Copies RemoteRole from another Actor and adds this actor to the list of network actors if necessary. */
	void CopyRemoteRoleFrom(const AActor* CopyFromActor);

	/** Returns how much control the remote machine has over this actor. */
	UFUNCTION(BlueprintCallable, Category = "Replication")
	ENetRole GetRemoteRole() const;

	/** Used for replication of our RootComponent's position and velocity */
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_ReplicatedMovement, Category=Replication, AdvancedDisplay)
	struct FRepMovement ReplicatedMovement;

	/** How long this Actor lives before dying, 0=forever. Note this is the INITIAL value and should not be modified once play has begun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Actor)
	float InitialLifeSpan;

private:
	/**
	 * Used for replicating attachment of this actor's RootComponent to another actor.
	 * This is filled in via GatherCurrentMovement() when the RootComponent has an AttachParent.
	 */
	UPROPERTY(Transient, replicatedUsing=OnRep_AttachmentReplication)
	struct FRepAttachment AttachmentReplication;

public:
	/** Get read-only access to current AttachmentReplication. */
	const struct FRepAttachment& GetAttachmentReplication() const { return AttachmentReplication; }

	/** Called on client when updated AttachmentReplication value is received for this actor. */
	UFUNCTION()
	virtual void OnRep_AttachmentReplication();

	/** Describes how much control the local machine has over the actor. */
	UPROPERTY(Replicated)
	TEnumAsByte<enum ENetRole> Role;

	/** Dormancy setting for actor to take itself off of the replication list without being destroyed on clients. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Replication)
	TEnumAsByte<enum ENetDormancy> NetDormancy;

	/** Gives the actor a chance to pause replication to a player represented by the passed in actor - only called on server */
	virtual bool IsReplicationPausedForConnection(const FNetViewer& ConnectionOwnerNetViewer);

	/** Called on the client when the replication paused value is changed */
	virtual void OnReplicationPausedChanged(bool bIsReplicationPaused);

	/** Automatically registers this actor to receive input from a player. */
	UPROPERTY(EditAnywhere, Category=Input)
	TEnumAsByte<EAutoReceiveInput::Type> AutoReceiveInput;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum EInputConsumeOptions> InputConsumeOption_DEPRECATED;
#endif

	/** The priority of this input component when pushed in to the stack. */
	UPROPERTY(EditAnywhere, Category=Input)
	int32 InputPriority;

	/** Component that handles input for this actor, if input is enabled. */
	UPROPERTY()
	class UInputComponent* InputComponent;

	/** Square of the max distance from the client's viewpoint that this actor is relevant and will be replicated. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category=Replication)
	float NetCullDistanceSquared;   

	/** Internal - used by UWorld::ServerTickClients() */
	UPROPERTY(transient)
	int32 NetTag;

	/** Next time this actor will be considered for replication, set by SetNetUpdateTime() */
	DEPRECATED(4.16, "NetUpdateTime has been deprecated. Please use NextUpdateTime that is now on FNetworkObjectInfo (obtained by calling GetNetworkObjectInfo)")
	float NetUpdateTime;

	/** How often (per second) this actor will be considered for replication, used to determine NetUpdateTime */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	float NetUpdateFrequency;

	/** Used to determine what rate to throttle down to when replicated properties are changing infrequently */
	UPROPERTY( Category=Replication, EditDefaultsOnly, BlueprintReadWrite )
	float MinNetUpdateFrequency;

	/** Priority for this actor when checking for replication in a low bandwidth or saturated situation, higher priority means it is more likely to replicate */
	UPROPERTY(Category=Replication, EditDefaultsOnly, BlueprintReadWrite)
	float NetPriority;

	/** Last time this actor was updated for replication via NetUpdateTime
	* @warning: internal net driver time, not related to WorldSettings.TimeSeconds */
	DEPRECATED(4.16, "LastNetUpdateTime has been deprecated. Please use LastNetUpdateTime that is now on FNetworkObjectInfo (obtained by calling GetNetworkObjectInfo)")
	float LastNetUpdateTime;

	/**
	 * Set the name of the net driver associated with this actor.  Will move the actor out of the list of network actors from the old net driver and add it to the new list
	 *
	 * @param NewNetDriverName name of the new net driver association
	 */
	void SetNetDriverName(FName NewNetDriverName );

	/** @return name of the net driver associated with this actor (all RPCs will go out via this connection) */
	FName GetNetDriverName() const { return NetDriverName; }

	/** Method that allows an actor to replicate subobjects on its actor channel */
	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags);

	/** Called on the actor when a new subobject is dynamically created via replication */
	virtual void OnSubobjectCreatedFromReplication(UObject *NewSubobject);

	/** Called on the actor when a subobject is dynamically destroyed via replication */
	virtual void OnSubobjectDestroyFromReplication(UObject *Subobject);

	/** Called on the actor right before replication occurs. 
	 * Only called on Server, and for autonomous proxies if recording a Client Replay. */
	virtual void PreReplication( IRepChangedPropertyTracker & ChangedPropertyTracker );

	/** Called on the actor right before replication occurs.
	 * Called for everyone when recording a Client Replay, including Simulated Proxies. */
	virtual void PreReplicationForReplay(IRepChangedPropertyTracker & ChangedPropertyTracker);

	/** Called by the networking system to call PreReplication on this actor and its components using the given NetDriver to find or create RepChangedPropertyTrackers. */
	void CallPreReplication(UNetDriver* NetDriver);

	/** If true then destroy self when "finished", meaning all relevant components report that they are done and no timelines or timers are in flight. */
	UPROPERTY(BlueprintReadWrite, Category=Actor)
	uint8 bAutoDestroyWhenFinished:1;

	/**
	 * Whether this actor can take damage. Must be true for damage events (e.g. ReceiveDamage()) to be called.
	 * @see https://www.unrealengine.com/blog/damage-in-ue4
	 * @see TakeDamage(), ReceiveDamage()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Replicated, Category=Actor)
	uint8 bCanBeDamaged:1;

private:
	/**
	 * Set when actor is about to be deleted.
	 */
	UPROPERTY(Transient, DuplicateTransient)
	uint8 bActorIsBeingDestroyed:1;    

public:

	/** This actor collides with the world when placing in the editor, even if RootComponent collision is disabled. Does not affect spawning, @see SpawnCollisionHandlingMethod */
	UPROPERTY()
	uint8 bCollideWhenPlacing:1;

	/** If true, this actor should search for an owned camera component to view through when used as a view target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Actor, AdvancedDisplay)
	uint8 bFindCameraComponentWhenViewTarget:1;
	
	/** If true, this actor will be replicated to network replays (default is true) */
	UPROPERTY()
	uint8 bRelevantForNetworkReplays:1;
	
    /** If true, this actor will generate overlap events when spawned as part of level streaming. You might enable this is in the case where a streaming level loads around an actor and you want overlaps to trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Actor)
	uint8 bGenerateOverlapEventsDuringLevelStreaming:1;

protected:

	/** If true, this actor can be put inside of a GC Cluster to improve Garbage Collection performance */
	UPROPERTY(Category = Actor, EditAnywhere, AdvancedDisplay)
	uint8 bCanBeInCluster:1;

protected:

	/**
	 * If false, the Blueprint ReceiveTick() event will be disabled on dedicated servers.
	 * @see AllowReceiveTickEventOnDedicatedServer()
	 */
	UPROPERTY()
	uint8 bAllowReceiveTickEventOnDedicatedServer:1;

private:

	/** 
	 *	Indicates that PreInitializeComponents/PostInitializeComponents have been called on this Actor 
	 *	Prevents re-initializing of actors spawned during level startup
	 */
	uint8 bActorInitialized:1;
	
	enum class EActorBeginPlayState : uint8
	{
		HasNotBegunPlay,
		BeginningPlay,
		HasBegunPlay,
	};

	/** 
	 *	Indicates that BeginPlay has been called for this Actor.
	 *  Set back to false once EndPlay has been called.
	 */
	EActorBeginPlayState ActorHasBegunPlay:2;

public:
	/** Indicates the actor was pulled through a seamless travel.  */
	UPROPERTY()
	uint8 bActorSeamlessTraveled:1;

	/** Whether this actor should not be affected by world origin shifting. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Actor)
	uint8 bIgnoresOriginShifting:1;
	
	/** If true, and if World setting has bEnableHierarchicalLOD equal to true, then it will generate LODActor from groups of clustered Actor */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = HLOD, meta = (DisplayName = "Include Actor for HLOD Mesh generation"))
	uint8 bEnableAutoLODGeneration:1;

public:

	/** Controls how to handle spawning this actor in a situation where it's colliding with something else. "Default" means AlwaysSpawn here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Actor)
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingMethod;
	
	/** The time this actor was created, relative to World->GetTimeSeconds().
	* @see UWorld::GetTimeSeconds()
	*/
	float CreationTime;

	/** Pawn responsible for damage caused by this actor. */
	UPROPERTY(BlueprintReadWrite, replicatedUsing=OnRep_Instigator, meta=(ExposeOnSpawn=true), Category=Actor)
	class APawn* Instigator;

	/** Called on clients when Instigator is replicated. */
	UFUNCTION()
	virtual void OnRep_Instigator();

	/** Array of Actors whose Owner is this actor */
	UPROPERTY(transient)
	TArray<AActor*> Children;

protected:
	/**
	 * Collision primitive that defines the transform (location, rotation, scale) of this Actor.
	 */
	UPROPERTY(BlueprintGetter=K2_GetRootComponent, Category="Utilities|Transformation")
	USceneComponent* RootComponent;

#if WITH_EDITORONLY_DATA
	/** Local space pivot offset for the actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Actor)
	FVector PivotOffset;
#endif

	/** The matinee actors that control this actor. */
	UPROPERTY(transient)
	TArray<class AMatineeActor*> ControllingMatineeActors;

	/** Handle for efficient management of LifeSpanExpired timer */
	FTimerHandle TimerHandle_LifeSpanExpired;

public:

	/** Return the value of bAllowReceiveTickEventOnDedicatedServer, indicating whether the Blueprint ReceiveTick() event will occur on dedicated servers. */
	FORCEINLINE bool AllowReceiveTickEventOnDedicatedServer() const { return bAllowReceiveTickEventOnDedicatedServer; }

	/** Layer's the actor belongs to.  This is outside of the editoronly data to allow hiding of LD-specified layers at runtime for profiling. */
	UPROPERTY()
	TArray< FName > Layers;

private:
#if WITH_EDITORONLY_DATA
	/** The Actor that owns the UChildActorComponent that owns this Actor. */
	UPROPERTY()
	TWeakObjectPtr<AActor> ParentComponentActor_DEPRECATED;	
#endif

	/** The UChildActorComponent that owns this Actor. */
	UPROPERTY()
	TWeakObjectPtr<UChildActorComponent> ParentComponent;	

public:

#if WITH_EDITORONLY_DATA

	/** The group this actor is a part of. */
	UPROPERTY(transient)
	AActor* GroupActor;

	/** The scale to apply to any billboard components in editor builds (happens in any WITH_EDITOR build, including non-cooked games). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rendering, meta=(DisplayName="Editor Billboard Scale"))
	float SpriteScale;

private:
	/**
	 * The friendly name for this actor, displayed in the editor.  You should always use AActor::GetActorLabel() to access the actual label to display,
	 * and call AActor::SetActorLabel() or FActorLabelUtilities::SetActorLabelUnique() to change the label.  Never set the label directly.
	 */
	UPROPERTY()
	FString ActorLabel;

	/** The folder path of this actor in the world (empty=root, / separated)*/
	UPROPERTY()
	FName FolderPath;

protected:

	UPROPERTY()
	uint8 bActorLabelEditable:1;    // Is the actor label editable by the user?

public:
	/** Whether this actor is hidden within the editor viewport. */
	UPROPERTY()
	uint8 bHiddenEd:1;

protected:
	/** Whether the actor can be manipulated by editor operations. */
	UPROPERTY()
	uint8 bEditable:1;

	/** Whether this actor should be listed in the scene outliner. */
	UPROPERTY()
	uint8 bListedInSceneOutliner:1;

public:
	/** True if this actor is the preview actor dragged out of the content browser */
	UPROPERTY()
	uint8 bIsEditorPreviewActor:1;

	/** Whether this actor is hidden by the layer browser. */
	UPROPERTY()
	uint8 bHiddenEdLayer:1;

private:
	/** Whether this actor is temporarily hidden within the editor; used for show/hide/etc functionality w/o dirtying the actor. */
	UPROPERTY(transient)
	uint8 bHiddenEdTemporary:1;

public:

	/** Whether this actor is hidden by the level browser. */
	UPROPERTY(transient)
	uint8 bHiddenEdLevel:1;

	/** If true, prevents the actor from being moved in the editor viewport. */
	UPROPERTY()
	uint8 bLockLocation:1;

	/** If true during PostEditMove the construction script will be run every time. If false it will only run when the drag finishes. */
	uint8 bRunConstructionScriptOnDrag:1;

	/** Returns how many lights are uncached for this actor. */
	int32 GetNumUncachedLights();
#endif // WITH_EDITORONLY_DATA

private:

	static uint32 BeginPlayCallDepth;

public:

	/** Array of tags that can be used for grouping and categorizing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=Actor)
	TArray<FName> Tags;

	/** Bitflag to represent which views this actor is hidden in, via per-view layer visibility. */
	UPROPERTY(transient)
	uint64 HiddenEditorViews;

	//~==============================================================================================
	// Delegates
	
	/** Called when the actor is damaged in any way. */
	UPROPERTY(BlueprintAssignable, Category="Game|Damage")
	FTakeAnyDamageSignature OnTakeAnyDamage;

	/** Called when the actor is damaged by point damage. */
	UPROPERTY(BlueprintAssignable, Category="Game|Damage")
	FTakePointDamageSignature OnTakePointDamage;
	
	/** 
	 *	Called when another actor begins to overlap this actor, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorBeginOverlapSignature OnActorBeginOverlap;

	/** 
	 *	Called when another actor stops overlapping this actor. 
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorEndOverlapSignature OnActorEndOverlap;

	/** Called when the mouse cursor is moved over this actor if mouse over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorBeginCursorOverSignature OnBeginCursorOver;

	/** Called when the mouse cursor is moved off this actor if mouse over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorEndCursorOverSignature OnEndCursorOver;

	/** Called when the left mouse button is clicked while the mouse is over this actor and click events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorOnClickedSignature OnClicked;

	/** Called when the left mouse button is released while the mouse is over this actor and click events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FActorOnReleasedSignature OnReleased;

	/** Called when a touch input is received over this actor when touch events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorOnInputTouchBeginSignature OnInputTouchBegin;
		
	/** Called when a touch input is received over this component when touch events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorOnInputTouchEndSignature OnInputTouchEnd;

	/** Called when a finger is moved over this actor when touch over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorBeginTouchOverSignature OnInputTouchEnter;

	/** Called when a finger is moved off this actor when touch over events are enabled in the player controller. */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FActorEndTouchOverSignature OnInputTouchLeave;

	/** 
	 *	Called when this Actor hits (or is hit by) something solid. This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 *	For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *	@note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FActorHitSignature OnActorHit;

	/** 
	 * Pushes this actor on to the stack of input being handled by a PlayerController.
	 * @param PlayerController The PlayerController whose input events we want to receive.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void EnableInput(class APlayerController* PlayerController);

	/** 
	 * Removes this actor from the stack of input being handled by a PlayerController.
	 * @param PlayerController The PlayerController whose input events we no longer want to receive. If null, this actor will stop receiving input from all PlayerControllers.
	 */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DisableInput(class APlayerController* PlayerController);

	/** Gets the value of the input axis if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", HideSelfPin="true", HidePin="InputAxisName"))
	float GetInputAxisValue(const FName InputAxisName) const;

	/** Gets the value of the input axis key if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", HideSelfPin="true", HidePin="InputAxisKey"))
	float GetInputAxisKeyValue(const FKey InputAxisKey) const;

	/** Gets the value of the input axis key if input is enabled for this actor. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly="true", HideSelfPin="true", HidePin="InputAxisKey"))
	FVector GetInputVectorAxisValue(const FKey InputAxisKey) const;

	/** Returns the instigator for this actor, or NULL if there is none. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintProtected = "true"), Category="Game")
	APawn* GetInstigator() const;

	/**
	 * Get the instigator, cast as a specific class.
	 * @return The instigator for this weapon if it is the specified type, NULL otherwise.
	 */
	template <class T>
	T* GetInstigator() const { return Cast<T>(Instigator); };

	/** Returns the instigator's controller for this actor, or NULL if there is none. */
	UFUNCTION(BlueprintCallable, meta=(BlueprintProtected = "true"), Category="Game")
	AController* GetInstigatorController() const;


	//~=============================================================================
	// General functions.

	/**
	 * Get the actor-to-world transform.
	 * @return The transform that transforms from actor space to world space.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetActorTransform"), Category="Utilities|Transformation")
	FTransform GetTransform() const
	{
		return ActorToWorld();
	}

	/** Get the local-to-world transform of the RootComponent. Identical to GetTransform(). */
	FORCEINLINE FTransform ActorToWorld() const
	{
		return (RootComponent ? RootComponent->GetComponentTransform() : FTransform::Identity);
	}


	/** Returns the location of the RootComponent of this Actor */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetActorLocation", Keywords="position"), Category="Utilities|Transformation")
	FVector K2_GetActorLocation() const;

	/** 
	 * Move the Actor to the specified location.
	 * @param NewLocation	The new location to move the Actor to.
	 * @param bSweep		Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *						Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport		Whether we teleport the physics state (if physics collision is enabled for this object).
	 *						If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *						If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *						If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param SweepHitResult	The hit result from the move if swept.
	 * @return	Whether the location was successfully set (if not swept), or whether movement occurred at all (if swept).
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "SetActorLocation", Keywords="position"), Category="Utilities|Transformation")
	bool K2_SetActorLocation(FVector NewLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);

	/** Returns rotation of the RootComponent of this Actor. */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "GetActorRotation"), Category="Utilities|Transformation")
	FRotator K2_GetActorRotation() const;

	/** Get the forward (X) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorForwardVector() const;

	/** Get the up (Z) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorUpVector() const;

	/** Get the right (Y) vector (length 1.0) from this Actor, in world space.  */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	FVector GetActorRightVector() const;

	/**
	 * Returns the bounding box of all components that make up this Actor (excluding ChildActorComponents).
	 * @param	bOnlyCollidingComponents	If true, will only return the bounding box for components with collision enabled.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DisplayName = "GetActorBounds"))
	void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent) const;

	/** Returns the RootComponent of this Actor */
	UFUNCTION(BlueprintGetter)
	USceneComponent* K2_GetRootComponent() const;

	/** Returns velocity (in cm/s (Unreal Units/second) of the rootcomponent if it is either using physics or has an associated MovementComponent */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	virtual FVector GetVelocity() const;

	/** 
	 * Move the actor instantly to the specified location. 
	 * 
	 * @param NewLocation	The new location to teleport the Actor to.
	 * @param bSweep		Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *						Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param Teleport		How we teleport the physics state (if physics collision is enabled for this object).
	 *						If equal to ETeleportType::TeleportPhysics, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *						If equal to ETeleportType::None, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *						If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param OutSweepHitResult The hit result from the move if swept.
	 * @return	Whether the location was successfully set if not swept, or whether movement occurred if swept.
	 */
	bool SetActorLocation(const FVector& NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** 
	 * Set the Actor's rotation instantly to the specified rotation.
	 * 
	 * @param	NewRotation	The new rotation for the Actor.
	 * @param	bTeleportPhysics Whether we teleport the physics state (if physics collision is enabled for this object).
	 *			If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *			If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 * @return	Whether the rotation was successfully set.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "SetActorRotation"), Category="Utilities|Transformation")
	bool K2_SetActorRotation(FRotator NewRotation, bool bTeleportPhysics);
	
	/**
	* Set the Actor's rotation instantly to the specified rotation.
	*
	* @param	NewRotation	The new rotation for the Actor.
	* @param	Teleport	How we teleport the physics state (if physics collision is enabled for this object).
	*						If equal to ETeleportType::TeleportPhysics, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	*						If equal to ETeleportType::None, physics velocity is updated based on the change in position (affecting ragdoll parts).
	* @return	Whether the rotation was successfully set.
	*/
	bool SetActorRotation(FRotator NewRotation, ETeleportType Teleport = ETeleportType::None);
	bool SetActorRotation(const FQuat& NewRotation, ETeleportType Teleport = ETeleportType::None);

	/** 
	 * Move the actor instantly to the specified location and rotation.
	 * 
	 * @param NewLocation		The new location to teleport the Actor to.
	 * @param NewRotation		The new rotation for the Actor.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param SweepHitResult	The hit result from the move if swept.
	 * @return	Whether the rotation was successfully set.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetActorLocationAndRotation"))
	bool K2_SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	
	/** 
	 * Move the actor instantly to the specified location and rotation.
	 * 
	 * @param NewLocation		The new location to teleport the Actor to.
	 * @param NewRotation		The new rotation for the Actor.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param OutSweepHitResult	The hit result from the move if swept.
	 * @return	Whether the rotation was successfully set.
	 */
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	bool SetActorLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/** Set the Actor's world-space scale. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetActorScale3D(FVector NewScale3D);

	/** Returns the Actor's world-space scale. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Orientation")
	FVector GetActorScale3D() const;

	/** Returns the distance from this Actor to OtherActor. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetDistanceTo(const AActor* OtherActor) const;

	/** Returns the squared distance from this Actor to OtherActor. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetSquaredDistanceTo(const AActor* OtherActor) const;

	/** Returns the distance from this Actor to OtherActor, ignoring Z. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetHorizontalDistanceTo(const AActor* OtherActor) const;

	/** Returns the distance from this Actor to OtherActor, ignoring XY. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetVerticalDistanceTo(const AActor* OtherActor) const;

	/** Returns the dot product from this Actor to OtherActor. Returns -2.0 on failure. Returns 0.0 for coincidental actors. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetDotProductTo(const AActor* OtherActor) const;

	/** Returns the dot product from this Actor to OtherActor, ignoring Z. Returns -2.0 on failure. Returns 0.0 for coincidental actors. */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Transformation")
	float GetHorizontalDotProductTo(const AActor* OtherActor) const;

	/**
	 * Adds a delta to the location of this actor in world space.
	 * 
	 * @param DeltaLocation		The change in location.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param SweepHitResult	The hit result from the move if swept.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorWorldOffset", Keywords="location position"))
	void K2_AddActorWorldOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);

	/**
	 * Adds a delta to the location of this actor in world space.
	 * 
	 * @param DeltaLocation		The change in location.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param Teleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If TeleportPhysics, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If None, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param SweepHitResult	The hit result from the move if swept.
	 */
	void AddActorWorldOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the rotation of this actor in world space.
	 * 
	 * @param DeltaRotation		The change in rotation.
	 * @param bSweep			Whether to sweep to the target rotation (not currently supported for rotation).
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 * @param SweepHitResult	The hit result from the move if swept.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorWorldRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_AddActorWorldRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddActorWorldRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorWorldRotation(const FQuat& DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/** Adds a delta to the transform of this actor in world space. Scale is unchanged. */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorWorldTransform"))
	void K2_AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddActorWorldTransform(const FTransform& DeltaTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/** 
	 * Set the Actors transform to the specified one.
	 * @param NewTransform		The new transform.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetActorTransform"))
	bool K2_SetActorTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	bool SetActorTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/** 
	 * Adds a delta to the location of this component in its local reference frame.
	 * @param DelatLocation		The change in location in local space.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorLocalOffset", Keywords="location position"))
	void K2_AddActorLocalOffset(FVector DeltaLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddActorLocalOffset(FVector DeltaLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the rotation of this component in its local reference frame
	 * @param DeltaRotation		The change in rotation in local space.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorLocalRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_AddActorLocalRotation(FRotator DeltaRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddActorLocalRotation(FRotator DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void AddActorLocalRotation(const FQuat& DeltaRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Adds a delta to the transform of this component in its local reference frame
	 * @param NewTransform		The change in transform in local space.
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="AddActorLocalTransform"))
	void K2_AddActorLocalTransform(const FTransform& NewTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void AddActorLocalTransform(const FTransform& NewTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);


	/**
	 * Set the actor's RootComponent to the specified relative location.
	 * @param NewRelativeLocation	New relative location of the actor's root component
	 * @param bSweep				Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *								Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport				Whether we teleport the physics state (if physics collision is enabled for this object).
	 *								If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *								If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *								If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetActorRelativeLocation"))
	void K2_SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetActorRelativeLocation(FVector NewRelativeLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Set the actor's RootComponent to the specified relative rotation
	 * @param NewRelativeRotation	New relative rotation of the actor's root component
	 * @param bSweep				Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *								Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport				Whether we teleport the physics state (if physics collision is enabled for this object).
	 *								If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *								If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *								If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetActorRelativeRotation", AdvancedDisplay="bSweep,SweepHitResult,bTeleport"))
	void K2_SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetActorRelativeRotation(FRotator NewRelativeRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);
	void SetActorRelativeRotation(const FQuat& NewRelativeRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Set the actor's RootComponent to the specified relative transform
	 * @param NewRelativeTransform		New relative transform of the actor's root component
	 * @param bSweep			Whether we sweep to the destination location, triggering overlaps along the way and stopping short of the target if blocked by something.
	 *							Only the root component is swept and checked for blocking collision, child components move without sweeping. If collision is off, this has no effect.
	 * @param bTeleport			Whether we teleport the physics state (if physics collision is enabled for this object).
	 *							If true, physics velocity for this object is unchanged (so ragdoll parts are not affected by change in location).
	 *							If false, physics velocity is updated based on the change in position (affecting ragdoll parts).
	 *							If CCD is on and not teleporting, this will affect objects along the entire swept volume.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation", meta=(DisplayName="SetActorRelativeTransform"))
	void K2_SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep, FHitResult& SweepHitResult, bool bTeleport);
	void SetActorRelativeTransform(const FTransform& NewRelativeTransform, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr, ETeleportType Teleport = ETeleportType::None);

	/**
	 * Set the actor's RootComponent to the specified relative scale 3d
	 * @param NewRelativeScale	New scale to set the actor's RootComponent to
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Transformation")
	void SetActorRelativeScale3D(FVector NewRelativeScale);

	/**
	 * Return the actor's relative scale 3d
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Orientation")
	FVector GetActorRelativeScale3D() const;

	/**
	 *	Sets the actor to be hidden in the game
	 *	@param	bNewHidden	Whether or not to hide the actor and all its components
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering", meta=( DisplayName = "Set Actor Hidden In Game", Keywords = "Visible Hidden Show Hide" ))
	virtual void SetActorHiddenInGame(bool bNewHidden);

	/** Allows enabling/disabling collision for the whole actor */
	UFUNCTION(BlueprintCallable, Category="Collision")
	void SetActorEnableCollision(bool bNewActorEnableCollision);

	/** Get current state of collision for the whole actor */
	UFUNCTION(BlueprintPure, Category="Collision")
	bool GetActorEnableCollision() const;

	/** Destroy the actor */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "Delete", DisplayName = "DestroyActor"))
	virtual void K2_DestroyActor();

	/** Returns whether this actor has network authority */
	UFUNCTION(BlueprintCallable, Category="Networking")
	bool HasAuthority() const;

	/** 
	 * Creates a new component and assigns ownership to the Actor this is 
	 * called for. Automatic attachment causes the first component created to 
	 * become the root, and all subsequent components to be attached under that 
	 * root. When bManualAttachment is set, automatic attachment is 
	 * skipped and it is up to the user to attach the resulting component (or 
	 * set it up as the root) themselves.
	 *
	 * @see UK2Node_AddComponent	DO NOT CALL MANUALLY - BLUEPRINT INTERNAL USE ONLY (for Add Component nodes)
	 *
	 * @param TemplateName					The name of the Component Template to use.
	 * @param bManualAttachment				Whether manual or automatic attachment is to be used
	 * @param RelativeTransform				The relative transform between the new component and its attach parent (automatic only)
	 * @param ComponentTemplateContext		Optional UBlueprintGeneratedClass reference to use to find the template in. If null (or not a BPGC), component is sought in this Actor's class
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", DefaultToSelf="ComponentTemplateContext", InternalUseParam="ComponentTemplateContext"))
	class UActorComponent* AddComponent(FName TemplateName, bool bManualAttachment, const FTransform& RelativeTransform, const UObject* ComponentTemplateContext);

	/** DEPRECATED - Use Component::DestroyComponent */
	DEPRECATED(4.17, "Use Component.DestroyComponent instead")
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use Component.DestroyComponent instead", BlueprintProtected = "true", DisplayName = "DestroyComponent"))
	void K2_DestroyComponent(UActorComponent* Component);

	/** 
	 *  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered. 
	 *   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	 */
	DEPRECATED(4.12, "Please use AttachToComponent.")
	void AttachRootComponentTo(USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = false);

	/**
	 *  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	 *   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	 */
	DEPRECATED(4.17, "Use AttachToComponent instead.")
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AttachActorToComponent (Deprecated)", AttachLocationType = "KeepRelativeOffset"), Category = "Utilities|Transformation")
	void K2_AttachRootComponentTo(USceneComponent* InParent, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = true);


	/**
	 * Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	 * @param  Parent					Parent to attach to.
	 * @param  SocketName				Optional socket to attach to on the parent.
	 * @param  AttachmentRules			How to handle transforms when attaching.
	 * @param  bWeldSimulatedBodies		Whether to weld together simulated physics bodies.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AttachToComponent", bWeldSimulatedBodies = true), Category = "Utilities|Transformation")
	void K2_AttachToComponent(USceneComponent* Parent, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies);

	/**
	 * Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	 * @param  Parent					Parent to attach to.
	 * @param  AttachmentRules			How to handle transforms and welding when attaching.
	 * @param  SocketName				Optional socket to attach to on the parent.
	 */
	void AttachToComponent(USceneComponent* Parent, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);

	/**
	 * Attaches the RootComponent of this Actor to the RootComponent of the supplied actor, optionally at a named socket.
	 * @param InParentActor				Actor to attach this actor's RootComponent to
	 * @param InSocketName				Socket name to attach to, if any
	 * @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	 */
	DEPRECATED(4.12, "Please use AttachToActor.")
	void AttachRootComponentToActor(AActor* InParentActor, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = false);

	/**
	*  Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	*   @param AttachLocationType	Type of attachment, AbsoluteWorld to keep its world position, RelativeOffset to keep the object's relative offset and SnapTo to snap to the new parent.
	*/
	DEPRECATED(4.17, "Use AttachToActor instead.")
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AttachActorToActor (Deprecated)", AttachLocationType = "KeepRelativeOffset"), Category = "Utilities|Transformation")
	void K2_AttachRootComponentToActor(AActor* InParentActor, FName InSocketName = NAME_None, EAttachLocation::Type AttachLocationType = EAttachLocation::KeepRelativeOffset, bool bWeldSimulatedBodies = true);

	/**
	 * Attaches the RootComponent of this Actor to the RootComponent of the supplied actor, optionally at a named socket.
	 * @param ParentActor				Actor to attach this actor's RootComponent to
	 * @param AttachmentRules			How to handle transforms and modification when attaching.
	 * @param SocketName				Socket name to attach to, if any
	 */
	void AttachToActor(AActor* ParentActor, const FAttachmentTransformRules& AttachmentRules, FName SocketName = NAME_None);

	/**
	 * Attaches the RootComponent of this Actor to the supplied component, optionally at a named socket. It is not valid to call this on components that are not Registered.
	 * @param ParentActor				Actor to attach this actor's RootComponent to
	 * @param SocketName				Socket name to attach to, if any
	 * @param LocationRule				How to handle translation when attaching.
	 * @param RotationRule				How to handle rotation when attaching.
	 * @param ScaleRule					How to handle scale when attaching.
	 * @param bWeldSimulatedBodies		Whether to weld together simulated physics bodies.
	 */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "AttachToActor", bWeldSimulatedBodies=true), Category = "Utilities|Transformation")
	void K2_AttachToActor(AActor* ParentActor, FName SocketName, EAttachmentRule LocationRule, EAttachmentRule RotationRule, EAttachmentRule ScaleRule, bool bWeldSimulatedBodies);

	/** 
	 *  Snap the RootComponent of this Actor to the supplied Actor's root component, optionally at a named socket. It is not valid to call this on components that are not Registered. 
	 *  If InSocketName == NAME_None, it will attach to origin of the InParentActor. 
	 */
	DEPRECATED(4.17, "Use AttachRootComponentTo with EAttachLocation::SnapToTarget option instead")
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use AttachRootComponentTo with EAttachLocation::SnapToTarget option instead", DisplayName = "SnapActorTo"), Category="Utilities|Transformation")
	void SnapRootComponentTo(AActor* InParentActor, FName InSocketName);

	/** 
	 *  Detaches the RootComponent of this Actor from any SceneComponent it is currently attached to. 
	 *   @param bMaintainWorldTransform	If true, update the relative location/rotation of this component to keep its world position the same
	 */
	DEPRECATED(4.17, "Use DetachFromActor instead")
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "DetachActorFromActor (Deprecated)"), Category="Utilities|Transformation")
	void DetachRootComponentFromParent(bool bMaintainWorldPosition = true);

	/** 
	 * Detaches the RootComponent of this Actor from any SceneComponent it is currently attached to. 
	 * @param  LocationRule				How to handle translation when detaching.
	 * @param  RotationRule				How to handle rotation when detaching.
	 * @param  ScaleRule				How to handle scale when detaching.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "DetachFromActor"), Category="Utilities|Transformation")
	void K2_DetachFromActor(EDetachmentRule LocationRule = EDetachmentRule::KeepRelative, EDetachmentRule RotationRule = EDetachmentRule::KeepRelative, EDetachmentRule ScaleRule = EDetachmentRule::KeepRelative);

	/** 
	 * Detaches the RootComponent of this Actor from any SceneComponent it is currently attached to. 
	 * @param  DetachmentRules			How to handle transforms when detaching.
	 */
	void DetachFromActor(const FDetachmentTransformRules& DetachmentRules);

	/** 
	 *	DEPRECATED: Detaches all SceneComponents in this Actor from the supplied parent SceneComponent. 
	 *	@param InParentComponent		SceneComponent to detach this actor's components from
	 *	@param bMaintainWorldTransform	If true, update the relative location/rotation of this component to keep its world position the same
	 */
	DEPRECATED(4.12, "Please use DetachAllSceneComponents")
	void DetachSceneComponentsFromParent(USceneComponent* InParentComponent, bool bMaintainWorldPosition = true);

	/**
	*	Detaches all SceneComponents in this Actor from the supplied parent SceneComponent.
	*	@param InParentComponent		SceneComponent to detach this actor's components from
	*	@param DetachmentRules			Rules to apply when detaching components
	*/
	void DetachAllSceneComponents(class USceneComponent* InParentComponent, const FDetachmentTransformRules& DetachmentRules);

	//~==============================================================================
	// Tags

	/** See if this actor contains the supplied tag */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool ActorHasTag(FName Tag) const;


	//~==============================================================================
	// Misc Blueprint support

	/** 
	 * Get CustomTimeDilation - this can be used for input control or speed control for slomo.
	 * We don't want to scale input globally because input can be used for UI, which do not care for TimeDilation.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|Time")
	float GetActorTimeDilation() const;

	/** Make this actor tick after PrerequisiteActor. This only applies to this actor's tick function; dependencies for owned components must be set up separately if desired. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Make this actor tick after PrerequisiteComponent. This only applies to this actor's tick function; dependencies for owned components must be set up separately if desired. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void AddTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);

	/** Remove tick dependency on PrerequisiteActor. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteActor(AActor* PrerequisiteActor);

	/** Remove tick dependency on PrerequisiteComponent. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	virtual void RemoveTickPrerequisiteComponent(UActorComponent* PrerequisiteComponent);
	
	/** Gets whether this actor can tick when paused. */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool GetTickableWhenPaused();

	/** Sets whether this actor can tick when paused. */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetTickableWhenPaused(bool bTickableWhenPaused);

	/** Allocate a MID for a given parent material. */
	DEPRECATED(4.17, "Use PrimitiveComponent.CreateAndSetMaterialInstanceDynamic instead.")
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PrimitiveComponent.CreateAndSetMaterialInstanceDynamic instead.", BlueprintProtected = "true"), Category="Rendering|Material")
	class UMaterialInstanceDynamic* MakeMIDForMaterial(class UMaterialInterface* Parent);

	/** The number of seconds (in game time) since this Actor was created, relative to Get Game Time In Seconds. */
	UFUNCTION(BlueprintPure, Category = Actor)
	float GetGameTimeSinceCreation();

	//~=============================================================================
	// AI functions.
	
	/**
	 * Trigger a noise caused by a given Pawn, at a given location.
	 * Note that the NoiseInstigator Pawn MUST have a PawnNoiseEmitterComponent for the noise to be detected by a PawnSensingComponent.
	 * Senders of MakeNoise should have an Instigator if they are not pawns, or pass a NoiseInstigator.
	 *
	 * @param Loudness The relative loudness of this noise. Usual range is 0 (no noise) to 1 (full volume). If MaxRange is used, this scales the max range, otherwise it affects the hearing range specified by the sensor.
	 * @param NoiseInstigator Pawn responsible for this noise.  Uses the actor's Instigator if NoiseInstigator=NULL
	 * @param NoiseLocation Position of noise source.  If zero vector, use the actor's location.
	 * @param MaxRange Max range at which the sound may be heard. A value of 0 indicates no max range (though perception may have its own range). Loudness scales the range. (Note: not supported for legacy PawnSensingComponent, only for AIPerception)
	 * @param Tag Identifier for the noise.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI", meta=(BlueprintProtected = "true"))
	void MakeNoise(float Loudness=1.f, APawn* NoiseInstigator=NULL, FVector NoiseLocation=FVector::ZeroVector, float MaxRange = 0.f, FName Tag = NAME_None);

protected:
	/** Event when play begins for this actor. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "BeginPlay"))
	void ReceiveBeginPlay();

	/** Overridable native event for when play begins for this actor. */
	virtual void BeginPlay();

public:

	/** Initiate a begin play call on this Actor, will handle . */
	void DispatchBeginPlay();

	/** Returns whether an actor has been initialized */
	bool IsActorInitialized() const { return bActorInitialized; }

	/** Returns whether an actor is in the process of beginning play */
	bool IsActorBeginningPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::BeginningPlay; }

	/** Returns whether an actor has had BeginPlay called on it (and not subsequently had EndPlay called) */
	bool HasActorBegunPlay() const { return ActorHasBegunPlay == EActorBeginPlayState::HasBegunPlay; }

	UFUNCTION(BlueprintCallable, Category="Game")
	bool IsActorBeingDestroyed() const 
	{
		return bActorIsBeingDestroyed;
	}

	/** Event when this actor takes ANY damage */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(DisplayName = "AnyDamage"), Category="Game|Damage")
	void ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);
	
	/** 
	 * Event when this actor takes RADIAL damage 
	 * @todo Pass it the full array of hits instead of just one?
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(DisplayName = "RadialDamage"), Category="Game|Damage")
	void ReceiveRadialDamage(float DamageReceived, const class UDamageType* DamageType, FVector Origin, const struct FHitResult& HitInfo, class AController* InstigatedBy, AActor* DamageCauser);

	/** Event when this actor takes POINT damage */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, meta=(DisplayName = "PointDamage"), Category="Game|Damage")
	void ReceivePointDamage(float Damage, const class UDamageType* DamageType, FVector HitLocation, FVector HitNormal, class UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, class AController* InstigatedBy, AActor* DamageCauser, const FHitResult& HitInfo);

	DEPRECATED(4.14, "Call the updated version of ReceivePointDamage that takes a FHitResult.")
	void ReceivePointDamage(float Damage, const class UDamageType* DamageType, FVector HitLocation, FVector HitNormal, class UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, class AController* InstigatedBy, AActor* DamageCauser);

	/** Event called every frame */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Tick"))
	void ReceiveTick(float DeltaSeconds);

	/** 
	 *	Event when this actor overlaps another actor, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	/** 
	 *	Event when this actor overlaps another actor, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorBeginOverlap"), Category="Collision")
	void ReceiveActorBeginOverlap(AActor* OtherActor);

	/** 
	 *	Event when an actor no longer overlaps another actor, and they have separated. 
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	virtual void NotifyActorEndOverlap(AActor* OtherActor);
	/** 
	 *	Event when an actor no longer overlaps another actor, and they have separated. 
	 *	@note Components on both this and the other Actor must have bGenerateOverlapEvents set to true to generate overlap events.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorEndOverlap"), Category="Collision")
	void ReceiveActorEndOverlap(AActor* OtherActor);

	/** Event when this actor has the mouse moved over it with the clickable interface. */
	virtual void NotifyActorBeginCursorOver();
	/** Event when this actor has the mouse moved over it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorBeginCursorOver"), Category="Mouse Input")
	void ReceiveActorBeginCursorOver();

	/** Event when this actor has the mouse moved off of it with the clickable interface. */
	virtual void NotifyActorEndCursorOver();
	/** Event when this actor has the mouse moved off of it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorEndCursorOver"), Category="Mouse Input")
	void ReceiveActorEndCursorOver();

	/** Event when this actor is clicked by the mouse when using the clickable interface. */
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton);
	/** Event when this actor is clicked by the mouse when using the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorOnClicked"), Category="Mouse Input")
	void ReceiveActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton);

	/** Event when this actor is under the mouse when left mouse button is released while using the clickable interface. */
	virtual void NotifyActorOnReleased(FKey ButtonReleased = EKeys::LeftMouseButton);
	/** Event when this actor is under the mouse when left mouse button is released while using the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "ActorOnReleased"), Category="Mouse Input")
	void ReceiveActorOnReleased(FKey ButtonReleased = EKeys::LeftMouseButton);

	/** Event when this actor is touched when click events are enabled. */
	virtual void NotifyActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex);
	/** Event when this actor is touched when click events are enabled. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "BeginInputTouch"), Category="Touch Input")
	void ReceiveActorOnInputTouchBegin(const ETouchIndex::Type FingerIndex);

	/** Event when this actor is under the finger when untouched when click events are enabled. */
	virtual void NotifyActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex);
	/** Event when this actor is under the finger when untouched when click events are enabled. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "EndInputTouch"), Category="Touch Input")
	void ReceiveActorOnInputTouchEnd(const ETouchIndex::Type FingerIndex);

	/** Event when this actor has a finger moved over it with the clickable interface. */
	virtual void NotifyActorOnInputTouchEnter(const ETouchIndex::Type FingerIndex);
	/** Event when this actor has a finger moved over it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "TouchEnter"), Category="Touch Input")
	void ReceiveActorOnInputTouchEnter(const ETouchIndex::Type FingerIndex);

	/** Event when this actor has a finger moved off of it with the clickable interface. */
	virtual void NotifyActorOnInputTouchLeave(const ETouchIndex::Type FingerIndex);
	/** Event when this actor has a finger moved off of it with the clickable interface. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "TouchLeave"), Category="Touch Input")
	void ReceiveActorOnInputTouchLeave(const ETouchIndex::Type FingerIndex);

	/** 
	 * Returns list of actors this actor is overlapping (any component overlapping any component). Does not return itself.
	 * @param OverlappingActors		[out] Returned list of overlapping actors
	 * @param ClassFilter			[optional] If set, only returns actors of this class or subclasses
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingActors(TArray<AActor*>& OverlappingActors, TSubclassOf<AActor> ClassFilter=nullptr) const;

	/** 
	 * Returns set of actors this actor is overlapping (any component overlapping any component). Does not return itself.
	 * @param OverlappingActors		[out] Returned list of overlapping actors
	 * @param ClassFilter			[optional] If set, only returns actors of this class or subclasses
	 */
	void GetOverlappingActors(TSet<AActor*>& OverlappingActors, TSubclassOf<AActor> ClassFilter=nullptr) const;

	/** Returns list of components this actor is overlapping. */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingComponents(TArray<UPrimitiveComponent*>& OverlappingComponents) const;

	/** Returns set of components this actor is overlapping. */
	void GetOverlappingComponents(TSet<UPrimitiveComponent*>& OverlappingComponents) const;

	/** 
	 * Event when this actor bumps into a blocking object, or blocks another actor that bumps into it.
	 * This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 * For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *
	 * @note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled.
	 * @note When receiving a hit from another object's movement (bSelfMoved is false), the directions of 'Hit.Normal' and 'Hit.ImpactNormal'
	 * will be adjusted to indicate force from the other object against this object.
	 */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);
	/** 
	 * Event when this actor bumps into a blocking object, or blocks another actor that bumps into it.
	 * This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 * For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *
	 * @note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled.
	 * @note When receiving a hit from another object's movement (bSelfMoved is false), the directions of 'Hit.Normal' and 'Hit.ImpactNormal'
	 * will be adjusted to indicate force from the other object against this object.
	 * @note NormalImpulse will be filled in for physics-simulating bodies, but will be zero for swept-component blocking collisions.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Hit"), Category="Collision")
	void ReceiveHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit);


	/** Set the lifespan of this actor. When it expires the object will be destroyed. If requested lifespan is 0, the timer is cleared and the actor will not be destroyed. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "delete destroy"))
	virtual void SetLifeSpan( float InLifespan );

	/** Get the remaining lifespan of this actor. If zero is returned the actor lives forever. */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "delete destroy"))
	virtual float GetLifeSpan() const;

	/**
	 * Construction script, the place to spawn components and do other setup.
	 * @note Name used in CreateBlueprint function
	 * @param	Location	The location.
	 * @param	Rotation	The rotation.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintInternalUseOnly = "true", DisplayName = "Construction Script"))
	void UserConstructionScript();

	/**
	 * Destroy this actor. Returns true the actor is destroyed or already marked for destruction, false if indestructible.
	 * Destruction is latent. It occurs at the end of the tick.
	 * @param	bNetForce				[opt] Ignored unless called during play.  Default is false.
	 * @param	bShouldModifyLevel		[opt] If true, Modify() the level before removing the actor.  Default is true.	
	 * returns	true if destroyed or already marked for destruction, false if indestructible.
	 */
	bool Destroy(bool bNetForce = false, bool bShouldModifyLevel = true );

	UFUNCTION(BlueprintImplementableEvent, meta = (Keywords = "delete", DisplayName = "Destroyed"))
	void ReceiveDestroyed();

	/** Event triggered when the actor is destroyed. */
	UPROPERTY(BlueprintAssignable, Category="Game")
	FActorDestroyedSignature OnDestroyed;


	/** Event to notify blueprints this actor is about to be deleted. */
	UFUNCTION(BlueprintImplementableEvent, meta=(Keywords = "delete", DisplayName = "End Play"))
	void ReceiveEndPlay(EEndPlayReason::Type EndPlayReason);

	/** Event triggered when the actor is being removed from a level. */
	UPROPERTY(BlueprintAssignable, Category="Game")
	FActorEndPlaySignature OnEndPlay;
	
	//~ Begin UObject Interface
	virtual bool CheckDefaultSubobjectsInternal() override;
	virtual void PostInitProperties() override;
	virtual bool Modify( bool bAlwaysMarkDirty=true ) override;
	virtual void ProcessEvent( UFunction* Function, void* Parameters ) override;
	virtual int32 GetFunctionCallspace( UFunction* Function, void* Parameters, FFrame* Stack ) override;
	virtual bool CallRemoteFunction( UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack ) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph ) override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual bool Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL, ERenameFlags Flags=REN_None ) override;
	virtual bool CanBeInCluster() const override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditImport() override;
	virtual bool IsSelectedInEditor() const override;

	struct FActorRootComponentReconstructionData
	{
		// Struct to store info about attached actors
		struct FAttachedActorInfo
		{
			TWeakObjectPtr<AActor> Actor;
			TWeakObjectPtr<USceneComponent> AttachParent;
			FName AttachParentName;
			FName SocketName;
			FTransform RelativeTransform;
		};

		// The RootComponent's transform
		FTransform Transform;

		// The RootComponent's relative rotation cache (enforces using the same rotator)
		FRotationConversionCache TransformRotationCache;

		// The Actor the RootComponent is attached to
		FAttachedActorInfo AttachedParentInfo;

		// Actors that are attached to this RootComponent
		TArray<FAttachedActorInfo> AttachedToInfo;
	};

	class FActorTransactionAnnotation : public ITransactionObjectAnnotation
	{
	public:
		FActorTransactionAnnotation(const AActor* Actor, const bool bCacheRootComponentData = true);

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

		bool HasInstanceData() const;

		FComponentInstanceDataCache ComponentInstanceData;

		// Root component reconstruction data
		bool bRootComponentDataCached;
		FActorRootComponentReconstructionData RootComponentData;
	};

	/* Cached pointer to the transaction annotation data from PostEditUndo to be used in the next RerunConstructionScript */
	TSharedPtr<FActorTransactionAnnotation> CurrentTransactionAnnotation;

	virtual TSharedPtr<ITransactionObjectAnnotation> GetTransactionAnnotation() const override;
	virtual void PostEditUndo(TSharedPtr<ITransactionObjectAnnotation> TransactionAnnotation) override;

	/** @return true if the component is allowed to re-register its components when modified.  False for CDOs or PIE instances. */
	bool ReregisterComponentsWhenModified() const;
#endif // WITH_EDITOR
	//~ End UObject Interface

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished);
#endif // WITH_EDITOR

	//-----------------------------------------------------------------------------------------------
	// PROPERTY REPLICATION

	/** fills ReplicatedMovement property */
	virtual void GatherCurrentMovement();

	/**
	 * See if this actor is owned by TestOwner.
	 */
	inline bool IsOwnedBy( const AActor* TestOwner ) const
	{
		for( const AActor* Arg=this; Arg; Arg=Arg->Owner )
		{
			if( Arg == TestOwner )
				return true;
		}
		return false;
	}

	/** Returns this actor's root component. */
	FORCEINLINE USceneComponent* GetRootComponent() const { return RootComponent; }

	/**
	 * Returns this actor's default attachment component for attaching children to
	 * @return The scene component to be used as parent
	 */
	virtual USceneComponent* GetDefaultAttachComponent() const { return GetRootComponent(); }

	/**
	 * Sets root component to be the specified component.  NewRootComponent's owner should be this actor.
	 * @return true if successful
	 */
	bool SetRootComponent(USceneComponent* NewRootComponent);

	/** Returns the transform of the RootComponent of this Actor*/ 
	FORCEINLINE FTransform GetActorTransform() const
	{
		return TemplateGetActorTransform(RootComponent);
	}

	/** Returns the location of the RootComponent of this Actor*/ 
	FORCEINLINE FVector GetActorLocation() const
	{
		return TemplateGetActorLocation(RootComponent);
	}

	/** Returns the rotation of the RootComponent of this Actor */
	FORCEINLINE FRotator GetActorRotation() const
	{
		return TemplateGetActorRotation(RootComponent);
	}

	/** Returns the scale of the RootComponent of this Actor */
	FORCEINLINE FVector GetActorScale() const
	{
		return TemplateGetActorScale(RootComponent);
	}

	/** Returns the quaternion of the RootComponent of this Actor */
	FORCEINLINE FQuat GetActorQuat() const
	{
		return TemplateGetActorQuat(RootComponent);
	}

#if WITH_EDITOR
	/** Sets the local space offset added to the actor's pivot as used by the editor */
	FORCEINLINE void SetPivotOffset(const FVector& InPivotOffset)
	{
		PivotOffset = InPivotOffset;
	}

	/** Gets the local space offset added to the actor's pivot as used by the editor */
	FORCEINLINE FVector GetPivotOffset() const
	{
		return PivotOffset;
	}
#endif

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

	/** 
	 * Called by owning level to shift an actor location and all relevant data structures by specified delta
	 *  
	 * @param InWorldOffset	 Offset vector to shift actor location
	 * @param bWorldShift	 Whether this call is part of whole world shifting
	 */
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift);

	/** 
	 * Indicates whether this actor should participate in level bounds calculations
	 */
	virtual bool IsLevelBoundsRelevant() const { return true; }

#if WITH_EDITOR
	// Editor specific

	/** @todo: Remove this flag once it is decided that additive interactive scaling is what we want */
	static bool bUsePercentageBasedScaling;

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to translate the actor's location.
	 */
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to modify the actor's rotation.
	 */
	virtual void EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/**
	 * Called by ApplyDeltaToActor to perform an actor class-specific operation based on widget manipulation.
	 * The default implementation is simply to modify the actor's draw scale.
	 */
	virtual void EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown);

	/** Called by MirrorActors to perform a mirroring operation on the actor */
	virtual void EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation);

	/** Set LOD Parent Primitive*/
	void SetLODParent(class UPrimitiveComponent* InLODParent, float InParentDrawDistance);

	/**
	 * Simple accessor to check if the actor is hidden upon editor startup
	 * @return	true if the actor is hidden upon editor startup; false if it is not
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Editing")
	bool IsHiddenEdAtStartup() const
	{
		return bHiddenEd;
	}

	// Returns true if this actor is hidden in the editor viewports.
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Editing")
	bool IsHiddenEd() const;

	/**
	 * Sets whether or not this actor is hidden in the editor for the duration of the current editor session
	 *
	 * @param bIsHidden	True if the actor is hidden
	 */
	UFUNCTION(BlueprintCallable, Category="Editor Scripting | Actor Editing")
	virtual void SetIsTemporarilyHiddenInEditor( bool bIsHidden );

	/**
	 * @param  bIncludeParent - Whether to recurse up child actor hierarchy or not
	 * @return Whether or not this actor is hidden in the editor for the duration of the current editor session
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Editing")
	bool IsTemporarilyHiddenInEditor(bool bIncludeParent = false) const;

	/** @return	Returns true if this actor is allowed to be displayed, selected and manipulated by the editor. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Editing")
	bool IsEditable() const;

	/** @return	Returns true if this actor can EVER be selected in a level in the editor.  Can be overridden by specific actors to make them unselectable. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Actor Editing")
	virtual bool IsSelectable() const { return true; }

	/** @return	Returns true if this actor should be shown in the scene outliner */
	bool IsListedInSceneOutliner() const;

	/** @return	Returns true if this actor is allowed to be attached to the given actor */
	virtual bool EditorCanAttachTo(const AActor* InParent, FText& OutReason) const;

	// Called before editor copy, true allow export
	virtual bool ShouldExport() { return true; }

	// Called before editor paste, true allow import
	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) { return true; }

	/** Called by InputKey when an unhandled key is pressed with a selected actor */
	virtual void EditorKeyPressed(FKey Key, EInputEvent Event) {}

	/** Called by ReplaceSelectedActors to allow a new actor to copy properties from an old actor when it is replaced */
	virtual void EditorReplacedActor(AActor* OldActor) {}

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForErrors();

	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
	virtual void CheckForDeprecated();

	/**
	 * Returns this actor's current label.  Actor labels are only available in development builds.
	 * @return	The label text
	 */
	const FString& GetActorLabel() const;

	/**
	 * Assigns a new label to this actor.  Actor labels are only available in development builds.
	 * @param	NewActorLabel	The new label string to assign to the actor.  If empty, the actor will have a default label.
	 * @param	bMarkDirty		If true the actor's package will be marked dirty for saving.  Otherwise it will not be.  You should pass false for this parameter if dirtying is not allowed (like during loads)
	 */
	void SetActorLabel( const FString& NewActorLabel, bool bMarkDirty = true );

	/** Advanced - clear the actor label. */
	void ClearActorLabel();

	/**
	 * Returns if this actor's current label is editable.  Actor labels are only available in development builds.
	 * @return	The editable status of the actor's label
	 */
	bool IsActorLabelEditable() const;

	/**
	 * Returns this actor's folder path. Actor folder paths are only available in development builds.
	 * @return	The folder path
	 */
	const FName& GetFolderPath() const;

	/**
	 * Assigns a new folder to this actor. Actor folder paths are only available in development builds.
	 * @param	NewFolderPath		The new folder to assign to the actor.
	 */
	void SetFolderPath(const FName& NewFolderPath);

	/**
	 * Assigns a new folder to this actor and any attached children. Actor folder paths are only available in development builds.
	 * @param	NewFolderPath		The new folder to assign to the actors.
	 */
	void SetFolderPath_Recursively(const FName& NewFolderPath);

	/**
	 * Used by the "Sync to Content Browser" right-click menu option in the editor.
	 * @param	Objects	Array to add content object references to.
	 * @return	Whether the object references content (all overrides of this function should return true)
	 */
	virtual bool GetReferencedContentObjects( TArray<UObject*>& Objects ) const;

	/** Returns NumUncachedStaticLightingInteractions for this actor */
	const int32 GetNumUncachedStaticLightingInteractions() const;

#endif		// WITH_EDITOR

	/**
	 * @param ViewPos		Position of the viewer
	 * @param ViewDir		Vector direction of viewer
	 * @param Viewer		"net object" owned by the client for whom net priority is being determined (typically player controller)
	 * @param ViewTarget	The actor that is currently being viewed/controlled by Viewer, usually a pawn
	 * @param InChannel		Channel on which this actor is being replicated.
	 * @param Time			Time since actor was last replicated
	 * @param bLowBandwidth True if low bandwidth of viewer
	 * @return				Priority of this actor for replication
	 */
	virtual float GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, class AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth);

	/**
	 * Similar to GetNetPriority, but will only be used for prioritizing actors while recording a replay.
	 *
	 * @param ViewPos		Position of the viewer
	 * @param ViewDir		Vector direction of viewer
	 * @param Viewer		"net object" owned by the client for whom net priority is being determined (typically player controller)
	 * @param ViewTarget	The actor that is currently being viewed/controlled by Viewer, usually a pawn
	 * @param InChannel		Channel on which this actor is being replicated.
	 * @param Time			Time since actor was last replicated
	 */
	virtual float GetReplayPriority(const FVector& ViewPos, const FVector& ViewDir, class AActor* Viewer, AActor* ViewTarget, UActorChannel* const InChannel, float Time);

	/** Returns true if the actor should be dormant for a specific net connection. Only checked for DORM_DormantPartial */
	virtual bool GetNetDormancy(const FVector& ViewPos, const FVector& ViewDir, class AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth);

	/** 
	 * Allows for a specific response from the actor when the actor channel is opened (client side)
	 * @param InBunch Bunch received at time of open
	 * @param Connection the connection associated with this actor
	 */
	virtual void OnActorChannelOpen(class FInBunch& InBunch, class UNetConnection* Connection) {};

	/**
	 * Used by the net connection to determine if a net owning actor should switch to using the shortened timeout value
	 * 
	 * @return true to switch from InitialConnectTimeout to ConnectionTimeout values on the net driver
	 */
	virtual bool UseShortConnectTimeout() const { return false; }

	/**
	 * SerializeNewActor has just been called on the actor before network replication (server side)
	 * @param OutBunch Bunch containing serialized contents of actor prior to replication
	 */
	virtual void OnSerializeNewActor(class FOutBunch& OutBunch) {};

	/** 
	 * Handles cleaning up the associated Actor when killing the connection 
	 * @param Connection the connection associated with this actor
	 */
	virtual void OnNetCleanup(class UNetConnection* Connection) {};

	/** Swaps Role and RemoteRole if client */
	void ExchangeNetRoles(bool bRemoteOwner);

	/** The replay system calls this to hack the Role and RemoteRole while recording replays on a client. Only call this if you know what you're doing! */
	void SwapRolesForReplay();

	/**
	 * When called, will call the virtual call chain to register all of the tick functions for both the actor and optionally all components
	 * Do not override this function or make it virtual
	 * @param bRegister - true to register, false, to unregister
	 * @param bDoComponents - true to also apply the change to all components
	 */
	void RegisterAllActorTickFunctions(bool bRegister, bool bDoComponents);

	/** 
	 * Set this actor's tick functions to be enabled or disabled. Only has an effect if the function is registered
	 * This only modifies the tick function on actor itself
	 * @param	bEnabled	Whether it should be enabled or not
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetActorTickEnabled(bool bEnabled);

	/**  Returns whether this actor has tick enabled or not	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	bool IsActorTickEnabled() const;

	/** 
	* Sets the tick interval of this actor's primary tick function. Will not enable a disabled tick function. Takes effect on next tick. 
	* @param TickInterval	The rate at which this actor should be ticking
	*/
	UFUNCTION(BlueprintCallable, Category="Utilities")
	void SetActorTickInterval(float TickInterval);

	/**  Returns the tick interval of this actor's primary tick function */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	float GetActorTickInterval() const;

	/**
	 *	ticks the actor
	 *	@param	DeltaTime			The time slice of this tick
	 *	@param	TickType			The type of tick that is happening
	 *	@param	ThisTickFunction	The tick function that is firing, useful for getting the completion handle
	 */
	virtual void TickActor( float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction );

	/**
	 * Called when an actor is done spawning into the world (from UWorld::SpawnActor).
	 * For actors with a root component, the location and rotation will have already been set.
	 * Takes place after any construction scripts have been called
	 */
	virtual void PostActorCreated();

	/** Called when the lifespan of an actor expires (if he has one). */
	virtual void LifeSpanExpired();

	// Always called immediately before properties are received from the remote.
	virtual void PreNetReceive() override;
	
	// Always called immediately after properties are received from the remote.
	virtual void PostNetReceive() override;

	/** IsNameStableForNetworking means an object can be referred to its path name (relative to outer) over the network */
	virtual bool IsNameStableForNetworking() const override;

	/** IsSupportedForNetworking means an object can be referenced over the network */
	virtual bool IsSupportedForNetworking() const override;

	/** Returns a list of sub-objects that have stable names for networking */
	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*> &ObjList) override;

	// Always called immediately after spawning and reading in replicated properties
	virtual void PostNetInit();

	/** ReplicatedMovement struct replication event */
	UFUNCTION()
	virtual void OnRep_ReplicatedMovement();

	/** Update location and rotation from ReplicatedMovement. Not called for simulated physics! */
	virtual void PostNetReceiveLocationAndRotation();

	/** Update velocity - typically from ReplicatedMovement, not called for simulated physics! */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity);

	/** Update and smooth simulated physic state, replaces PostNetReceiveLocation() and PostNetReceiveVelocity() */
	virtual void PostNetReceivePhysicState();

protected:
	/** Sync IsSimulatingPhysics() with ReplicatedMovement.bRepPhysics */
	void SyncReplicatedPhysicsSimulation();

public:

	/** 
	 * Set the owner of this Actor, used primarily for network replication. 
	 * @param NewOwner	The Actor whom takes over ownership of this Actor
	 */
	UFUNCTION(BlueprintCallable, Category=Actor)
	virtual void SetOwner( AActor* NewOwner );

	/**
	 * Get the owner of this Actor, used primarily for network replication.
	 * @return Actor that owns this Actor
	 */
	UFUNCTION(BlueprintCallable, Category=Actor)
	AActor* GetOwner() const;

	/**
	 * This will check to see if the Actor is still in the world.  It will check things like
	 * the KillZ, outside world bounds, etc. and handle the situation.
	 */
	virtual bool CheckStillInWorld();

	//--------------------------------------------------------------------------------------
	// Actor overlap tracking
	
	/**
	 * Dispatch all EndOverlap for all of the Actor's PrimitiveComponents. 
	 * Generally used when removing the Actor from the world.
	 */
	void ClearComponentOverlaps();

	/** 
	 * Queries world and updates overlap detection state for this actor.
	 * @param bDoNotifies		True to dispatch being/end overlap notifications when these events occur.
	 */
	void UpdateOverlaps(bool bDoNotifies=true);
	
	/** 
	 * Check whether any component of this Actor is overlapping any component of another Actor.
	 * @param Other The other Actor to test against
	 * @return Whether any component of this Actor is overlapping any component of another Actor.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	bool IsOverlappingActor(const AActor* Other) const;

	/** Returns whether a MatineeActor is currently controlling this Actor */
	bool IsMatineeControlled() const;

	/** See if the root component has ModifyFrequency of MF_Static */
	bool IsRootComponentStatic() const;

	/** See if the root component has Mobility of EComponentMobility::Stationary */
	bool IsRootComponentStationary() const;

	/** See if the root component has Mobility of EComponentMobility::Movable */
	bool IsRootComponentMovable() const;

	//--------------------------------------------------------------------------------------
	// Actor ticking

	/** accessor for the value of bCanEverTick */
	FORCEINLINE bool CanEverTick() const { return PrimaryActorTick.bCanEverTick; }

	/** 
	 *	Function called every frame on this Actor. Override this function to implement custom logic to be executed every frame.
	 *	Note that Tick is disabled by default, and you will need to check PrimaryActorTick.bCanEverTick is set to true to enable it.
	 *
	 *	@param	DeltaSeconds	Game time elapsed during last frame modified by the time dilation
	 */
	virtual void Tick( float DeltaSeconds );

	/** If true, actor is ticked even if TickType==LEVELTICK_ViewportsOnly	 */
	virtual bool ShouldTickIfViewportsOnly() const;

	//--------------------------------------------------------------------------------------
	// Actor relevancy determination

protected:

	/**
	 * Determines whether or not the distance between the given SrcLocation and the Actor's location
	 * is within the net relevancy distance. Actors outside relevancy distance may not be replicated.
	 *
	 * @param SrcLocation	Location to test against.
	 * @return True if the actor is within net relevancy distance, false otherwise.
	 */
	bool IsWithinNetRelevancyDistance(const FVector& SrcLocation) const;
	
public:	

	/** 
	  * @param RealViewer - is the "controlling net object" associated with the client for which network relevancy is being checked (typically player controller)
	  * @param ViewTarget - is the Actor being used as the point of view for the RealViewer
	  * @param SrcLocation - is the viewing location
	  *
	  * @return bool - true if this actor is network relevant to the client associated with RealViewer 
	  */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const;

	/**
	* @param RealViewer - is the "controlling net object" associated with the client for which network relevancy is being checked (typically player controller)
	* @param ViewTarget - is the Actor being used as the point of view for the RealViewer
	* @param SrcLocation - is the viewing location
	*
	* @return bool - true if this actor is replay relevant to the client associated with RealViewer
	*/
	virtual bool IsReplayRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation, const float CullDistanceSquared) const;

	/**
	 * Check if this actor is the owner when doing relevancy checks for actors marked bOnlyRelevantToOwner
	 *
	 * @param ReplicatedActor - the actor we're doing a relevancy test on
	 * @param ActorOwner - the owner of ReplicatedActor
	 * @param ConnectionActor - the controller of the connection that we're doing relevancy checks for
	 *
	 * @return bool - true if this actor should be considered the owner
	 */
	virtual bool IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const;

	/** Called after the actor is spawned in the world.  Responsible for setting up actor for play. */
	void PostSpawnInitialize(FTransform const& SpawnTransform, AActor* InOwner, APawn* InInstigator, bool bRemoteOwned, bool bNoFail, bool bDeferConstruction);

	/** Called to finish the spawning process, generally in the case of deferred spawning */
	void FinishSpawning(const FTransform& Transform, bool bIsDefaultTransform = false, const FComponentInstanceDataCache* InstanceDataCache = nullptr);

	/** Called after the actor has run its construction. Responsible for finishing the actor spawn process. */
	void PostActorConstruction();

public:
	/** Called immediately before gameplay begins. */
	virtual void PreInitializeComponents();

	// Allow actors to initialize themselves on the C++ side
	virtual void PostInitializeComponents();

	/**
	 * Adds a controlling matinee actor for use during matinee playback
	 * @param InMatineeActor	The matinee actor which controls this actor
	 * @todo UE4 would be nice to move this out of Actor to MatineeInterface, but it needs variable, and I can't declare variable in interface
	 *	do we still need this?
	 */
	void AddControllingMatineeActor( AMatineeActor& InMatineeActor );

	/**
	 * Removes a controlling matinee actor
	 * @param InMatineeActor	The matinee actor which currently controls this actor
	 */
	void RemoveControllingMatineeActor( AMatineeActor& InMatineeActor );

	/** Dispatches ReceiveHit virtual and OnComponentHit delegate */
	virtual void DispatchPhysicsCollisionHit(const struct FRigidBodyCollisionInfo& MyInfo, const struct FRigidBodyCollisionInfo& OtherInfo, const FCollisionImpactData& RigidCollisionData);
	
	/** @return the actor responsible for replication, if any.  Typically the player controller */
	virtual const AActor* GetNetOwner() const;

	/** @return the owning UPlayer (if any) of this actor. This will be a local player, a net connection, or NULL. */
	virtual class UPlayer* GetNetOwningPlayer();

	/**
	 * Get the owning connection used for communicating between client/server 
	 * @return NetConnection to the client or server for this actor
	 */
	virtual class UNetConnection* GetNetConnection() const;

	/**
	 * Called by DestroyActor(), gives actors a chance to op out of actor destruction
	 * Used by network code to have the net connection timeout/cleanup first
	 *
	 * @return true if DestroyActor() should not continue with actor destruction, false otherwise
	 */
	virtual bool DestroyNetworkActorHandled();

	/**
	 * Get the network mode (dedicated server, client, standalone, etc) for this actor.
	 * @see IsNetMode()
	 */
	ENetMode GetNetMode() const;

	/**
	* Test whether net mode is the given mode.
	* In optimized non-editor builds this can be more efficient than GetNetMode()
	* because it can check the static build flags without considering PIE.
	*/
	bool IsNetMode(ENetMode Mode) const;

	class UNetDriver * GetNetDriver() const;

	/** Puts actor in dormant networking state */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Networking")
	void SetNetDormancy(ENetDormancy NewDormancy);

	/** Forces dormant actor to replicate but doesn't change NetDormancy state (i.e., they will go dormant again if left dormant) */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category="Networking")
	void FlushNetDormancy();

	/** Forces properties on this actor to do a compare for one frame (rather than share shadow state) */
	void ForcePropertyCompare();

	/** Returns whether this Actor was spawned by a child actor component */
	UFUNCTION(BlueprintCallable, Category="Actor")
	bool IsChildActor() const;

	/** Returns a list of all child actors, including children of children */
	UFUNCTION(BlueprintCallable, Category="Actor")
	void GetAllChildActors(TArray<AActor*>& ChildActors, bool bIncludeDescendants = true) const;

	/** If this Actor was created by a Child Actor Component returns that Child Actor Component  */
	UFUNCTION(BlueprintCallable, Category="Actor")
	UChildActorComponent* GetParentComponent() const;

	/** If this Actor was created by a Child Actor Component returns the Actor that owns that Child Actor Component  */
	UFUNCTION(BlueprintCallable, Category="Actor")
	AActor* GetParentActor() const;

	/** Ensure that all the components in the Components array are registered */
	virtual void RegisterAllComponents();

	/** Called after all the components in the Components array are registered */
	virtual void PostRegisterAllComponents();

	/** Returns true if Actor has deferred the RegisterAllComponents() call at spawn time (e.g. pending Blueprint SCS execution to set up a scene root component). */
	FORCEINLINE bool HasDeferredComponentRegistration() const { return bHasDeferredComponentRegistration; }

	/** Returns true if Actor has a registered root component */
	bool HasValidRootComponent();

	/** Unregister all currently registered components */
	virtual void UnregisterAllComponents(bool bForReregister = false);

	/** Called after all currently registered components are cleared */
	virtual void PostUnregisterAllComponents() {}

	/** Will reregister all components on this actor. Does a lot of work - should only really be used in editor, generally use UpdateComponentTransforms or MarkComponentsRenderStateDirty. */
	virtual void ReregisterAllComponents();

	/**
	 * Incrementally registers components associated with this actor
	 *
	 * @param NumComponentsToRegister  Number of components to register in this run, 0 for all
	 * @return true when all components were registered for this actor
	 */
	bool IncrementalRegisterComponents(int32 NumComponentsToRegister);

	/** Flags all component's render state as dirty	 */
	void MarkComponentsRenderStateDirty();

	/** Update all components transforms */
	void UpdateComponentTransforms();

	/** Iterate over components array and call InitializeComponent */
	void InitializeComponents();

	/** Iterate over components array and call UninitializeComponent */
	void UninitializeComponents();

	/** Debug rendering to visualize the component tree for this actor. */
	void DrawDebugComponents(FColor const& BaseColor=FColor::White) const;

	virtual void MarkComponentsAsPendingKill();

	/** Returns true if this actor has begun the destruction process.
	 *  This is set to true in UWorld::DestroyActor, after the network connection has been closed but before any other shutdown has been performed.
	 *	@return true if this actor has begun destruction, or if this actor has been destroyed already.
	 **/
	inline bool IsPendingKillPending() const
	{
		return bActorIsBeingDestroyed || IsPendingKill();
	}

	/** Invalidate lighting cache with default options. */
	void InvalidateLightingCache()
	{
		InvalidateLightingCacheDetailed(false);
	}

	/** Invalidates anything produced by the last lighting build. */
	virtual void InvalidateLightingCacheDetailed(bool bTranslationOnly);

	/**
	  * Used for adding actors to levels or teleporting them to a new location.
	  * The result of this function is independent of the actor's current location and rotation.
	  * If the actor doesn't fit exactly at the location specified, tries to slightly move it out of walls and such if bNoCheck is false.
	  *
	  * @param DestLocation The target destination point
	  * @param DestRotation The target rotation at the destination
	  * @param bIsATest is true if this is a test movement, which shouldn't cause any notifications (used by AI pathfinding, for example)
	  * @param bNoCheck is true if we should skip checking for encroachment in the world or other actors
	  * @return true if the actor has been successfully moved, or false if it couldn't fit.
	  */
	virtual bool TeleportTo( const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest=false, bool bNoCheck=false );

	/**
	 * Teleport this actor to a new location. If the actor doesn't fit exactly at the location specified, tries to slightly move it out of walls and such.
	 *
	 * @param DestLocation The target destination point
	 * @param DestRotation The target rotation at the destination
	 * @return true if the actor has been successfully moved, or false if it couldn't fit.
	 */
	UFUNCTION(BlueprintCallable, meta=( DisplayName="Teleport", Keywords = "Move Position" ), Category="Utilities|Transformation")
	bool K2_TeleportTo( FVector DestLocation, FRotator DestRotation );

	/** Called from TeleportTo() when teleport succeeds */
	virtual void TeleportSucceeded(bool bIsATest) {}

	/**
	 *  Trace a ray against the Components of this Actor and return the first blocking hit
	 *  @param  OutHit          First blocking hit found
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  TraceChannel    The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params          Additional parameters used for the trace
	 *  @return TRUE if a blocking hit is found
	 */
	bool ActorLineTraceSingle(struct FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionQueryParams& Params);

	/** 
	 * returns Distance to closest Body Instance surface. 
	 * Checks against all components of this Actor having valid collision and blocking TraceChannel.
	 *
	 * @param Point						World 3D vector
	 * @param TraceChannel				The 'channel' used to determine which components to consider.
	 * @param ClosestPointOnCollision	Point on the surface of collision closest to Point
	 * @param OutPrimitiveComponent		PrimitiveComponent ClosestPointOnCollision is on.
	 * 
	 * @return		Success if returns > 0.f, if returns 0.f, it is either not convex or inside of the point
	 *				If returns < 0.f, this Actor does not have any primitive with collision
	 */
	float ActorGetDistanceToCollision(const FVector& Point, ECollisionChannel TraceChannel, FVector& ClosestPointOnCollision, UPrimitiveComponent** OutPrimitiveComponent = NULL) const;

	/**
	 * Returns true if this actor is contained by TestLevel.
	 * @todo seamless: update once Actor->Outer != Level
	 */
	bool IsInLevel(const class ULevel *TestLevel) const;

	/** Return the ULevel that this Actor is part of. */
	ULevel* GetLevel() const;

	/**	Do anything needed to clear out cross level references; Called from ULevel::PreSave	 */
	virtual void ClearCrossLevelReferences();
	
	/** Non-virtual function to evaluate which portions of the EndPlay process should be dispatched for each actor */
	void RouteEndPlay(const EEndPlayReason::Type EndPlayReason);

	/** Overridable function called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/** iterates up the Base chain to see whether or not this Actor is based on the given Actor
	 * @param Other the Actor to test for
	 * @return true if this Actor is based on Other Actor
	 */
	virtual bool IsBasedOnActor(const AActor* Other) const;
	

	/** iterates up the Base chain to see whether or not this Actor is attached to the given Actor
	* @param Other the Actor to test for
	* @return true if this Actor is attached on Other Actor
	*/
	virtual bool IsAttachedTo( const AActor* Other ) const;

	/** Get the extent used when placing this actor in the editor, used for 'pulling back' hit. */
	FVector GetPlacementExtent() const;

	// Blueprint 

#if WITH_EDITOR
	/** Find all FRandomStream structs in this ACtor and generate new random seeds for them. */
	void SeedAllRandomStreams();
#endif // WITH_EDITOR

	/** Reset private properties to defaults, and all FRandomStream structs in this Actor, so they will start their sequence of random numbers again. */
	void ResetPropertiesForConstruction();

	/** Rerun construction scripts, destroying all autogenerated components; will attempt to preserve the root component location. */
	virtual void RerunConstructionScripts();

	/** 
	 * Debug helper to show the component hierarchy of this actor.
	 * @param Info	Optional String to display at top of info
	 */
	void DebugShowComponentHierarchy( const TCHAR* Info, bool bShowPosition  = true);
	
	/** 
	 * Debug helper for showing the component hierarchy of one component
	 * @param Info	Optional String to display at top of info
	 */
	void DebugShowOneComponentHierarchy( USceneComponent* SceneComp, int32& NestLevel, bool bShowPosition );

	/**
	 * Run any construction script for this Actor. Will call OnConstruction.
	 * @param	Transform			The transform to construct the actor at.
	 * @param   TransformRotationCache Optional rotation cache to use when applying the transform.
	 * @param	InstanceDataCache	Optional cache of state to apply to newly created components (e.g. precomputed lighting)
	 * @param	bIsDefaultTransform	Whether or not the given transform is a "default" transform, in which case it can be overridden by template defaults
	 *
	 * @return Returns false if the hierarchy was not error free and we've put the Actor is disaster recovery mode
	 */
	bool ExecuteConstruction(const FTransform& Transform, const struct FRotationConversionCache* TransformRotationCache, const class FComponentInstanceDataCache* InstanceDataCache, bool bIsDefaultTransform = false);

	/**
	 * Called when an instance of this class is placed (in editor) or spawned.
	 * @param	Transform			The transform the actor was constructed at.
	 */
	virtual void OnConstruction(const FTransform& Transform) {}

	/**
	 * Helper function to register the specified component, and add it to the serialized components array
	 * @param	Component	Component to be finalized
	 */
	void FinishAndRegisterComponent(UActorComponent* Component);

	/**  Util to create a component based on a template	 */
	UActorComponent* CreateComponentFromTemplate(UActorComponent* Template, const FName InName = NAME_None );
	UActorComponent* CreateComponentFromTemplateData(const struct FBlueprintCookedComponentInstancingData* TemplateData, const FName InName = NAME_None);

	DEPRECATED(4.11, "Use CreateComponentFromTemplate that takes a FName instead of a FString")
	UActorComponent* CreateComponentFromTemplate(UActorComponent* Template, const FString& InName);

	/** Destroys the constructed components. */
	void DestroyConstructedComponents();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	private:
	// this is the old name of the tick function. We just want to avoid mistakes with an attempt to override this
	virtual void Tick( float DeltaTime, enum ELevelTick TickType ) final
	{
		check(0);
	}
#endif

protected:
	/**
	 * Virtual call chain to register all tick functions for the actor class hierarchy
	 * @param bRegister - true to register, false, to unregister
	 */
	virtual void RegisterActorTickFunctions(bool bRegister);

	/** Runs UserConstructionScript, delays component registration until it's complete. */
	void ProcessUserConstructionScript();

	/**
	* Checks components for validity, implemented in AActor
	*/
	bool CheckActorComponents();

	/** Called after instancing a new Blueprint Component from either a template or cooked data. */
	void PostCreateBlueprintComponent(UActorComponent* NewActorComp);

public:

	/** Checks for and resolve any name conflicts prior to instancing a new Blueprint Component. */
	void CheckComponentInstanceName(const FName InName);

	/** Walk up the attachment chain from RootComponent until we encounter a different actor, and return it. If we are not attached to a component in a different actor, returns NULL */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	AActor* GetAttachParentActor() const;

	/** Walk up the attachment chain from RootComponent until we encounter a different actor, and return the socket name in the component. If we are not attached to a component in a different actor, returns NAME_None */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	FName GetAttachParentSocketName() const;

	/** Find all Actors which are attached directly to a component in this actor */
	UFUNCTION(BlueprintPure, Category = "Utilities")
	void GetAttachedActors(TArray<AActor*>& OutActors) const;

	/**
	 * Sets the ticking group for this actor.
	 * @param NewTickGroup the new value to assign
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities", meta=(Keywords = "dependency"))
	void SetTickGroup(ETickingGroup NewTickGroup);

	/** Called once this actor has been deleted */
	virtual void Destroyed();

	/** Call ReceiveHit, as well as delegates on Actor and Component */
	void DispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit);

	/** called when the actor falls out of the world 'safely' (below KillZ and such) */
	virtual void FellOutOfWorld(const class UDamageType& dmgType);

	/** called when the Actor is outside the hard limit on world bounds */
	virtual void OutsideWorldBounds();

	/** 
	 *	Returns the world space bounding box of all components in this Actor.
	 *	@param bNonColliding Indicates that you want to include non-colliding components in the bounding box
	 */
	virtual FBox GetComponentsBoundingBox(bool bNonColliding = false) const;

	/** 
	 *	Calculates the actor space bounding box of all components in this Actor.  This is slower than GetComponentsBoundingBox(), because the local bounds of the components are not cached -- they are recalculated every time this function is called.
	 *	@param bNonColliding Indicates that you want to include non-colliding components in the bounding box
	 */
	virtual FBox CalculateComponentsBoundingBoxInLocalSpace(bool bNonColliding = false) const;

	/* Get half-height/radius of a big axis-aligned cylinder around this actors registered colliding components, or all registered components if bNonColliding is false. */
	virtual void GetComponentsBoundingCylinder(float& CollisionRadius, float& CollisionHalfHeight, bool bNonColliding = false) const;

	/**
	 * Get axis-aligned cylinder around this actor, used for simple collision checks (ie Pawns reaching a destination).
	 * If IsRootComponentCollisionRegistered() returns true, just returns its bounding cylinder, otherwise falls back to GetComponentsBoundingCylinder.
	 */
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const;

	/** @returns the radius of the collision cylinder from GetSimpleCollisionCylinder(). */
	float GetSimpleCollisionRadius() const;

	/** @returns the half height of the collision cylinder from GetSimpleCollisionCylinder(). */
	float GetSimpleCollisionHalfHeight() const;

	/** @returns collision extents vector for this Actor, based on GetSimpleCollisionCylinder(). */
	FVector GetSimpleCollisionCylinderExtent() const;

	/** @returns true if the root component is registered and has collision enabled.  */
	virtual bool IsRootComponentCollisionRegistered() const;

	/**
	 * Networking - called on client when actor is torn off (bTearOff==true), meaning it's no longer replicated to clients.
	 * @see bTearOff
	 */
	virtual void TornOff();

	//~=============================================================================
	// Collision functions.
 
	/** 
	 * Get Collision Response to the Channel that entered for all components
	 * It returns Max of state - i.e. if Component A overlaps, but if Component B blocks, it will return block as response
	 * if Component A ignores, but if Component B overlaps, it will return overlap
	 *
	 * @param Channel - The channel to change the response of
	 */
	virtual ECollisionResponse GetComponentsCollisionResponseToChannel(ECollisionChannel Channel) const;

	//~=============================================================================
	// Physics

	/** Stop all simulation from all components in this actor */
	void DisableComponentsSimulatePhysics();

public:
	/** @return WorldSettings for the World the actor is in
	 - if you'd like to know UWorld this placed actor (not dynamic spawned actor) belong to, use GetTypedOuter<UWorld>() **/
	class AWorldSettings* GetWorldSettings() const;

	/**
	 * Return true if the given Pawn can be "based" on this actor (ie walk on it).
	 * @param Pawn - The pawn that wants to be based on this actor
	 */
	virtual bool CanBeBaseForCharacter(class APawn* Pawn) const;

	/** Apply damage to this actor.
	 * @see https://www.unrealengine.com/blog/damage-in-ue4
	 * @param DamageAmount		How much damage to apply
	 * @param DamageEvent		Data package that fully describes the damage received.
	 * @param EventInstigator	The Controller responsible for the damage.
	 * @param DamageCauser		The Actor that directly caused the damage (e.g. the projectile that exploded, the rock that landed on you)
	 * @return					The amount of damage actually applied.
	 */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser);

protected:
	virtual float InternalTakeRadialDamage(float Damage, struct FRadialDamageEvent const& RadialDamageEvent, class AController* EventInstigator, AActor* DamageCauser);
	virtual float InternalTakePointDamage(float Damage, struct FPointDamageEvent const& RadialDamageEvent, class AController* EventInstigator, AActor* DamageCauser);
public:

	/* Called when this actor becomes the given PlayerController's ViewTarget. Triggers the Blueprint event K2_OnBecomeViewTarget. */
	virtual void BecomeViewTarget( class APlayerController* PC );

	/* Called when this actor is no longer the given PlayerController's ViewTarget. Also triggers the Blueprint event K2_OnEndViewTarget. */
	virtual void EndViewTarget( class APlayerController* PC );

	/** Event called when this Actor becomes the view target for the given PlayerController. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnBecomeViewTarget", Keywords="Activate Camera"), Category=Actor)
	void K2_OnBecomeViewTarget( class APlayerController* PC );

	/** Event called when this Actor is no longer the view target for the given PlayerController. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnEndViewTarget", Keywords="Deactivate Camera"), Category=Actor)
	void K2_OnEndViewTarget( class APlayerController* PC );

	/**
	 *	Calculate camera view point, when viewing this actor.
	 *
	 * @param	DeltaTime	Delta time seconds since last update
	 * @param	OutResult	Camera configuration
	 */
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult);

	// Returns true if the actor contains an active camera component
	virtual bool HasActiveCameraComponent() const;

	// Returns true if the actor contains an active locked to HMD camera component
	virtual bool HasActivePawnControlCameraComponent() const;

	// Returns the human readable string representation of an object.
	virtual FString GetHumanReadableName() const;

	/** Reset actor to initial state - used when restarting level without reloading. */
	virtual void Reset();

	/** Event called when this Actor is reset to its initial state - used when restarting level without reloading. */
	UFUNCTION(BlueprintImplementableEvent, Category=Actor, meta=(DisplayName="OnReset"))
	void K2_OnReset();

	/**
	 * Returns true if this actor has been rendered "recently", with a tolerance in seconds to define what "recent" means. 
	 * e.g.: If a tolerance of 0.1 is used, this function will return true only if the actor was rendered in the last 0.1 seconds of game time. 
	 *
	 * @param Tolerance  How many seconds ago the actor last render time can be and still count as having been "recently" rendered.
	 * @return Whether this actor was recently rendered.
	 */
	UFUNCTION(Category="Rendering", BlueprintCallable, meta=(Keywords="scene visible"))
	bool WasRecentlyRendered(float Tolerance = 0.2) const;

	/** Returns the most recent time any of this actor's components were rendered */
	virtual float GetLastRenderTime() const;

	/** Forces this actor to be net relevant if it is not already by default	 */
	virtual void ForceNetRelevant();

	/** Updates NetUpdateTime to the new value for future net relevancy checks */
	void SetNetUpdateTime(float NewUpdateTime);

	/** Return the FNetworkObjectInfo struct associated with this actor (for the main NetDriver) */
	FNetworkObjectInfo* GetNetworkObjectInfo() const;

	/** Force actor to be updated to clients */
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category="Networking")
	virtual void ForceNetUpdate();

	/**
	 *	Calls PrestreamTextures() for all the actor's meshcomponents.
	 *	@param Seconds - Number of seconds to force all mip-levels to be resident
	 *	@param bEnableStreaming	- Whether to start (true) or stop (false) streaming
	 *	@param CinematicTextureGroups - Bitfield indicating which texture groups that use extra high-resolution mips
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	virtual void PrestreamTextures( float Seconds, bool bEnableStreaming, int32 CinematicTextureGroups = 0 );

	/**
	 * Returns the point of view of the actor.
	 * Note that this doesn't mean the camera, but the 'eyes' of the actor.
	 * For example, for a Pawn, this would define the eye height location,
	 * and view rotation (which is different from the pawn rotation which has a zeroed pitch component).
	 * A camera first person view will typically use this view point. Most traces (weapon, AI) will be done from this view point.
	 *
	 * @param	OutLocation - location of view point
	 * @param	OutRotation - view rotation of actor.
	 */
	UFUNCTION(BlueprintCallable, Category = Actor)
	virtual void GetActorEyesViewPoint( FVector& OutLocation, FRotator& OutRotation ) const;

	/**
	 * @param RequestedBy - the Actor requesting the target location
	 * @return the optimal location to fire weapons at this actor
	 */
	virtual FVector GetTargetLocation(AActor* RequestedBy = NULL) const;

	/** 
	  * Hook to allow actors to render HUD overlays for themselves.  Called from AHUD::DrawActorOverlays(). 
	  * @param PC is the PlayerController on whose view this overlay is rendered
	  * @param Canvas is the Canvas on which to draw the overlay
	  * @param CameraPosition Position of Camera
	  * @param CameraDir direction camera is pointing in.
	  */
	virtual void PostRenderFor(class APlayerController* PC, class UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

	/** whether this Actor is in the persistent level, i.e. not a sublevel */
	bool IsInPersistentLevel(bool bIncludeLevelStreamingPersistent = false) const;

	/** Getter for the cached world pointer */
	virtual UWorld* GetWorld() const override;

	/** Get the timer instance from the actors world */
	class FTimerManager& GetWorldTimerManager() const;

	/** Gets the GameInstance that ultimately contains this actor. */
	class UGameInstance* GetGameInstance() const;

	/** Returns true if this is a replicated actor that was placed in the map */
	bool IsNetStartupActor() const;

	/** Searches components array and returns first encountered component of the specified class. */
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const;
	
	/** Script exposed version of FindComponentByClass */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (ComponentClass = "ActorComponent"), meta = (DeterminesOutputType = "ComponentClass"))
	UActorComponent* GetComponentByClass(TSubclassOf<UActorComponent> ComponentClass) const;

	/* Gets all the components that inherit from the given class.
	Currently returns an array of UActorComponent which must be cast to the correct type. */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (ComponentClass = "ActorComponent"), meta = (DeterminesOutputType = "ComponentClass"))
	TArray<UActorComponent*> GetComponentsByClass(TSubclassOf<UActorComponent> ComponentClass) const;

	/* Gets all the components that inherit from the given class with a given tag. */
	UFUNCTION(BlueprintCallable, Category = "Actor", meta = (ComponentClass = "ActorComponent"), meta = (DeterminesOutputType = "ComponentClass"))
	TArray<UActorComponent*> GetComponentsByTag(TSubclassOf<UActorComponent> ComponentClass, FName Tag) const;

	/** Templatized version for syntactic nicety. */
	template<class T>
	T* FindComponentByClass() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, const UActorComponent>::Value, "'T' template parameter to FindComponentByClass must be derived from UActorComponent");

		return (T*)FindComponentByClass(T::StaticClass());
	}

	/**
	 * Get all components derived from class 'T' and fill in the OutComponents array with the result.
	 * It's recommended to use TArrays with a TInlineAllocator to potentially avoid memory allocation costs.
	 * TInlineComponentArray is defined to make this easier, for example:
	 * {
	 * 	   TInlineComponentArray<UPrimitiveComponent*> PrimComponents(Actor);
	 * }
	 *
	 * @param bIncludeFromChildActors	If true then recurse in to ChildActor components and find components of the appropriate type in those Actors as well
	 */
	template<class T, class AllocatorType>
	void GetComponents(TArray<T*, AllocatorType>& OutComponents, bool bIncludeFromChildActors = false) const
	{
		static_assert(TPointerIsConvertibleFromTo<T, const UActorComponent>::Value, "'T' template parameter to GetComponents must be derived from UActorComponent");
		SCOPE_CYCLE_COUNTER(STAT_GetComponentsTime);

		// Empty input array, but don't affect allocated size.
		OutComponents.Reset(0);

		TArray<UChildActorComponent*> ChildActorComponents;

		for (UActorComponent* OwnedComponent : OwnedComponents)
		{
			if (T* Component = Cast<T>(OwnedComponent))
			{
				OutComponents.Add(Component);
			}
			else if (bIncludeFromChildActors)
			{
				if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(OwnedComponent))
				{
					ChildActorComponents.Add(ChildActorComponent);
				}
			}
		}

		if (bIncludeFromChildActors)
		{
			TArray<T*, AllocatorType> ComponentsInChildActor;
			for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
			{
				if (AActor* ChildActor = ChildActorComponent->GetChildActor())
				{
					ChildActor->GetComponents(ComponentsInChildActor, true);
					OutComponents.Append(MoveTemp(ComponentsInChildActor));
				}
			}
		}
	}

	/**
	 * UActorComponent specialization of GetComponents() to avoid unnecessary casts.
	 * It's recommended to use TArrays with a TInlineAllocator to potentially avoid memory allocation costs.
	 * TInlineComponentArray is defined to make this easier, for example:
	 * {
	 * 	   TInlineComponentArray<UActorComponent*> PrimComponents;
	 *     Actor->GetComponents(PrimComponents);
	 * }
	 *
	 * @param bIncludeFromChildActors	If true then recurse in to ChildActor components and find components of the appropriate type in those Actors as well
	 */
	template<class AllocatorType>
	void GetComponents(TArray<UActorComponent*, AllocatorType>& OutComponents, bool bIncludeFromChildActors = false) const
	{
		SCOPE_CYCLE_COUNTER(STAT_GetComponentsTime);

		OutComponents.Reset(OwnedComponents.Num());

		TArray<UChildActorComponent*> ChildActorComponents;

		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component)
			{
				OutComponents.Add(Component);
			}
			else if (bIncludeFromChildActors)
			{
				if (UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Component))
				{
					ChildActorComponents.Add(ChildActorComponent);
				}
			}
		}

		if (bIncludeFromChildActors)
		{
			TArray<UActorComponent*, AllocatorType> ComponentsInChildActor;
			for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
			{
				if (AActor* ChildActor = ChildActorComponent->GetChildActor())
				{
					ChildActor->GetComponents(ComponentsInChildActor, true);
					OutComponents.Append(MoveTemp(ComponentsInChildActor));
				}
			}
		}

	}

	/**
	 * Get a direct reference to the Components set rather than a copy with the null pointers removed.
	 * WARNING: anything that could cause the component to change ownership or be destroyed will invalidate
	 * this array, so use caution when iterating this set!
	 */
	const TSet<UActorComponent*>& GetComponents() const
	{
		return OwnedComponents;
	}

	/** Puts a component in to the OwnedComponents array of the Actor.
	 *  The Component must be owned by the Actor or else it will assert
	 *  In general this should not need to be called directly by anything other than UActorComponent functions
	 */
	void AddOwnedComponent(UActorComponent* Component);

	/** Removes a component from the OwnedComponents array of the Actor.
	 *  In general this should not need to be called directly by anything other than UActorComponent functions
	 */
	void RemoveOwnedComponent(UActorComponent* Component);

#if DO_CHECK
	// Utility function for validating that a component is correctly in its Owner's OwnedComponents array
	bool OwnsComponent(UActorComponent* Component) const;
#endif

	/** Force the Actor to clear and rebuild its OwnedComponents array by evaluating all children (recursively) and locating components
	 *  In general this should not need to be called directly, but can sometimes be necessary as part of undo/redo code paths.
	 */
	void ResetOwnedComponents();

	/** Called when the replicated state of a component changes to update the Actor's cached ReplicatedComponents array
	 */
	void UpdateReplicatedComponent(UActorComponent* Component);

	/** Completely synchronizes the replicated components array so that it contains exactly the number of replicated components currently owned
	 */
	void UpdateAllReplicatedComponents();

	/** Returns whether replication is enabled or not. */
	FORCEINLINE bool GetIsReplicated() const
	{
		return bReplicates;
	}

	/** Returns a constant reference to the replicated components set
	 */
	const TSet<UActorComponent*>& GetReplicatedComponents() const 
	{ 
		return ReplicatedComponents; 
	}

private:
	/**
	 * All ActorComponents owned by this Actor.
	 * @see GetComponents()
	 */
	TSet<UActorComponent*> OwnedComponents;

	/** Set of replicated components. */
	TSet<UActorComponent*> ReplicatedComponents;

#if WITH_EDITOR
	/** Maps natively-constructed components to properties that reference them. */
	TMultiMap<FName, UObjectProperty*> NativeConstructedComponentToPropertyMap;
#endif

public:

	/** Array of ActorComponents that are created by blueprints and serialized per-instance. */
	UPROPERTY(TextExportTransient, NonTransactional)
	TArray<UActorComponent*> BlueprintCreatedComponents;

private:
	/** Array of ActorComponents that have been added by the user on a per-instance basis. */
	UPROPERTY(Instanced)
	TArray<UActorComponent*> InstanceComponents;

public:

	/** Adds a component to the instance components array */
	void AddInstanceComponent(UActorComponent* Component);

	/** Removes a component from the instance components array */
	void RemoveInstanceComponent(UActorComponent* Component);

	/** Clears the instance components array */
	void ClearInstanceComponents(bool bDestroyComponents);

	/** Returns the instance components array */
	const TArray<UActorComponent*>& GetInstanceComponents() const;

public:
	//~=============================================================================
	// Navigation related functions
	// 

	/** Check if owned component should be relevant for navigation
	 *  Allows implementing master switch to disable e.g. collision export in projectiles
	 */
	virtual bool IsComponentRelevantForNavigation(UActorComponent* Component) const { return true; }

	//~=============================================================================
	// Debugging functions
public:
	/**
	 * Draw important Actor variables on canvas.  HUD will call DisplayDebug() on the current ViewTarget when the ShowDebug exec is used
	 *
	 * @param Canvas			Canvas to draw on
	 *
	 * @param DebugDisplay		Contains information about what debug data to display
	 *
	 * @param YL				[in]	Height of the previously drawn line.
	 *							[out]	Height of the last line drawn by this function.
	 *
	 * @param YPos				[in]	Y position on Canvas for the previously drawn line. YPos += YL, gives position to draw text for next debug line.
	 *							[out]	Y position on Canvas for the last line drawn by this function.
	 */
	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	/** Retrieves actor's name used for logging, or string "NULL" if Actor == NULL */
	static FString GetDebugName(const AActor* Actor) { return Actor ? Actor->GetName() : TEXT("NULL"); }

	//* Sets the friendly actor label and name */
	void SetActorLabelInternal( const FString& NewActorLabelDirty, bool bMakeGloballyUniqueFName, bool bMarkDirty );

	static FMakeNoiseDelegate MakeNoiseDelegate;

public:
#if !UE_BUILD_SHIPPING
	/** Delegate for globally hooking ProccessEvent calls - used by a non-public testing plugin */
	static FOnProcessEvent ProcessEventDelegate;
#endif

	static void MakeNoiseImpl(AActor* NoiseMaker, float Loudness, APawn* NoiseInstigator, const FVector& NoiseLocation, float MaxRange, FName Tag);
	static void SetMakeNoiseDelegate(const FMakeNoiseDelegate& NewDelegate);

	/** A fence to track when the primitive is detached from the scene in the rendering thread. */
	FRenderCommandFence DetachFence;

private:

	// Helper that already assumes the Hit info is reversed, and avoids creating a temp FHitResult if possible.
	void InternalDispatchBlockingHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp, bool bSelfMoved, FHitResult const& Hit);

	/** Private version without inlining that does *not* check Dedicated server build flags (which should already have been done). */
	ENetMode InternalGetNetMode() const;

	/** Unified implementation function to be called from the two implementations of PostEditUndo for the AActor specific elements that need to happen. */
	bool InternalPostEditUndo();

	friend struct FMarkActorIsBeingDestroyed;
	friend struct FActorParentComponentSetter;

private:

	// Static helpers for accessing functions on SceneComponent.
	// These are templates for no other reason than to delay compilation until USceneComponent is defined.

	/*~
	 * Returns transform of the RootComponent 
	 */ 
	template<class T>
	static FORCEINLINE FTransform TemplateGetActorTransform(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetComponentTransform() : FTransform();
	}

	/*~
	 * Returns location of the RootComponent 
	 */ 
	template<class T>
	static FORCEINLINE FVector TemplateGetActorLocation(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetComponentLocation() : FVector::ZeroVector;
	}

	/*~
	 * Returns rotation of the RootComponent 
	 */ 
	template<class T>
	static FORCEINLINE FRotator TemplateGetActorRotation(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetComponentRotation() : FRotator::ZeroRotator;
	}

	/*~
	 * Returns scale of the RootComponent 
	 */ 
	template<class T>
	static FORCEINLINE FVector TemplateGetActorScale(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetComponentScale() : FVector(1.f,1.f,1.f);
	}

	/*~
	 * Returns quaternion of the RootComponent
	 */ 
	template<class T>
	static FORCEINLINE FQuat TemplateGetActorQuat(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetComponentQuat() : FQuat(ForceInit);
	}

	/*~
	 * Returns the forward (X) vector (length 1.0) of the RootComponent, in world space.
	 */ 
	template<class T>
	static FORCEINLINE FVector TemplateGetActorForwardVector(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetForwardVector() : FVector::ForwardVector;
	}

	/*~
	* Returns the Up (Z) vector (length 1.0) of the RootComponent, in world space.
	*/ 
	template<class T>
	static FORCEINLINE FVector TemplateGetActorUpVector(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetUpVector() : FVector::UpVector;
	}

	/*~
	* Returns the Right (Y) vector (length 1.0) of the RootComponent, in world space.	
	*/ 
	template<class T>
	static FORCEINLINE FVector TemplateGetActorRightVector(const T* RootComponent)
	{
		return (RootComponent != nullptr) ? RootComponent->GetRightVector() : FVector::RightVector;
	}
};


struct FMarkActorIsBeingDestroyed
{
private:
	FMarkActorIsBeingDestroyed(AActor* InActor)
	{
		InActor->bActorIsBeingDestroyed = true;
	}

	friend UWorld;
};

/**
 * TInlineComponentArray is simply a TArray that reserves a fixed amount of space on the stack
 * to try to avoid heap allocation when there are fewer than a specified number of elements expected in the result.
 */
template<class T, uint32 NumElements = NumInlinedActorComponents>
class TInlineComponentArray : public TArray<T, TInlineAllocator<NumElements>>
{
	typedef TArray<T, TInlineAllocator<NumElements>> Super;

public:
	TInlineComponentArray() : Super() { }
	TInlineComponentArray(const class AActor* Actor) : Super()
	{
		if (Actor)
		{
			Actor->GetComponents(*this);
		}
	};
};

/** Helper function for executing tick functions based on the normal conditions previous found in UActorComponent::ConditionalTick */

template <typename ExecuteTickLambda>
void FActorComponentTickFunction::ExecuteTickHelper(UActorComponent* Target, bool bTickInEditor, float DeltaTime, ELevelTick TickType, const ExecuteTickLambda& ExecuteTickFunc)
{
	if (Target && !Target->IsPendingKillOrUnreachable())
	{
		FScopeCycleCounterUObject ComponentScope(Target);
		FScopeCycleCounterUObject AdditionalScope(Target->AdditionalStatObject());

		if (Target->bRegistered)
		{
			AActor* MyOwner = Target->GetOwner();
			//@optimization, I imagine this is all unnecessary in a shipping game with no editor
			if (TickType != LEVELTICK_ViewportsOnly ||
				(bTickInEditor && TickType == LEVELTICK_ViewportsOnly) ||
				(MyOwner && MyOwner->ShouldTickIfViewportsOnly())
				)
			{
				const float TimeDilation = (MyOwner ? MyOwner->CustomTimeDilation : 1.f);
				ExecuteTickFunc(DeltaTime * TimeDilation);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE_DEBUGGABLE FVector AActor::K2_GetActorLocation() const
{
	return GetActorLocation();
}

FORCEINLINE_DEBUGGABLE FRotator AActor::K2_GetActorRotation() const
{
	return GetActorRotation();
}

FORCEINLINE_DEBUGGABLE USceneComponent* AActor::K2_GetRootComponent() const
{
	return GetRootComponent();
}

FORCEINLINE_DEBUGGABLE FVector AActor::GetActorForwardVector() const
{
	return TemplateGetActorForwardVector(RootComponent);
}

FORCEINLINE_DEBUGGABLE FVector AActor::GetActorUpVector() const
{
	return TemplateGetActorUpVector(RootComponent);
}

FORCEINLINE_DEBUGGABLE FVector AActor::GetActorRightVector() const
{
	return TemplateGetActorRightVector(RootComponent);
}


FORCEINLINE float AActor::GetSimpleCollisionRadius() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return Radius;
}

FORCEINLINE float AActor::GetSimpleCollisionHalfHeight() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return HalfHeight;
}

FORCEINLINE FVector AActor::GetSimpleCollisionCylinderExtent() const
{
	float Radius, HalfHeight;
	GetSimpleCollisionCylinder(Radius, HalfHeight);
	return FVector(Radius, Radius, HalfHeight);
}

FORCEINLINE_DEBUGGABLE bool AActor::GetActorEnableCollision() const
{
	return bActorEnableCollision;
}

FORCEINLINE_DEBUGGABLE bool AActor::HasAuthority() const
{
	return (Role == ROLE_Authority);
}

FORCEINLINE_DEBUGGABLE AActor* AActor::GetOwner() const
{ 
	return Owner; 
}

FORCEINLINE_DEBUGGABLE const AActor* AActor::GetNetOwner() const
{
	// NetOwner is the Actor Owner unless otherwise overridden (see PlayerController/Pawn/Beacon)
	// Used in ServerReplicateActors
	return Owner;
}

FORCEINLINE_DEBUGGABLE ENetRole AActor::GetRemoteRole() const
{
	return RemoteRole;
}

FORCEINLINE_DEBUGGABLE ENetMode AActor::GetNetMode() const
{
	// IsRunningDedicatedServer() is a compile-time check in optimized non-editor builds.
	if (IsRunningDedicatedServer() && (NetDriverName == NAME_None || NetDriverName == NAME_GameNetDriver))
	{
		// Only normal net driver actors can have this optimization
		return NM_DedicatedServer;
	}

	return InternalGetNetMode();
}

FORCEINLINE_DEBUGGABLE bool AActor::IsNetMode(ENetMode Mode) const
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
	else if (NetDriverName == NAME_None || NetDriverName == NAME_GameNetDriver)
	{
		// Only normal net driver actors can have this optimization
		return !IsRunningDedicatedServer() && (InternalGetNetMode() == Mode);
	}
	else
	{
		return (InternalGetNetMode() == Mode);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
// Macro to hide common Transform functions in native code for classes where they don't make sense.
// Note that this doesn't prevent access through function calls from parent classes (ie an AActor*), but
// does prevent use in the class that hides them and any derived child classes.

#define HIDE_ACTOR_TRANSFORM_FUNCTIONS() private: \
	FTransform GetTransform() const { return Super::GetTransform(); } \
	FVector GetActorLocation() const { return Super::GetActorLocation(); } \
	FRotator GetActorRotation() const { return Super::GetActorRotation(); } \
	FQuat GetActorQuat() const { return Super::GetActorQuat(); } \
	FVector GetActorScale() const { return Super::GetActorScale(); } \
	bool SetActorLocation(const FVector& NewLocation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr) { return Super::SetActorLocation(NewLocation, bSweep, OutSweepHitResult); } \
	bool SetActorRotation(FRotator NewRotation) { return Super::SetActorRotation(NewRotation); } \
	bool SetActorRotation(const FQuat& NewRotation) { return Super::SetActorRotation(NewRotation); } \
	bool SetActorLocationAndRotation(FVector NewLocation, FRotator NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr) { return Super::SetActorLocationAndRotation(NewLocation, NewRotation, bSweep, OutSweepHitResult); } \
	bool SetActorLocationAndRotation(FVector NewLocation, const FQuat& NewRotation, bool bSweep=false, FHitResult* OutSweepHitResult=nullptr) { return Super::SetActorLocationAndRotation(NewLocation, NewRotation, bSweep, OutSweepHitResult); } \
	virtual bool TeleportTo( const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck ) override { return Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck); } \
	virtual FVector GetVelocity() const override { return Super::GetVelocity(); } \
	float GetHorizontalDistanceTo(AActor* OtherActor)  { return Super::GetHorizontalDistanceTo(OtherActor); } \
	float GetVerticalDistanceTo(AActor* OtherActor)  { return Super::GetVerticalDistanceTo(OtherActor); } \
	float GetDotProductTo(AActor* OtherActor) { return Super::GetDotProductTo(OtherActor); } \
	float GetHorizontalDotProductTo(AActor* OtherActor) { return Super::GetHorizontalDotProductTo(OtherActor); } \
	float GetDistanceTo(AActor* OtherActor) { return Super::GetDistanceTo(OtherActor); }




