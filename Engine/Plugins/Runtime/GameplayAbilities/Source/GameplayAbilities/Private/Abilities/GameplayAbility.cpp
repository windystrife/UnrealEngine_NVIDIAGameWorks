// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbility.h"
#include "TimerManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/NetDriver.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayCue_Types.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayAbility
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

UGameplayAbility::UGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	auto ImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& (Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass()) || Func->GetOuter()->IsA(UDynamicClass::StaticClass()));
	};

	{
		static FName FuncName = FName(TEXT("K2_ShouldAbilityRespondToEvent"));
		UFunction* ShouldRespondFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintShouldAbilityRespondToEvent = ImplementedInBlueprint(ShouldRespondFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_CanActivateAbility"));
		UFunction* CanActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintCanUse = ImplementedInBlueprint(CanActivateFunction);
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbility"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		// FIXME: temp to work around crash
		if (ActivateFunction && (HasAnyFlags(RF_ClassDefaultObject) || ActivateFunction->IsValidLowLevelFast()))
		{
			bHasBlueprintActivate = ImplementedInBlueprint(ActivateFunction);
		}
	}
	{
		static FName FuncName = FName(TEXT("K2_ActivateAbilityFromEvent"));
		UFunction* ActivateFunction = GetClass()->FindFunctionByName(FuncName);
		bHasBlueprintActivateFromEvent = ImplementedInBlueprint(ActivateFunction);
	}
	
#if WITH_EDITOR
	/** Autoregister abilities with the blueprint debugger in the editor.*/
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UBlueprint* BP = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (BP && (BP->GetWorldBeingDebugged() == nullptr || BP->GetWorldBeingDebugged() == GetWorld()))
		{
			BP->SetObjectBeingDebugged(this);
		}
	}
#endif

	bServerRespectsRemoteAbilityCancellation = true;
	bReplicateInputDirectly = false;
	RemoteInstanceEnded = false;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;

	ScopeLockCount = 0;
}

int32 UGameplayAbility::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}
	check(GetOuter() != NULL);
	return GetOuter()->GetFunctionCallspace(Function, Parameters, Stack);
}

bool UGameplayAbility::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));
	check(GetOuter() != NULL);

	AActor* Owner = CastChecked<AActor>(GetOuter());
	UNetDriver* NetDriver = Owner->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Owner, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}

// TODO: polymorphic payload
void UGameplayAbility::SendGameplayEvent(FGameplayTag EventTag, FGameplayEventData Payload)
{
	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	if (ensure(AbilitySystemComponent))
	{
		FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
		AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
	}
}

void UGameplayAbility::PostNetInit()
{
	/** We were dynamically spawned from replication - we need to init a currentactorinfo by looking at outer.
	 *  This may need to be updated further if we start having abilities live on different outers than player AbilitySystemComponents.
	 */
	
	if (CurrentActorInfo == NULL)
	{
		AActor* OwnerActor = Cast<AActor>(GetOuter());
		if (ensure(OwnerActor))
		{
			UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerActor);
			if (ensure(AbilitySystemComponent))
			{
				CurrentActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
			}
		}
	}
}

bool UGameplayAbility::IsActive() const
{
	// Only Instanced-Per-Actor abilities persist between activations
	if (GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		return bIsActive;
	}

	// this should not be called on NonInstanced warn about it, Should call IsActive on the ability spec instead
	if (GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayAbility::IsActive() called on %s NonInstanced ability, call IsActive on the Ability Spec instead"), *GetName());
	}

	// NonInstanced and Instanced-Per-Execution abilities are by definition active unless they are pending kill
	return !IsPendingKill();
}

bool UGameplayAbility::IsSupportedForNetworking() const
{
	/**
	 *	We can only replicate references to:
	 *		-CDOs and DataAssets (e.g., static, non-instanced gameplay abilities)
	 *		-Instanced abilities that are replicating (and will thus be created on clients).
	 *		
	 *	Otherwise it is not supported, and it will be recreated on the client
	 */

	bool Supported = GetReplicationPolicy() != EGameplayAbilityReplicationPolicy::ReplicateNo || GetOuter()->IsA(UPackage::StaticClass());

	return Supported;
}

bool UGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	bool bBlocked = false;
	bool bMissing = false;

	const FGameplayTag& BlockedTag = UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag;
	const FGameplayTag& MissingTag = UAbilitySystemGlobals::Get().ActivateFailTagsMissingTag;

	// Check if any of this ability's tags are currently blocked
	if (AbilitySystemComponent.AreAbilityTagsBlocked(AbilityTags))
	{
		bBlocked = true;
	}

	// Check to see the required/blocked tags for this ability
	if (ActivationBlockedTags.Num() || ActivationRequiredTags.Num())
	{
		static FGameplayTagContainer AbilitySystemComponentTags;
		AbilitySystemComponentTags.Reset();

		AbilitySystemComponent.GetOwnedGameplayTags(AbilitySystemComponentTags);

		if (AbilitySystemComponentTags.HasAny(ActivationBlockedTags))
		{
			bBlocked = true;
		}

		if (!AbilitySystemComponentTags.HasAll(ActivationRequiredTags))
		{
			bMissing = true;
		}
	}

	if (SourceTags != nullptr)
	{
		if (SourceBlockedTags.Num() || SourceRequiredTags.Num())
		{
			if (SourceTags->HasAny(SourceBlockedTags))
			{
				bBlocked = true;
			}

			if (!SourceTags->HasAll(SourceRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (TargetTags != nullptr)
	{
		if (TargetBlockedTags.Num() || TargetRequiredTags.Num())
		{
			if (TargetTags->HasAny(TargetBlockedTags))
			{
				bBlocked = true;
			}

			if (!TargetTags->HasAll(TargetRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (bBlocked)
	{
		if (OptionalRelevantTags && BlockedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(BlockedTag);
		}
		return false;
	}
	if (bMissing)
	{
		if (OptionalRelevantTags && MissingTag.IsValid())
		{
			OptionalRelevantTags->AddTag(MissingTag);
		}
		return false;
	}
	
	return true;
}

bool UGameplayAbility::ShouldActivateAbility(ENetRole Role) const
{
	return Role != ROLE_SimulatedProxy;
}

void UGameplayAbility::K2_CancelAbility()
{
	check(CurrentActorInfo);

	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

bool UGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// Don't set the actor info, CanActivate is called on the CDO

	// A valid AvatarActor is required. Simulated proxy check means only authority or autonomous proxies should be executing abilities.
	if (ActorInfo == nullptr || ActorInfo->AvatarActor == nullptr || !ShouldActivateAbility(ActorInfo->AvatarActor->Role))
	{
		return false;
	}

	//make into a reference for simplicity
	static FGameplayTagContainer DummyContainer;
	DummyContainer.Reset();

	FGameplayTagContainer& OutTags = OptionalRelevantTags ? *OptionalRelevantTags : DummyContainer;

	// make sure the ActorInfo and its ability system component are valid, if not bail out.
	if (!ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	if (ActorInfo->AbilitySystemComponent->GetUserAbilityActivationInhibited())
	{
		/**
		 *	Input is inhibited (UI is pulled up, another ability may be blocking all other input, etc).
		 *	When we get into triggered abilities, we may need to better differentiate between CanActviate and CanUserActivate or something.
		 *	E.g., we would want LMB/RMB to be inhibited while the user is in the menu UI, but we wouldn't want to prevent a 'buff when I am low health'
		 *	ability to not trigger.
		 *	
		 *	Basically: CanActivateAbility is only used by user activated abilities now. If triggered abilities need to check costs/cooldowns, then we may
		 *	want to split this function up and change the calling API to distinguish between 'can I initiate an ability activation' and 'can this ability be activated'.
		 */ 
		return false;
	}
	
	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	if (!DoesAbilitySatisfyTagRequirements(*ActorInfo->AbilitySystemComponent.Get(), SourceTags, TargetTags, OptionalRelevantTags))
	{	// If the ability's tags are blocked, or if it has a "Blocking" tag or is missing a "Required" tag, then it can't activate.
		return false;
	}

	FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("CanActivateAbility called with invalid Handle"));
		return false;
	}

	// Check if this ability's input binding is currently blocked
	if (ActorInfo->AbilitySystemComponent->IsAbilityInputBlocked(Spec->InputID))
	{
		return false;
	}

	if (bHasBlueprintCanUse)
	{
		if (K2_CanActivateAbility(*ActorInfo, OutTags) == false)
		{
			ABILITY_LOG(Log, TEXT("CanActivateAbility %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UGameplayAbility::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (bHasBlueprintShouldAbilityRespondToEvent)
	{
		if (K2_ShouldAbilityRespondToEvent(*ActorInfo, *Payload) == false)
		{
			ABILITY_LOG(Log, TEXT("ShouldAbilityRespondToEvent %s failed, blueprint refused"), *GetName());
			return false;
		}
	}

	return true;
}

bool UGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CommitCheck(Handle, ActorInfo, ActivationInfo))
	{
		return false;
	}

	CommitExecute(Handle, ActorInfo, ActivationInfo);

	// Fixme: Should we always call this or only if it is implemented? A noop may not hurt but could be bad for perf (storing a HasBlueprintCommit per instance isn't good either)
	K2_CommitExecute();

	// Broadcast this commitment
	ActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);

	return true;
}

bool UGameplayAbility::CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown)
{
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
	{
		return true;
	}

	if (!ForceCooldown)
	{
		// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
		if (!CheckCooldown(Handle, ActorInfo))
		{
			return false;
		}
	}

	ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UGameplayAbility::CommitAbilityCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCosts())
	{
		return true;
	}

	// Last chance to fail (maybe we no longer have resources to commit since we after we started this ability activation)
	if (!CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	ApplyCost(Handle, ActorInfo, ActivationInfo);
	return true;
}

bool UGameplayAbility::CommitCheck(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	/**
	 *	Checks if we can (still) commit this ability. There are some subtleties here.
	 *		-An ability can start activating, play an animation, wait for a user confirmation/target data, and then actually commit
	 *		-Commit = spend resources/cooldowns. Its possible the source has changed state since he started activation, so a commit may fail.
	 *		-We don't want to just call CanActivateAbility() since right now that also checks things like input inhibition.
	 *			-E.g., its possible the act of starting your ability makes it no longer activatable (CanaCtivateAbility() may be false if called here).
	 */

	const bool bValidHandle = Handle.IsValid();
	const bool bValidActorInfoPieces = (ActorInfo && (ActorInfo->AbilitySystemComponent != nullptr));
	const bool bValidSpecFound = bValidActorInfoPieces && (ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) != nullptr);

	// Ensure that the ability spec is even valid before trying to process the commit
	if (!bValidHandle || !bValidActorInfoPieces || !bValidSpecFound)
	{
		ensureMsgf(false, TEXT("UGameplayAbility::CommitCheck provided an invalid handle or actor info or couldn't find ability spec: %s Handle Valid: %d ActorInfo Valid: %d Spec Not Found: %d"), *GetName(), bValidHandle, bValidActorInfoPieces, bValidSpecFound);
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() && !CheckCooldown(Handle, ActorInfo))
	{
		return false;
	}

	if (!UAbilitySystemGlobals::Get().ShouldIgnoreCosts() && !CheckCost(Handle, ActorInfo))
	{
		return false;
	}

	return true;
}

void UGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	ApplyCooldown(Handle, ActorInfo, ActivationInfo);

	ApplyCost(Handle, ActorInfo, ActivationInfo);
}

bool UGameplayAbility::CanBeCanceled() const
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return bIsCancelable;
	}

	// Non instanced are always cancelable
	return true;
}

void UGameplayAbility::SetCanBeCanceled(bool bCanBeCanceled)
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced && bCanBeCanceled != bIsCancelable)
	{
		bIsCancelable = bCanBeCanceled;

		UAbilitySystemComponent* Comp = CurrentActorInfo->AbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, bCanBeCanceled);
		}
	}
}

bool UGameplayAbility::IsBlockingOtherAbilities() const
{
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return bIsBlockingOtherAbilities;
	}

	// Non instanced are always marked as blocking other abilities
	return true;
}

void UGameplayAbility::SetShouldBlockOtherAbilities(bool bShouldBlockAbilities)
{
	if (bIsActive && GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced && bShouldBlockAbilities != bIsBlockingOtherAbilities)
	{
		bIsBlockingOtherAbilities = bShouldBlockAbilities;

		UAbilitySystemComponent* Comp = CurrentActorInfo->AbilitySystemComponent.Get();
		if (Comp)
		{
			Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, bIsBlockingOtherAbilities, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
		}
	}
}

void UGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	if (CanBeCanceled())
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UGameplayAbility::CancelAbility, Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility));
			return;
		}

		// Replicate the the server/client if needed
		if (bReplicateCancelAbility)
		{
			ActorInfo->AbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, true);
		}

		// Gives the Ability BP a chance to perform custom logic/cleanup when any active ability states are active
		if (OnGameplayAbilityCancelled.IsBound())
		{
			OnGameplayAbilityCancelled.Broadcast();
		}

		// End the ability but don't replicate it, we replicate the CancelAbility call directly
		bool bReplicateEndAbility = false;
		bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

bool UGameplayAbility::IsEndAbilityValid(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	// Protect against EndAbility being called multiple times
	// Ending an AbilityState may cause this to be invoked again
	if (bIsActive == false && GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		return false;
	}

	// check if ability has valid owner
	UAbilitySystemComponent* AbilityComp = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (AbilityComp == nullptr)
	{
		return false;
	}

	// check to see if this is an NonInstanced or if the ability is active.
	const FGameplayAbilitySpec* Spec = AbilityComp->FindAbilitySpecFromHandle(Handle);
	const bool bIsSpecActive = Spec ? Spec->IsActive() : IsActive();

	if (!bIsSpecActive)
	{
		return false;
	}

	return true;
}

void UGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UGameplayAbility::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}

		// Give blueprint a chance to react
		K2_OnEndAbility();

		// Protect against blueprint causing us to EndAbility already
		if (bIsActive == false && GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			return;
		}

		// Stop any timers or latent actions for the ability
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}

		// Execute our delegate and unbind it, as we are no longer active and listeners can re-register when we become active again.
		OnGameplayAbilityEnded.Broadcast(this);
		OnGameplayAbilityEnded.Clear();

		OnGameplayAbilityEndedWithData.Broadcast(FAbilityEndedData(this, Handle, bReplicateEndAbility, bWasCancelled));
		OnGameplayAbilityEndedWithData.Clear();

		if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			bIsActive = false;
		}

		// Tell all our tasks that we are finished and they should cleanup
		for (int32 TaskIdx = ActiveTasks.Num() - 1; TaskIdx >= 0 && ActiveTasks.Num() > 0; --TaskIdx)
		{
			UGameplayTask* Task = ActiveTasks[TaskIdx];
			if (Task)
			{
				Task->TaskOwnerEnded();
			}
		}
		ActiveTasks.Reset();	// Empty the array but dont resize memory, since this object is probably going to be destroyed very soon anyways.

		// TODO: is this condition still required? validity of AbilitySystemComponent is checked by IsEndAbilityValid()
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			if (bReplicateEndAbility)
			{
				ActorInfo->AbilitySystemComponent->ReplicateEndOrCancelAbility(Handle, ActivationInfo, this, false);
			}

			// Remove tags we added to owner
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTags(ActivationOwnedTags);

			// Remove tracked GameplayCues that we added
			for (FGameplayTag& GameplayCueTag : TrackedGameplayCues)
			{
				ActorInfo->AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
			}
			TrackedGameplayCues.Empty();

			if (CanBeCanceled())
			{
				// If we're still cancelable, cancel it now
				ActorInfo->AbilitySystemComponent->HandleChangeAbilityCanBeCanceled(AbilityTags, this, false);
			}
			
			if (IsBlockingOtherAbilities())
			{
				// If we're still blocking other abilities, cancel now
				ActorInfo->AbilitySystemComponent->ApplyAbilityBlockAndCancelTags(AbilityTags, this, false, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
			}

			// Tell owning AbilitySystemComponent that we ended so it can do stuff (including MarkPendingKill us)
			ActorInfo->AbilitySystemComponent->NotifyAbilityEnded(Handle, this, bWasCancelled);
		}
	}
}

void UGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (bHasBlueprintActivate)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbility();
	}
	else if (bHasBlueprintActivateFromEvent)
	{
		if (TriggerEventData)
		{
			// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
			K2_ActivateAbilityFromEvent(*TriggerEventData);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("Ability %s expects event data but none is being supplied. Use Activate Ability instead of Activate Ability From Event."), *GetName());
			bool bReplicateEndAbility = false;
			bool bWasCancelled = true;
			EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		}
	}
	else
	{
		// Native child classes may want to override ActivateAbility and do something like this:

		// Do stuff...

		if (CommitAbility(Handle, ActorInfo, ActivationInfo))		// ..then commit the ability...
		{			
			//	Then do more stuff...
		}
	}
}

void UGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate)
{
	UAbilitySystemComponent* Comp = ActorInfo->AbilitySystemComponent.Get();

	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		bIsActive = true;
		bIsBlockingOtherAbilities = true;
		bIsCancelable = true;
	}

	RemoteInstanceEnded = false;

	Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, true);
	Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, true, BlockAbilitiesWithTag, true, CancelAbilitiesWithTag);
	Comp->AddLooseGameplayTags(ActivationOwnedTags);

	if (OnGameplayAbilityEndedDelegate)
	{
		OnGameplayAbilityEnded.Add(*OnGameplayAbilityEndedDelegate);
	}

	SetCurrentInfo(Handle, ActorInfo, ActivationInfo);

	Comp->NotifyAbilityActivated(Handle, this);
}

void UGameplayAbility::CallActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);
	ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UGameplayAbility::ConfirmActivateSucceed()
{
	// On instanced abilities, update CurrentActivationInfo and call any registered delegates.
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		PostNetInit();
		check(CurrentActorInfo);
		CurrentActivationInfo.SetActivationConfirmed();

		OnConfirmDelegate.Broadcast(this);
		OnConfirmDelegate.Clear();
	}
}

UGameplayEffect* UGameplayAbility::GetCooldownGameplayEffect() const
{
	if ( CooldownGameplayEffectClass )
	{
		return CooldownGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	else
	{
		return nullptr;
	}
}

UGameplayEffect* UGameplayAbility::GetCostGameplayEffect() const
{
	if ( CostGameplayEffectClass )
	{
		return CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	else
	{
		return nullptr;
	}
}

bool UGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags)
	{
		check(ActorInfo->AbilitySystemComponent.IsValid());
		if (CooldownTags->Num() > 0 && ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(*CooldownTags))
		{
			const FGameplayTag& CooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;

			if (OptionalRelevantTags && CooldownTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CooldownTag);
			}

			return false;
		}
	}
	return true;
}

void UGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

bool UGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		check(ActorInfo->AbilitySystemComponent.IsValid());
		if (!ActorInfo->AbilitySystemComponent->CanApplyAttributeModifiers(CostGE, GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo)))
		{
			const FGameplayTag& CostTag = UAbilitySystemGlobals::Get().ActivateFailCostTag;

			if (OptionalRelevantTags && CostTag.IsValid())
			{
				OptionalRelevantTags->AddTag(CostTag);
			}
			return false;
		}
	}
	return true;
}

void UGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CostGE = GetCostGameplayEffect();
	if (CostGE)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CostGE, GetAbilityLevel(Handle, ActorInfo));
	}
}

void UGameplayAbility::SetMovementSyncPoint(FName SyncName)
{
}

float UGameplayAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayAbilityGetCooldownTimeRemaining);

	if (ActorInfo->AbilitySystemComponent.IsValid())
	{
		const FGameplayTagContainer* CooldownTags = GetCooldownTags();
		if (CooldownTags && CooldownTags->Num() > 0)
		{
			FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
			TArray< float > Durations = ActorInfo->AbilitySystemComponent->GetActiveEffectsTimeRemaining(Query);
			if (Durations.Num() > 0)
			{
				Durations.Sort();
				return Durations[Durations.Num() - 1];
			}
		}
	}

	return 0.f;
}

void UGameplayAbility::InvalidateClientPredictionKey() const
{
	if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		CurrentActorInfo->AbilitySystemComponent->ScopedPredictionKey = FPredictionKey();
	}
}

void UGameplayAbility::GetCooldownTimeRemainingAndDuration(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayAbilityGetCooldownTimeRemainingAndDuration);

	check(ActorInfo->AbilitySystemComponent.IsValid());

	TimeRemaining = 0.f;
	CooldownDuration = 0.f;
	
	const FGameplayTagContainer* CooldownTags = GetCooldownTags();
	if (CooldownTags && CooldownTags->Num() > 0)
	{
		FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
		TArray< TPair<float,float> > DurationAndTimeRemaining = ActorInfo->AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);
		if (DurationAndTimeRemaining.Num() > 0)
		{
			int32 BestIdx = 0;
			float LongestTime = DurationAndTimeRemaining[0].Key;
			for (int32 Idx = 1; Idx < DurationAndTimeRemaining.Num(); ++Idx)
			{
				if (DurationAndTimeRemaining[Idx].Key > LongestTime)
				{
					LongestTime = DurationAndTimeRemaining[Idx].Key;
					BestIdx = Idx;
				}
			}

			TimeRemaining = DurationAndTimeRemaining[BestIdx].Key;
			CooldownDuration = DurationAndTimeRemaining[BestIdx].Value;
		}
	}
}

const FGameplayTagContainer* UGameplayAbility::GetCooldownTags() const
{
	UGameplayEffect* CDGE = GetCooldownGameplayEffect();
	return CDGE ? &CDGE->InheritableOwnedTagsContainer.CombinedTags : nullptr;
}

FGameplayAbilityActorInfo UGameplayAbility::GetActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return FGameplayAbilityActorInfo();
	}
	return *CurrentActorInfo;
}

AActor* UGameplayAbility::GetOwningActorFromActorInfo() const
{
	if (!ensureMsgf(IsInstantiated(), TEXT("%s: GetOwningActorFromActorInfo can not be called on a non-instanced ability"), *GetName()))
	{
		ABILITY_LOG(Warning, TEXT("%s: GetOwningActorFromActorInfo can not be called on a non-instanced ability"), *GetName());
		return nullptr;
	}

	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->OwnerActor.Get();
}

AActor* UGameplayAbility::GetAvatarActorFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}
	return CurrentActorInfo->AvatarActor.Get();
}

USkeletalMeshComponent* UGameplayAbility::GetOwningComponentFromActorInfo() const
{
	if (!ensure(CurrentActorInfo))
	{
		return nullptr;
	}

	return CurrentActorInfo->SkeletalMeshComponent.Get();
}

FGameplayEffectSpecHandle UGameplayAbility::MakeOutgoingGameplayEffectSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid());
	return MakeOutgoingGameplayEffectSpec(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, GameplayEffectClass, Level);
}

int32 AbilitySystemShowMakeOutgoingGameplayEffectSpecs = 0;
static FAutoConsoleVariableRef CVarAbilitySystemShowMakeOutgoingGameplayEffectSpecs(TEXT("AbilitySystem.ShowClientMakeOutgoingSpecs"), AbilitySystemShowMakeOutgoingGameplayEffectSpecs, TEXT("Displays all GameplayEffect specs created on non authority clients"), ECVF_Default );

FGameplayEffectSpecHandle UGameplayAbility::MakeOutgoingGameplayEffectSpec(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(ActorInfo);

	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (AbilitySystemShowMakeOutgoingGameplayEffectSpecs && HasAuthority(&ActivationInfo) == false)
	{
		ABILITY_LOG(Warning, TEXT("%s, MakeOutgoingGameplayEffectSpec: %s"), *ActorInfo->AbilitySystemComponent->GetFullName(),  *GameplayEffectClass->GetName()); 
	}
#endif

	FGameplayEffectSpecHandle NewHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(GameplayEffectClass, Level, MakeEffectContext(Handle, ActorInfo));
	if (NewHandle.IsValid())
	{
		FGameplayAbilitySpec* AbilitySpec =  ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		ApplyAbilityTagsToGameplayEffectSpec(*NewHandle.Data.Get(), AbilitySpec);
	}
	return NewHandle;
}

void UGameplayAbility::ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const
{
	Spec.CapturedSourceTags.GetSpecTags().AppendTags(AbilityTags);

	// Allow the source object of the ability to propagate tags along as well
	if (AbilitySpec)
	{
		const IGameplayTagAssetInterface* SourceObjAsTagInterface = Cast<IGameplayTagAssetInterface>(AbilitySpec->SourceObject);
		if (SourceObjAsTagInterface)
		{
			FGameplayTagContainer SourceObjTags;
			SourceObjAsTagInterface->GetOwnedGameplayTags(SourceObjTags);

			Spec.CapturedSourceTags.GetSpecTags().AppendTags(SourceObjTags);
		}
	}
}

/** Fixme: Naming is confusing here */

bool UGameplayAbility::K2_CommitAbility()
{
	check(CurrentActorInfo);
	return CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UGameplayAbility::K2_CommitAbilityCooldown(bool BroadcastCommitEvent, bool ForceCooldown)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ForceCooldown);
}

bool UGameplayAbility::K2_CommitAbilityCost(bool BroadcastCommitEvent)
{
	check(CurrentActorInfo);
	if (BroadcastCommitEvent)
	{
		CurrentActorInfo->AbilitySystemComponent->NotifyAbilityCommit(this);
	}
	return CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
}

bool UGameplayAbility::K2_CheckAbilityCooldown()
{
	check(CurrentActorInfo);
	return UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns() || CheckCooldown(CurrentSpecHandle, CurrentActorInfo);
}

bool UGameplayAbility::K2_CheckAbilityCost()
{
	check(CurrentActorInfo);
	return UAbilitySystemGlobals::Get().ShouldIgnoreCosts() || CheckCost(CurrentSpecHandle, CurrentActorInfo);
}

void UGameplayAbility::K2_EndAbility()
{
	check(CurrentActorInfo);

	bool bReplicateEndAbility = true;
	bool bWasCancelled = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --------------------------------------------------------------------

void UGameplayAbility::MontageJumpToSection(FName SectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->AbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->AbilitySystemComponent->CurrentMontageJumpToSection(SectionName);
	}
}

void UGameplayAbility::MontageSetNextSectionName(FName FromSectionName, FName ToSectionName)
{
	check(CurrentActorInfo);

	if (CurrentActorInfo->AbilitySystemComponent->IsAnimatingAbility(this))
	{
		CurrentActorInfo->AbilitySystemComponent->CurrentMontageSetNextSectionName(FromSectionName, ToSectionName);
	}
}

void UGameplayAbility::MontageStop(float OverrideBlendOutTime)
{
	check(CurrentActorInfo);

	UAbilitySystemComponent* AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	if (AbilitySystemComponent != NULL)
	{
		// We should only stop the current montage if we are the animating ability
		if (AbilitySystemComponent->IsAnimatingAbility(this))
		{
			AbilitySystemComponent->CurrentMontageStop();
		}
	}
}

void UGameplayAbility::SetCurrentMontage(class UAnimMontage* InCurrentMontage)
{
	ensure(IsInstantiated());
	CurrentMontage = InCurrentMontage;
}

UAnimMontage* UGameplayAbility::GetCurrentMontage() const
{
	return CurrentMontage;
}

// --------------------------------------------------------------------

FGameplayAbilityTargetingLocationInfo UGameplayAbility::MakeTargetLocationInfoFromOwnerActor()
{
	FGameplayAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
	ReturnLocation.SourceActor = GetActorInfo().AvatarActor.Get();
	ReturnLocation.SourceAbility = this;
	return ReturnLocation;
}

FGameplayAbilityTargetingLocationInfo UGameplayAbility::MakeTargetLocationInfoFromOwnerSkeletalMeshComponent(FName SocketName)
{
	FGameplayAbilityTargetingLocationInfo ReturnLocation;
	ReturnLocation.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;
	ReturnLocation.SourceComponent = GetActorInfo().SkeletalMeshComponent.Get();
	ReturnLocation.SourceAbility = this;
	ReturnLocation.SourceSocketName = SocketName;
	return ReturnLocation;
}

//----------------------------------------------------------------------

UGameplayTasksComponent* UGameplayAbility::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	return GetCurrentActorInfo() ? GetCurrentActorInfo()->AbilitySystemComponent.Get() : nullptr;
}

AActor* UGameplayAbility::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Info->OwnerActor.Get() : nullptr;
}

AActor* UGameplayAbility::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	return Info ? Info->AvatarActor.Get() : nullptr;
}

void UGameplayAbility::OnGameplayTaskInitialized(UGameplayTask& Task)
{
	UAbilityTask* AbilityTask = Cast<UAbilityTask>(&Task);
	if (AbilityTask)
	{
		AbilityTask->SetAbilitySystemComponent(GetCurrentActorInfo()->AbilitySystemComponent.Get());
		AbilityTask->Ability = this;
	}
}

void UGameplayAbility::OnGameplayTaskActivated(UGameplayTask& Task)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Started %s"), *Task.GetName());

	ActiveTasks.Add(&Task);
}

void UGameplayAbility::OnGameplayTaskDeactivated(UGameplayTask& Task)
{
	ABILITY_VLOG(CastChecked<AActor>(GetOuter()), Log, TEXT("Task Ended %s"), *Task.GetName());

	ActiveTasks.Remove(&Task);

	if (ENABLE_ABILITYTASK_DEBUGMSG)
	{
		AddAbilityTaskDebugMessage(&Task, TEXT("Ended."));
	}
}

void UGameplayAbility::ConfirmTaskByInstanceName(FName InstanceName, bool bEndTask)
{
	TArray<UGameplayTask*, TInlineAllocator<8> > NamedTasks;

	for (UGameplayTask* Task : ActiveTasks)
	{
		if (Task && Task->GetInstanceName() == InstanceName)
		{
			NamedTasks.Add(Task);
		}
	}
	
	for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
	{
		UGameplayTask* CurrentTask = NamedTasks[i];
		if (CurrentTask && CurrentTask->IsPendingKill() == false)
		{
			CurrentTask->ExternalConfirm(bEndTask);
		}
	}
}

void UGameplayAbility::EndOrCancelTasksByInstanceName()
{
	// Static array for avoiding memory allocations
	TArray<UGameplayTask*, TInlineAllocator<8> > NamedTasks;

	// Call Endtask on everything in EndTaskInstanceNames list
	for (int32 j = 0; j < EndTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = EndTaskInstanceNames[j];
		NamedTasks.Reset();

		// Find every current task that needs to end before ending any
		for (UGameplayTask* Task : ActiveTasks)
		{
			if (Task && Task->GetInstanceName() == InstanceName)
			{
				NamedTasks.Add(Task);
			}
		}
		
		// End each one individually. Not ending a task may do "anything" including killing other tasks or the ability itself
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			UGameplayTask* CurrentTask = NamedTasks[i];
			if (CurrentTask && CurrentTask->IsPendingKill() == false)
			{
				CurrentTask->EndTask();
			}
		}
	}
	EndTaskInstanceNames.Empty();


	// Call ExternalCancel on everything in CancelTaskInstanceNames list
	for (int32 j = 0; j < CancelTaskInstanceNames.Num(); ++j)
	{
		FName InstanceName = CancelTaskInstanceNames[j];
		NamedTasks.Reset();
		
		// Find every current task that needs to cancel before cancelling any
		for (UGameplayTask* Task : ActiveTasks)
		{
			if (Task && Task->GetInstanceName() == InstanceName)
			{
				NamedTasks.Add(Task);
			}
		}

		// Cancel each one individually.  Not canceling a task may do "anything" including killing other tasks or the ability itself
		for (int32 i = NamedTasks.Num() - 1; i >= 0; --i)
		{
			UGameplayTask* CurrentTask = NamedTasks[i];
			if (CurrentTask && CurrentTask->IsPendingKill() == false)
			{
				CurrentTask->ExternalCancel();
			}
		}
	}
	CancelTaskInstanceNames.Empty();
}

void UGameplayAbility::EndTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	EndTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGameplayAbility::EndOrCancelTasksByInstanceName);
}

void UGameplayAbility::CancelTaskByInstanceName(FName InstanceName)
{
	//Avoid race condition by delaying for one frame
	CancelTaskInstanceNames.AddUnique(InstanceName);
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGameplayAbility::EndOrCancelTasksByInstanceName);
}

void UGameplayAbility::EndAbilityState(FName OptionalStateNameToEnd)
{
	check(CurrentActorInfo);

	if (OnGameplayAbilityStateEnded.IsBound())
	{
		OnGameplayAbilityStateEnded.Broadcast(OptionalStateNameToEnd);
	}
}

void UGameplayAbility::AddAbilityTaskDebugMessage(UGameplayTask* AbilityTask, FString DebugMessage)
{
	TaskDebugMessages.AddDefaulted();
	FAbilityTaskDebugMessage& Msg = TaskDebugMessages.Last();
	Msg.FromTask = AbilityTask;
	Msg.Message = FString::Printf(TEXT("{%s} %s"), AbilityTask ? *AbilityTask->GetDebugString() : TEXT(""), *DebugMessage);
}

/**
 *	Helper methods for adding GaAmeplayCues without having to go through GameplayEffects.
 *	For now, none of these will happen predictively. We can eventually build this out more to 
 *	work with the PredictionKey system.
 */

void UGameplayAbility::K2_ExecuteGameplayCue(FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context)
{
	check(CurrentActorInfo);
	CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, Context);
}

void UGameplayAbility::K2_ExecuteGameplayCueWithParams(FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	check(CurrentActorInfo);
	const_cast<FGameplayCueParameters&>(GameplayCueParameters).AbilityLevel = GetAbilityLevel();
	CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(GameplayCueTag, GameplayCueParameters);
}

void UGameplayAbility::K2_AddGameplayCue(FGameplayTag GameplayCueTag, FGameplayEffectContextHandle Context, bool bRemoveOnAbilityEnd)
{
	check(CurrentActorInfo);

	// Make default context if nothing is passed in
	if (Context.IsValid() == false)
	{
		Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
	}

	Context.SetAbility(this);

	CurrentActorInfo->AbilitySystemComponent->AddGameplayCue(GameplayCueTag, Context);

	if (bRemoveOnAbilityEnd)
	{
		TrackedGameplayCues.Add(GameplayCueTag);
	}
}

void UGameplayAbility::K2_RemoveGameplayCue(FGameplayTag GameplayCueTag)
{
	check(CurrentActorInfo);
	CurrentActorInfo->AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);

	TrackedGameplayCues.Remove(GameplayCueTag);
}

FGameplayEffectContextHandle UGameplayAbility::GetContextFromOwner(FGameplayAbilityTargetDataHandle OptionalTargetData) const
{
	check(CurrentActorInfo);
	FGameplayEffectContextHandle Context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
	
	for (auto Data : OptionalTargetData.Data)
	{
		if (Data.IsValid())
		{
			Data->AddTargetDataToContext(Context, true);
		}
	}

	return Context;
}

int32 UGameplayAbility::GetAbilityLevel() const
{
	if (IsInstantiated() == false || CurrentActorInfo == nullptr)
	{
		return 1;
	}
	
	return GetAbilityLevel(CurrentSpecHandle, CurrentActorInfo);
}

/** Returns current ability level for non instanced abilities. You m ust call this version in these contexts! */
int32 UGameplayAbility::GetAbilityLevel(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	check(Spec);

	return Spec->Level;
}

FGameplayAbilitySpec* UGameplayAbility::GetCurrentAbilitySpec() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	return CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
}

FGameplayEffectContextHandle UGameplayAbility::GetGrantedByEffectContext() const
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	if (CurrentActorInfo)
	{
		FActiveGameplayEffectHandle ActiveHandle = CurrentActorInfo->AbilitySystemComponent->FindActiveGameplayEffectHandle(GetCurrentAbilitySpecHandle());
		if (ActiveHandle.IsValid())
		{
			return CurrentActorInfo->AbilitySystemComponent->GetEffectContextFromActiveGEHandle(ActiveHandle);
		}
	}

	return FGameplayEffectContextHandle();
}

void UGameplayAbility::RemoveGrantedByEffect()
{
	check(IsInstantiated()); // You should not call this on non instanced abilities.
	check(CurrentActorInfo);
	if (CurrentActorInfo)
	{
		FActiveGameplayEffectHandle ActiveHandle = CurrentActorInfo->AbilitySystemComponent->FindActiveGameplayEffectHandle(GetCurrentAbilitySpecHandle());
		if (ActiveHandle.IsValid())
		{
			CurrentActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveHandle);
		}
	}
}

UObject* UGameplayAbility::GetSourceObject(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	if (ActorInfo != NULL)
	{
		UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
		if (AbilitySystemComponent != NULL)
		{
			FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
			if (AbilitySpec)
			{
				return AbilitySpec->SourceObject;
			}
		}
	}
	return nullptr;
}

UObject* UGameplayAbility::GetCurrentSourceObject() const
{
	FGameplayAbilitySpec* AbilitySpec = GetCurrentAbilitySpec();
	if (AbilitySpec)
	{
		return AbilitySpec->SourceObject;
	}
	return nullptr;
}

FGameplayEffectContextHandle UGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo *ActorInfo) const
{
	check(ActorInfo);
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	// By default use the owner and avatar as the instigator and causer
	Context.AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->AvatarActor.Get());

	// add in the ability tracking here.
	Context.SetAbility(this);

	return Context;
}

bool UGameplayAbility::IsTriggered() const
{
	// Assume that if there is triggered data, then we are triggered. 
	// If we need to support abilities that can be both, this will need to be expanded.
	return AbilityTriggers.Num() > 0;
}

bool UGameplayAbility::IsPredictingClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		// LocalPredicted and ServerInitiated are both valid because in both those modes the ability also runs on the client
		if (!bIsAuthority && bIsLocallyControlled && (GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted || GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerInitiated))
		{
			return true;
		}
	}

	return false;
}

bool UGameplayAbility::IsForRemoteClient() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		bool bIsLocallyControlled = GetCurrentActorInfo()->IsLocallyControlled();
		bool bIsAuthority = GetCurrentActorInfo()->IsNetAuthority();

		if (bIsAuthority && !bIsLocallyControlled)
		{
			return true;
		}
	}

	return false;
}

bool UGameplayAbility::IsLocallyControlled() const
{
	if (GetCurrentActorInfo()->OwnerActor.IsValid())
	{
		return GetCurrentActorInfo()->IsLocallyControlled();
	}

	return false;
}

bool UGameplayAbility::HasAuthority(const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return (ActivationInfo->ActivationMode == EGameplayAbilityActivationMode::Authority);
}

bool UGameplayAbility::HasAuthorityOrPredictionKey(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return ActorInfo->AbilitySystemComponent->HasAuthorityOrPredictionKey(ActivationInfo);
}

void UGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	SetCurrentActorInfo(Spec.Handle, ActorInfo);

	// If we already have an avatar set, call the OnAvatarSet event as well
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		OnAvatarSet(ActorInfo, Spec);
	}
}

void UGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// Projects may want to initiate passives or do other "BeginPlay" type of logic here.
}

// -------------------------------------------------------

FActiveGameplayEffectHandle UGameplayAbility::BP_ApplyGameplayEffectToOwner(TSubclassOf<UGameplayEffect> GameplayEffectClass, int32 GameplayEffectLevel, int32 Stacks)
{
	check(CurrentActorInfo);
	check(CurrentSpecHandle.IsValid());

	if ( GameplayEffectClass )
	{
		const UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
		return ApplyGameplayEffectToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, GameplayEffect, GameplayEffectLevel, Stacks);
	}

	ABILITY_LOG(Error, TEXT("BP_ApplyGameplayEffectToOwner called on ability %s with no GameplayEffectClass."), *GetName());
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UGameplayAbility::ApplyGameplayEffectToOwner(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const UGameplayEffect* GameplayEffect, float GameplayEffectLevel, int32 Stacks) const
{
	if (GameplayEffect && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffect->GetClass(), GameplayEffectLevel);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->StackCount = Stacks;
			return ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// We cannot apply GameplayEffects in this context. Return an empty handle.
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UGameplayAbility::K2_ApplyGameplayEffectSpecToOwner(const FGameplayEffectSpecHandle EffectSpecHandle)
{
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, EffectSpecHandle);
}

FActiveGameplayEffectHandle UGameplayAbility::ApplyGameplayEffectSpecToOwner(const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle) const
{
	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (SpecHandle.IsValid() && (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo)))
	{
		return ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get(), ActorInfo->AbilitySystemComponent->GetPredictionKeyForNewAction());

	}
	return FActiveGameplayEffectHandle();
}

// -------------------------------

TArray<FActiveGameplayEffectHandle> UGameplayAbility::BP_ApplyGameplayEffectToTarget(FGameplayAbilityTargetDataHandle Target, TSubclassOf<UGameplayEffect> GameplayEffectClass, int32 GameplayEffectLevel, int32 Stacks)
{
	return ApplyGameplayEffectToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, Target, GameplayEffectClass, GameplayEffectLevel, Stacks);
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::ApplyGameplayEffectToTarget(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayAbilityTargetDataHandle& Target, TSubclassOf<UGameplayEffect> GameplayEffectClass, float GameplayEffectLevel, int32 Stacks) const
{
	SCOPE_CYCLE_COUNTER(STAT_ApplyGameplayEffectToTarget);

	TArray<FActiveGameplayEffectHandle> EffectHandles;

	if (HasAuthority(&ActivationInfo) == false && UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects() == false)
	{
		// Early out to avoid making effect specs that we can't apply
		return EffectHandles;
	}

	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
	}
	else if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffectClass, GameplayEffectLevel);
		SpecHandle.Data->StackCount = Stacks;
		EffectHandles.Append(ApplyGameplayEffectSpecToTarget(Handle, ActorInfo, ActivationInfo, SpecHandle, Target));
	}

	return EffectHandles;
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::K2_ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpecHandle SpecHandle, FGameplayAbilityTargetDataHandle TargetData)
{
	return ApplyGameplayEffectSpecToTarget(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, SpecHandle, TargetData);
}

TArray<FActiveGameplayEffectHandle> UGameplayAbility::ApplyGameplayEffectSpecToTarget(const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	
	if (SpecHandle.IsValid() && HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		TARGETLIST_SCOPE_LOCK(*ActorInfo->AbilitySystemComponent);
		for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetData.Data)
		{
			EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), ActorInfo->AbilitySystemComponent->GetPredictionKeyForNewAction()));
		}
	}
	return EffectHandles;
}

void UGameplayAbility::IncrementListLock() const
{
	++ScopeLockCount;
}
void UGameplayAbility::DecrementListLock() const
{
	if (--ScopeLockCount == 0)
	{
		// execute delayed functions in the order they came in
		// These may end or cancel this ability
		for (int32 Idx = 0; Idx < WaitingToExecute.Num(); ++Idx)
		{
			WaitingToExecute[Idx].ExecuteIfBound();
		}

		WaitingToExecute.Empty();
	}
}

void UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithAssetTags(FGameplayTagContainer WithTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(WithTags);
	CurrentActorInfo->AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

void UGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithGrantedTags(FGameplayTagContainer WithGrantedTags, int32 StacksToRemove)
{
	if (HasAuthority(&CurrentActivationInfo) == false)
	{
		return;
	}

	FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(WithGrantedTags);
	CurrentActorInfo->AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
}

float UGameplayAbility::GetCooldownTimeRemaining() const
{
	return IsInstantiated() ? GetCooldownTimeRemaining(CurrentActorInfo) : 0.f;
}

void UGameplayAbility::SetRemoteInstanceHasEnded()
{
	// This could potentially happen in shutdown corner cases
	if (IsPendingKill() || CurrentActorInfo == nullptr || CurrentActorInfo->AbilitySystemComponent.IsValid() == false)
	{
		return;
	}

	RemoteInstanceEnded = true;
	for (UGameplayTask* Task : ActiveTasks)
	{
		if (Task && Task->IsPendingKill() == false && Task->IsWaitingOnRemotePlayerdata())
		{
			// We have a task that is waiting for player input, but the remote player has ended the ability, so he will not send it.
			// Kill the ability to avoid getting stuck active.
			
			ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s is waiting on remote player input and the  remote player has just ended the ability."), *GetName(), *Task->GetDebugString());
			CurrentActorInfo->AbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
			break;
		}
	}
}

void UGameplayAbility::NotifyAvatarDestroyed()
{
	// This could potentially happen in shutdown corner cases
	if (IsPendingKill() || CurrentActorInfo == nullptr || CurrentActorInfo->AbilitySystemComponent.IsValid() == false)
	{
		return;
	}

	RemoteInstanceEnded = true;
	for (UGameplayTask* Task : ActiveTasks)
	{
		if (Task && Task->IsPendingKill() == false && Task->IsWaitingOnAvatar())
		{
			// We have a task waiting on some Avatar state but the avatar is destroyed, so force end the ability to avoid getting stuck on.
			
			ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s is waiting on avatar data avatar has been destroyed."), *GetName(), *Task->GetDebugString());
			CurrentActorInfo->AbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
			break;
		}
	}
}

void UGameplayAbility::NotifyAbilityTaskWaitingOnPlayerData(class UAbilityTask* AbilityTask)
{
	// This should never happen since it will only be called from actively running ability tasks
	check(CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid());

	if (RemoteInstanceEnded)
	{
		ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s has started after the remote player has ended the ability."), *GetName(), *AbilityTask->GetDebugString());
		CurrentActorInfo->AbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
	}
}

void UGameplayAbility::NotifyAbilityTaskWaitingOnAvatar(class UAbilityTask* AbilityTask)
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid() == false)
	{
		ABILITY_LOG(Log, TEXT("Ability %s is force cancelling because Task %s has started while there is no valid AvatarActor"), *GetName(), *AbilityTask->GetDebugString());
		CurrentActorInfo->AbilitySystemComponent->ForceCancelAbilityDueToReplication(this);
	}
}
