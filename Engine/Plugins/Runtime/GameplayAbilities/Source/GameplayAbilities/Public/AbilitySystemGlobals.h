// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitiesModule.h"
#include "AbilitySystemGlobals.generated.h"

class UAbilitySystemComponent;
class UGameplayCueManager;
class UGameplayTagReponseTable;
struct FGameplayAbilityActorInfo;
struct FGameplayEffectSpec;
struct FGameplayEffectSpecForRPC;

/** Called when ability fails to activate, passes along the failed ability and a tag explaining why */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbilitySystemAssetOpenedDelegate, FString , int );
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbilitySystemAssetFoundDelegate, FString, int);


/** Holds global data for the ability system. Can be configured per project via config file */
UCLASS(config=Game)
class GAMEPLAYABILITIES_API UAbilitySystemGlobals : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Gets the single instance of the globals object, will create it as necessary */
	static UAbilitySystemGlobals& Get()
	{
		return *IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals();
	}

	/** Should be called once as part of project setup to load global data tables and tags */
	virtual void InitGlobalData();

	/** Returns true if InitGlobalData has been called */
	bool IsAbilitySystemGlobalsInitialized()
	{
		return GlobalAttributeSetInitter.IsValid();
	}

	/** Returns the curvetable used as the default for scalable floats that don't specify a curve table */
	UCurveTable* GetGlobalCurveTable();

	/** Returns the data table defining attribute metadata (NOTE: Currently not in use) */
	UDataTable* GetGlobalAttributeMetaDataTable();

	/** Returns data used to initialize attributes to their default values */
	FAttributeSetInitter* GetAttributeSetInitter() const;

	/** Searches the passed in actor for an ability system component, will use the AbilitySystemInterface */
	static UAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent=false);

	/** Should allocate a project specific AbilityActorInfo struct. Caller is responsible for deallocation */
	virtual FGameplayAbilityActorInfo* AllocAbilityActorInfo() const;

	/** Should allocate a project specific GameplayEffectContext struct. Caller is responsible for deallocation */
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const;

	/** Global callback that can handle game-specific code that needs to run before applying a gameplay effect spec */
	virtual void GlobalPreGameplayEffectSpecApply(FGameplayEffectSpec& Spec, UAbilitySystemComponent* AbilitySystemComponent);

	// Stubs for WIP feature that will come to engine
	virtual void PushCurrentAppliedGE(const FGameplayEffectSpec* Spec, UAbilitySystemComponent* AbilitySystemComponent) { }
	virtual void SetCurrentAppliedGE(const FGameplayEffectSpec* Spec) { }
	virtual void PopCurrentAppliedGE() { }

	/** Returns true if the ability system should try to predict gameplay effects applied to non local targets */
	bool ShouldPredictTargetGameplayEffects() const
	{
		return PredictTargetGameplayEffects;
	}

	/** Searches the passed in class to look for a UFunction implementing the gameplay cue tag, sets MatchedTag to the exact tag found */
	UFunction* GetGameplayCueFunction(const FGameplayTag &Tag, UClass* Class, FName &MatchedTag);

	/** Returns the gameplay cue manager singleton object, creating if necessary */
	virtual UGameplayCueManager* GetGameplayCueManager();

	/** Returns the gameplay tag response object, creating if necessary */
	UGameplayTagReponseTable* GetGameplayTagResponseTable();

	/** Sets a default gameplay cue tag using the asset's name. Returns true if it changed the tag. */
	static bool DeriveGameplayCueTagFromAssetName(FString AssetName, FGameplayTag& GameplayCueTag, FName& GameplayCueName);

	template<class T>
	static void DeriveGameplayCueTagFromClass(T* CDO)
	{
#if WITH_EDITOR
		UClass* ParentClass = CDO->GetClass()->GetSuperClass();
		if (T* ParentCDO = Cast<T>(ParentClass->GetDefaultObject()))
		{
			if (ParentCDO->GameplayCueTag.IsValid() && (ParentCDO->GameplayCueTag == CDO->GameplayCueTag))
			{
				// Parente has a valid tag. But maybe there is a better one for this class to use.
				// Reset our GameplayCueTag and see if we find one.
				FGameplayTag ParentTag = ParentCDO->GameplayCueTag;
				CDO->GameplayCueTag = FGameplayTag();
				if (UAbilitySystemGlobals::DeriveGameplayCueTagFromAssetName(CDO->GetName(), CDO->GameplayCueTag, CDO->GameplayCueName) == false)
				{
					// We did not find one, so parent tag it is.
					CDO->GameplayCueTag = ParentTag;
				}
				return;
			}
		}
		UAbilitySystemGlobals::DeriveGameplayCueTagFromAssetName(CDO->GetName(), CDO->GameplayCueTag, CDO->GameplayCueName);
#endif
	}

#if WITH_EDITOR
	// Allows projects to override PostEditChangeProeprty on GEs without having to subclass Gameplayeffect. Intended for validation/auto populating based on changed data.
	virtual void GameplayEffectPostEditChangeProperty(class UGameplayEffect* GE, FPropertyChangedEvent& PropertyChangedEvent) { }
#endif

	/** The class to instantiate as the globals object. Defaults to this class but can be overridden */
	UPROPERTY(config)
	FSoftClassPath AbilitySystemGlobalsClassName;

	void AutomationTestOnly_SetGlobalCurveTable(UCurveTable *InTable)
	{
		GlobalCurveTable = InTable;
	}

	void AutomationTestOnly_SetGlobalAttributeDataTable(UDataTable *InTable)
	{
		GlobalAttributeMetaDataTable = InTable;
	}

	// Cheat functions

	/** Toggles whether we should ignore ability cooldowns. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreAbilitySystemCooldowns();

	/** Toggles whether we should ignore ability costs. Does nothing in shipping builds */
	UFUNCTION(exec)
	virtual void ToggleIgnoreAbilitySystemCosts();

	/** Returns true if ability cooldowns are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCooldowns() const;

	/** Returns true if ability costs are ignored, returns false otherwise. Always returns false in shipping builds. */
	bool ShouldIgnoreCosts() const;

	DECLARE_MULTICAST_DELEGATE(FOnClientServerDebugAvailable);
	FOnClientServerDebugAvailable OnClientServerDebugAvailable;

	/** Global place to accumulate debug strings for ability system component. Used when we fill up client side debug string immediately, and then wait for server to send server strings */
	TArray<FString>	AbilitySystemDebugStrings;


	// Helper functions for applying global scaling to various ability system tasks. This isn't meant to be a shipping feature, but to help with debugging and interation via cvar AbilitySystem.GlobalAbilityScale.
	static void NonShipping_ApplyGlobalAbilityScaler_Rate(float& Rate);
	static void NonShipping_ApplyGlobalAbilityScaler_Duration(float& Duration);

	// Global Tags

	UPROPERTY()
	FGameplayTag ActivateFailCooldownTag; // TryActivate failed due to being on cooldown
	UPROPERTY(config)
	FName ActivateFailCooldownName;

	UPROPERTY()
	FGameplayTag ActivateFailCostTag; // TryActivate failed due to not being able to spend costs
	UPROPERTY(config)
	FName ActivateFailCostName;

	UPROPERTY()
	FGameplayTag ActivateFailTagsBlockedTag; // TryActivate failed due to being blocked by other abilities
	UPROPERTY(config)
	FName ActivateFailTagsBlockedName;

	UPROPERTY()
	FGameplayTag ActivateFailTagsMissingTag; // TryActivate failed due to missing required tags
	UPROPERTY(config)
	FName ActivateFailTagsMissingName;

	UPROPERTY()
	FGameplayTag ActivateFailNetworkingTag; // Failed to activate due to invalid networking settings, this is designer error
	UPROPERTY(config)
	FName ActivateFailNetworkingName;

	/** How many bits to use for "number of tags" in FMinimapReplicationTagCountMap::NetSerialize.  */
	UPROPERTY(config)
	int32	MinimalReplicationTagCountBits;

	virtual void InitGlobalTags()
	{
		if (ActivateFailCooldownName != NAME_None)
		{
			ActivateFailCooldownTag = FGameplayTag::RequestGameplayTag(ActivateFailCooldownName);
		}

		if (ActivateFailCostName != NAME_None)
		{
			ActivateFailCostTag = FGameplayTag::RequestGameplayTag(ActivateFailCostName);
		}

		if (ActivateFailTagsBlockedName != NAME_None)
		{
			ActivateFailTagsBlockedTag = FGameplayTag::RequestGameplayTag(ActivateFailTagsBlockedName);
		}

		if (ActivateFailTagsMissingName != NAME_None)
		{
			ActivateFailTagsMissingTag = FGameplayTag::RequestGameplayTag(ActivateFailTagsMissingName);
		}

		if (ActivateFailNetworkingName != NAME_None)
		{
			ActivateFailNetworkingTag = FGameplayTag::RequestGameplayTag(ActivateFailNetworkingName);
		}
	}

	// GameplayCue Parameters
	virtual void InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectSpecForRPC &Spec);
	virtual void InitGameplayCueParameters_GESpec(FGameplayCueParameters& CueParameters, const FGameplayEffectSpec &Spec);
	virtual void InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectContextHandle& EffectContext);

	// Trigger async loading of the gameplay cue object libraries. By default, the manager will do this on creation,
	// but that behavior can be changed by a derived class overriding ShouldAsyncLoadObjectLibrariesAtStart and returning false.
	// In that case, this function must be called to begin the load
	virtual void StartAsyncLoadingObjectLibraries();

	/** Simple accessor to whether gameplay modifier evaluation channels should be allowed or not */
	bool ShouldAllowGameplayModEvaluationChannels() const;

	/**
	 * Returns whether the specified gameplay mod evaluation channel is valid for use or not.
	 * Considers whether channel usage is allowed at all, as well as if the specified channel has a valid alias for the game.
	 */
	bool IsGameplayModEvaluationChannelValid(EGameplayModEvaluationChannel Channel) const;

	/** Simple channel-based accessor to the alias name for the specified gameplay mod evaluation channel, if any */
	const FName& GetGameplayModEvaluationChannelAlias(EGameplayModEvaluationChannel Channel) const;

	/** Simple index-based accessor to the alias name for the specified gameplay mod evaluation channel, if any */
	const FName& GetGameplayModEvaluationChannelAlias(int32 Index) const;

	virtual TArray<FString> GetGameplayCueNotifyPaths() { return GameplayCueNotifyPaths; }

protected:

	virtual void InitAttributeDefaults();
	virtual void ReloadAttributeDefaults();
	virtual void AllocAttributeSetInitter();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// data used for ability system cheat commands

	/** If we should ignore the cooldowns when activating abilities in the ability system. Set with ToggleIgnoreAbilitySystemCooldowns() */
	bool bIgnoreAbilitySystemCooldowns;

	/** If we should ignore the costs when activating abilities in the ability system. Set with ToggleIgnoreAbilitySystemCosts() */
	bool bIgnoreAbilitySystemCosts;
#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Whether the game should allow the usage of gameplay mod evaluation channels or not */
	UPROPERTY(config)
	bool bAllowGameplayModEvaluationChannels;

	/** The default mod evaluation channel for the game */
	UPROPERTY(config)
	EGameplayModEvaluationChannel DefaultGameplayModEvaluationChannel;

	/** Game-specified named aliases for gameplay mod evaluation channels; Only those with valid aliases are eligible to be used in a game (except Channel0, which is always valid) */
	UPROPERTY(config)
	FName GameplayModEvaluationChannelAliases[static_cast<int32>(EGameplayModEvaluationChannel::Channel_MAX)];

	/** Name of global curve table to use as the default for scalable floats, etc. */
	UPROPERTY(config)
	FSoftObjectPath GlobalCurveTableName;

	/** Holds information about the valid attributes' min and max values and stacking rules */
	UPROPERTY(config)
	FSoftObjectPath GlobalAttributeMetaDataTableName;

	/** Holds default values for attribute sets, keyed off of Name/Levels. NOTE: Preserved for backwards compatibility, should use the array version below now */
	UPROPERTY(config)
	FSoftObjectPath GlobalAttributeSetDefaultsTableName;

	/** Array of curve table names to use for default values for attribute sets, keyed off of Name/Levels */
	UPROPERTY(config)
	TArray<FSoftObjectPath> GlobalAttributeSetDefaultsTableNames;

	/** Class reference to gameplay cue manager. Use this if you want to just instantiate a class for your gameplay cue manager without having to create an asset. */
	UPROPERTY(config)
	FSoftObjectPath GlobalGameplayCueManagerClass;

	/** Object reference to gameplay cue manager (E.g., reference to a specific blueprint of your GameplayCueManager class. This is not necessary unless you want to have data or blueprints in your gameplay cue manager. */
	UPROPERTY(config)
	FSoftObjectPath GlobalGameplayCueManagerName;

	/** Look in these paths for GameplayCueNotifies. These are your "always loaded" set. */
	UPROPERTY(config)
	TArray<FString>	GameplayCueNotifyPaths;

	/** The class to instantiate as the GameplayTagResponseTable. */
	UPROPERTY(config)
	FSoftObjectPath GameplayTagResponseTableName;

	UPROPERTY()
	UGameplayTagReponseTable* GameplayTagResponseTable;

	/** Set to true if you want clients to try to predict gameplay effects done to targets. If false it will only predict self effects */
	UPROPERTY(config)
	bool PredictTargetGameplayEffects;

	UPROPERTY()
	UCurveTable* GlobalCurveTable;

	/** Curve tables containing default values for attribute sets, keyed off of Name/Levels */
	UPROPERTY()
	TArray<UCurveTable*> GlobalAttributeDefaultsTables;

	UPROPERTY()
	UDataTable* GlobalAttributeMetaDataTable;

	UPROPERTY()
	UGameplayCueManager* GlobalGameplayCueManager;

	TSharedPtr<FAttributeSetInitter> GlobalAttributeSetInitter;

	template <class T>
	T* InternalGetLoadTable(T*& Table, FString TableName);

#if WITH_EDITOR
	void OnTableReimported(UObject* InObject);

	void OnPreBeginPIE(const bool bIsSimulatingInEditor);
#endif

	void ResetCachedData();
	void HandlePreLoadMap(const FString& MapName);

#if WITH_EDITORONLY_DATA
	bool RegisteredReimportCallback;
#endif

public:
	//To add functionality for opening assets directly from the game.
	void Notify_OpenAssetInEditor(FString AssetName, int AssetType);
	FOnAbilitySystemAssetOpenedDelegate AbilityOpenAssetInEditorCallbacks;

	//...for finding assets directly from the game.
	void Notify_FindAssetInEditor(FString AssetName, int AssetType);
	FOnAbilitySystemAssetFoundDelegate AbilityFindAssetInEditorCallbacks;
};


struct FScopeCurrentGameplayEffectBeingApplied
{
	FScopeCurrentGameplayEffectBeingApplied(const FGameplayEffectSpec* Spec, UAbilitySystemComponent* AbilitySystemComponent)
	{
		UAbilitySystemGlobals::Get().PushCurrentAppliedGE(Spec, AbilitySystemComponent);
	}
	~FScopeCurrentGameplayEffectBeingApplied()
	{
		UAbilitySystemGlobals::Get().PopCurrentAppliedGE();
	}
};