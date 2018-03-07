// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Canvas.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "AbilitySystemStats.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"

#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "GameplayEffectCustomApplicationRequirement.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogAbilitySystemComponent);

#define LOCTEXT_NAMESPACE "AbilitySystemComponent"

/** Enable to log out all render state create, destroy and updatetransform events */
#define LOG_RENDER_STATE 0

UAbilitySystemComponent::UAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, GameplayTagCountContainer()
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bStartWithTickEnabled = true; // FIXME! Just temp until timer manager figured out
	bAutoActivate = true;	// Forcing AutoActivate since above we manually force tick enabled.
							// if we don't have this, UpdateShouldTick() fails to have any effect
							// because we'll be receiving ticks but bIsActive starts as false
	
	CachedIsNetSimulated = false;
	UserAbilityActivationInhibited = false;

	GenericConfirmInputID = INDEX_NONE;
	GenericCancelInputID = INDEX_NONE;

	bSuppressGrantAbility = false;
	bSuppressGameplayCues = false;
	bPendingMontagerep = false;

	AbilityLastActivatedTime = 0.f;

	ReplicationMode = EReplicationMode::Full;

	ClientActivateAbilityFailedStartTime = 0.f;
	ClientActivateAbilityFailedCountRecent = 0;
}

const UAttributeSet* UAbilitySystemComponent::InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	const UAttributeSet* AttributeObj = NULL;
	if (Attributes)
	{
		AttributeObj = GetOrCreateAttributeSubobject(Attributes);
		if (AttributeObj && DataTable)
		{
			// This const_cast is OK - this is one of the few places we want to directly modify our AttributeSet properties rather
			// than go through a gameplay effect
			const_cast<UAttributeSet*>(AttributeObj)->InitFromMetaDataTable(DataTable);
		}
	}
	return AttributeObj;
}

void UAbilitySystemComponent::K2_InitStats(TSubclassOf<class UAttributeSet> Attributes, const UDataTable* DataTable)
{
	InitStats(Attributes, DataTable);
}

const UAttributeSet* UAbilitySystemComponent::GetOrCreateAttributeSubobject(TSubclassOf<UAttributeSet> AttributeClass)
{
	AActor* OwningActor = GetOwner();
	const UAttributeSet* MyAttributes = nullptr;
	if (OwningActor && AttributeClass)
	{
		MyAttributes = GetAttributeSubobject(AttributeClass);
		if (!MyAttributes)
		{
			UAttributeSet* Attributes = NewObject<UAttributeSet>(OwningActor, AttributeClass);
			SpawnedAttributes.AddUnique(Attributes);
			MyAttributes = Attributes;
		}
	}

	return MyAttributes;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobjectChecked(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	const UAttributeSet* Set = GetAttributeSubobject(AttributeClass);
	check(Set);
	return Set;
}

const UAttributeSet* UAbilitySystemComponent::GetAttributeSubobject(const TSubclassOf<UAttributeSet> AttributeClass) const
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsA(AttributeClass))
		{
			return Set;
		}
	}
	return NULL;
}

bool UAbilitySystemComponent::HasAttributeSetForAttribute(FGameplayAttribute Attribute) const
{
	return (Attribute.IsValid() && (Attribute.IsSystemAttribute() || GetAttributeSubobject(Attribute.GetAttributeSetClass()) != nullptr));
}

void UAbilitySystemComponent::GetAllAttributes(OUT TArray<FGameplayAttribute>& Attributes)
{
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		for ( TFieldIterator<UProperty> It(Set->GetClass()); It; ++It)
		{
			if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(*It))
			{
				Attributes.Push( FGameplayAttribute(FloatProperty) );
			}
			else if (FGameplayAttribute::IsGameplayAttributeDataProperty(*It))
			{
				Attributes.Push(FGameplayAttribute(*It));
			}
		}
	}
}

void UAbilitySystemComponent::OnRegister()
{
	Super::OnRegister();

	// Cached off netrole to avoid constant checking on owning actor
	CacheIsNetSimulated();

	// Init starting data
	for (int32 i=0; i < DefaultStartingData.Num(); ++i)
	{
		if (DefaultStartingData[i].Attributes && DefaultStartingData[i].DefaultStartingTable)
		{
			UAttributeSet* Attributes = const_cast<UAttributeSet*>(GetOrCreateAttributeSubobject(DefaultStartingData[i].Attributes));
			Attributes->InitFromMetaDataTable(DefaultStartingData[i].DefaultStartingTable);
		}
	}

	ActiveGameplayEffects.RegisterWithOwner(this);
	ActivatableAbilities.RegisterWithOwner(this);
	ActiveGameplayCues.Owner = this;
	ActiveGameplayCues.bMinimalReplication = false;
	MinimalReplicationGameplayCues.Owner = this;
	MinimalReplicationGameplayCues.bMinimalReplication = true;
	MinimalReplicationTags.Owner = this;

	/** Allocate an AbilityActorInfo. Note: this goes through a global function and is a SharedPtr so projects can make their own AbilityActorInfo */
	AbilityActorInfo = TSharedPtr<FGameplayAbilityActorInfo>(UAbilitySystemGlobals::Get().AllocAbilityActorInfo());
}

void UAbilitySystemComponent::OnUnregister()
{
	Super::OnUnregister();

	DestroyActiveState();
}

void UAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache net role here as well since for map-placed actors on clients, the Role may not be set correctly yet in OnRegister.
	CacheIsNetSimulated();
}

void UAbilitySystemComponent::CacheIsNetSimulated()
{
	CachedIsNetSimulated = IsNetSimulating();
	ActiveGameplayEffects.OwnerIsNetAuthority = IsOwnerActorAuthoritative();
}

// ---------------------------------------------------------

const FActiveGameplayEffect* UAbilitySystemComponent::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle) const
{
	return ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
}

bool UAbilitySystemComponent::HasNetworkAuthorityToApplyGameplayEffect(FPredictionKey PredictionKey) const
{
	return (IsOwnerActorAuthoritative() || PredictionKey.IsValidForMorePrediction());
}

void UAbilitySystemComponent::SetNumericAttributeBase(const FGameplayAttribute &Attribute, float NewFloatValue)
{
	// Go through our active gameplay effects container so that aggregation/mods are handled properly.
	ActiveGameplayEffects.SetAttributeBaseValue(Attribute, NewFloatValue);
}

float UAbilitySystemComponent::GetNumericAttributeBase(const FGameplayAttribute &Attribute) const
{
	if (Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	return ActiveGameplayEffects.GetAttributeBaseValue(Attribute);
}

void UAbilitySystemComponent::SetNumericAttribute_Internal(const FGameplayAttribute &Attribute, float& NewFloatValue)
{
	// Set the attribute directly: update the UProperty on the attribute set.
	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	Attribute.SetNumericValueChecked(NewFloatValue, const_cast<UAttributeSet*>(AttributeSet));
}

float UAbilitySystemComponent::GetNumericAttribute(const FGameplayAttribute &Attribute) const
{
	if (Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* const AttributeSetOrNull = GetAttributeSubobject(Attribute.GetAttributeSetClass());
	if (AttributeSetOrNull == nullptr)
	{
		return 0.f;
	}

	return Attribute.GetNumericValue(AttributeSetOrNull);
}

float UAbilitySystemComponent::GetNumericAttributeChecked(const FGameplayAttribute &Attribute) const
{
	if(Attribute.IsSystemAttribute())
	{
		return 0.f;
	}

	const UAttributeSet* AttributeSet = GetAttributeSubobjectChecked(Attribute.GetAttributeSetClass());
	return Attribute.GetNumericValueChecked(AttributeSet);
}

void UAbilitySystemComponent::ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude)
{
	// We can only apply loose mods on the authority. If we ever need to predict these, they would need to be turned into GEs and be given a prediction key so that
	// they can be rolled back.
	if (IsOwnerActorAuthoritative())
	{
		ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
	}
}

void UAbilitySystemComponent::ApplyModToAttributeUnsafe(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude)
{
	ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
}

FGameplayEffectSpecHandle UAbilitySystemComponent::MakeOutgoingSpec(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle Context) const
{
	SCOPE_CYCLE_COUNTER(STAT_GetOutgoingSpec);
	if (Context.IsValid() == false)
	{
		Context = MakeEffectContext();
	}

	if (GameplayEffectClass)
	{
		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();

		FGameplayEffectSpec* NewSpec = new FGameplayEffectSpec(GameplayEffect, Context, Level);
		return FGameplayEffectSpecHandle(NewSpec);
	}

	return FGameplayEffectSpecHandle(nullptr);
}

FGameplayEffectContextHandle UAbilitySystemComponent::MakeEffectContext() const
{
	FGameplayEffectContextHandle Context = FGameplayEffectContextHandle(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	// By default use the owner and avatar as the instigator and causer
	check(AbilityActorInfo.IsValid());
	
	Context.AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	return Context;
}

int32 UAbilitySystemComponent::GetGameplayEffectCount(TSubclassOf<UGameplayEffect> SourceGameplayEffect, UAbilitySystemComponent* OptionalInstigatorFilterComponent, bool bEnforceOnGoingCheck)
{
	int32 Count = 0;

	if (SourceGameplayEffect)
	{
		FGameplayEffectQuery Query;
		Query.CustomMatchDelegate.BindLambda([&](const FActiveGameplayEffect& CurEffect)
		{
			bool bMatches = false;

			// First check at matching: backing GE class must be the exact same
			if (CurEffect.Spec.Def && SourceGameplayEffect == CurEffect.Spec.Def->GetClass())
			{
				// If an instigator is specified, matching is dependent upon it
				if (OptionalInstigatorFilterComponent)
				{
					bMatches = (OptionalInstigatorFilterComponent == CurEffect.Spec.GetEffectContext().GetInstigatorAbilitySystemComponent());
				}
				else
				{
					bMatches = true;
				}
			}

			return bMatches;
		});

		Count = ActiveGameplayEffects.GetActiveEffectCount(Query, bEnforceOnGoingCheck);
	}

	return Count;
}

int32 UAbilitySystemComponent::GetAggregatedStackCount(const FGameplayEffectQuery& Query)
{
	return ActiveGameplayEffects.GetActiveEffectCount(Query);
}

FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget(TSubclassOf<UGameplayEffect> GameplayEffectClass, UAbilitySystemComponent* Target, float Level, FGameplayEffectContextHandle Context)
{
	if (Target == nullptr)
	{
		ABILITY_LOG(Log, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget called with null Target. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::BP_ApplyGameplayEffectToTarget called with null GameplayEffectClass. %s. Context: %s"), *GetFullName(), *Context.ToString());
		return FActiveGameplayEffectHandle();
	}

	UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	return ApplyGameplayEffectToTarget(GameplayEffect, Target, Level, Context);	
}

/** This is a helper function used in automated testing, I'm not sure how useful it will be to gamecode or blueprints */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context, FPredictionKey PredictionKey)
{
	check(GameplayEffect);
	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		if (!Context.IsValid())
		{
			Context = MakeEffectContext();
		}

		FGameplayEffectSpec	Spec(GameplayEffect, Context, Level);
		return ApplyGameplayEffectSpecToTarget(Spec, Target, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

/** Helper function since we can't have default/optional values for FModifierQualifier in K2 function */
FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level, FGameplayEffectContextHandle EffectContext)
{
	if ( GameplayEffectClass )
	{
		UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
		return ApplyGameplayEffectToSelf(GameplayEffect, Level, EffectContext);
	}

	return FActiveGameplayEffectHandle();
}

/** This is a helper function - it seems like this will be useful as a blueprint interface at the least, but Level parameter may need to be expanded */
FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey)
{
	if (GameplayEffect == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::ApplyGameplayEffectToSelf called by Instigator %s with a null GameplayEffect."), *EffectContext.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);
		return ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return FActiveGameplayEffectHandle();
}

FOnActiveGameplayEffectRemoved* UAbilitySystemComponent::OnGameplayEffectRemovedDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnRemovedDelegate;
	}

	return nullptr;
}

FOnActiveGameplayEffectRemoved_Info* UAbilitySystemComponent::OnGameplayEffectRemoved_InfoDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnRemoved_InfoDelegate;
	}

	return nullptr;
}

FOnGivenActiveGameplayEffectRemoved& UAbilitySystemComponent::OnAnyGameplayEffectRemovedDelegate()
{
	return ActiveGameplayEffects.OnActiveGameplayEffectRemovedDelegate;
}

FOnActiveGameplayEffectStackChange* UAbilitySystemComponent::OnGameplayEffectStackChangeDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnStackChangeDelegate;
	}

	return nullptr;
}

FOnActiveGameplayEffectTimeChange* UAbilitySystemComponent::OnGameplayEffectTimeChangeDelegate(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveEffect)
	{
		return &ActiveEffect->OnTimeChangeDelegate;
	}

	return nullptr;
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGameplayTagEvent(FGameplayTag Tag, EGameplayTagEventType::Type EventType)
{
	return GameplayTagCountContainer.RegisterGameplayTagEvent(Tag, EventType);
}

void UAbilitySystemComponent::RegisterAndCallGameplayTagEvent(FGameplayTag Tag, FOnGameplayEffectTagCountChanged::FDelegate Delegate, EGameplayTagEventType::Type EventType)
{
	GameplayTagCountContainer.RegisterGameplayTagEvent(Tag, EventType).Add(Delegate);

	const int32 TagCount = GetTagCount(Tag);
	if (TagCount > 0)
	{
		Delegate.Execute(Tag, TagCount);
	}
}

FOnGameplayEffectTagCountChanged& UAbilitySystemComponent::RegisterGenericGameplayTagEvent()
{
	return GameplayTagCountContainer.RegisterGenericGameplayEvent();
}

FOnGameplayAttributeChange& UAbilitySystemComponent::RegisterGameplayAttributeEvent(FGameplayAttribute Attribute)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ActiveGameplayEffects.RegisterGameplayAttributeEvent(Attribute);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FOnGameplayAttributeValueChange& UAbilitySystemComponent::GetGameplayAttributeValueChangeDelegate(FGameplayAttribute Attribute)
{
	return ActiveGameplayEffects.GetGameplayAttributeValueChangeDelegate(Attribute);
}

UProperty* UAbilitySystemComponent::GetOutgoingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UAbilitySystemComponent, OutgoingDuration));
	return DurationProperty;
}

UProperty* UAbilitySystemComponent::GetIncomingDurationProperty()
{
	static UProperty* DurationProperty = FindFieldChecked<UProperty>(UAbilitySystemComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UAbilitySystemComponent, IncomingDuration));
	return DurationProperty;
}

const FGameplayEffectAttributeCaptureDefinition& UAbilitySystemComponent::GetOutgoingDurationCapture()
{
	// We will just always take snapshots of the source's duration mods
	static FGameplayEffectAttributeCaptureDefinition OutgoingDurationCapture(GetOutgoingDurationProperty(), EGameplayEffectAttributeCaptureSource::Source, true);
	return OutgoingDurationCapture;

}
const FGameplayEffectAttributeCaptureDefinition& UAbilitySystemComponent::GetIncomingDurationCapture()
{
	// Never take snapshots of the target's duration mods: we are going to evaluate this on apply only.
	static FGameplayEffectAttributeCaptureDefinition IncomingDurationCapture(GetIncomingDurationProperty(), EGameplayEffectAttributeCaptureSource::Target, false);
	return IncomingDurationCapture;
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::ResetTagMap()
{
	GameplayTagCountContainer.Reset();
}

void UAbilitySystemComponent::NotifyTagMap_StackCountChange(const FGameplayTagContainer& Container)
{
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FGameplayTag& Tag = *TagIt;
		GameplayTagCountContainer.Notify_StackCountChange(Tag);
	}
}

// ------------------------------------------------------------------------

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToTarget(OUT FGameplayEffectSpec &Spec, UAbilitySystemComponent *Target, FPredictionKey PredictionKey)
{
	if (!UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	FActiveGameplayEffectHandle ReturnHandle;

	if (!UAbilitySystemGlobals::Get().ShouldPredictTargetGameplayEffects())
	{
		// If we don't want to predict target effects, clear prediction key
		PredictionKey = FPredictionKey();
	}

	if (Target)
	{
		ReturnHandle = Target->ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}

	return ReturnHandle;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(OUT FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Scope lock the container after the addition has taken place to prevent the new effect from potentially getting mangled during the remainder
	// of the add operation
	FScopedActiveGameplayEffectLock ScopeLock(ActiveGameplayEffects);

	FScopeCurrentGameplayEffectBeingApplied ScopedGEApplication(&Spec, this);

	const bool bIsNetAuthority = IsOwnerActorAuthoritative();

	// Check Network Authority
	if (!HasNetworkAuthorityToApplyGameplayEffect(PredictionKey))
	{
		return FActiveGameplayEffectHandle();
	}

	// Don't allow prediction of periodic effects
	if(PredictionKey.IsValidKey() && Spec.GetPeriod() > 0.f)
	{
		if(IsOwnerActorAuthoritative())
		{
			// Server continue with invalid prediction key
			PredictionKey = FPredictionKey();
		}
		else
		{
			// Client just return now
			return FActiveGameplayEffectHandle();
		}
	}

	// Are we currently immune to this? (ApplicationImmunity)
	const FActiveGameplayEffect* ImmunityGE=nullptr;
	if (ActiveGameplayEffects.HasApplicationImmunityToSpec(Spec, ImmunityGE))
	{
		OnImmunityBlockGameplayEffect(Spec, ImmunityGE);
		return FActiveGameplayEffectHandle();
	}

	// Check AttributeSet requirements: make sure all attributes are valid
	// We may want to cache this off in some way to make the runtime check quicker.
	// We also need to handle things in the execution list
	for (const FGameplayModifierInfo& Mod : Spec.Def->Modifiers)
	{
		if (!Mod.Attribute.IsValid())
		{
			ABILITY_LOG(Warning, TEXT("%s has a null modifier attribute."), *Spec.Def->GetPathName());
			return FActiveGameplayEffectHandle();
		}
	}

	// check if the effect being applied actually succeeds
	float ChanceToApply = Spec.GetChanceToApplyToTarget();
	if ((ChanceToApply < 1.f - SMALL_NUMBER) && (FMath::FRand() > ChanceToApply))
	{
		return FActiveGameplayEffectHandle();
	}

	// Get MyTags.
	//	We may want to cache off a GameplayTagContainer instead of rebuilding it every time.
	//	But this will also be where we need to merge in context tags? (Headshot, executing ability, etc?)
	//	Or do we push these tags into (our copy of the spec)?

	{
		// Note: static is ok here since the scope is so limited, but wider usage of MyTags is not safe since this function can be recursively called
		static FGameplayTagContainer MyTags;
		MyTags.Reset();

		GetOwnedGameplayTags(MyTags);

		if (Spec.Def->ApplicationTagRequirements.RequirementsMet(MyTags) == false)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// Custom application requirement check
	for (const TSubclassOf<UGameplayEffectCustomApplicationRequirement>& AppReq : Spec.Def->ApplicationRequirements)
	{
		if (*AppReq && AppReq->GetDefaultObject<UGameplayEffectCustomApplicationRequirement>()->CanApplyGameplayEffect(Spec.Def, Spec, this) == false)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// Clients should treat predicted instant effects as if they have infinite duration. The effects will be cleaned up later.
	bool bTreatAsInfiniteDuration = GetOwnerRole() != ROLE_Authority && PredictionKey.IsLocalClientKey() && Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant;

	// Make sure we create our copy of the spec in the right place
	// We initialize the FActiveGameplayEffectHandle here with INDEX_NONE to handle the case of instant GE
	// Initializing it like this will set the bPassedFiltersAndWasExecuted on the FActiveGameplayEffectHandle to true so we can know that we applied a GE
	FActiveGameplayEffectHandle	MyHandle(INDEX_NONE);
	bool bInvokeGameplayCueApplied = Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant; // Cache this now before possibly modifying predictive instant effect to infinite duration effect.
	bool bFoundExistingStackableGE = false;

	FActiveGameplayEffect* AppliedEffect = nullptr;

	FGameplayEffectSpec* OurCopyOfSpec = nullptr;
	TSharedPtr<FGameplayEffectSpec> StackSpec;
	{
		if (Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant || bTreatAsInfiniteDuration)
		{
			AppliedEffect = ActiveGameplayEffects.ApplyGameplayEffectSpec(Spec, PredictionKey, bFoundExistingStackableGE);
			if (!AppliedEffect)
			{
				return FActiveGameplayEffectHandle();
			}

			MyHandle = AppliedEffect->Handle;
			OurCopyOfSpec = &(AppliedEffect->Spec);

			// Log results of applied GE spec
			if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
			{
				ABILITY_VLOG(OwnerActor, Log, TEXT("Applied %s"), *OurCopyOfSpec->Def->GetFName().ToString());

				for (FGameplayModifierInfo Modifier : Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
					ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}
		}

		if (!OurCopyOfSpec)
		{
			StackSpec = TSharedPtr<FGameplayEffectSpec>(new FGameplayEffectSpec(Spec));
			OurCopyOfSpec = StackSpec.Get();
			UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(*OurCopyOfSpec, this);
			OurCopyOfSpec->CaptureAttributeDataFromTarget(this);
		}

		// if necessary add a modifier to OurCopyOfSpec to force it to have an infinite duration
		if (bTreatAsInfiniteDuration)
		{
			// This should just be a straight set of the duration float now
			OurCopyOfSpec->SetDuration(UGameplayEffect::INFINITE_DURATION, true);
		}
	}

	if (OurCopyOfSpec)
	{
		// Update (not push) the global spec being applied [we want to switch it to our copy, from the const input copy)
		UAbilitySystemGlobals::Get().SetCurrentAppliedGE(OurCopyOfSpec);
	}
	

	// We still probably want to apply tags and stuff even if instant?
	// If bSuppressStackingCues is set for this GameplayEffect, only add the GameplayCue if this is the first instance of the GameplayEffect
	if (!bSuppressGameplayCues && bInvokeGameplayCueApplied && AppliedEffect && !AppliedEffect->bIsInhibited && 
		(!bFoundExistingStackableGE || !Spec.Def->bSuppressStackingCues))
	{
		// We both added and activated the GameplayCue here.
		// On the client, who will invoke the gameplay cue from an OnRep, he will need to look at the StartTime to determine
		// if the Cue was actually added+activated or just added (due to relevancy)

		// Fixme: what if we wanted to scale Cue magnitude based on damage? E.g, scale an cue effect when the GE is buffed?

		if (OurCopyOfSpec->StackCount > Spec.StackCount)
		{
			// Because PostReplicatedChange will get called from modifying the stack count
			// (and not PostReplicatedAdd) we won't know which GE was modified.
			// So instead we need to explicitly RPC the client so it knows the GC needs updating
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueAddedAndWhileActive_FromSpec(this, *OurCopyOfSpec, PredictionKey);
		}
		else
		{
			// Otherwise these will get replicated to the client when the GE gets added to the replicated array
			InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::OnActive);
			InvokeGameplayCueEvent(*OurCopyOfSpec, EGameplayCueEvent::WhileActive);
		}
	}
	
	// Execute the GE at least once (if instant, this will execute once and be done. If persistent, it was added to ActiveGameplayEffects above)
	
	// Execute if this is an instant application effect
	if (bTreatAsInfiniteDuration)
	{
		// This is an instant application but we are treating it as an infinite duration for prediction. We should still predict the execute GameplayCUE.
		// (in non predictive case, this will happen inside ::ExecuteGameplayEffect)

		if (!bSuppressGameplayCues)
		{
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_FromSpec(this, *OurCopyOfSpec, PredictionKey);
		}
	}
	else if (Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		if (OurCopyOfSpec->Def->OngoingTagRequirements.IsEmpty())
		{
			ExecuteGameplayEffect(*OurCopyOfSpec, PredictionKey);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("%s is instant but has tag requirements. Tag requirements can only be used with gameplay effects that have a duration. This gameplay effect will be ignored."), *Spec.Def->GetPathName());
		}
	}

	if (Spec.GetPeriod() != UGameplayEffect::NO_PERIOD && Spec.TargetEffectSpecs.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("%s is periodic but also applies GameplayEffects to its target. GameplayEffects will only be applied once, not every period."), *Spec.Def->GetPathName());
	}

	// ------------------------------------------------------
	//	Remove gameplay effects with tags
	//		Remove any active gameplay effects that match the RemoveGameplayEffectsWithTags in the definition for this spec
	//		Only call this if we are the Authoritative owner and we have some RemoveGameplayEffectsWithTags.CombinedTag to remove
	// ------------------------------------------------------
	if (bIsNetAuthority && Spec.Def->RemoveGameplayEffectsWithTags.CombinedTags.Num() > 0)
	{
		// Clear tags is always removing all stacks.
		FGameplayEffectQuery ClearQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Spec.Def->RemoveGameplayEffectsWithTags.CombinedTags);
		if (MyHandle.IsValid())
		{
			ClearQuery.IgnoreHandles.Add(MyHandle);
		}
		ActiveGameplayEffects.RemoveActiveEffects(ClearQuery, -1);
	}
	
	// ------------------------------------------------------
	// Apply Linked effects
	// todo: this is ignoring the returned handles, should we put them into a TArray and return all of the handles?
	// ------------------------------------------------------
	for (const FGameplayEffectSpecHandle TargetSpec: Spec.TargetEffectSpecs)
	{
		if (TargetSpec.IsValid())
		{
			ApplyGameplayEffectSpecToSelf(*TargetSpec.Data.Get(), PredictionKey);
		}
	}

	UAbilitySystemComponent* InstigatorASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();

	// Send ourselves a callback	
	OnGameplayEffectAppliedToSelf(InstigatorASC, *OurCopyOfSpec, MyHandle);

	// Send the instigator a callback
	if (InstigatorASC)
	{
		InstigatorASC->OnGameplayEffectAppliedToTarget(this, *OurCopyOfSpec, MyHandle);
	}

	return MyHandle;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectSpecToTarget(FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* Target)
{
	FActiveGameplayEffectHandle ReturnHandle;
	if (SpecHandle.IsValid() && Target)
	{
		ReturnHandle = ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), Target);
	}

	return ReturnHandle;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::BP_ApplyGameplayEffectSpecToSelf(FGameplayEffectSpecHandle& SpecHandle)
{
	FActiveGameplayEffectHandle ReturnHandle;
	if (SpecHandle.IsValid())
	{
		ReturnHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return ReturnHandle;
}

void UAbilitySystemComponent::ExecutePeriodicEffect(FActiveGameplayEffectHandle	Handle)
{
	ActiveGameplayEffects.ExecutePeriodicGameplayEffect(Handle);
}

void UAbilitySystemComponent::ExecuteGameplayEffect(FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	// Should only ever execute effects that are instant application or periodic application
	// Effects with no period and that aren't instant application should never be executed
	check( (Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION || Spec.GetPeriod() != UGameplayEffect::NO_PERIOD) );

	if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
	{
		ABILITY_VLOG(OwnerActor, Log, TEXT("Executed %s"), *Spec.Def->GetFName().ToString());
		
		for (FGameplayModifierInfo Modifier : Spec.Def->Modifiers)
		{
			float Magnitude = 0.f;
			Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
			ABILITY_VLOG(OwnerActor, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
		}
	}

	ActiveGameplayEffects.ExecuteActiveEffectsFrom(Spec, PredictionKey);
}

void UAbilitySystemComponent::CheckDurationExpired(FActiveGameplayEffectHandle Handle)
{
	ActiveGameplayEffects.CheckDuration(Handle);
}

const UGameplayEffect* UAbilitySystemComponent::GetGameplayEffectDefForHandle(FActiveGameplayEffectHandle Handle)
{
	FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	if (ActiveGE)
	{
		return ActiveGE->Spec.Def;
	}

	return nullptr;
}

bool UAbilitySystemComponent::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove)
{
	return ActiveGameplayEffects.RemoveActiveGameplayEffect(Handle, StacksToRemove);
}

void UAbilitySystemComponent::RemoveActiveGameplayEffectBySourceEffect(TSubclassOf<UGameplayEffect> GameplayEffect, UAbilitySystemComponent* InstigatorAbilitySystemComponent, int32 StacksToRemove /*= -1*/)
{
	if (GameplayEffect)
	{
		FGameplayEffectQuery Query;
		Query.CustomMatchDelegate.BindLambda([&](const FActiveGameplayEffect& CurEffect)
		{
			bool bMatches = false;

			// First check at matching: backing GE class must be the exact same
			if (CurEffect.Spec.Def && GameplayEffect == CurEffect.Spec.Def->GetClass())
			{
				// If an instigator is specified, matching is dependent upon it
				if (InstigatorAbilitySystemComponent)
				{
					bMatches = (InstigatorAbilitySystemComponent == CurEffect.Spec.GetEffectContext().GetInstigatorAbilitySystemComponent());
				}
				else
				{
					bMatches = true;
				}
			}

			return bMatches;
		});

		ActiveGameplayEffects.RemoveActiveEffects(Query, StacksToRemove);
	}
}

float UAbilitySystemComponent::GetGameplayEffectDuration(FActiveGameplayEffectHandle Handle) const
{
	float StartEffectTime = 0.0f;
	float Duration = 0.0f;
	ActiveGameplayEffects.GetGameplayEffectStartTimeAndDuration(Handle, StartEffectTime, Duration);

	return Duration;
}

void UAbilitySystemComponent::GetGameplayEffectStartTimeAndDuration(FActiveGameplayEffectHandle Handle, float& StartEffectTime, float& Duration) const
{
	return ActiveGameplayEffects.GetGameplayEffectStartTimeAndDuration(Handle, StartEffectTime, Duration);
}

float UAbilitySystemComponent::GetGameplayEffectMagnitude(FActiveGameplayEffectHandle Handle, FGameplayAttribute Attribute) const
{
	return ActiveGameplayEffects.GetGameplayEffectMagnitude(Handle, Attribute);
}

void UAbilitySystemComponent::SetActiveGameplayEffectLevel(FActiveGameplayEffectHandle ActiveHandle, int32 NewLevel)
{
	ActiveGameplayEffects.SetActiveGameplayEffectLevel(ActiveHandle, NewLevel);
}

void UAbilitySystemComponent::SetActiveGameplayEffectLevelUsingQuery(FGameplayEffectQuery Query, int32 NewLevel)
{
	TArray<FActiveGameplayEffectHandle> ActiveGameplayEffectHandles = ActiveGameplayEffects.GetActiveEffects(Query);
	for (FActiveGameplayEffectHandle ActiveHandle : ActiveGameplayEffectHandles)
	{
		SetActiveGameplayEffectLevel(ActiveHandle, NewLevel);
	}
}

int32 UAbilitySystemComponent::GetCurrentStackCount(FActiveGameplayEffectHandle Handle) const
{
	if (const FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle))
	{
		return ActiveGE->Spec.StackCount;
	}
	return 0;
}

int32 UAbilitySystemComponent::GetCurrentStackCount(FGameplayAbilitySpecHandle Handle) const
{
	FActiveGameplayEffectHandle GEHandle = FindActiveGameplayEffectHandle(Handle);
	if (GEHandle.IsValid())
	{
		return GetCurrentStackCount(GEHandle);
	}
	return 0;
}

FString UAbilitySystemComponent::GetActiveGEDebugString(FActiveGameplayEffectHandle Handle) const
{
	FString Str;

	if (const FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Handle))
	{
		Str = FString::Printf(TEXT("%s - (Level: %.2f. Stacks: %d)"), *ActiveGE->Spec.Def->GetName(), ActiveGE->Spec.GetLevel(), ActiveGE->Spec.StackCount);
	}

	return Str;
}

FActiveGameplayEffectHandle UAbilitySystemComponent::FindActiveGameplayEffectHandle(FGameplayAbilitySpecHandle Handle) const
{
	for (const FActiveGameplayEffect& ActiveGE : &ActiveGameplayEffects)
	{
		for (const FGameplayAbilitySpecDef& AbilitySpecDef : ActiveGE.Spec.GrantedAbilitySpecs)
		{
			if (AbilitySpecDef.AssignedHandle == Handle)
			{
				return ActiveGE.Handle;
			}
		}
	}
	return FActiveGameplayEffectHandle();
}

void UAbilitySystemComponent::OnImmunityBlockGameplayEffect(const FGameplayEffectSpec& Spec, const FActiveGameplayEffect* ImmunityGE)
{
	OnImmunityBlockGameplayEffectDelegate.Broadcast(Spec, ImmunityGE);
}

void UAbilitySystemComponent::InitDefaultGameplayCueParameters(FGameplayCueParameters& Parameters)
{
	Parameters.Instigator = OwnerActor;
	Parameters.EffectCauser = AvatarActor;
}

bool UAbilitySystemComponent::IsReadyForGameplayCues()
{
	// check if the avatar actor is valid and ready to take gameplaycues
	AActor* ActorAvatar = nullptr;
	if (AbilityActorInfo.IsValid())
	{
		ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	}
	return ActorAvatar != nullptr;
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayEffectSpecForRPC &Spec, EGameplayCueEvent::Type EventType)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	if (ActorAvatar == nullptr || bSuppressGameplayCues)
	{
		// No avatar actor to call this gameplaycue on.
		return;
	}

	if (!Spec.Def)
	{
		ABILITY_LOG(Warning, TEXT("InvokeGameplayCueEvent Actor %s that has no gameplay effect!"), ActorAvatar ? *ActorAvatar->GetName() : TEXT("NULL"));
		return;
	}
	
	float ExecuteLevel = Spec.GetLevel();

	FGameplayCueParameters CueParameters(Spec);

	for (FGameplayEffectCue CueInfo : Spec.Def->GameplayCues)
	{
		if (CueInfo.MagnitudeAttribute.IsValid())
		{
			if (const FGameplayEffectModifiedAttribute* ModifiedAttribute = Spec.GetModifiedAttribute(CueInfo.MagnitudeAttribute))
			{
				CueParameters.RawMagnitude = ModifiedAttribute->TotalMagnitude;
			}
			else
			{
				CueParameters.RawMagnitude = 0.0f;
			}
		}
		else
		{
			CueParameters.RawMagnitude = 0.0f;
		}

		CueParameters.NormalizedMagnitude = CueInfo.NormalizeLevel(ExecuteLevel);

		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCues(ActorAvatar, CueInfo.GameplayCueTags, EventType, CueParameters);
	}
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayEffectContextHandle EffectContext)
{
	FGameplayCueParameters CueParameters(EffectContext);

	CueParameters.NormalizedMagnitude = 1.f;
	CueParameters.RawMagnitude = 0.f;

	InvokeGameplayCueEvent(GameplayCueTag, EventType, CueParameters);
}

void UAbilitySystemComponent::InvokeGameplayCueEvent(const FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& GameplayCueParameters)
{
	AActor* ActorAvatar = AbilityActorInfo->AvatarActor.Get();
	
	if (ActorAvatar != nullptr && !bSuppressGameplayCues)
	{
		UAbilitySystemGlobals::Get().GetGameplayCueManager()->HandleGameplayCue(ActorAvatar, GameplayCueTag, EventType, GameplayCueParameters);
	}
}

void UAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext)
{
	// Send to the wrapper on the cue manager
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted(this, GameplayCueTag, ScopedPredictionKey, EffectContext);
}

void UAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters)
{
	// Send to the wrapper on the cue manager
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueExecuted_WithParams(this, GameplayCueTag, ScopedPredictionKey, GameplayCueParameters);
}

void UAbilitySystemComponent::AddGameplayCue_Internal(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle& EffectContext, FActiveGameplayCueContainer& GameplayCueContainer)
{
	if (EffectContext.IsValid() == false)
	{
		EffectContext = MakeEffectContext();
	}

	FGameplayCueParameters Parameters(EffectContext);

	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingGameplayTag(GameplayCueTag);

		ForceReplication();
		GameplayCueContainer.AddCue(GameplayCueTag, ScopedPredictionKey, Parameters);
		
		// For mixed minimal replication mode, we do NOT want the owning client to play the OnActive event through this RPC, since he will get the full replicated 
		// GE in his AGE array. Generate a prediction key for him, which he will look for on the _Implementation function and ignore.
		{
			FPredictionKey PredictionKeyForRPC = ScopedPredictionKey;
			if (GameplayCueContainer.bMinimalReplication && (ReplicationMode == EReplicationMode::Mixed) && ScopedPredictionKey.IsValidKey() == false)
			{
				PredictionKeyForRPC = FPredictionKey::CreateNewServerInitiatedKey(this);
			}
			NetMulticast_InvokeGameplayCueAdded_WithParams(GameplayCueTag, PredictionKeyForRPC, Parameters);
		}

		if (!bWasInList)
		{
			// Call on server here, clients get it from repnotify
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, Parameters);
		}
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		GameplayCueContainer.PredictiveAdd(GameplayCueTag, ScopedPredictionKey);

		// Allow for predictive gameplaycue events? Needs more thought
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, Parameters);
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, Parameters);
	}
}

void UAbilitySystemComponent::RemoveGameplayCue_Internal(const FGameplayTag GameplayCueTag, FActiveGameplayCueContainer& GameplayCueContainer)
{
	if (IsOwnerActorAuthoritative())
	{
		bool bWasInList = HasMatchingGameplayTag(GameplayCueTag);

		GameplayCueContainer.RemoveCue(GameplayCueTag);

		if (bWasInList)
		{
			FGameplayCueParameters Parameters;
			InitDefaultGameplayCueParameters(Parameters);

			// Call on server here, clients get it from repnotify
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed, Parameters);
		}
		// Don't need to multicast broadcast this, ActiveGameplayCues replication handles it
	}
	else if (ScopedPredictionKey.IsLocalClientKey())
	{
		GameplayCueContainer.PredictiveRemove(GameplayCueTag);
	}
}

void UAbilitySystemComponent::RemoveAllGameplayCues()
{
	for (int32 i = (ActiveGameplayCues.GameplayCues.Num() - 1); i >= 0; --i)
	{
		RemoveGameplayCue(ActiveGameplayCues.GameplayCues[i].GameplayCueTag);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_FromSpec_Implementation(const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::Executed);
	}
}

// -----------

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCuesExecuted_Implementation(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FGameplayTag& GameplayCueTag : GameplayCueTags)
		{
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, EffectContext);
		}
	}
}

// -----------

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_WithParams_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, GameplayCueParameters);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCuesExecuted_WithParams_Implementation(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FGameplayTag& GameplayCueTag : GameplayCueTags)
		{
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Executed, GameplayCueParameters);
		}
	}
}

// -----------

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, EffectContext);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded_WithParams_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters Parameters)
{
	// If server generated prediction key and auto proxy, skip this message. 
	// This is an RPC from mixed replication mode code, we will get the "real" message from our OnRep on the autonomous proxy
	// See UAbilitySystemComponent::AddGameplayCue_Internal for more info.
	bool bIsMixedReplicationFromServer = (ReplicationMode == EReplicationMode::Mixed && PredictionKey.IsServerInitiatedKey() && AbilityActorInfo->IsLocallyControlledPlayer());

	if (IsOwnerActorAuthoritative() || (PredictionKey.IsLocalClientKey() == false && !bIsMixedReplicationFromServer))
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, Parameters);
	}
}

// -----------

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec_Implementation(const FGameplayEffectSpecForRPC& Spec, FPredictionKey PredictionKey)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::OnActive);
		InvokeGameplayCueEvent(Spec, EGameplayCueEvent::WhileActive);
	}
}

void UAbilitySystemComponent::NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams_Implementation(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, GameplayCueParameters);
		InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, GameplayCueParameters);
	}
}

	
void UAbilitySystemComponent::NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams_Implementation(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	if (IsOwnerActorAuthoritative() || PredictionKey.IsLocalClientKey() == false)
	{
		for (const FGameplayTag& GameplayCueTag : GameplayCueTags)
		{
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::OnActive, GameplayCueParameters);
			InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, GameplayCueParameters);
		}
	}
}

// ----------------------------------------------------------------------------------------

TArray<float> UAbilitySystemComponent::GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsTimeRemaining(Query);
}

TArray<TPair<float,float>> UAbilitySystemComponent::GetActiveEffectsTimeRemainingAndDuration(const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsTimeRemainingAndDuration(Query);
}

TArray<float> UAbilitySystemComponent::GetActiveEffectsDuration(const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.GetActiveEffectsDuration(Query);
}

TArray<FActiveGameplayEffectHandle> UAbilitySystemComponent::GetActiveEffects(const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.GetActiveEffects(Query);
}

int32 UAbilitySystemComponent::RemoveActiveEffectsWithTags(const FGameplayTagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(Tags));
	}
	return 0;
}

int32 UAbilitySystemComponent::RemoveActiveEffectsWithSourceTags(FGameplayTagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnySourceTags(Tags));
	}
	return 0;
}

int32 UAbilitySystemComponent::RemoveActiveEffectsWithAppliedTags(FGameplayTagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	}
	return 0;
}

int32 UAbilitySystemComponent::RemoveActiveEffectsWithGrantedTags(const FGameplayTagContainer Tags)
{
	if (IsOwnerActorAuthoritative())
	{
		return RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
	}
	return 0;
}

int32 UAbilitySystemComponent::RemoveActiveEffects(const FGameplayEffectQuery& Query, int32 StacksToRemove)
{
	if (IsOwnerActorAuthoritative())
	{
		return ActiveGameplayEffects.RemoveActiveEffects(Query, StacksToRemove);
	}

	return 0;
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{	
	DOREPLIFETIME(UAbilitySystemComponent, SpawnedAttributes);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayEffects);
	DOREPLIFETIME(UAbilitySystemComponent, ActiveGameplayCues);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ActivatableAbilities, COND_ReplayOrOwner);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, BlockedAbilityBindings, COND_OwnerOnly);

	DOREPLIFETIME(UAbilitySystemComponent, OwnerActor);
	DOREPLIFETIME(UAbilitySystemComponent, AvatarActor);

	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ReplicatedPredictionKeyMap, COND_OwnerOnly);
	DOREPLIFETIME(UAbilitySystemComponent, RepAnimMontageInfo);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, MinimalReplicationGameplayCues, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, MinimalReplicationTags, COND_SkipOwner);
	
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ClientDebugStrings, COND_ReplayOnly);
	DOREPLIFETIME_CONDITION(UAbilitySystemComponent, ServerDebugStrings, COND_ReplayOnly);
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void UAbilitySystemComponent::ForceReplication()
{
	AActor *OwningActor = GetOwner();
	if (OwningActor && OwningActor->Role == ROLE_Authority)
	{
		OwningActor->ForceNetUpdate();
	}
}

void UAbilitySystemComponent::ForceAvatarReplication()
{
	if (AvatarActor && AvatarActor->Role == ROLE_Authority)
	{
		AvatarActor->ForceNetUpdate();
	}
}

bool UAbilitySystemComponent::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set)
		{
			WroteSomething |= Channel->ReplicateSubobject(const_cast<UAttributeSet*>(Set), *Bunch, *RepFlags);
		}
	}

	for (UGameplayAbility* Ability : AllReplicatedInstancedAbilities)
	{
		if (Ability && !Ability->IsPendingKill())
		{
			WroteSomething |= Channel->ReplicateSubobject(Ability, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

void UAbilitySystemComponent::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& Objs)
{
	for (const UAttributeSet* Set : SpawnedAttributes)
	{
		if (Set && Set->IsNameStableForNetworking())
		{
			Objs.Add(const_cast<UAttributeSet*>(Set));
		}
	}
}

void UAbilitySystemComponent::PreNetReceive()
{
	// Update the cached IsNetSimulated value here if this component is still considered authority.
	// Even though the value is also cached in OnRegister and BeginPlay, clients may
	// receive properties before OnBeginPlay, so this ensures the role is correct
	// for that case.
	if (!CachedIsNetSimulated)
	{
		CacheIsNetSimulated();
	}
	ActiveGameplayEffects.IncrementLock();
}
	
void UAbilitySystemComponent::PostNetReceive()
{
	ActiveGameplayEffects.DecrementLock();
}

bool UAbilitySystemComponent::HasAuthorityOrPredictionKey(const FGameplayAbilityActivationInfo* ActivationInfo) const
{
	return ((ActivationInfo->ActivationMode == EGameplayAbilityActivationMode::Authority) || CanPredict());
}

void UAbilitySystemComponent::SetReplicationMode(EReplicationMode NewReplicationMode)
{
	ReplicationMode = NewReplicationMode;
}

void UAbilitySystemComponent::OnPredictiveGameplayCueCatchup(FGameplayTag Tag)
{
	// Remove it
	RemoveOneTagCount_NoReturn(Tag);

	if (HasMatchingGameplayTag(Tag) == 0)
	{
		// Invoke Removed event if we no longer have this tag (probably a mispredict)
		InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Removed);
	}
}

void UAbilitySystemComponent::ReinvokeActiveGameplayCues()
{
	for (const FActiveGameplayEffect& Effect : &ActiveGameplayEffects)
	{
		if (Effect.bIsInhibited == false)
		{
			InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::WhileActive);
		}
	}
}

// ---------------------------------------------------------------------------------------

void UAbilitySystemComponent::PrintAllGameplayEffects() const
{
	ABILITY_LOG(Log, TEXT("Owner: %s. Avatar: %s"), *GetOwner()->GetName(), *AbilityActorInfo->AvatarActor->GetName());
	ActiveGameplayEffects.PrintAllGameplayEffects();
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::OnAttributeAggregatorDirty(FAggregator* Aggregator, FGameplayAttribute Attribute, bool bFromRecursiveCall)
{
	ActiveGameplayEffects.OnAttributeAggregatorDirty(Aggregator, Attribute, bFromRecursiveCall);
}

void UAbilitySystemComponent::OnMagnitudeDependencyChange(FActiveGameplayEffectHandle Handle, const FAggregator* ChangedAggregator)
{
	ActiveGameplayEffects.OnMagnitudeDependencyChange(Handle, ChangedAggregator);
}

void UAbilitySystemComponent::OnGameplayEffectDurationChange(struct FActiveGameplayEffect& ActiveEffect)
{

}

void UAbilitySystemComponent::OnGameplayEffectAppliedToTarget(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	OnGameplayEffectAppliedDelegateToTarget.Broadcast(Target, SpecApplied, ActiveHandle);
	ActiveGameplayEffects.ApplyStackingLogicPostApplyAsSource(Target, SpecApplied, ActiveHandle);
}

void UAbilitySystemComponent::OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	OnGameplayEffectAppliedDelegateToSelf.Broadcast(Source, SpecApplied, ActiveHandle);
}

void UAbilitySystemComponent::OnPeriodicGameplayEffectExecuteOnTarget(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecExecuted, FActiveGameplayEffectHandle ActiveHandle)
{
	OnPeriodicGameplayEffectExecuteDelegateOnTarget.Broadcast(Target, SpecExecuted, ActiveHandle);
}

void UAbilitySystemComponent::OnPeriodicGameplayEffectExecuteOnSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& SpecExecuted, FActiveGameplayEffectHandle ActiveHandle)
{
	OnPeriodicGameplayEffectExecuteDelegateOnSelf.Broadcast(Source, SpecExecuted, ActiveHandle);
}

TArray<UGameplayTask*>&	UAbilitySystemComponent::GetAbilityActiveTasks(UGameplayAbility* Ability)
{
	return Ability->ActiveTasks;
}

AActor* UAbilitySystemComponent::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	check(AbilityActorInfo.IsValid());
	return AbilityActorInfo->AvatarActor.Get();
}

AActor* UAbilitySystemComponent::GetAvatarActor() const
{
	check(AbilityActorInfo.IsValid());
	return AbilityActorInfo->AvatarActor.Get();
}

void UAbilitySystemComponent::HandleDeferredGameplayCues(const FActiveGameplayEffectsContainer* GameplayEffectsContainer)
{
	for (const FActiveGameplayEffect& Effect : GameplayEffectsContainer)
	{
		if (Effect.bIsInhibited == false)
		{
			if (Effect.bPendingRepOnActiveGC)
			{
				InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::OnActive);
			}
			if (Effect.bPendingRepWhileActiveGC)
			{
				InvokeGameplayCueEvent(Effect.Spec, EGameplayCueEvent::WhileActive);
			}
		}

		Effect.bPendingRepOnActiveGC = false;
		Effect.bPendingRepWhileActiveGC = false;
	}
}

void UAbilitySystemComponent::DebugCyclicAggregatorBroadcasts(FAggregator* Aggregator)
{
	ActiveGameplayEffects.DebugCyclicAggregatorBroadcasts(Aggregator);
}

// ------------------------------------------------------------------------

void UAbilitySystemComponent::OnRep_ClientDebugString()
{
	ABILITY_LOG(Display, TEXT(" "));
	ABILITY_LOG(Display, TEXT("Received Client AbilitySystem Debug information: (%d lines)"), ClientDebugStrings.Num());
	for (FString& Str : ClientDebugStrings)
	{
		ABILITY_LOG(Display, TEXT("%s"), *Str);
	}
}
void UAbilitySystemComponent::OnRep_ServerDebugString()
{
	ABILITY_LOG(Display, TEXT(" "));
	ABILITY_LOG(Display, TEXT("Server AbilitySystem Debug information: (%d lines)"), ClientDebugStrings.Num());
	for (FString& Str : ServerDebugStrings)
	{
		ABILITY_LOG(Display, TEXT("%s"), *Str);
	}
}

float UAbilitySystemComponent::GetFilteredAttributeValue(const FGameplayAttribute& Attribute, const FGameplayTagRequirements& SourceTags, const FGameplayTagContainer& TargetTags)
{
	float AttributeValue = 0.f;

	if (SourceTags.RequireTags.Num() == 0 && SourceTags.IgnoreTags.Num() == 0)
	{
		// No qualifiers so we can just read this attribute normally
		AttributeValue = GetNumericAttribute(Attribute);
	}
	else
	{
		// Need to capture qualified attributes
		FGameplayEffectAttributeCaptureDefinition CaptureDef(Attribute.GetUProperty(), EGameplayEffectAttributeCaptureSource::Source, false);
		FGameplayEffectAttributeCaptureSpec CaptureSpec(CaptureDef);

		CaptureAttributeForGameplayEffect(CaptureSpec);

		// Source Tags
		static FGameplayTagContainer QuerySourceTags;
		QuerySourceTags.Reset();

		GetOwnedGameplayTags(QuerySourceTags);
		QuerySourceTags.AppendTags(SourceTags.RequireTags);

		// Target Tags
		static FGameplayTagContainer QueryTargetTags;
		QueryTargetTags.Reset();

		QueryTargetTags.AppendTags(TargetTags);

		FAggregatorEvaluateParameters Params;
		Params.SourceTags = &QuerySourceTags;
		Params.TargetTags = &QueryTargetTags;
		Params.IncludePredictiveMods = true;

		if (CaptureSpec.AttemptCalculateAttributeMagnitude(Params, AttributeValue) == false)
		{
			UE_LOG(LogAbilitySystemComponent, Warning, TEXT("Failed to calculate Attribute %s. On: %s"), *Attribute.GetName(), *GetFullName());
		}
	}

	return AttributeValue;
}

bool UAbilitySystemComponent::ServerPrintDebug_RequestWithStrings_Validate(const TArray<FString>& Strings)
{
	return true;
}

void UAbilitySystemComponent::ServerPrintDebug_RequestWithStrings_Implementation(const TArray<FString>& Strings)
{
	ABILITY_LOG(Display, TEXT(" "));
	ABILITY_LOG(Display, TEXT("Received Client AbilitySystem Debug information: "));
	for (const FString& Str : Strings)
	{
		ABILITY_LOG(Display, TEXT("%s"), *Str);
	}

	ClientDebugStrings = Strings;
	ServerPrintDebug_Request_Implementation();
}

bool UAbilitySystemComponent::ServerPrintDebug_Request_Validate()
{
	return true;
}

void UAbilitySystemComponent::ServerPrintDebug_Request_Implementation()
{
	OnServerPrintDebug_Request();

	FAbilitySystemComponentDebugInfo DebugInfo;
	DebugInfo.bShowAbilities = true;
	DebugInfo.bShowAttributes = true;
	DebugInfo.bShowGameplayEffects = true;
	DebugInfo.Accumulate = true;
	DebugInfo.bPrintToLog = true;

	Debug_Internal(DebugInfo);

	ServerDebugStrings = DebugInfo.Strings;

	ClientPrintDebug_Response(DebugInfo.Strings, DebugInfo.GameFlags);
}

void UAbilitySystemComponent::OnServerPrintDebug_Request()
{

}

void UAbilitySystemComponent::ClientPrintDebug_Response_Implementation(const TArray<FString>& Strings, int32 GameFlags)
{
	OnClientPrintDebug_Response(Strings, GameFlags);
}
void UAbilitySystemComponent::OnClientPrintDebug_Response(const TArray<FString>& Strings, int32 GameFlags)
{
	ABILITY_LOG(Display, TEXT(" "));
	ABILITY_LOG(Display, TEXT("Server State: "));
	for (const FString& Str : Strings)
	{
		ABILITY_LOG(Display, TEXT("%s"), *Str);
	}


	// Now that we've heard back from server, append his strings and broadcast the delegate
	UAbilitySystemGlobals::Get().AbilitySystemDebugStrings.Append(Strings);
	UAbilitySystemGlobals::Get().OnClientServerDebugAvailable.Broadcast();
	UAbilitySystemGlobals::Get().AbilitySystemDebugStrings.Reset(); // we are done with this now. Clear it to signal that this can be ran again
}

FString UAbilitySystemComponent::CleanupName(FString Str)
{
	Str.RemoveFromStart(TEXT("Default__"));
	Str.RemoveFromEnd(TEXT("_c"));
	return Str;
}

void UAbilitySystemComponent::AccumulateScreenPos(FAbilitySystemComponentDebugInfo& Info)
{
	const float ColumnWidth = Info.Canvas ? Info.Canvas->ClipX * 0.4f : 0.f;

	float NewY = Info.YPos + Info.YL;
	if (NewY > Info.MaxY)
	{
		// Need new column, reset Y to original height
		NewY = Info.NewColumnYPadding;
		Info.XPos += ColumnWidth;
	}
	Info.YPos = NewY;
}

void UAbilitySystemComponent::DebugLine(FAbilitySystemComponentDebugInfo& Info, FString Str, float XOffset, float YOffset)
{
	if (Info.Canvas)
	{
		Info.YL = Info.Canvas->DrawText(GEngine->GetTinyFont(), Str, Info.XPos + XOffset, Info.YPos );
		AccumulateScreenPos(Info);
	}

	if (Info.bPrintToLog)
	{
		FString LogStr;
		for (int32 i=0; i < (int32)XOffset; ++i)
		{
			LogStr += TEXT(" ");
		}
		LogStr += Str;
		ABILITY_LOG(Warning, TEXT("%s"), *LogStr);
	}

	if (Info.Accumulate)
	{
		FString LogStr;
		for (int32 i=0; i < (int32)XOffset; ++i)
		{
			LogStr += TEXT(" ");
		}
		LogStr += Str;
		Info.Strings.Add(Str);
	}
}

struct FASCDebugTargetInfo
{
	FASCDebugTargetInfo()
	{
		DebugCategoryIndex = 0;
		DebugCategories.Add(TEXT("Attributes"));
		DebugCategories.Add(TEXT("GameplayEffects"));
		DebugCategories.Add(TEXT("Ability"));
	}

	TArray<FName> DebugCategories;
	int32 DebugCategoryIndex;

	TWeakObjectPtr<UWorld>	TargetWorld;
	TWeakObjectPtr<UAbilitySystemComponent>	LastDebugTarget;
};

TArray<FASCDebugTargetInfo>	AbilitySystemDebugInfoList;

FASCDebugTargetInfo* GetDebugTargetInfo(UWorld* World)
{
	FASCDebugTargetInfo* TargetInfo = nullptr;
	for (FASCDebugTargetInfo& Info : AbilitySystemDebugInfoList )
	{
		if (Info.TargetWorld.Get() == World)
		{
			TargetInfo = &Info;
			break;
		}
	}
	if (TargetInfo == nullptr)
	{
		TargetInfo = &AbilitySystemDebugInfoList[AbilitySystemDebugInfoList.AddDefaulted()];
		TargetInfo->TargetWorld = World;
	}
	return TargetInfo;
}

static void CycleDebugCategory(UWorld* InWorld)
{
	FASCDebugTargetInfo* TargetInfo = GetDebugTargetInfo(InWorld);
	TargetInfo->DebugCategoryIndex = (TargetInfo->DebugCategoryIndex+1) % TargetInfo->DebugCategories.Num();
}

UAbilitySystemComponent* GetDebugTarget(FASCDebugTargetInfo* Info)
{
	// Return target if we already have one
	if (UAbilitySystemComponent* ASC = Info->LastDebugTarget.Get())
	{
		return ASC;
	}

	// Find one
	for (TObjectIterator<UAbilitySystemComponent> It; It; ++It)
	{
		if (UAbilitySystemComponent* ASC = *It)
		{
			// Make use it belongs to our world and will be valid in a TWeakObjPtr (e.g.  not pending kill)
			if (ASC->GetWorld() == Info->TargetWorld.Get() && TWeakObjectPtr<UAbilitySystemComponent>(ASC).Get())
			{
				Info->LastDebugTarget = ASC;
				if (ASC->AbilityActorInfo->IsLocallyControlledPlayer())
				{
					// Default to local player first
					break;
				}
			}
		}
	}

	return Info->LastDebugTarget.Get();
}

void CycleDebugTarget(FASCDebugTargetInfo* TargetInfo, bool Next)
{
	GetDebugTarget(TargetInfo);

	// Build a list	of ASCs
	TArray<UAbilitySystemComponent*> List;
	for (TObjectIterator<UAbilitySystemComponent> It; It; ++It)
	{
		if (UAbilitySystemComponent* ASC = *It)
		{
			if (ASC->GetWorld() == TargetInfo->TargetWorld.Get())
			{
				List.Add(ASC);
			}
		}
	}

	// Search through list to find prev/next target
	UAbilitySystemComponent* Previous = nullptr;
	for (int32 idx=0; idx < List.Num() + 1; ++idx)
	{
		UAbilitySystemComponent* ASC = List[idx % List.Num()];

		if (Next && Previous == TargetInfo->LastDebugTarget.Get())
		{
			TargetInfo->LastDebugTarget = ASC;
			return;
		}
		if (!Next && ASC == TargetInfo->LastDebugTarget.Get())
		{
			TargetInfo->LastDebugTarget = Previous;
			return;
		}

		Previous = ASC;
	}
}

static void	AbilitySystemCycleDebugTarget(UWorld* InWorld, bool Next)
{
	CycleDebugTarget( GetDebugTargetInfo(InWorld), Next );
}

FAutoConsoleCommandWithWorld AbilitySystemNextDebugTargetCmd(
	TEXT("AbilitySystem.Debug.NextTarget"),
	TEXT("Targets next AbilitySystemComponent in ShowDebug AbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(AbilitySystemCycleDebugTarget, true)
	);

FAutoConsoleCommandWithWorld AbilitySystemPrevDebugTargetCmd(
	TEXT("AbilitySystem.Debug.PrevTarget"),
	TEXT("Targets previous AbilitySystemComponent in ShowDebug AbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(AbilitySystemCycleDebugTarget, false)
	);

static void	AbilitySystemDebugNextCategory(UWorld* InWorld, bool Next)
{
	CycleDebugTarget( GetDebugTargetInfo(InWorld), Next );
}

FAutoConsoleCommandWithWorld AbilitySystemDebugNextCategoryCmd(
	TEXT("AbilitySystem.Debug.NextCategory"),
	TEXT("Targets previous AbilitySystemComponent in ShowDebug AbilitySystem"),
	FConsoleCommandWithWorldDelegate::CreateStatic(CycleDebugCategory)
	);

void UAbilitySystemComponent::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	if (DisplayInfo.IsDisplayOn(TEXT("AbilitySystem")))
	{
		UWorld* World = HUD->GetWorld();
		FASCDebugTargetInfo* TargetInfo = GetDebugTargetInfo(World);
	
		if (UAbilitySystemComponent* ASC = GetDebugTarget(TargetInfo))
		{
			TArray<FName> LocalDisplayNames;
			LocalDisplayNames.Add( TargetInfo->DebugCategories[ TargetInfo->DebugCategoryIndex ] );

			FDebugDisplayInfo LocalDisplayInfo( LocalDisplayNames, TArray<FName>() );

			ASC->DisplayDebug(Canvas, LocalDisplayInfo, YL, YPos);
		}
	}
}

void UAbilitySystemComponent::DisplayDebug(class UCanvas* Canvas, const class FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FAbilitySystemComponentDebugInfo DebugInfo;

	if (DebugDisplay.IsDisplayOn(FName(TEXT("Attributes"))))
	{
		DebugInfo.bShowAbilities = false;
		DebugInfo.bShowAttributes = true;
		DebugInfo.bShowGameplayEffects = false;
	}
	if (DebugDisplay.IsDisplayOn(FName(TEXT("Ability"))))
	{
		DebugInfo.bShowAbilities = true;
		DebugInfo.bShowAttributes = false;
		DebugInfo.bShowGameplayEffects = false;
	}
	else if (DebugDisplay.IsDisplayOn(FName(TEXT("GameplayEffects"))))
	{
		DebugInfo.bShowAbilities = false;
		DebugInfo.bShowAttributes = false;
		DebugInfo.bShowGameplayEffects = true;
	}

	DebugInfo.bPrintToLog = false;
	DebugInfo.Canvas = Canvas;
	DebugInfo.XPos = 0.f;
	DebugInfo.YPos = YPos;
	DebugInfo.OriginalX = 0.f;
	DebugInfo.OriginalY = YPos;
	DebugInfo.MaxY = Canvas->ClipY - 150.f; // Give some padding for any non-columnizing debug output following this output
	DebugInfo.NewColumnYPadding = 30.f;

	Debug_Internal(DebugInfo);

	YPos = DebugInfo.YPos;
	YL = DebugInfo.YL;
}

bool UAbilitySystemComponent::ShouldSendClientDebugStringsToServer() const
{
	// This implements basic throttling so that debug strings can't be sent more than once a second to the server
	const double MinTimeBetweenClientDebugSends = 1.f;
	static double LastSendTime = 0.f;

	double CurrentTime = FPlatformTime::Seconds();
	bool ShouldSend = (CurrentTime - LastSendTime) > MinTimeBetweenClientDebugSends;
	if (ShouldSend)
	{
		LastSendTime = CurrentTime;
	}
	return ShouldSend;
}

void UAbilitySystemComponent::PrintDebug()
{
	FAbilitySystemComponentDebugInfo DebugInfo;
	DebugInfo.bShowAbilities = true;
	DebugInfo.bShowAttributes = true;
	DebugInfo.bShowGameplayEffects = true;
	DebugInfo.bPrintToLog = true;
	DebugInfo.Accumulate = true;

	Debug_Internal(DebugInfo);

	// Store our local strings in the global debug array. Wait for server to respond with his.
	if (UAbilitySystemGlobals::Get().AbilitySystemDebugStrings.Num() > 0)
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemComponent::PrintDebug called while AbilitySystemDebugStrings was not empty. Still waiting for server response from a previous call?"));
	}
		
	UAbilitySystemGlobals::Get().AbilitySystemDebugStrings = DebugInfo.Strings;

	if (IsOwnerActorAuthoritative() == false)
	{
		// See what the server thinks
		if (ShouldSendClientDebugStringsToServer())
		{
			ServerPrintDebug_RequestWithStrings(DebugInfo.Strings);
		}
		else
		{
			ServerPrintDebug_Request();
		}
	}
	else
	{
		UAbilitySystemGlobals::Get().OnClientServerDebugAvailable.Broadcast();
		UAbilitySystemGlobals::Get().AbilitySystemDebugStrings.Reset();
	}
}

void UAbilitySystemComponent::Debug_Internal(FAbilitySystemComponentDebugInfo& Info)
{
	// Draw title at top of screen (default HUD debug text starts at 50 ypos, we can position this on top)*
	//   *until someone changes it unknowingly
	{
		FString DebugTitle("");
		// Category
		if (Info.bShowAbilities) DebugTitle += TEXT("ABILITIES ");
		if (Info.bShowAttributes) DebugTitle += TEXT("ATTRIBUTES ");
		if (Info.bShowGameplayEffects) DebugTitle += TEXT("GAMEPLAYEFFECTS ");
		// Avatar info
		if (AvatarActor)
		{
			DebugTitle += FString::Printf(TEXT("for avatar %s "), *AvatarActor->GetName());
			if (AvatarActor->Role == ROLE_AutonomousProxy) DebugTitle += TEXT("(local player) ");
			else if (AvatarActor->Role == ROLE_SimulatedProxy) DebugTitle += TEXT("(simulated) ");
			else if (AvatarActor->Role == ROLE_Authority) DebugTitle += TEXT("(authority) ");
		}
		// Owner info
		if (OwnerActor && OwnerActor != AvatarActor)
		{
			DebugTitle += FString::Printf(TEXT("for owner %s "), *OwnerActor->GetName());
			if (OwnerActor->Role == ROLE_AutonomousProxy) DebugTitle += TEXT("(autonomous) ");
			else if (OwnerActor->Role == ROLE_SimulatedProxy) DebugTitle += TEXT("(simulated) ");
			else if (OwnerActor->Role == ROLE_Authority) DebugTitle += TEXT("(authority) ");
		}

		if (Info.Canvas)
		{
			Info.Canvas->SetDrawColor(FColor::White);
			Info.Canvas->DrawText(GEngine->GetLargeFont(), DebugTitle, Info.XPos + 4.f, 10.f, 1.5f, 1.5f);
		}
		else
		{
			DebugLine(Info, DebugTitle, 0.f, 0.f);
		}
	}

	FGameplayTagContainer OwnerTags;
	GetOwnedGameplayTags(OwnerTags);

	if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);

	DebugLine(Info, FString::Printf(TEXT("Owned Tags: %s"), *OwnerTags.ToStringSimple()), 4.f, 0.f);

	if (BlockedAbilityTags.GetExplicitGameplayTags().Num() > 0)
	{
		DebugLine(Info, FString::Printf(TEXT("BlockedAbilityTags: %s"), *BlockedAbilityTags.GetExplicitGameplayTags().ToStringSimple()), 4.f, 0.f);
		
	}

	TSet<FGameplayAttribute> DrawAttributes;

	float MaxCharHeight = 10;
	if (GetOwner()->GetNetMode() != NM_DedicatedServer)
	{
		MaxCharHeight = GEngine->GetTinyFont()->GetMaxCharHeight();
	}

	// -------------------------------------------------------------

	if (Info.bShowAttributes)
	{
		// Draw the attribute aggregator map.
		for (auto It = ActiveGameplayEffects.AttributeAggregatorMap.CreateConstIterator(); It; ++It)
		{
			FGameplayAttribute Attribute = It.Key();
			const FAggregatorRef& AggregatorRef = It.Value();
			if (AggregatorRef.Get())
			{
				FAggregator& Aggregator = *AggregatorRef.Get();

				TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*> ModMap;
				Aggregator.GetAllAggregatorMods(ModMap);

				if (ModMap.Num() == 0)
				{
					continue;
				}

				float FinalValue = GetNumericAttribute(Attribute);
				float BaseValue = Aggregator.GetBaseValue();

				FString AttributeString = FString::Printf(TEXT("%s %.2f "), *Attribute.GetName(), GetNumericAttribute(Attribute));
				if (FMath::Abs<float>(BaseValue - FinalValue) > SMALL_NUMBER)
				{
					AttributeString += FString::Printf(TEXT(" (Base: %.2f)"), BaseValue);
				}

				if (Info.Canvas)
				{
					Info.Canvas->SetDrawColor(FColor::White);
				}

				DebugLine(Info, AttributeString, 4.f, 0.f);

				DrawAttributes.Add(Attribute);

 				for (const auto& CurMapElement : ModMap)
 				{
					const EGameplayModEvaluationChannel Channel = CurMapElement.Key;
					const TArray<FAggregatorMod>* ModArrays = CurMapElement.Value;

					const FString ChannelNameString = UAbilitySystemGlobals::Get().GetGameplayModEvaluationChannelAlias(Channel).ToString();
					for (int32 ModOpIdx = 0; ModOpIdx < EGameplayModOp::Max; ++ModOpIdx)
					{
						const TArray<FAggregatorMod>& CurModArray = ModArrays[ModOpIdx];
						for (const FAggregatorMod& Mod : CurModArray)
						{
							FAggregatorEvaluateParameters EmptyParams;
							bool IsActivelyModifyingAttribute = Mod.Qualifies(EmptyParams);
							if (Info.Canvas)
							{
								Info.Canvas->SetDrawColor(IsActivelyModifyingAttribute ? FColor::Yellow : FColor(128, 128, 128));
							}

							FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(Mod.ActiveHandle);
							FString SrcName = ActiveGE ? ActiveGE->Spec.Def->GetName() : FString(TEXT(""));

							if (IsActivelyModifyingAttribute == false)
							{
								if (Mod.SourceTagReqs)
								{
									SrcName += FString::Printf(TEXT(" SourceTags: [%s] "), *Mod.SourceTagReqs->ToString());
								}
								if (Mod.TargetTagReqs)
								{
									SrcName += FString::Printf(TEXT("TargetTags: [%s]"), *Mod.TargetTagReqs->ToString());
								}
							}

							DebugLine(Info, FString::Printf(TEXT("   %s %s\t %.2f - %s"), *ChannelNameString, *EGameplayModOpToString(ModOpIdx), Mod.EvaluatedMagnitude, *SrcName), 7.f, 0.f);
							Info.NewColumnYPadding = FMath::Max<float>(Info.NewColumnYPadding, Info.YPos + Info.YL);
						}
					}
 				}

				AccumulateScreenPos(Info);
			}
		}
	}

	// -------------------------------------------------------------

	if (Info.bShowGameplayEffects)
	{
		for (FActiveGameplayEffect& ActiveGE : &ActiveGameplayEffects)
		{

			if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);

			FString DurationStr = TEXT("Infinite Duration ");
			if (ActiveGE.GetDuration() > 0.f)
			{
				DurationStr = FString::Printf(TEXT("Duration: %.2f. Remaining: %.2f (Start: %.2f / %.2f / %.2f) %s "), ActiveGE.GetDuration(), ActiveGE.GetTimeRemaining(GetWorld()->GetTimeSeconds()), ActiveGE.StartServerWorldTime, ActiveGE.CachedStartServerWorldTime, ActiveGE.StartWorldTime, ActiveGE.DurationHandle.IsValid() ? TEXT("Valid Handle") : TEXT("INVALID Handle") );
				if (ActiveGE.DurationHandle.IsValid())
				{
					DurationStr += FString::Printf(TEXT("(Local Duration: %.2f)"), GetWorld()->GetTimerManager().GetTimerRemaining(ActiveGE.DurationHandle));
				}
			}
			if (ActiveGE.GetPeriod() > 0.f)
			{
				DurationStr += FString::Printf(TEXT("Period: %.2f"), ActiveGE.GetPeriod());
			}

			FString StackString;
			if (ActiveGE.Spec.StackCount > 1)
			{

				if (ActiveGE.Spec.Def->StackingType == EGameplayEffectStackingType::AggregateBySource)
				{
					StackString = FString::Printf(TEXT("(Stacks: %d. From: %s) "), ActiveGE.Spec.StackCount, *GetNameSafe(ActiveGE.Spec.GetContext().GetInstigatorAbilitySystemComponent()->AvatarActor));
				}
				else
				{
					StackString = FString::Printf(TEXT("(Stacks: %d) "), ActiveGE.Spec.StackCount);
				}
			}

			FString LevelString;
			if (ActiveGE.Spec.GetLevel() > 1.f)
			{
				LevelString = FString::Printf(TEXT("Level: %.2f"), ActiveGE.Spec.GetLevel());
			}

			FString PredictionString;
			if (ActiveGE.PredictionKey.IsValidKey())
			{
				if (ActiveGE.PredictionKey.WasLocallyGenerated() )
				{
					PredictionString = FString::Printf(TEXT("(Predicted and Waiting)"));
				}
				else
				{
					PredictionString = FString::Printf(TEXT("(Predicted and Caught Up)"));
				}
			}

			if (Info.Canvas) Info.Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128) : FColor::White);

			DebugLine(Info, FString::Printf(TEXT("%s %s %s %s %s"), *CleanupName(GetNameSafe(ActiveGE.Spec.Def)), *DurationStr, *StackString, *LevelString, *PredictionString), 4.f, 0.f);

			FGameplayTagContainer GrantedTags;
			ActiveGE.Spec.GetAllGrantedTags(GrantedTags);
			if (GrantedTags.Num() > 0)
			{
				DebugLine(Info, FString::Printf(TEXT("Granted Tags: %s"), *GrantedTags.ToStringSimple()), 7.f, 0.f);
			}

			for (int32 ModIdx = 0; ModIdx < ActiveGE.Spec.Modifiers.Num(); ++ModIdx)
			{
				if (ActiveGE.Spec.Def == nullptr)
				{
					DebugLine(Info, FString::Printf(TEXT("null def! (Backwards compat?)")), 7.f, 0.f);
					continue;
				}

				const FModifierSpec& ModSpec = ActiveGE.Spec.Modifiers[ModIdx];
				const FGameplayModifierInfo& ModInfo = ActiveGE.Spec.Def->Modifiers[ModIdx];

				// Do a quick Qualifies() check to see if this mod is active.
				FAggregatorMod TempMod;
				TempMod.SourceTagReqs = &ModInfo.SourceTags;
				TempMod.TargetTagReqs = &ModInfo.TargetTags;
				TempMod.IsPredicted = false;

				FAggregatorEvaluateParameters EmptyParams;
				bool IsActivelyModifyingAttribute = TempMod.Qualifies(EmptyParams);

				if (IsActivelyModifyingAttribute == false)
				{
					if (Info.Canvas) Info.Canvas->SetDrawColor(FColor(128, 128, 128));
				}

				DebugLine(Info, FString::Printf(TEXT("Mod: %s. %s. %.2f"), *ModInfo.Attribute.GetName(), *EGameplayModOpToString(ModInfo.ModifierOp), ModSpec.GetEvaluatedMagnitude()), 7.f, 0.f);

				if (Info.Canvas) Info.Canvas->SetDrawColor(ActiveGE.bIsInhibited ? FColor(128, 128, 128) : FColor::White);
			}

			AccumulateScreenPos(Info);
		}
	}

	// -------------------------------------------------------------

	if (Info.bShowAttributes)
	{
		if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);
		for (UAttributeSet* Set : SpawnedAttributes)
		{
			for (TFieldIterator<UProperty> It(Set->GetClass()); It; ++It)
			{
				FGameplayAttribute	Attribute(*It);

				if (DrawAttributes.Contains(Attribute))
					continue;

				if (Attribute.IsValid())
				{
					float Value = GetNumericAttribute(Attribute);

					DebugLine(Info, FString::Printf(TEXT("%s %.2f"), *Attribute.GetName(), Value), 4.f, 0.f);
				}
			}
		}
		AccumulateScreenPos(Info);
	}

	// -------------------------------------------------------------

	bool bShowAbilityTaskDebugMessages = true;

	if (Info.bShowAbilities)
	{
		for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (AbilitySpec.Ability == nullptr)
				continue;

			FString StatusText;
			FColor AbilityTextColor = FColor(128, 128, 128);
			FGameplayTagContainer FailureTags;

			if (AbilitySpec.IsActive())
			{
				StatusText = FString::Printf(TEXT(" (Active %d)"), AbilitySpec.ActiveCount);
				AbilityTextColor = FColor::Yellow;
			}
			else if (BlockedAbilityBindings.IsValidIndex(AbilitySpec.InputID) && BlockedAbilityBindings[AbilitySpec.InputID])
			{
				StatusText = TEXT(" (InputBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->AbilityTags.HasAny(BlockedAbilityTags.GetExplicitGameplayTags()))
			{
				StatusText = TEXT(" (TagBlocked)");
				AbilityTextColor = FColor::Red;
			}
			else if (AbilitySpec.Ability->CanActivateAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), nullptr, nullptr, &FailureTags) == false)
			{
				StatusText = FString::Printf(TEXT(" (CantActivate %s)"), *FailureTags.ToString());
				AbilityTextColor = FColor::Red;

				float Cooldown =  AbilitySpec.Ability->GetCooldownTimeRemaining(AbilityActorInfo.Get());
				if (Cooldown > 0.f)
				{
					StatusText += FString::Printf(TEXT("   Cooldown: %.2f\n"), Cooldown);
				}
			}

			FString InputPressedStr = AbilitySpec.InputPressed ? TEXT("(InputPressed)") : TEXT("");
			FString ActivationModeStr = AbilitySpec.IsActive() ? UEnum::GetValueAsString(TEXT("GameplayAbilities.EGameplayAbilityActivationMode"), AbilitySpec.ActivationInfo.ActivationMode) : TEXT("");

			if (Info.Canvas) Info.Canvas->SetDrawColor(AbilityTextColor);

			DebugLine(Info, FString::Printf(TEXT("%s %s %s %s"), *CleanupName(GetNameSafe(AbilitySpec.Ability)), *StatusText, *InputPressedStr, *ActivationModeStr), 4.f, 0.f);

			if (AbilitySpec.IsActive())
			{
				TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
				for (int32 InstanceIdx = 0; InstanceIdx < Instances.Num(); ++InstanceIdx)
				{
					UGameplayAbility* Instance = Instances[InstanceIdx];
					if (!Instance)
						continue;

					if (Info.Canvas) Info.Canvas->SetDrawColor(FColor::White);
					for (UGameplayTask* Task : Instance->ActiveTasks)
					{
						if (Task)
						{
							DebugLine(Info, FString::Printf(TEXT("%s"), *Task->GetDebugString()), 7.f, 0.f);

							if (bShowAbilityTaskDebugMessages)
							{
								for (FAbilityTaskDebugMessage& Msg : Instance->TaskDebugMessages)
								{
									if (Msg.FromTask == Task)
									{
										DebugLine(Info, FString::Printf(TEXT("%s"), *Msg.Message), 9.f, 0.f);
									}
								}
							}
						}
					}

					bool FirstTaskMsg=true;
					int32 MsgCount = 0;
					for (FAbilityTaskDebugMessage& Msg : Instance->TaskDebugMessages)
					{
						// Cap finished task msgs to 5 per ability if we are printing to screen (else things will scroll off)
						if ( Info.Canvas && ++MsgCount > 5 )
						{
							break;
						}

						if (Instance->ActiveTasks.Contains(Msg.FromTask) == false)
						{
							if (FirstTaskMsg)
							{
								DebugLine(Info, TEXT("[FinishedTasks]"), 7.f, 0.f);
								FirstTaskMsg = false;
							}

							DebugLine(Info, FString::Printf(TEXT("%s"), *Msg.Message), 9.f, 0.f);
						}
					}

					if (InstanceIdx < Instances.Num() - 2)
					{
						if (Info.Canvas) Info.Canvas->SetDrawColor(FColor(128, 128, 128));
						DebugLine(Info, FString::Printf(TEXT("--------")), 7.f, 0.f);
					}
				}
			}
		}
		AccumulateScreenPos(Info);
	}

	if (Info.XPos > Info.OriginalX)
	{
		// We flooded to new columns, returned YPos should be max Y (and some padding)
		Info.YPos = Info.MaxY + MaxCharHeight*2.f;
	}
	Info.YL = MaxCharHeight;
}


#undef LOCTEXT_NAMESPACE
