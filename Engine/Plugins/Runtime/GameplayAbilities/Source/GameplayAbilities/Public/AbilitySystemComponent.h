// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "EngineDefines.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "GameplayCueInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTasksComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.generated.h"

class AGameplayAbilityTargetActor;
class AHUD;
class FDebugDisplayInfo;
class UAnimMontage;
class UCanvas;
class UInputComponent;

/** 
 *	UAbilitySystemComponent	
 *
 *	A component to easily interface with the 3 aspects of the AbilitySystem:
 *		-GameplayAbilities
 *		-GameplayEffects
 *		-GameplayAttributes
 *		
 *	This component will make life easier for interfacing with these subsystems, but is not completely required. The main functions are:
 *	
 *	GameplayAbilities:
 *		-Provides a way to give/assign abilities that can be used (by a player or AI for example)
 *		-Provides management of instanced abilities (something must hold onto them)
 *		-Provides replication functionality
 *			-Ability state must always be replicated on the UGameplayAbility itself, but UAbilitySystemComponent can provide RPC replication
 *			for non-instanced gameplay abilities. (Explained more in GameplayAbility.h).
 *			
 *	GameplayEffects:
 *		-Provides an FActiveGameplayEffectsContainer for holding active GameplayEffects
 *		-Provides methods for apply GameplayEffect to a target or to self
 *		-Provides wrappers for querying information in FActiveGameplayEffectsContainers (duration, magnitude, etc)
 *		-Provides methods for clearing/remove GameplayEffects
 *		
 *	GameplayAttributes
 *		-Provides methods for allocating and initializing attribute sets
 *		-Provides methods for getting AttributeSets
 *  
 * 
 */

/** Called when a targeting actor rejects target confirmation */
DECLARE_MULTICAST_DELEGATE_OneParam(FTargetingRejectedConfirmation, int32);

/** Called when ability fails to activate, passes along the failed ability and a tag explaining why */
DECLARE_MULTICAST_DELEGATE_TwoParams(FAbilityFailedDelegate, const UGameplayAbility*, const FGameplayTagContainer&);

/** Called when ability ends */
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilityEnded, UGameplayAbility*);

/** Notify interested parties that ability spec has been modified */
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilitySpecDirtied, const FGameplayAbilitySpec&);

/** Notifies when GameplayEffectSpec is blocked by an ActiveGameplayEffect due to immunity  */
DECLARE_MULTICAST_DELEGATE_TwoParams(FImmunityBlockGE, const FGameplayEffectSpec& /*BlockedSpec*/, const FActiveGameplayEffect* /*ImmunityGameplayEffect*/);

UENUM()
enum class EReplicationMode : uint8
{
	/** Only replicate minimal gameplay effect info*/
	Minimal,
	/** Only replicate minimal gameplay effect info to simulated proxies but full info to owners and autonomous proxies */
	Mixed,
	/** Replicate full gameplay info to all */
	Full,
};

/**
 *	The core ActorComponent for interfacing with the GameplayAbilities System
 */
UCLASS(ClassGroup=AbilitySystem, hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming), editinlinenew, meta=(BlueprintSpawnableComponent))
class GAMEPLAYABILITIES_API UAbilitySystemComponent : public UGameplayTasksComponent, public IGameplayTagAssetInterface
{
	GENERATED_UCLASS_BODY()

	/** Used to register callbacks to ability-key input */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityAbilityKey, /*UGameplayAbility*, Ability, */int32, InputID);

	/** Used to register callbacks to confirm/cancel input */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAbilityConfirmOrCancel);

	friend struct FActiveGameplayEffectAction_Add;
	friend FGameplayEffectSpec;
	friend class AAbilitySystemDebugHUD;

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** retrieves information whether this component should be ticking taken current
	 *	activity into consideration*/
	virtual bool GetShouldTick() const override;

	/** Finds existing AttributeSet */
	template <class T >
	const T*	GetSet() const
	{
		return (T*)GetAttributeSubobject(T::StaticClass());
	}

	/** Finds existing AttributeSet. Asserts if it isn't there. */
	template <class T >
	const T*	GetSetChecked() const
	{
		return (T*)GetAttributeSubobjectChecked(T::StaticClass());
	}

	/** Adds a new AttributeSet (initialized to default values) */
	template <class T >
	const T*  AddSet()
	{
		return (T*)GetOrCreateAttributeSubobject(T::StaticClass());
	}

	/** Adds a new AttributeSet that is a DSO (created by called in their CStor) */
	template <class T>
	const T*	AddDefaultSubobjectSet(T* Subobject)
	{
		SpawnedAttributes.AddUnique(Subobject);
		return Subobject;
	}

	/**
	* Does this ability system component have this attribute?
	*
	* @param Attribute	Handle of the gameplay effect to retrieve target tags from
	*
	* @return true if Attribute is valid and this ability system component contains an attribute set that contains Attribute. Returns false otherwise.
	*/
	bool HasAttributeSetForAttribute(FGameplayAttribute Attribute) const;

	const UAttributeSet* InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable);

	UFUNCTION(BlueprintCallable, Category="Skills", meta=(DisplayName="InitStats"))
	void K2_InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable);
		
	/** Returns a list of all attributes for this abiltiy system component */
	void GetAllAttributes(OUT TArray<FGameplayAttribute>& Attributes);

	UPROPERTY(EditAnywhere, Category="AttributeTest")
	TArray<FAttributeDefaults>	DefaultStartingData;

	UPROPERTY(Replicated)
	TArray<UAttributeSet*>	SpawnedAttributes;

	/** Sets the base value of an attribute. Existing active modifiers are NOT cleared and will act upon the new base value. */
	void SetNumericAttributeBase(const FGameplayAttribute &Attribute, float NewBaseValue);

	/** Gets the base value of an attribute. That is, the value of the attribute with no stateful modifiers */
	float GetNumericAttributeBase(const FGameplayAttribute &Attribute) const;

	/**
	 *	Applies an inplace mod to the given attribute. This correctly update the attribute's aggregator, updates the attribute set property,
	 *	and invokes the OnDirty callbacks.
	 *	
	 *	This does not invoke Pre/PostGameplayEffectExecute calls on the attribute set. This does no tag checking, application requirements, immunity, etc.
	 *	No GameplayEffectSpec is created or is applied!
	 *
	 *	This should only be used in cases where applying a real GameplayEffectSpec is too slow or not possible.
	 */
	void ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude);

	/**
	 *  Applies an inplace mod to the given attribute. Unlike ApplyModToAttribute this function will run on the client or server.
	 *  This may result in problems related to prediction and will not roll back properly.
	 */
	void ApplyModToAttributeUnsafe(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude);

	/** Returns current (final) value of an attribute */
	float GetNumericAttribute(const FGameplayAttribute &Attribute) const;
	float GetNumericAttributeChecked(const FGameplayAttribute &Attribute) const;

	// -- Replication -------------------------------------------------------------------------------------------------

	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags) override;
	
	/** Force owning actor to update it's replication, to make sure that gameplay cues get sent down quickly. Override to change how aggressive this is */
	virtual void ForceReplication();

	/** Forces avatar actor to update it's replication. Useful for things like needing to replication for movement / locations reasons. */
	virtual void ForceAvatarReplication();

	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs) override;

	virtual void PreNetReceive() override;
	
	virtual void PostNetReceive() override;

	/** When true, we will not replicate active gameplay effects for this abiltiy system component, so attributes and tags */
	void SetReplicationMode(EReplicationMode NewReplicationMode);

	EReplicationMode ReplicationMode;

	FPredictionKey	ScopedPredictionKey;

	FPredictionKey GetPredictionKeyForNewAction() const
	{
		return ScopedPredictionKey.IsValidForMorePrediction() ? ScopedPredictionKey : FPredictionKey();
	}

	/** Do we have a valid prediction key to do more predictive actions with */
	bool CanPredict() const
	{
		return ScopedPredictionKey.IsValidForMorePrediction();
	}

	bool HasAuthorityOrPredictionKey(const FGameplayAbilityActivationInfo* ActivationInfo) const;

	// A pending activation that cannot be activated yet, will be rechecked at a later point
	struct FPendingAbilityInfo
	{
		bool operator==(const FPendingAbilityInfo& Other) const
		{
			// Don't compare event data, not valid to have multiple activations in flight with same key and handle but different event data
			return PredictionKey == Other.PredictionKey	&& Handle == Other.Handle;
		}

		/** Properties of the ability that needs to be activated */
		FGameplayAbilitySpecHandle Handle;
		FPredictionKey	PredictionKey;
		FGameplayEventData TriggerEventData;

		/** True if this ability was activated remotely and needs to follow up, false if the ability hasn't been activated at all yet */
		bool bPartiallyActivated;

		FPendingAbilityInfo()
			: bPartiallyActivated(false)
		{}
	};

	// This is a list of GameplayAbilities that are predicted by the client and were triggered by abilities that were also predicted by the client
	// When the server version of the predicted ability executes it should trigger copies of these and the copies will be associated with the correct prediction keys
	TArray<FPendingAbilityInfo> PendingClientActivatedAbilities;

	// This is a list of GameplayAbilities that were activated on the server and can't yet execute on the client. It will try to execute these at a later point
	TArray<FPendingAbilityInfo> PendingServerActivatedAbilities;

	enum class EAbilityExecutionState : uint8
	{
		Executing,
		Succeeded,
		Failed,
	};

	struct FExecutingAbilityInfo
	{
		FExecutingAbilityInfo() : State(EAbilityExecutionState::Executing) {};

		bool operator==(const FExecutingAbilityInfo& Other) const
		{
			return PredictionKey == Other.PredictionKey	&& State == Other.State;
		}

		FPredictionKey PredictionKey;
		EAbilityExecutionState State;
		FGameplayAbilitySpecHandle Handle;
	};

	TArray<FExecutingAbilityInfo> ExecutingServerAbilities;

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	GameplayEffects	
	//	
	// ----------------------------------------------------------------------------------------------------------------

	// --------------------------------------------
	// Primary outward facing API for other systems:
	// --------------------------------------------
	FActiveGameplayEffectHandle ApplyGameplayEffectSpecToTarget(OUT FGameplayEffectSpec& GameplayEffect, UAbilitySystemComponent *Target, FPredictionKey PredictionKey=FPredictionKey());
	FActiveGameplayEffectHandle ApplyGameplayEffectSpecToSelf(OUT FGameplayEffectSpec& GameplayEffect, FPredictionKey PredictionKey = FPredictionKey());

	UFUNCTION(BlueprintCallable, Category = GameplayEffects, meta=(DisplayName = "ApplyGameplayEffectSpecToTarget"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectSpecToTarget(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* Target);

	UFUNCTION(BlueprintCallable, Category = GameplayEffects, meta=(DisplayName = "ApplyGameplayEffectSpecToSelf"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectSpecToSelf(UPARAM(ref) FGameplayEffectSpecHandle& SpecHandle);
	
	/** Gets the FActiveGameplayEffect based on the passed in Handle */
	const UGameplayEffect* GetGameplayEffectDefForHandle(FActiveGameplayEffectHandle Handle);

	/** Removes GameplayEffect by Handle. StacksToRemove=-1 will remove all stacks. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameplayEffects)
	bool RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1);

	/** 
	 * Remove active gameplay effects whose backing definition are the specified gameplay effect class
	 *
	 * @param GameplayEffect					Class of gameplay effect to remove; Does nothing if left null
	 * @param InstigatorAbilitySystemComponent	If specified, will only remove gameplay effects applied from this instigator ability system component
	 * @param StacksToRemove					Number of stacks to remove, -1 means remove all
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameplayEffects)
	void RemoveActiveGameplayEffectBySourceEffect(TSubclassOf<UGameplayEffect> GameplayEffect, UAbilitySystemComponent* InstigatorAbilitySystemComponent, int32 StacksToRemove = -1);

	/** Get an outgoing GameplayEffectSpec that is ready to be applied to other things. */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	FGameplayEffectSpecHandle MakeOutgoingSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle Context) const;

	/** Create an EffectContext for the owner of this AbilitySystemComponent */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	FGameplayEffectContextHandle MakeEffectContext() const;

	/**
	 * Get the count of the specified source effect on the ability system component. For non-stacking effects, this is the sum of all active instances.
	 * For stacking effects, this is the sum of all valid stack counts. If an instigator is specified, only effects from that instigator are counted.
	 * 
	 * @param SourceGameplayEffect					Effect to get the count of
	 * @param OptionalInstigatorFilterComponent		If specified, only count effects applied by this ability system component
	 * 
	 * @return Count of the specified source effect
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=GameplayEffects)
	int32 GetGameplayEffectCount(TSubclassOf<UGameplayEffect> SourceGameplayEffect, UAbilitySystemComponent* OptionalInstigatorFilterComponent, bool bEnforceOnGoingCheck = true);

	/** Returns the sum of StackCount of all gameplay effects that pass query */
	int32 GetAggregatedStackCount(const FGameplayEffectQuery& Query);

	/** This only exists so it can be hooked up to a multicast delegate */
	void RemoveActiveGameplayEffect_NoReturn(FActiveGameplayEffectHandle Handle, int32 StacksToRemove=-1)
	{
		RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}

	/** Needed for delegate callback for tag prediction */
	void RemoveOneTagCount_NoReturn(FGameplayTag Tag)
	{
		UpdateTagMap(Tag, -1);
	}

	/** Called for predictively added gameplay cue. Needs to remove tag count and possible invoke OnRemove event if misprediction */
	void OnPredictiveGameplayCueCatchup(FGameplayTag Tag);

	float GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const;

	void GetGameplayEffectStartTimeAndDuration(FActiveGameplayEffectHandle Handle, float& StartEffectTime, float& Duration) const;

	/** Updates the level of an already applied gameplay effect. The intention is that this is 'seemless' and doesnt behave like removing/reapplying */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameplayEffects)
	void SetActiveGameplayEffectLevel(FActiveGameplayEffectHandle ActiveHandle, int32 NewLevel);

	/** Updates the level of an already applied gameplay effect. The intention is that this is 'seemless' and doesnt behave like removing/reapplying */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameplayEffects)
	void SetActiveGameplayEffectLevelUsingQuery(FGameplayEffectQuery Query, int32 NewLevel);

	// Not happy with this interface but don't see a better way yet. How should outside code (UI, etc) ask things like 'how much is this gameplay effect modifying my damage by'
	// (most likely we want to catch this on the backend - when damage is applied we can get a full dump/history of how the number got to where it is. But still we may need polling methods like below (how much would my damage be)
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	float GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const;

	/** Returns current stack count of an already applied GE */
	int32 GetCurrentStackCount(FActiveGameplayEffectHandle Handle) const;

	/** Returns current stack count of an already applied GE, but given the ability spec handle that was granted by the GE */
	int32 GetCurrentStackCount(FGameplayAbilitySpecHandle Handle) const;

	/** Returns debug string describing active gameplay effect */
	FString GetActiveGEDebugString(FActiveGameplayEffectHandle Handle) const;

	/** Gets the GE Handle of the GE that granted the passed in Ability */
	FActiveGameplayEffectHandle FindActiveGameplayEffectHandle(FGameplayAbilitySpecHandle Handle) const;

	/**
	 * Get the source tags from the gameplay spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the gameplay effect to retrieve source tags from
	 * 
	 * @return Source tags from the gameplay spec represented by the handle, if possible
	 */
	const FGameplayTagContainer* GetGameplayEffectSourceTagsFromHandle(FActiveGameplayEffectHandle Handle) const
	{
		return ActiveGameplayEffects.GetGameplayEffectSourceTagsFromHandle(Handle);
	}

	/**
	 * Get the target tags from the gameplay spec represented by the specified handle, if possible
	 * 
	 * @param Handle	Handle of the gameplay effect to retrieve target tags from
	 * 
	 * @return Target tags from the gameplay spec represented by the handle, if possible
	 */
	const FGameplayTagContainer* GetGameplayEffectTargetTagsFromHandle(FActiveGameplayEffectHandle Handle) const
	{
		return ActiveGameplayEffects.GetGameplayEffectTargetTagsFromHandle(Handle);
	}

	/**
	 * Populate the specified capture spec with the data necessary to capture an attribute from the component
	 * 
	 * @param OutCaptureSpec	[OUT] Capture spec to populate with captured data
	 */
	void CaptureAttributeForGameplayEffect(OUT FGameplayEffectAttributeCaptureSpec& OutCaptureSpec)
	{
		// Verify the capture is happening on an attribute the component actually has a set for; if not, can't capture the value
		const FGameplayAttribute& AttributeToCapture = OutCaptureSpec.BackingDefinition.AttributeToCapture;
		if (AttributeToCapture.IsValid() && (AttributeToCapture.IsSystemAttribute() || GetAttributeSubobject(AttributeToCapture.GetAttributeSetClass())))
		{
			ActiveGameplayEffects.CaptureAttributeForGameplayEffect(OutCaptureSpec);
		}
	}
	
	// --------------------------------------------
	// Callbacks / Notifies
	// (these need to be at the UObject level so we can safetly bind, rather than binding to raw at the ActiveGameplayEffect/Container level which is unsafe if the AbilitySystemComponent were killed).
	// --------------------------------------------

	void OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute, bool FromRecursiveCall=false);

	void OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAggregator);

	void ForceAttributeAggregatorPropertyUpdate(FAggregator* Aggregator);

	/** This ASC has successfully applied a GE to something (potentially itself) */
	void OnGameplayEffectAppliedToTarget(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	void OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

	void OnPeriodicGameplayEffectExecuteOnTarget(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecExecuted, FActiveGameplayEffectHandle ActiveHandle);

	void OnPeriodicGameplayEffectExecuteOnSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& SpecExecuted, FActiveGameplayEffectHandle ActiveHandle);

	virtual void OnGameplayEffectDurationChange(struct FActiveGameplayEffect& ActiveEffect);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGameplayEffectAppliedDelegate, UAbilitySystemComponent*, const FGameplayEffectSpec&, FActiveGameplayEffectHandle);

	/** Called on server whenever a GE is applied to self. This includes instant and duration based GEs. */
	FOnGameplayEffectAppliedDelegate OnGameplayEffectAppliedDelegateToSelf;

	/** Called on server whenever a GE is applied to someone else. This includes instant and duration based GEs. */
	FOnGameplayEffectAppliedDelegate OnGameplayEffectAppliedDelegateToTarget;

	/** Called on both client and server whenever a duraton based GE is added (E.g., instant GEs do not trigger this). */
	FOnGameplayEffectAppliedDelegate OnActiveGameplayEffectAddedDelegateToSelf;

	/** Called on server whenever a periodic GE executes on self */
	FOnGameplayEffectAppliedDelegate OnPeriodicGameplayEffectExecuteDelegateOnSelf;

	/** Called on server whenever a periodic GE executes on target */
	FOnGameplayEffectAppliedDelegate OnPeriodicGameplayEffectExecuteDelegateOnTarget;

	// --------------------------------------------
	// Tags
	// --------------------------------------------
	FORCEINLINE bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override
	{
		return GameplayTagCountContainer.HasMatchingGameplayTag(TagToCheck);
	}

	FORCEINLINE bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override
	{
		return GameplayTagCountContainer.HasAllMatchingGameplayTags(TagContainer);
	}

	FORCEINLINE bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override
	{
		return GameplayTagCountContainer.HasAnyMatchingGameplayTags(TagContainer);
	}

	FORCEINLINE void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override
	{
		TagContainer.AppendTags(GameplayTagCountContainer.GetExplicitGameplayTags());
	}

	FORCEINLINE int32 GetTagCount(FGameplayTag TagToCheck) const
	{
		return GameplayTagCountContainer.GetTagCount(TagToCheck);
	}

	/** 	 
	 *  Allows GameCode to add loose gameplaytags which are not backed by a GameplayEffect. 
	 *
	 *	Tags added this way are not replicated! 
	 *	
	 *	It is up to the calling GameCode to make sure these tags are added on clients/server where necessary
	 */

	FORCEINLINE void AddLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count=1)
	{
		UpdateTagMap(GameplayTag, Count);
	}

	FORCEINLINE void AddLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count = 1)
	{
		UpdateTagMap(GameplayTags, Count);
	}

	FORCEINLINE void RemoveLooseGameplayTag(const FGameplayTag& GameplayTag, int32 Count = 1)
	{
		UpdateTagMap(GameplayTag, -Count);
	}

	FORCEINLINE void RemoveLooseGameplayTags(const FGameplayTagContainer& GameplayTags, int32 Count = 1)
	{
		UpdateTagMap(GameplayTags, -Count);
	}

	FORCEINLINE void SetLooseGameplayTagCount(const FGameplayTag& GameplayTag, int32 NewCount)
	{
		SetTagMapCount(GameplayTag, NewCount);
	}

	/** 	 
	 * Minimally replicated tags are replicated tags that come from GEs when in bMinimalReplication mode. 
	 * (The GEs do not replicate, but the tags they grant do replicate via these functions)
	 */

	FORCEINLINE void AddMinimalReplicationGameplayTag(const FGameplayTag& GameplayTag)
	{
		MinimalReplicationTags.AddTag(GameplayTag);
	}

	FORCEINLINE void AddMinimalReplicationGameplayTags(const FGameplayTagContainer& GameplayTags)
	{
		MinimalReplicationTags.AddTags(GameplayTags);
	}

	FORCEINLINE void RemoveMinimalReplicationGameplayTag(const FGameplayTag& GameplayTag)
	{
		MinimalReplicationTags.RemoveTag(GameplayTag);
	}

	FORCEINLINE void RemoveMinimalReplicationGameplayTags(const FGameplayTagContainer& GameplayTags)
	{
		MinimalReplicationTags.RemoveTags(GameplayTags);
	}
	
	/** Allow events to be registered for specific gameplay tags being added or removed */
	FOnGameplayEffectTagCountChanged& RegisterGameplayTagEvent(FGameplayTag Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);

	void RegisterAndCallGameplayTagEvent(FGameplayTag Tag, FOnGameplayEffectTagCountChanged::FDelegate Delegate, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);

	/** Returns multicast delegate that is invoked whenever a tag is added or removed (but not if just count is increased. Only for 'new' and 'removed' events) */
	FOnGameplayEffectTagCountChanged& RegisterGenericGameplayTagEvent();


	FOnGameplayAttributeChange& RegisterGameplayAttributeEvent(FGameplayAttribute Attribute);

	FOnGameplayAttributeValueChange& GetGameplayAttributeValueChangeDelegate(FGameplayAttribute Attribute);

	// --------------------------------------------
	// System Attributes
	// --------------------------------------------
	
	UPROPERTY(meta=(SystemGameplayAttribute="true"))
	float OutgoingDuration;

	UPROPERTY(meta = (SystemGameplayAttribute = "true"))
	float IncomingDuration;

	static UProperty* GetOutgoingDurationProperty();
	static UProperty* GetIncomingDurationProperty();

	static const FGameplayEffectAttributeCaptureDefinition& GetOutgoingDurationCapture();
	static const FGameplayEffectAttributeCaptureDefinition& GetIncomingDurationCapture();


	// --------------------------------------------
	// Additional Helper Functions
	// --------------------------------------------
	
	DEPRECATED(4.17, "Use OnGameplayEffectRemoved_InfoDelegate (the delegate signature has changed)")
	FOnActiveGameplayEffectRemoved* OnGameplayEffectRemovedDelegate(FActiveGameplayEffectHandle Handle);

	FOnActiveGameplayEffectRemoved_Info* OnGameplayEffectRemoved_InfoDelegate(FActiveGameplayEffectHandle Handle);

	FOnGivenActiveGameplayEffectRemoved& OnAnyGameplayEffectRemovedDelegate();

	FOnActiveGameplayEffectStackChange* OnGameplayEffectStackChangeDelegate(FActiveGameplayEffectHandle Handle);

	FOnActiveGameplayEffectTimeChange* OnGameplayEffectTimeChangeDelegate(FActiveGameplayEffectHandle Handle);

	UFUNCTION(BlueprintCallable, Category = GameplayEffects, meta=(DisplayName = "ApplyGameplayEffectToTarget"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToTarget(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context);
	FActiveGameplayEffectHandle ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level = UGameplayEffect::INVALID_LEVEL, FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(), FPredictionKey PredictionKey = FPredictionKey());

	UFUNCTION(BlueprintCallable, Category = GameplayEffects, meta=(DisplayName = "ApplyGameplayEffectToSelf"))
	FActiveGameplayEffectHandle BP_ApplyGameplayEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext);
	
	FActiveGameplayEffectHandle ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey = FPredictionKey());

	// Returns the number of gameplay effects that are currently active on this ability system component
	int32 GetNumActiveGameplayEffects() const
	{
		return ActiveGameplayEffects.GetNumGameplayEffects();
	}

	// Makes a copy of all the active effects on this ability component
	void GetAllActiveGameplayEffectSpecs(TArray<FGameplayEffectSpec>& OutSpecCopies) const
	{
		ActiveGameplayEffects.GetAllActiveGameplayEffectSpecs(OutSpecCopies);
	}

	void SetBaseAttributeValueFromReplication(float NewValue, FGameplayAttribute Attribute)
	{
		ActiveGameplayEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue);
	}

	void SetBaseAttributeValueFromReplication(FGameplayAttributeData NewValue, FGameplayAttribute Attribute)
	{
		ActiveGameplayEffects.SetBaseAttributeValueFromReplication(Attribute, NewValue.GetBaseValue());
	}

	/** Tests if all modifiers in this GameplayEffect will leave the attribute > 0.f */
	bool CanApplyAttributeModifiers(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext)
	{
		return ActiveGameplayEffects.CanApplyAttributeModifiers(GameplayEffect, Level, EffectContext);
	}

	// Generic 'Get expected magnitude (list) if I was to apply this outgoing or incoming'

	// Get duration or magnitude (list) of active effects
	//		-Get duration of CD
	//		-Get magnitude + duration of a movespeed buff

	TArray<float> GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const;

	TArray<float> GetActiveEffectsDuration(const FGameplayEffectQuery& Query) const;

	TArray<TPair<float,float>> GetActiveEffectsTimeRemainingAndDuration(const FGameplayEffectQuery& Query) const;

	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category = "GameplayEffects", meta=(DisplayName = "Get Activate Gameplay Effects for Query"))
	TArray<FActiveGameplayEffectHandle> GetActiveEffects(const FGameplayEffectQuery& Query) const;

	/** This will give the world time that all effects matching this query will be finished. If multiple effects match, it returns the one that returns last.*/
	float GetActiveEffectsEndTime(const FGameplayEffectQuery& Query) const
	{
		return ActiveGameplayEffects.GetActiveEffectsEndTime(Query);
	}

	bool GetActiveEffectsEndTimeAndDuration(const FGameplayEffectQuery& Query, float& EndTime, float& Duration) const
	{
		return ActiveGameplayEffects.GetActiveEffectsEndTimeAndDuration(Query, EndTime, Duration);
	}

	void ModifyActiveEffectStartTime(FActiveGameplayEffectHandle Handle, float StartTimeDiff)
	{
		ActiveGameplayEffects.ModifyActiveEffectStartTime(Handle, StartTimeDiff);
	}

	/** Removes all active effects that contain any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	int32 RemoveActiveEffectsWithTags(FGameplayTagContainer Tags);

	/** Removes all active effects with captured source tags that contain any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	int32 RemoveActiveEffectsWithSourceTags(FGameplayTagContainer Tags);

	/** Removes all active effects that apply any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	int32 RemoveActiveEffectsWithAppliedTags(FGameplayTagContainer Tags);

	/** Removes all active effects that grant any of the tags in Tags */
	UFUNCTION(BlueprintCallable, Category = GameplayEffects)
	int32 RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer Tags);

	/** Removes all active effects that match given query. StacksToRemove=-1 will remove all stacks. */
	int32 RemoveActiveEffects(const FGameplayEffectQuery& Query, int32 StacksToRemove = -1);

	/** Invokes the WhileActive event for all GCs on active, non inhibited, GEs. This would typically be used on "respawn" or something where the mesh/avatar has changed */
	void ReinvokeActiveGameplayCues();

	// -----------------------------------------------------------

	void OnRestackGameplayEffects();	
	
	void PrintAllGameplayEffects() const;

	bool CachedIsNetSimulated;

	/** Returns true of this component has authority */
	bool IsOwnerActorAuthoritative() const
	{
		return !CachedIsNetSimulated;
	}

	// ----------------------------------------------------------------------------------------------------------------
	//
	//	GameplayCues
	// 
	// ----------------------------------------------------------------------------------------------------------------
	 
	// Do not call these functions directly, call the wrappers on GameplayCueManager instead
	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueExecuted_FromSpec(const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueExecuted(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCuesExecuted(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueExecuted_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCuesExecuted_WithParams(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueAdded(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueAdded_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters Parameters);
	
	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(const FGameplayEffectSpecForRPC& Spec, FPredictionKey PredictionKey);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	UFUNCTION(NetMulticast, unreliable)
	void NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters);

	// GameplayCues can also come on their own. These take an optional effect context to pass through hit result, etc
	void ExecuteGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext = FGameplayEffectContextHandle());

	// This version allows the caller to set an explicit FGameplayCueParmeters.
	void ExecuteGameplayCue(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters);

	// -------------------------

	void AddGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext = FGameplayEffectContextHandle())
	{
		AddGameplayCue_Internal(GameplayCueTag, EffectContext, ActiveGameplayCues);
	}

	/** Add gameplaycue for minimal replication mode. Should only be called in paths that would replicate gameplaycues in other ways (through GE for example) if not in minimal replication mode */
	void AddGameplayCue_MinimalReplication(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext = FGameplayEffectContextHandle())
	{
		AddGameplayCue_Internal(GameplayCueTag, EffectContext, MinimalReplicationGameplayCues);
	}

	void AddGameplayCue_Internal(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle& EffectContext, FActiveGameplayCueContainer& GameplayCueContainer );

	// -------------------------
	
	void RemoveGameplayCue(const FGameplayTag GameplayCueTag)
	{
		RemoveGameplayCue_Internal(GameplayCueTag, ActiveGameplayCues);
	}

	/** Remove gameplaycue for minimal replication mode. Should only be called in paths that would replicate gameplaycues in other ways (through GE for example) if not in minimal replication mode */
	void RemoveGameplayCue_MinimalReplication(const FGameplayTag GameplayCueTag)
	{
		RemoveGameplayCue_Internal(GameplayCueTag, MinimalReplicationGameplayCues);
	}

	void RemoveGameplayCue_Internal(const FGameplayTag GameplayCueTag, FActiveGameplayCueContainer& GameplayCueContainer);

	// -------------------------

	/** Removes any GameplayCue added on its own, i.e. not as part of a GameplayEffect. */
	void RemoveAllGameplayCues();
	
	void InvokeGameplayCueEvent(const FGameplayEffectSpecForRPC& Spec, EGameplayCueEvent::Type EventType);

	void InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayEffectContextHandle EffectContext = FGameplayEffectContextHandle());

	void InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& GameplayCueParameters);

	/** Allows polling to see if a GameplayCue is active. We expect most GameplayCue handling to be event based, but some cases we may need to check if a GamepalyCue is active (Animation Blueprint for example) */
	UFUNCTION(BlueprintCallable, Category="GameplayCue", meta=(GameplayTagFilter="GameplayCue"))
	bool IsGameplayCueActive(const FGameplayTag GameplayCueTag) const
	{
		return HasMatchingGameplayTag(GameplayCueTag);
	}

	/** Will initialize gameplay cue parameters with this ASC's Owner (Instigator) and AvatarActor (EffectCauser) */
	virtual void InitDefaultGameplayCueParameters(FGameplayCueParameters& Parameters);

	/** Are we ready to invoke gameplaycues yet? */
	virtual bool IsReadyForGameplayCues();

	// ----------------------------------------------------------------------------------------------------------------

	/**
	 *	GameplayAbilities
	 *	
	 *	The role of the AbilitySystemComponent with respect to Abilities is to provide:
	 *		-Management of ability instances (whether per actor or per execution instance).
	 *			-Someone *has* to keep track of these instances.
	 *			-Non instanced abilities *could* be executed without any ability stuff in AbilitySystemComponent.
	 *				They should be able to operate on an GameplayAbilityActorInfo + GameplayAbility.
	 *		
	 *	As convenience it may provide some other features:
	 *		-Some basic input binding (whether instanced or non instanced abilities).
	 *		-Concepts like "this component has these abilities
	 *	
	 */

	/** Grants Ability. Returns handle that can be used in TryActivateAbility, etc. */
	FGameplayAbilitySpecHandle GiveAbility(const FGameplayAbilitySpec& AbilitySpec);

	/** Grants an ability and attempts to activate it exactly one time, which will cause it to be removed. Only valid on the server! */
	FGameplayAbilitySpecHandle GiveAbilityAndActivateOnce(const FGameplayAbilitySpec& AbilitySpec);

	/** Wipes all 'given' abilities. */
	void ClearAllAbilities();

	/** Removes the specified ability */
	void ClearAbility(const FGameplayAbilitySpecHandle& Handle);
	
	/** Sets an ability spec to remove when its finished. If the spec is not currently active, it terminates it immediately. Also clears InputID of the Spec. */
	void SetRemoveAbilityOnEnd(FGameplayAbilitySpecHandle AbilitySpecHandle);

	/** 
	 * Gets all Activatable Gameplay Abilities that match all tags in GameplayTagContainer AND for which
	 * DoesAbilitySatisfyTagRequirements() is true.  The latter requirement allows this function to find the correct
	 * ability without requiring advanced knowledge.  For example, if there are two "Melee" abilities, one of which
	 * requires a weapon and one of which requires being unarmed, then those abilities can use Blocking and Required
	 * tags to determine when they can fire.  Using the Satisfying Tags requirements simplifies a lot of usage cases.
	 * For example, Behavior Trees can use various decorators to test an ability fetched using this mechanism as well
	 * as the Task to execute the ability without needing to know that there even is more than one such ability.
	 */
	void GetActivatableGameplayAbilitySpecsByAllMatchingTags(const FGameplayTagContainer& GameplayTagContainer, TArray < struct FGameplayAbilitySpec* >& MatchingGameplayAbilities, bool bOnlyAbilitiesThatSatisfyTagRequirements = true) const;

	/** 
	 * Attempts to activate every gameplay ability that matches the given tag and DoesAbilitySatisfyTagRequirements().
	 * Returns true if anything attempts to activate. Can activate more than one ability and the ability may fail later.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate abilities.
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool TryActivateAbilitiesByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation = true);

	/**
	 * Attempts to activate the ability that is passed in. This will check costs and requirements before doing so.
	 * Returns true if it thinks it activated, but it may return false positives due to failure later in activation.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate the ability
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool TryActivateAbilityByClass(TSubclassOf<UGameplayAbility> InAbilityToActivate, bool bAllowRemoteActivation = true);

	/** 
	 * Attempts to activate the given ability, will check costs and requirements before doing so.
	 * Returns true if it thinks it activated, but it may return false positives due to failure later in activation.
	 * If bAllowRemoteActivation is true, it will remotely activate local/server abilities, if false it will only try to locally activate the ability
	 */
	bool TryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool bAllowRemoteActivation = true);

	/** Triggers an ability from a gameplay event, will only trigger on local/server depending on execution flags */
	bool TriggerAbilityFromGameplayEvent(FGameplayAbilitySpecHandle AbilityToTrigger, FGameplayAbilityActorInfo* ActorInfo, FGameplayTag Tag, const FGameplayEventData* Payload, UAbilitySystemComponent& Component);

	// --------------------------------------------
	// Ability Cancelling/Interrupts
	// --------------------------------------------

	/** Cancels the specified ability CDO. */
	void CancelAbility(UGameplayAbility* Ability);	

	/** Cancels the ability indicated by passed in spec handle. If handle is not found among reactivated abilities nothing happens. */
	void CancelAbilityHandle(const FGameplayAbilitySpecHandle& AbilityHandle);

	/** Cancel all abilities with the specified tags. Will not cancel the Ignore instance */
	void CancelAbilities(const FGameplayTagContainer* WithTags=nullptr, const FGameplayTagContainer* WithoutTags=nullptr, UGameplayAbility* Ignore=nullptr);

	/** Cancels all abilities regardless of tags. Will not cancel the ignore instance */
	void CancelAllAbilities(UGameplayAbility* Ignore=nullptr);

	/** Cancels all abilities and kills any remaining instanced abilities */
	virtual void DestroyActiveState();

	// ----------------------------------------------------------------------------------------------------------------
	/** 
	 * Called from ability activation or native code, will apply the correct ability blocking tags and cancel existing abilities. Subclasses can override the behavior 
	 * @param AbilityTags The tags of the ability that has block and cancel flags
	 * @param RequestingAbility The gameplay ability requesting the change, can be NULL for native events
	 * @param bEnableBlockTags If true will enable the block tags, if false will disable the block tags
	 * @param BlockTags What tags to block
	 * @param bExecuteCancelTags If true will cancel abilities matching tags
	 * @param CancelTags what tags to cancel
	 */
	virtual void ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags);

	/** Called when an ability is cancellable or not. Doesn't do anything by default, can be overridden to tie into gameplay events */
	virtual void HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled) {}

	/** Returns true if any passed in tags are blocked */
	virtual bool AreAbilityTagsBlocked(const FGameplayTagContainer& Tags) const;

	void BlockAbilitiesWithTags(const FGameplayTagContainer& Tags);
	void UnBlockAbilitiesWithTags(const FGameplayTagContainer& Tags);

	/** Checks if the ability system is currently blocking InputID. Returns true if InputID is blocked, false otherwise.  */
	bool IsAbilityInputBlocked(int32 InputID) const;

	void BlockAbilityByInputID(int32 InputID);
	void UnBlockAbilityByInputID(int32 InputID);

	// Functions meant to be called from GameplayAbility and subclasses, but not meant for general use

	/** Returns the list of all activatable abilities */
	const TArray<FGameplayAbilitySpec>& GetActivatableAbilities() const
	{
		return ActivatableAbilities.Items;
	}

	TArray<FGameplayAbilitySpec>& GetActivatableAbilities()
	{
		return ActivatableAbilities.Items;
	}

	/** Returns local world time that an ability was activated. Valid on authority (server) and autonomous proxy (controlling client).  */
	float GetAbilityLastActivatedTime() const { return AbilityLastActivatedTime; }

	/** Returns an ability spec from a handle. If modifying call MarkAbilitySpecDirty */
	FGameplayAbilitySpec* FindAbilitySpecFromHandle(FGameplayAbilitySpecHandle Handle);
	
	/** Returns an ability spec from a GE handle. If modifying call MarkAbilitySpecDirty */
	FGameplayAbilitySpec* FindAbilitySpecFromGEHandle(FActiveGameplayEffectHandle Handle);

	/** Returns an ability spec corresponding to given ability class. If modifying call MarkAbilitySpecDirty */
	FGameplayAbilitySpec* FindAbilitySpecFromClass(TSubclassOf<UGameplayAbility> InAbilityClass);

	/** Returns an ability spec from a handle. If modifying call MarkAbilitySpecDirty */
	FGameplayAbilitySpec* FindAbilitySpecFromInputID(int32 InputID);

	/** Retrieves the EffectContext of the GameplayEffect of the active GameplayEffect. */
	FGameplayEffectContextHandle GetEffectContextFromActiveGEHandle(FActiveGameplayEffectHandle Handle);

	/** Call to mark that an ability spec has been modified */
	void MarkAbilitySpecDirty(FGameplayAbilitySpec& Spec);

	/** Attempts to activate the given ability, will only work if called from the correct client/server context */
	bool InternalTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey InPredictionKey = FPredictionKey(), UGameplayAbility ** OutInstancedAbility = nullptr, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate = nullptr, const FGameplayEventData* TriggerEventData = nullptr);

	// Failure tags used by InternalTryActivateAbility (E.g., this stores the  FailureTags of the last call to InternalTryActivateAbility
	FGameplayTagContainer InternalTryActivateAbilityFailureTags;

	/** Called from the ability to let the component know it is ended */
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled);

	/** Replicate that an ability has ended, to the client or server as appropriate */
	void ReplicateEndAbility(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo, UGameplayAbility* Ability);

	void IncrementAbilityListLock();
	void DecrementAbilityListLock();

	// --------------------------------------------
	// Debugging
	// --------------------------------------------

	struct FAbilitySystemComponentDebugInfo
	{
		FAbilitySystemComponentDebugInfo()
		{
			FMemory::Memzero(*this);
		}

		class UCanvas* Canvas;

		bool bPrintToLog;

		bool bShowAttributes;
		bool bShowGameplayEffects;;
		bool bShowAbilities;

		float XPos;
		float YPos;
		float OriginalX;
		float OriginalY;
		float MaxY;
		float NewColumnYPadding;
		float YL;

		bool Accumulate;
		TArray<FString>	Strings;

		int32 GameFlags; // arbitrary flags for games to set/read in Debug_Internal
	};

	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	virtual void DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	virtual void PrintDebug();

	void AccumulateScreenPos(FAbilitySystemComponentDebugInfo& Info);
	virtual void Debug_Internal(struct FAbilitySystemComponentDebugInfo& Info);
	void DebugLine(struct FAbilitySystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset);
	FString CleanupName(FString Str);

	/** Ask the server to send ability system debug information back to the client, via ClientPrintDebug_Response  */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerPrintDebug_Request();

	/** Same as ServerPrintDebug_Request but this includes the client debug strings so that the server can embed them in replays */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerPrintDebug_RequestWithStrings(const TArray<FString>& Strings);

	/** Virtual function games can override to do their own stuff when either ServerPrintDebug function runs on the server */
	virtual void OnServerPrintDebug_Request();

	/** Determines whether to call ServerPrintDebug_Request or ServerPrintDebug_RequestWithStrings.   */
	virtual bool ShouldSendClientDebugStringsToServer() const;

	UFUNCTION(Client, reliable)
	void ClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags);
	virtual void OnClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags);

	/** Called when the ability is forced cancelled due to replication */
	void ForceCancelAbilityDueToReplication(UGameplayAbility* Instance);

	UPROPERTY(ReplicatedUsing=OnRep_ClientDebugString)
	TArray<FString>	ClientDebugStrings;

	UPROPERTY(ReplicatedUsing=OnRep_ServerDebugString)
	TArray<FString>	ServerDebugStrings;

	UFUNCTION()
	virtual void OnRep_ClientDebugString();

	UFUNCTION()
	virtual void OnRep_ServerDebugString();

	float GetFilteredAttributeValue(const FGameplayAttribute& Attribute, const FGameplayTagRequirements& SourceTags, const FGameplayTagContainer& TargetTags);

protected:

	/**
	 *	The abilities we can activate. 
	 *		-This will include CDOs for non instanced abilities and per-execution instanced abilities. 
	 *		-Actor-instanced abilities will be the actual instance (not CDO)
	 *		
	 *	This array is not vital for things to work. It is a convenience thing for 'giving abilities to the actor'. But abilities could also work on things
	 *	without an AbilitySystemComponent. For example an ability could be written to execute on a StaticMeshActor. As long as the ability doesn't require 
	 *	instancing or anything else that the AbilitySystemComponent would provide, then it doesn't need the component to function.
	 */

	UPROPERTY(ReplicatedUsing=OnRep_ActivateAbilities, BlueprintReadOnly, Category = "Abilities")
	FGameplayAbilitySpecContainer	ActivatableAbilities;

	/** Maps from an ability spec to the target data. Used to track replicated data and callbacks */
	TMap<FGameplayAbilitySpecHandleAndPredictionKey, FAbilityReplicatedDataCache> AbilityTargetDataMap;

	/** Full list of all instance-per-execution gameplay abilities associated with this component */
	UPROPERTY()
	TArray<UGameplayAbility*>	AllReplicatedInstancedAbilities;

	/** Will be called from GiveAbility or from OnRep. Initializes events (triggers and inputs) with the given ability */
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec);

	/** Will be called from RemoveAbility or from OnRep. Unbinds inputs with the given ability */
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec);

	/** Called from ClearAbility, ClearAllAbilities or OnRep. Clears any triggers that should no longer exist. */
	void CheckForClearedAbilities();

	/** Cancel a specific ability spec */
	void CancelAbilitySpec(FGameplayAbilitySpec& Spec, UGameplayAbility* Ignore);

	/** Creates a new instance of an ability, storing it in the spec */
	UGameplayAbility* CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability);

	int32 AbilityScopeLockCount;
	TArray<FGameplayAbilitySpecHandle, TInlineAllocator<2> > AbilityPendingRemoves;
	TArray<FGameplayAbilitySpec, TInlineAllocator<2> > AbilityPendingAdds;

	/** Local World time of the last ability activation. This is used for AFK/idle detection */
	float AbilityLastActivatedTime;

	UFUNCTION()
	void	OnRep_ActivateAbilities();

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed, FPredictionKey PredictionKey);

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerTryActivateAbilityWithEventData(FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData);

	UFUNCTION(Client, reliable)
	void	ClientTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate);

	/** Called by ServerEndAbility and ClientEndAbility; avoids code duplication. */
	void	RemoteEndOrCancelAbility(FGameplayAbilitySpecHandle AbilityToEnd, FGameplayAbilityActivationInfo ActivationInfo, bool bWasCanceled);

	UFUNCTION(Server, reliable, WithValidation)
	void	ServerEndAbility(FGameplayAbilitySpecHandle AbilityToEnd, FGameplayAbilityActivationInfo ActivationInfo, FPredictionKey PredictionKey);

	UFUNCTION(Client, reliable)
	void	ClientEndAbility(FGameplayAbilitySpecHandle AbilityToEnd, FGameplayAbilityActivationInfo ActivationInfo);

	UFUNCTION(Server, reliable, WithValidation)
	void    ServerCancelAbility(FGameplayAbilitySpecHandle AbilityToCancel, FGameplayAbilityActivationInfo ActivationInfo);

	UFUNCTION(Client, reliable)
	void    ClientCancelAbility(FGameplayAbilitySpecHandle AbilityToCancel, FGameplayAbilityActivationInfo ActivationInfo);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilityFailed(FGameplayAbilitySpecHandle AbilityToActivate, int16 PredictionKey);
	int32	ClientActivateAbilityFailedCountRecent;
	float	ClientActivateAbilityFailedStartTime;

	
	void	OnClientActivateAbilityCaughtUp(FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey::KeyType PredictionKey);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilitySucceed(FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey PredictionKey);

	UFUNCTION(Client, Reliable)
	void	ClientActivateAbilitySucceedWithEventData(FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData);

public:

	// ----------------------------------------------------------------------------------------------------------------

	// This is meant to be used to inhibit activating an ability from an input perspective. (E.g., the menu is pulled up, another game mechanism is consuming all input, etc)
	// This should only be called on locally owned players.
	// This should not be used to game mechanics like silences or disables. Those should be done through gameplay effects.

	UFUNCTION(BlueprintCallable, Category="Abilities")
	bool	GetUserAbilityActivationInhibited() const;
	
	/** Disable or Enable a local user from being able to activate abilities. This should only be used for input/UI etc related inhibition. Do not use for game mechanics. */
	UFUNCTION(BlueprintCallable, Category="Abilities")
	void	SetUserAbilityActivationInhibited(bool NewInhibit);

	bool	UserAbilityActivationInhibited;

	// ----------------------------------------------------------------------------------------------------------------

	virtual void BindToInputComponent(UInputComponent* InputComponent);

	void SetBlockAbilityBindingsArray(FGameplayAbiliyInputBinds BindInfo);
	virtual void BindAbilityActivationToInputComponent(UInputComponent* InputComponent, FGameplayAbiliyInputBinds BindInfo);

	virtual void AbilityLocalInputPressed(int32 InputID);
	virtual void AbilityLocalInputReleased(int32 InputID);
	
	virtual void LocalInputConfirm();
	virtual void LocalInputCancel();
	
	/** Replicate that an ability has ended/canceled, to the client or server as appropriate */
	void	ReplicateEndOrCancelAbility(FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo, UGameplayAbility* Ability, bool bWasCanceled);

	/** InputID for binding GenericConfirm/Cancel events */
	int32 GenericConfirmInputID;
	int32 GenericCancelInputID;

	bool IsGenericConfirmInputBound(int32 InputID) const	{ return ((InputID == GenericConfirmInputID) && GenericLocalConfirmCallbacks.IsBound()); }
	bool IsGenericCancelInputBound(int32 InputID) const		{ return ((InputID == GenericCancelInputID) && GenericLocalCancelCallbacks.IsBound()); }

	/** Generic local callback for generic ConfirmEvent that any ability can listen to */
	FAbilityConfirmOrCancel	GenericLocalConfirmCallbacks;

	FAbilityEnded AbilityEndedCallbacks;

	FAbilitySpecDirtied AbilitySpecDirtiedCallbacks;

	/** Generic local callback for generic CancelEvent that any ability can listen to */
	FAbilityConfirmOrCancel	GenericLocalCancelCallbacks;

	/** A generic callback anytime an ability is activated (started) */	
	FGenericAbilityDelegate AbilityActivatedCallbacks;

	/** callback anytime an ability is ended */
	FGameplayAbilityEndedDelegate OnAbilityEnded;

	/** A generic callback anytime an ability is committed (cost/cooldown applied) */
	FGenericAbilityDelegate AbilityCommittedCallbacks;
	FAbilityFailedDelegate AbilityFailedCallbacks;

	/** Executes a gameplay event. Returns the number of successful ability activations triggered by the event */
	virtual int32 HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

	/** Generic callbacks for gameplay events. See UAbilityTask_WaitGameplayEvent */
	TMap<FGameplayTag, FGameplayEventMulticastDelegate> GenericGameplayEventCallbacks;

	virtual void NotifyAbilityCommit(UGameplayAbility* Ability);
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability);
	virtual void NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason);

	UPROPERTY()
	TArray<AGameplayAbilityTargetActor*>	SpawnedTargetActors;

	/** Any active targeting actors will be told to stop and return current targeting data */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void TargetConfirm();

	/** Any active targeting actors will be stopped and canceled, not returning any targeting data */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	void TargetCancel();

	// ----------------------------------------------------------------------------------------------------------------
	//	AnimMontage Support
	//	
	//	TODO:
	//	-Continously update RepAnimMontageInfo on server for join in progress clients.
	//	-Some missing functionality may still be needed (GetCurrentSectionTime, etc)	
	// ----------------------------------------------------------------------------------------------------------------	

	/** Plays a montage and handles replication and prediction based on passed in ability/activation info */
	float PlayMontage(UGameplayAbility* AnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	/** Plays a montage without updating replication/prediction structures. Used by simulated proxies when replication tells them to play a montage. */
	float PlayMontageSimulated(UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None);

	/** Stops whatever montage is currently playing. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageStop(float OverrideBlendOutTime = -1.0f);

	/** Clear the animating ability that is passed in, if it's still currently animating */
	void ClearAnimatingAbility(UGameplayAbility* Ability);

	/** Jumps current montage to given section. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageJumpToSection(FName SectionName);

	/** Sets current montages next section name. Expectation is caller should only be stopping it if they are the current animating ability (or have good reason not to check) */
	void CurrentMontageSetNextSectionName(FName FromSectionName, FName ToSectionName);

	/** Sets current montage's play rate */
	void CurrentMontageSetPlayRate(float InPlayRate);

	/** Returns true if the passed in ability is the current animating ability */
	bool IsAnimatingAbility(UGameplayAbility* Ability) const;

	/** Returns the current animating ability */
	UGameplayAbility* GetAnimatingAbility();

	/** Returns montage that is currently playing */
	UAnimMontage* GetCurrentMontage() const;

	/** Get SectionID of currently playing AnimMontage */
	int32 GetCurrentMontageSectionID() const;

	/** Get SectionName of currently playing AnimMontage */
	FName GetCurrentMontageSectionName() const;

	/** Get length in time of current section */
	float GetCurrentMontageSectionLength() const;

	/** Returns amount of time left in current section */
	float GetCurrentMontageSectionTimeLeft() const;

protected:

	/** Implementation of ServerTryActivateAbility */
	virtual void InternalServerTryActiveAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed, const FPredictionKey& PredictionKey, const FGameplayEventData* TriggerEventData);

	/** Called when a prediction key that played a montage is rejected */
	void OnPredictiveMontageRejected(UAnimMontage* PredictiveMontage);

	/** Copy LocalAnimMontageInfo into RepAnimMontageInfo */
	void AnimMontage_UpdateReplicatedData();

	/** Data structure for replicating montage info to simulated clients */
	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedAnimMontage)
	FGameplayAbilityRepAnimMontage RepAnimMontageInfo;

	/** Set if montage rep happens while we don't have the animinstance associated with us yet */
	bool bPendingMontagerep;

	/** Data structure for montages that were instigated locally (everything if server, predictive if client. replicated if simulated proxy) */
	UPROPERTY()
	FGameplayAbilityLocalAnimMontage LocalAnimMontageInfo;

	UFUNCTION()
	void OnRep_ReplicatedAnimMontage();

	/** Returns true if we are ready to handle replicated montage information */
	virtual bool IsReadyForReplicatedMontage();

	/** RPC function called from CurrentMontageSetNextSectopnName, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageSetNextSectionName(UAnimMontage* ClientAnimMontage, float ClientPosition, FName SectionName, FName NextSectionName);

	/** RPC function called from CurrentMontageJumpToSection, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageJumpToSectionName(UAnimMontage* ClientAnimMontage, FName SectionName);

	/** RPC function called from CurrentMontageSetPlayRate, replicates to other clients */
	UFUNCTION(reliable, server, WithValidation)
	void ServerCurrentMontageSetPlayRate(UAnimMontage* ClientAnimMontage, float InPlayRate);

	/** Abilities that are triggered from a gameplay event */
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle > > GameplayEventTriggeredAbilities;

	/** Abilities that are triggered from a tag being added to the owner */
	TMap<FGameplayTag, TArray<FGameplayAbilitySpecHandle > > OwnedTagTriggeredAbilities;

	/** Callback that is called when an owned tag bound to an ability changes */
	virtual void MonitoredTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** Returns true if the specified ability should be activated from an event in this network mode */
	bool HasNetworkAuthorityToActivateTriggeredAbility(const FGameplayAbilitySpec &Spec) const;

	virtual void OnImmunityBlockGameplayEffect(const FGameplayEffectSpec& Spec, const FActiveGameplayEffect* ImmunityGE);

	// -----------------------------------------------------------------------------
public:

	/** Immunity notification support */
	FImmunityBlockGE OnImmunityBlockGameplayEffectDelegate;

	/** The actor that owns this component logically */
	UPROPERTY(ReplicatedUsing = OnRep_OwningActor)
	AActor* OwnerActor;

	/** The actor that is the physical representation used for abilities. Can be NULL */
	UPROPERTY(ReplicatedUsing = OnRep_OwningActor)
	AActor* AvatarActor;
	
	UFUNCTION()
	void OnRep_OwningActor();

	/** Cached off data about the owning actor that abilities will need to frequently access (movement component, mesh component, anim instance, etc) */
	TSharedPtr<FGameplayAbilityActorInfo>	AbilityActorInfo;

	/**
	 *	Initialized the Abilities' ActorInfo - the structure that holds information about who we are acting on and who controls us.
	 *      OwnerActor is the actor that logically owns this component.
	 *		AvatarActor is what physical actor in the world we are acting on. Usually a Pawn but it could be a Tower, Building, Turret, etc, may be the same as Owner
	 */
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

	/** Changes the avatar actor, leaves the owner actor the same */
	void SetAvatarActor(AActor* InAvatarActor);

	/** called when the ASC's AbilityActorInfo has a PlayerController set. */
	virtual void OnPlayerControllerSet() { }

	/**
	* This is called when the actor that is initialized to this system dies, this will clear that actor from this system and FGameplayAbilityActorInfo
	*/
	void ClearActorInfo();

	/**
	 *	This will refresh the Ability's ActorInfo structure based on the current ActorInfo. That is, AvatarActor will be the same but we will look for new
	 *	AnimInstance, MovementComponent, PlayerController, etc.
	 */	
	void RefreshAbilityActorInfo();

	// -----------------------------------------------------------------------------

	/**
	 *	While these appear to be state, these are actually synchronization events w/ some payload data
	 */
	
	/** Replicates the Generic Replicated Event to the server. */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey);

	/** Replicates the Generic Replicated Event to the server with payload. */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedEventWithPayload(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload);

	/** Replicates the Generic Replicated Event to the client. */
	UFUNCTION(Client, reliable)
	void ClientSetReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls local callbacks that are registered with the given Generic Replicated Event */
	bool InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey = FPredictionKey());

	/** Calls local callbacks that are registered with the given Generic Replicated Event */
	bool InvokeReplicatedEventWithPayload(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey, FVector_NetQuantize100 VectorPayload);
	
	/**  */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedTargetData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FGameplayAbilityTargetDataHandle& ReplicatedTargetDataHandle, FGameplayTag ApplicationTag, FPredictionKey CurrentPredictionKey);

	/** Replicates to the server that targeting has been cancelled */
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetReplicatedTargetDataCancelled(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FPredictionKey CurrentPredictionKey);

	/** Sets the current target data and calls applicable callbacks */
	void ConfirmAbilityTargetData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, const FGameplayAbilityTargetDataHandle& TargetData, const FGameplayTag& ApplicationTag);

	/** Cancels the ability target data and calls callbacks */
	void CancelAbilityTargetData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Deletes all cached ability client data (Was: ConsumeAbilityTargetData)*/
	void ConsumeAllReplicatedData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);
	/** Consumes cached TargetData from client (only TargetData) */
	void ConsumeClientReplicatedTargetData(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Consumes the given Generic Replicated Event (unsets it). */
	void ConsumeGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Gets replicated data of the given Generic Replicated Event. */
	FAbilityReplicatedData GetReplicatedDataOfGenericReplicatedEvent(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);
	
	/** Calls any Replicated delegates that have been sent (TargetData or Generic Replicated Events). Note this can be dangerous if multiple places in an ability register events and then call this function. */
	void CallAllReplicatedDelegatesIfSet(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls the TargetData Confirm/Cancel events if they have been sent. */
	bool CallReplicatedTargetDataDelegatesIfSet(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls a given Generic Replicated Event delegate if the event has already been sent */
	bool CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Calls passed in delegate if the Client Event has already been sent. If not, it adds the delegate to our multicast callback that will fire when it does. */
	bool CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey, FSimpleMulticastDelegate::FDelegate Delegate);

	/** Returns TargetDataSet delegate for a given Ability/PredictionKey pair */
	FAbilityTargetDataSetDelegate& AbilityTargetDataSetDelegate(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Returns TargetData Cancelled delegate for a given Ability/PredictionKey pair */
	FSimpleMulticastDelegate& AbilityTargetDataCancelledDelegate(FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	/** Returns Generic Replicated Event for a given Ability/PredictionKey pair */
	FSimpleMulticastDelegate& AbilityReplicatedEventDelegate(EAbilityGenericReplicatedEvent::Type EventType, FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey AbilityOriginalPredictionKey);

	// Direct Input state replication. These will be called if bReplicateInputDirectly is true on the ability and is generally not a good thing to use. (Instead, prefer to use Generic Replicated Events).
	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetInputPressed(FGameplayAbilitySpecHandle AbilityHandle);

	UFUNCTION(Server, reliable, WithValidation)
	void ServerSetInputReleased(FGameplayAbilitySpecHandle AbilityHandle);

	/** Called on local player always. Called on server only if bReplicateInputDirectly is set on the GameplayAbility. */
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec);

	/** Called on local player always. Called on server only if bReplicateInputDirectly is set on the GameplayAbility. */
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec);

	// ---------------------------------------------------------------------

	FORCEINLINE void SetTagMapCount(const FGameplayTag& Tag, int32 NewCount)
	{
		GameplayTagCountContainer.SetTagCount(Tag, NewCount);
	}
	
	FORCEINLINE void UpdateTagMap(const FGameplayTag& BaseTag, int32 CountDelta)
	{
		if (GameplayTagCountContainer.UpdateTagCount(BaseTag, CountDelta))
		{
			OnTagUpdated(BaseTag, CountDelta > 0);
		}
	}
	
	FORCEINLINE void UpdateTagMap(const FGameplayTagContainer& Container, int32 CountDelta)
	{
		for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FGameplayTag& Tag = *TagIt;
			UpdateTagMap(Tag, CountDelta);
		}
	}	


#if ENABLE_VISUAL_LOG
	void ClearDebugInstantEffects();
#endif // ENABLE_VISUAL_LOG

	const FActiveGameplayEffect* GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle) const;

	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	AActor* GetAvatarActor() const;

	/** Suppress all ability granting through GEs on this component */
	bool bSuppressGrantAbility;

	/** Suppress all GameplayCues on this component */
	bool bSuppressGameplayCues;

	/** Handle GameplayCues that may have been deferred while doing the NetDeltaSerialize and waiting for the avatar actor to get loaded */
	void HandleDeferredGameplayCues(const FActiveGameplayEffectsContainer* GameplayEffectsContainer);

protected:

	/** Actually pushes the final attribute value to the attribute set's property. Should not be called by outside code since this does not go through the attribute aggregator system. */
	void SetNumericAttribute_Internal(const FGameplayAttribute &Attribute, float& NewFloatValue);

	bool HasNetworkAuthorityToApplyGameplayEffect(FPredictionKey PredictionKey) const;

	void ExecutePeriodicEffect(FActiveGameplayEffectHandle	Handle);

	void ExecuteGameplayEffect(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey);

	void CheckDurationExpired(FActiveGameplayEffectHandle Handle);

	void OnAttributeGameplayEffectSpecExected(const FGameplayAttribute &Attribute, const struct FGameplayEffectSpec &Spec, struct FGameplayModifierEvaluatedData &Data);
		
	TArray<UGameplayTask*>&	GetAbilityActiveTasks(UGameplayAbility* Ability);
	// --------------------------------------------
	
	// Contains all of the gameplay effects that are currently active on this component
	UPROPERTY(Replicated)
	FActiveGameplayEffectsContainer	ActiveGameplayEffects;

	UPROPERTY(Replicated)
	FActiveGameplayCueContainer	ActiveGameplayCues;

	/** Replicated gameplaycues when in minimal replication mode. These are cues that would come normally come from ActiveGameplayEffects */
	UPROPERTY(Replicated)
	FActiveGameplayCueContainer	MinimalReplicationGameplayCues;

	/** Abilities with these tags are not able to be activated */
	FGameplayTagCountContainer BlockedAbilityTags;

	/** Tracks abilities that are blocked based on input binding. An ability is blocked if BlockedAbilityBindings[InputID] > 0 */
	UPROPERTY(Transient, Replicated)
	TArray<uint8> BlockedAbilityBindings;

	void DebugCyclicAggregatorBroadcasts(struct FAggregator* Aggregator);

	// ---------------------------------------------
	
	// Acceleration map for all gameplay tags (OwnedGameplayTags from GEs and explicit GameplayCueTags)
	FGameplayTagCountContainer GameplayTagCountContainer;

	UPROPERTY(Replicated)
	FMinimalReplicationTagCountMap MinimalReplicationTags;

	void ResetTagMap();

	void NotifyTagMap_StackCountChange(const FGameplayTagContainer& Container);

	virtual void OnTagUpdated(const FGameplayTag& Tag, bool TagExists) {};
	
	// ---------------------------------------------

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual void BeginPlay() override;

	const UAttributeSet*	GetAttributeSubobject(const TSubclassOf<UAttributeSet> AttributeClass) const;
	const UAttributeSet*	GetAttributeSubobjectChecked(const TSubclassOf<UAttributeSet> AttributeClass) const;
	const UAttributeSet*	GetOrCreateAttributeSubobject(TSubclassOf<UAttributeSet> AttributeClass);

	friend struct FActiveGameplayEffect;
	friend struct FActiveGameplayEffectAction;
	friend struct FActiveGameplayEffectsContainer;
	friend struct FActiveGameplayCue;
	friend struct FActiveGameplayCueContainer;
	friend struct FGameplayAbilitySpec;
	friend struct FGameplayAbilitySpecContainer;
	friend struct FAggregator;

private:
	FDelegateHandle MonitoredTagChangedDelegateHandle;
	FTimerHandle    OnRep_ActivateAbilitiesTimerHandle;

	/** Caches the flags that indicate whether this component has network authority. */
	void CacheIsNetSimulated();

public:

	/** PredictionKeys, see more info in GameplayPrediction.h. This has to come *last* in all replicated properties on the AbilitySystemComponent to ensure OnRep/callback order. */
	UPROPERTY(Replicated)
	FReplicatedPredictionKeyMap	ReplicatedPredictionKeyMap;
};
