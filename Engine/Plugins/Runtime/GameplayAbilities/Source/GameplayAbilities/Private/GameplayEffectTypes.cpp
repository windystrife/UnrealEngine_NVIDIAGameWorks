// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectTypes.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayEffect.h"
#include "GameFramework/Controller.h"
#include "Misc/ConfigCacheIni.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#if WITH_EDITORONLY_DATA
const FName FGameplayModEvaluationChannelSettings::ForceHideMetadataKey(TEXT("ForceHideEvaluationChannel"));
const FString FGameplayModEvaluationChannelSettings::ForceHideMetadataEnabledValue(TEXT("True"));
#endif // #if WITH_EDITORONLY_DATA

FGameplayModEvaluationChannelSettings::FGameplayModEvaluationChannelSettings()
{
	static const UEnum* EvalChannelEnum = nullptr;
	static EGameplayModEvaluationChannel DefaultChannel = EGameplayModEvaluationChannel::Channel0;

	// The default value for this struct is actually dictated by a config value, so a degree of trickery is involved.
	// The first time through, try to find the enum and the default value, if any, and then use that to set the
	// static default channel used to initialize this struct
	if (!EvalChannelEnum)
	{
		EvalChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayModEvaluationChannel"));
		if (ensure(EvalChannelEnum) && ensure(GConfig))
		{
			const FString INISection(TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"));
			const FString INIKey(TEXT("DefaultGameplayModEvaluationChannel"));
			
			FString DefaultEnumString;
			if (GConfig->GetString(*INISection, *INIKey, DefaultEnumString, GGameIni))
			{
				if (!DefaultEnumString.IsEmpty())
				{
					const int32 EnumVal = EvalChannelEnum->GetValueByName(FName(*DefaultEnumString));
					if (EnumVal != INDEX_NONE)
					{
						DefaultChannel = static_cast<EGameplayModEvaluationChannel>(EnumVal);
					}
				}
			}
		}
	}

	Channel = DefaultChannel;
}

EGameplayModEvaluationChannel FGameplayModEvaluationChannelSettings::GetEvaluationChannel() const
{
	if (ensure(UAbilitySystemGlobals::Get().IsGameplayModEvaluationChannelValid(Channel)))
	{
		return Channel;
	}

	return EGameplayModEvaluationChannel::Channel0;
}

float GameplayEffectUtilities::GetModifierBiasByModifierOp(EGameplayModOp::Type ModOp)
{
	static const float ModifierOpBiases[EGameplayModOp::Max] = {0.f, 1.f, 1.f, 0.f};
	check(ModOp >= 0 && ModOp < EGameplayModOp::Max);

	return ModifierOpBiases[ModOp];
}

float GameplayEffectUtilities::ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EGameplayModOp::Type ModOp)
{
	const float OperationBias = GameplayEffectUtilities::GetModifierBiasByModifierOp(ModOp);

	StackCount = FMath::Clamp<int32>(StackCount, 0, StackCount);

	float StackMag = BaseComputedMagnitude;
	
	// Override modifiers don't care about stack count at all. All other modifier ops need to subtract out their bias value in order to handle
	// stacking correctly
	if (ModOp != EGameplayModOp::Override)
	{
		StackMag -= OperationBias;
		StackMag *= StackCount;
		StackMag += OperationBias;
	}

	return StackMag;
}

bool FGameplayEffectAttributeCaptureDefinition::operator==(const FGameplayEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture == Other.AttributeToCapture) && (AttributeSource == Other.AttributeSource) && (bSnapshot == Other.bSnapshot));
}

bool FGameplayEffectAttributeCaptureDefinition::operator!=(const FGameplayEffectAttributeCaptureDefinition& Other) const
{
	return ((AttributeToCapture != Other.AttributeToCapture) || (AttributeSource != Other.AttributeSource) || (bSnapshot != Other.bSnapshot));
}

FString FGameplayEffectAttributeCaptureDefinition::ToSimpleString() const
{
	return FString::Printf(TEXT("Attribute: %s, Capture: %s, Snapshot: %d"), *AttributeToCapture.GetName(), AttributeSource == EGameplayEffectAttributeCaptureSource::Source ? TEXT("Source") : TEXT("Target"), bSnapshot);
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	FGameplayEffectContext
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

void FGameplayEffectContext::AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
{
	Instigator = InInstigator;
	EffectCauser = InEffectCauser;
	InstigatorAbilitySystemComponent = NULL;

	// Cache off his AbilitySystemComponent.
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Instigator.Get());
	if (AbilitySystemInterface)
	{
		InstigatorAbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent();
	}
}

void FGameplayEffectContext::SetAbility(const UGameplayAbility* InGameplayAbility)
{
	if (InGameplayAbility)
	{
		AbilityInstanceNotReplicated = InGameplayAbility;
		AbilityCDO = InGameplayAbility->GetClass()->GetDefaultObject<UGameplayAbility>();
		AbilityLevel = InGameplayAbility->GetAbilityLevel();
	}
}

const UGameplayAbility* FGameplayEffectContext::GetAbility() const
{
	return AbilityCDO.Get();
}

const UGameplayAbility* FGameplayEffectContext::GetAbilityInstance_NotReplicated() const
{
	return AbilityInstanceNotReplicated.Get();
}


void FGameplayEffectContext::AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors, bool bReset)
{
	if (bReset && Actors.Num())
	{
		Actors.Reset();
	}

	Actors.Append(InActors);
}

void FGameplayEffectContext::AddHitResult(const FHitResult& InHitResult, bool bReset)
{
	if (bReset && HitResult.IsValid())
	{
		HitResult.Reset();
		bHasWorldOrigin = false;
	}

	check(!HitResult.IsValid());
	HitResult = TSharedPtr<FHitResult>(new FHitResult(InHitResult));
	if (bHasWorldOrigin == false)
	{
		AddOrigin(InHitResult.TraceStart);
	}
}

bool FGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	uint8 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (Instigator.IsValid() )
		{
			RepBits |= 1 << 0;
		}
		if (EffectCauser.IsValid() )
		{
			RepBits |= 1 << 1;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= 1 << 2;
		}
		if (SourceObject.IsValid())
		{
			RepBits |= 1 << 3;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= 1 << 4;
		}
		if (HitResult.IsValid())
		{
			RepBits |= 1 << 5;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= 1 << 6;
		}
	}

	Ar.SerializeBits(&RepBits, 7);

	if (RepBits & (1 << 0))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << 1))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << 2))
	{
		Ar << AbilityCDO;
	}
	if (RepBits & (1 << 3))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << 4))
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if (RepBits & (1 << 5))
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << 6))
	{
		Ar << WorldOrigin;
		bHasWorldOrigin = true;
	}
	else
	{
		bHasWorldOrigin = false;
	}

	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}	
	
	bOutSuccess = true;
	return true;
}

bool FGameplayEffectContext::IsLocallyControlled() const
{
	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return false;
}

bool FGameplayEffectContext::IsLocallyControlledPlayer() const
{
	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn && Pawn->Controller)
	{
		return Pawn->Controller->IsLocalPlayerController();
	}
	return false;
}

void FGameplayEffectContext::AddOrigin(FVector InOrigin)
{
	bHasWorldOrigin = true;
	WorldOrigin = InOrigin;
}

void FGameplayEffectContext::GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const
{
	IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Instigator.Get());
	if (TagInterface)
	{
		TagInterface->GetOwnedGameplayTags(ActorTagContainer);
	}
	else if (InstigatorAbilitySystemComponent.IsValid())
	{
		InstigatorAbilitySystemComponent->GetOwnedGameplayTags(ActorTagContainer);
	}
}

bool FGameplayEffectContextHandle::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	bool ValidData = Data.IsValid();
	Ar.SerializeBits(&ValidData,1);

	if (ValidData)
	{
		if (Ar.IsLoading())
		{
			// For now, just always reset/reallocate the data when loading.
			// Longer term if we want to generalize this and use it for property replication, we should support
			// only reallocating when necessary
			
			if (Data.IsValid() == false)
			{
				Data = TSharedPtr<FGameplayEffectContext>(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
			}
		}

		void* ContainerPtr = Data.Get();
		UScriptStruct* ScriptStruct = Data->GetScriptStruct();

		if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
		{
			ScriptStruct->GetCppStructOps()->NetSerialize(Ar, Map, bOutSuccess, Data.Get());
		}
		else
		{
			// This won't work since UStructProperty::NetSerializeItem is deprecrated.
			//	1) we have to manually crawl through the topmost struct's fields since we don't have a UStructProperty for it (just the UScriptProperty)
			//	2) if there are any UStructProperties in the topmost struct's fields, we will assert in UStructProperty::NetSerializeItem.

			ABILITY_LOG(Fatal, TEXT("FGameplayEffectContextHandle::NetSerialize called on data struct %s without a native NetSerialize"), *ScriptStruct->GetName());
		}
	}

	bOutSuccess = true;
	return true;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	Misc
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

FString EGameplayModOpToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayModOp"));
	return e->GetNameStringByValue(Type);
}

FString EGameplayModEffectToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayModEffect"));
	return e->GetNameStringByValue(Type);
}

FString EGameplayCueEventToString(int32 Type)
{
	static UEnum *e = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGameplayCueEvent"));
	return e->GetNameStringByValue(Type);
}

void FGameplayTagCountContainer::Notify_StackCountChange(const FGameplayTag& Tag)
{	
	// The purpose of this function is to let anyone listening on the EGameplayTagEventType::AnyCountChange event know that the 
	// stack count of a GE that was backing this GE has changed. We do not update our internal map/count with this info, since that
	// map only counts the number of GE/sources that are giving that tag.
	FGameplayTagContainer TagAndParentsContainer = Tag.GetGameplayTagParents();
	for (auto CompleteTagIt = TagAndParentsContainer.CreateConstIterator(); CompleteTagIt; ++CompleteTagIt)
	{
		const FGameplayTag& CurTag = *CompleteTagIt;
		FDelegateInfo* DelegateInfo = GameplayTagEventMap.Find(CurTag);
		if (DelegateInfo)
		{
			int32 TagCount = GameplayTagCountMap.FindOrAdd(CurTag);
			DelegateInfo->OnAnyChange.Broadcast(CurTag, TagCount);
		}
	}
}

FOnGameplayEffectTagCountChanged& FGameplayTagCountContainer::RegisterGameplayTagEvent(const FGameplayTag& Tag, EGameplayTagEventType::Type EventType)
{
	FDelegateInfo& Info = GameplayTagEventMap.FindOrAdd(Tag);

	if (EventType == EGameplayTagEventType::NewOrRemoved)
	{
		return Info.OnNewOrRemove;
	}

	return Info.OnAnyChange;
}

void FGameplayTagCountContainer::Reset()
{
	GameplayTagEventMap.Reset();
	GameplayTagCountMap.Reset();
	ExplicitTagCountMap.Reset();
	ExplicitTags.Reset();
	OnAnyTagChangeDelegate.Clear();
}

bool FGameplayTagCountContainer::UpdateTagMap_Internal(const FGameplayTag& Tag, int32 CountDelta)
{
	const bool bTagAlreadyExplicitlyExists = ExplicitTags.HasTagExact(Tag);

	// Need special case handling to maintain the explicit tag list correctly, adding the tag to the list if it didn't previously exist and a
	// positive delta comes in, and removing it from the list if it did exist and a negative delta comes in.
	if (!bTagAlreadyExplicitlyExists)
	{
		// Brand new tag with a positive delta needs to be explicitly added
		if (CountDelta > 0)
		{
			ExplicitTags.AddTag(Tag);
		}
		// Block attempted reduction of non-explicit tags, as they were never truly added to the container directly
		else
		{
			// only warn about tags that are in the container but will not be removed because they aren't explicitly in the container
			if (ExplicitTags.HasTag(Tag))
			{
				ABILITY_LOG(Warning, TEXT("Attempted to remove tag: %s from tag count container, but it is not explicitly in the container!"), *Tag.ToString());
			}
			return false;
		}
	}

	// Update the explicit tag count map. This has to be separate than the map below because otherwise the count of nested tags ends up wrong
	int32& ExistingCount = ExplicitTagCountMap.FindOrAdd(Tag);

	ExistingCount = FMath::Max(ExistingCount + CountDelta, 0);

	// If our new count is 0, remove us from the explicit tag list
	if (ExistingCount <= 0)
	{
		// Remove from the explicit list
		ExplicitTags.RemoveTag(Tag);
	}

	// Check if change delegates are required to fire for the tag or any of its parents based on the count change
	FGameplayTagContainer TagAndParentsContainer = Tag.GetGameplayTagParents();
	bool CreatedSignificantChange = false;
	for (auto CompleteTagIt = TagAndParentsContainer.CreateConstIterator(); CompleteTagIt; ++CompleteTagIt)
	{
		const FGameplayTag& CurTag = *CompleteTagIt;

		// Get the current count of the specified tag. NOTE: Stored as a reference, so subsequent changes propogate to the map.
		int32& TagCountRef = GameplayTagCountMap.FindOrAdd(CurTag);

		const int32 OldCount = TagCountRef;

		// Apply the delta to the count in the map
		int32 NewTagCount = FMath::Max(OldCount + CountDelta, 0);
		TagCountRef = NewTagCount;

		// If a significant change (new addition or total removal) occurred, trigger related delegates
		bool SignificantChange = (OldCount == 0 || NewTagCount == 0);
		CreatedSignificantChange |= SignificantChange;
		if (SignificantChange)
		{
			OnAnyTagChangeDelegate.Broadcast(CurTag, NewTagCount);
		}

		FDelegateInfo* DelegateInfo = GameplayTagEventMap.Find(CurTag);
		if (DelegateInfo)
		{
			// Prior to calling OnAnyChange delegate, copy our OnNewOrRemove delegate, since things listening to OnAnyChange could add or remove 
			// to this map causing our pointer to become invalid.
			FOnGameplayEffectTagCountChanged OnNewOrRemoveLocalCopy = DelegateInfo->OnNewOrRemove;

			DelegateInfo->OnAnyChange.Broadcast(CurTag, NewTagCount);
			if (SignificantChange)
			{
				OnNewOrRemoveLocalCopy.Broadcast(CurTag, NewTagCount);
			}
		}
	}

	return CreatedSignificantChange;
}

bool FGameplayTagRequirements::RequirementsMet(const FGameplayTagContainer& Container) const
{
	bool HasRequired = Container.HasAll(RequireTags);
	bool HasIgnored = Container.HasAny(IgnoreTags);

	return HasRequired && !HasIgnored;
}

bool FGameplayTagRequirements::IsEmpty() const
{
	return (RequireTags.Num() == 0 && IgnoreTags.Num() == 0);
}

FString FGameplayTagRequirements::ToString() const
{
	FString Str;

	if (RequireTags.Num() > 0)
	{
		Str += FString::Printf(TEXT("require: %s "), *RequireTags.ToStringSimple());
	}
	if (IgnoreTags.Num() >0)
	{
		Str += FString::Printf(TEXT("ignore: %s "), *IgnoreTags.ToStringSimple());
	}

	return Str;
}

void FActiveGameplayEffectsContainer::PrintAllGameplayEffects() const
{
	for (const FActiveGameplayEffect& Effect : this)
	{
		Effect.PrintAll();
	}
}

void FActiveGameplayEffect::PrintAll() const
{
	ABILITY_LOG(Log, TEXT("Handle: %s"), *Handle.ToString());
	ABILITY_LOG(Log, TEXT("StartWorldTime: %.2f"), StartWorldTime);
	Spec.PrintAll();
}

void FGameplayEffectSpec::PrintAll() const
{
	ABILITY_LOG(Log, TEXT("Def: %s"), *Def->GetName());
	ABILITY_LOG(Log, TEXT("Duration: %.2f"), GetDuration());
	ABILITY_LOG(Log, TEXT("Period: %.2f"), GetPeriod());
	ABILITY_LOG(Log, TEXT("Modifiers:"));
}

FString FGameplayEffectSpec::ToSimpleString() const
{
	return FString::Printf(TEXT("%s"), *GetNameSafe(Def));
}

const FGameplayTagContainer* FTagContainerAggregator::GetAggregatedTags() const
{
	if (CacheIsValid == false)
	{
		CacheIsValid = true;
		CachedAggregator.Reset(CapturedActorTags.Num() + CapturedSpecTags.Num() + ScopedTags.Num());
		CachedAggregator.AppendTags(CapturedActorTags);
		CachedAggregator.AppendTags(CapturedSpecTags);
		CachedAggregator.AppendTags(ScopedTags);
	}

	return &CachedAggregator;
}

FGameplayTagContainer& FTagContainerAggregator::GetActorTags()
{
	CacheIsValid = false;
	return CapturedActorTags;
}

const FGameplayTagContainer& FTagContainerAggregator::GetActorTags() const
{
	return CapturedActorTags;
}

FGameplayTagContainer& FTagContainerAggregator::GetSpecTags()
{
	CacheIsValid = false;
	return CapturedSpecTags;
}

const FGameplayTagContainer& FTagContainerAggregator::GetSpecTags() const
{
	CacheIsValid = false;
	return CapturedSpecTags;
}


FGameplayEffectSpecHandle::FGameplayEffectSpecHandle()
{

}

FGameplayEffectSpecHandle::FGameplayEffectSpecHandle(FGameplayEffectSpec* DataPtr)
	: Data(DataPtr)
{

}

FGameplayCueParameters::FGameplayCueParameters(const FGameplayEffectSpecForRPC& Spec)
: NormalizedMagnitude(0.0f)
, RawMagnitude(0.0f)
, Location(ForceInitToZero)
, Normal(ForceInitToZero)
, GameplayEffectLevel(1)
, AbilityLevel(1)
{
	UAbilitySystemGlobals::Get().InitGameplayCueParameters(*this, Spec);
}

FGameplayCueParameters::FGameplayCueParameters(const struct FGameplayEffectContextHandle& InEffectContext)
: NormalizedMagnitude(0.0f)
, RawMagnitude(0.0f)
, Location(ForceInitToZero)
, Normal(ForceInitToZero)
, GameplayEffectLevel(1)
, AbilityLevel(1)
{
	UAbilitySystemGlobals::Get().InitGameplayCueParameters(*this, InEffectContext);
}

bool FGameplayCueParameters::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	static const uint8 NUM_LEVEL_BITS = 5; // need to bump this up to support 20 levels for AbilityLevel
	static const uint8 MAX_LEVEL = (1 << NUM_LEVEL_BITS) - 1;

	enum RepFlag
	{
		REP_NormalizedMagnitude = 0,
		REP_RawMagnitude,
		REP_EffectContext,
		REP_Location,
		REP_Normal,
		REP_Instigator,
		REP_EffectCauser,
		REP_SourceObject,
		REP_TargetAttachComponent,
		REP_PhysMaterial,
		REP_GELevel,
		REP_AbilityLevel,

		REP_MAX
	};

	uint16 RepBits = 0;
	if (Ar.IsSaving())
	{
		if (NormalizedMagnitude != 0.f)
		{
			RepBits |= (1 << REP_NormalizedMagnitude);
		}
		if (RawMagnitude != 0.f)
		{
			RepBits |= (1 << REP_RawMagnitude);
		}
		if (EffectContext.IsValid())
		{
			RepBits |= (1 << REP_EffectContext);
		}
		if (Location.IsNearlyZero() == false)
		{
			RepBits |= (1 << REP_Location);
		}
		if (Normal.IsNearlyZero() == false)
		{
			RepBits |= (1 << REP_Normal);
		}
		if (Instigator.IsValid())
		{
			RepBits |= (1 << REP_Instigator);
		}
		if (EffectCauser.IsValid())
		{
			RepBits |= (1 << REP_EffectCauser);
		}
		if (SourceObject.IsValid())
		{
			RepBits |= (1 << REP_SourceObject);
		}
		if (TargetAttachComponent.IsValid())
		{
			RepBits |= (1 << REP_TargetAttachComponent);
		}
		if (PhysicalMaterial.IsValid())
		{
			RepBits |= (1 << REP_PhysMaterial);
		}
		if (GameplayEffectLevel != 1)
		{
			RepBits |= (1 << REP_GELevel);
		}
		if (AbilityLevel != 1)
		{
			RepBits |= (1 << REP_AbilityLevel);
		}
	}

	Ar.SerializeBits(&RepBits, REP_MAX);

	// Tag containers serialize empty containers with 1 bit, so no need to serialize this in the RepBits field.
	AggregatedSourceTags.NetSerialize(Ar, Map, bOutSuccess);
	AggregatedTargetTags.NetSerialize(Ar, Map, bOutSuccess);

	if (RepBits & (1 << REP_NormalizedMagnitude))
	{
		Ar << NormalizedMagnitude;
	}
	if (RepBits & (1 << REP_RawMagnitude))
	{
		Ar << RawMagnitude;
	}
	if (RepBits & (1 << REP_EffectContext))
	{
		EffectContext.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Location))
	{
		Location.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Normal))
	{
		Normal.NetSerialize(Ar, Map, bOutSuccess);
	}
	if (RepBits & (1 << REP_Instigator))
	{
		Ar << Instigator;
	}
	if (RepBits & (1 << REP_EffectCauser))
	{
		Ar << EffectCauser;
	}
	if (RepBits & (1 << REP_SourceObject))
	{
		Ar << SourceObject;
	}
	if (RepBits & (1 << REP_TargetAttachComponent))
	{
		Ar << TargetAttachComponent;
	}
	if (RepBits & (1 << REP_PhysMaterial))
	{
		Ar << PhysicalMaterial;
	}
	if (RepBits & (1 << REP_GELevel))
	{
		ensureMsgf(GameplayEffectLevel <= MAX_LEVEL, TEXT("FGameplayCueParameters::NetSerialize trying to serialize GC parameters with a GameplayEffectLevel of %d"), GameplayEffectLevel);
		if (Ar.IsLoading())
		{
			GameplayEffectLevel = 0;
		}

		Ar.SerializeBits(&GameplayEffectLevel, NUM_LEVEL_BITS);
	}
	if (RepBits & (1 << REP_AbilityLevel))
	{
		ensureMsgf(AbilityLevel <= MAX_LEVEL, TEXT("FGameplayCueParameters::NetSerialize trying to serialize GC parameters with an AbilityLevel of %d"), AbilityLevel);
		if (Ar.IsLoading())
		{
			AbilityLevel = 0;
		}

		Ar.SerializeBits(&AbilityLevel, NUM_LEVEL_BITS);
	}

	bOutSuccess = true;
	return true;
}

bool FGameplayCueParameters::IsInstigatorLocallyControlled() const
{
	if (EffectContext.IsValid())
	{
		return EffectContext.IsLocallyControlled();
	}

	APawn* Pawn = Cast<APawn>(Instigator.Get());
	if (!Pawn)
	{
		Pawn = Cast<APawn>(EffectCauser.Get());
	}
	if (Pawn)
	{
		return Pawn->IsLocallyControlled();
	}
	return false;
}

bool FGameplayCueParameters::IsInstigatorLocallyControlledPlayer(AActor* FallbackActor) const
{
	// If there is an effect context, just ask it
	if (EffectContext.IsValid())
	{
		return EffectContext.IsLocallyControlledPlayer();
	}
	
	// Look for a pawn and use its controller
	{
		APawn* Pawn = Cast<APawn>(Instigator.Get());
		if (!Pawn)
		{
			// If no instigator, look at effect causer
			Pawn = Cast<APawn>(EffectCauser.Get());
			if (!Pawn && FallbackActor != nullptr)
			{
				// Fallback to passed in actor
				Pawn = Cast<APawn>(FallbackActor);
				if (!Pawn)
				{
					Pawn = FallbackActor->GetInstigator<APawn>();
				}
			}
		}

		if (Pawn && Pawn->Controller)
		{
			return Pawn->Controller->IsLocalPlayerController();
		}
	}

	return false;
}

AActor* FGameplayCueParameters::GetInstigator() const
{
	if (Instigator.IsValid())
	{
		return Instigator.Get();
	}

	// Fallback to effect context if the explicit data on gameplaycue parameters is not there.
	return EffectContext.GetInstigator();
}

AActor* FGameplayCueParameters::GetEffectCauser() const
{
	if (EffectCauser.IsValid())
	{
		return EffectCauser.Get();
	}

	// Fallback to effect context if the explicit data on gameplaycue parameters is not there.
	return EffectContext.GetEffectCauser();
}

const UObject* FGameplayCueParameters::GetSourceObject() const
{
	if (SourceObject.IsValid())
	{
		return SourceObject.Get();
	}

	// Fallback to effect context if the explicit data on gameplaycue parameters is not there.
	return EffectContext.GetSourceObject();
}

bool FMinimalReplicationTagCountMap::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	const int32 CountBits = UAbilitySystemGlobals::Get().MinimalReplicationTagCountBits;
	const int32 MaxCount = ((1 << CountBits)-1);

	if (Ar.IsSaving())
	{
		int32 Count = TagMap.Num();
		if (Count > MaxCount)
		{
			ABILITY_LOG(Error, TEXT("FMinimapReplicationTagCountMap has too many tags (%d). This will cause tags to not replicate. See FMinimapReplicationTagCountMap::NetSerialize"), TagMap.Num());
			Count = MaxCount;
		}

		Ar.SerializeBits(&Count, CountBits);
		for(auto& It : TagMap)
		{
			FGameplayTag& Tag = It.Key;
			Tag.NetSerialize(Ar, Map, bOutSuccess);
			if (--Count <= 0)
			{
				break;
			}
		}
	}
	else
	{
		// Update MapID even when loading so that when the property is compared for replication,
		// it will be different, ensuring the data will be recorded in client replays.
		MapID++;

		int32 Count = TagMap.Num();
		Ar.SerializeBits(&Count, CountBits);

		// Reset our local map
		for(auto& It : TagMap)
		{
			It.Value = 0;
		}

		// See what we have
		while(Count-- > 0)
		{
			FGameplayTag Tag;
			Tag.NetSerialize(Ar, Map, bOutSuccess);
			TagMap.FindOrAdd(Tag) = 1;
		}

		if (Owner)
		{
			// Update our tags with owner tags
			for(auto It = TagMap.CreateIterator(); It; ++It)
			{
				Owner->SetTagMapCount(It->Key, It->Value);

				// Remove tags with a count of zero from the map. This prevents them
				// from being replicated incorrectly when recording client replays.
				if (It->Value == 0)
				{
					It.RemoveCurrent();
				}
			}
		}
	}


	bOutSuccess = true;
	return true;
}
