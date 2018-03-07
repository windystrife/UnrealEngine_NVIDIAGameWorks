// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "AbilitySystemLog.h"
#include "GameplayEffectTypes.generated.h"

#define SKILL_SYSTEM_AGGREGATOR_DEBUG 1

#if SKILL_SYSTEM_AGGREGATOR_DEBUG
	#define SKILL_AGG_DEBUG( Format, ... ) *FString::Printf(Format, ##__VA_ARGS__)
#else
	#define SKILL_AGG_DEBUG( Format, ... ) NULL
#endif

class Error;
class UAbilitySystemComponent;
class UGameplayAbility;
struct FActiveGameplayEffect;
struct FGameplayEffectModCallbackData;
struct FGameplayEffectSpec;

GAMEPLAYABILITIES_API FString EGameplayModOpToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayModToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayModEffectToString(int32 Type);

GAMEPLAYABILITIES_API FString EGameplayCueEventToString(int32 Type);

/** Valid gameplay modifier evaluation channels; Displayed and renamed via game-specific aliases and options */
UENUM()
enum class EGameplayModEvaluationChannel : uint8
{
	Channel0 UMETA(Hidden),
	Channel1 UMETA(Hidden),
	Channel2 UMETA(Hidden),
	Channel3 UMETA(Hidden),
	Channel4 UMETA(Hidden),
	Channel5 UMETA(Hidden),
	Channel6 UMETA(Hidden),
	Channel7 UMETA(Hidden),
	Channel8 UMETA(Hidden),
	Channel9 UMETA(Hidden),

	// Always keep last
	Channel_MAX UMETA(Hidden)
};

/** Struct representing evaluation channel settings for a gameplay modifier */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayModEvaluationChannelSettings
{
	GENERATED_USTRUCT_BODY()
	
	/** Constructor */
	FGameplayModEvaluationChannelSettings();

	/**
	 * Get the modifier evaluation channel to use
	 * 
	 * @return	Either the channel directly specified within the settings, if valid, or Channel0 in the event of a game not using modifier
	 *			channels or in the case of an invalid channel being specified within the settings
	 */
	EGameplayModEvaluationChannel GetEvaluationChannel() const;

	/** Editor-only constants to aid in hiding evaluation channel settings when appropriate */
#if WITH_EDITORONLY_DATA
	static const FName ForceHideMetadataKey;
	static const FString ForceHideMetadataEnabledValue;
#endif // #if WITH_EDITORONLY_DATA

protected:

	/** Channel the settings would prefer to use, if possible/valid */
	UPROPERTY(EditDefaultsOnly, Category=EvaluationChannel)
	EGameplayModEvaluationChannel Channel;

	// Allow the details customization as a friend so it can handle custom display of the struct
	friend class FGameplayModEvaluationChannelSettingsDetails;
};

UENUM(BlueprintType)
namespace EGameplayModOp
{
	enum Type
	{		
		/** Numeric. */
		Additive = 0		UMETA(DisplayName="Add"),
		/** Numeric. */
		Multiplicitive		UMETA(DisplayName = "Multiply"),
		/** Numeric. */
		Division			UMETA(DisplayName = "Divide"),

		/** Other. */
		Override 			UMETA(DisplayName="Override"),	// This should always be the first non numeric ModOp

		// This must always be at the end.
		Max					UMETA(DisplayName="Invalid")
	};
}

namespace GameplayEffectUtilities
{
	/**
	 * Helper function to retrieve the modifier bias based upon modifier operation
	 * 
	 * @param ModOp	Modifier operation to retrieve the modifier bias for
	 * 
	 * @return Modifier bias for the specified operation
	 */
	GAMEPLAYABILITIES_API float GetModifierBiasByModifierOp(EGameplayModOp::Type ModOp);

	/**
	 * Helper function to compute the stacked modifier magnitude from a base magnitude, given a stack count and modifier operation
	 * 
	 * @param BaseComputedMagnitude	Base magnitude to compute from
	 * @param StackCount			Stack count to use for the calculation
	 * @param ModOp					Modifier operation to use
	 * 
	 * @return Computed modifier magnitude with stack count factored in
	 */
	GAMEPLAYABILITIES_API float ComputeStackedModifierMagnitude(float BaseComputedMagnitude, int32 StackCount, EGameplayModOp::Type ModOp);
}


/** Enumeration for options of where to capture gameplay attributes from for gameplay effects. */
UENUM()
enum class EGameplayEffectAttributeCaptureSource : uint8
{
	/** Source (caster) of the gameplay effect. */
	Source,	
	/** Target (recipient) of the gameplay effect. */
	Target	
};

/** Enumeration for ways a single GameplayEffect asset can stack. */
UENUM()
enum class EGameplayEffectStackingType : uint8
{
	/** No stacking. Multiple applications of this GameplayEffect are treated as separate instances. */
	None,
	/** Each caster has its own stack. */
	AggregateBySource,
	/** Each target has its own stack. */
	AggregateByTarget,
};

/**
 * This handle is required for things outside of FActiveGameplayEffectsContainer to refer to a specific active GameplayEffect
 *	For example if a skill needs to create an active effect and then destroy that specific effect that it created, it has to do so
 *	through a handle. a pointer or index into the active list is not sufficient.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FActiveGameplayEffectHandle
{
	GENERATED_USTRUCT_BODY()

	FActiveGameplayEffectHandle()
		: Handle(INDEX_NONE),
		bPassedFiltersAndWasExecuted(false)
	{

	}

	FActiveGameplayEffectHandle(int32 InHandle)
		: Handle(InHandle),
		bPassedFiltersAndWasExecuted(true)
	{

	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	bool WasSuccessfullyApplied() const
	{
		return bPassedFiltersAndWasExecuted;
	}

	static FActiveGameplayEffectHandle GenerateNewHandle(UAbilitySystemComponent* OwningComponent);

	static void ResetGlobalHandleMap();

	UAbilitySystemComponent* GetOwningAbilitySystemComponent();
	const UAbilitySystemComponent* GetOwningAbilitySystemComponent() const;

	void RemoveFromGlobalMap();

	bool operator==(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveGameplayEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FActiveGameplayEffectHandle& InHandle)
	{
		return InHandle.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

private:

	UPROPERTY()
	int32 Handle;

	UPROPERTY()
	bool bPassedFiltersAndWasExecuted;
};

USTRUCT(BlueprintType)
struct FGameplayModifierEvaluatedData
{
	GENERATED_USTRUCT_BODY()

	FGameplayModifierEvaluatedData()
		: Attribute()
		, ModifierOp(EGameplayModOp::Additive)
		, Magnitude(0.f)
		, IsValid(false)
	{
	}

	FGameplayModifierEvaluatedData(const FGameplayAttribute& InAttribute, TEnumAsByte<EGameplayModOp::Type> InModOp, float InMagnitude, FActiveGameplayEffectHandle InHandle = FActiveGameplayEffectHandle())
		: Attribute(InAttribute)
		, ModifierOp(InModOp)
		, Magnitude(InMagnitude)
		, Handle(InHandle)
		, IsValid(true)
	{
	}

	UPROPERTY()
	FGameplayAttribute Attribute;

	/** The numeric operation of this modifier: Override, Add, Multiply, etc  */
	UPROPERTY()
	TEnumAsByte<EGameplayModOp::Type> ModifierOp;

	UPROPERTY()
	float Magnitude;

	/** Handle of the active gameplay effect that originated us. Will be invalid in many cases */
	UPROPERTY()
	FActiveGameplayEffectHandle	Handle;

	UPROPERTY()
	bool IsValid;

	FString ToSimpleString() const
	{
		return FString::Printf(TEXT("%s %s EvalMag: %f"), *Attribute.GetName(), *EGameplayModOpToString(ModifierOp), Magnitude);
	}
};

/** Struct defining gameplay attribute capture options for gameplay effects */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectAttributeCaptureDefinition
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectAttributeCaptureDefinition()
	{
		AttributeSource = EGameplayEffectAttributeCaptureSource::Source;
		bSnapshot = false;
	}

	FGameplayEffectAttributeCaptureDefinition(FGameplayAttribute InAttribute, EGameplayEffectAttributeCaptureSource InSource, bool InSnapshot)
		: AttributeToCapture(InAttribute), AttributeSource(InSource), bSnapshot(InSnapshot)
	{

	}

	/** Gameplay attribute to capture */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	FGameplayAttribute AttributeToCapture;

	/** Source of the gameplay attribute */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	EGameplayEffectAttributeCaptureSource AttributeSource;

	/** Whether the attribute should be snapshotted or not */
	UPROPERTY(EditDefaultsOnly, Category=Capture)
	bool bSnapshot;

	/** Equality/Inequality operators */
	bool operator==(const FGameplayEffectAttributeCaptureDefinition& Other) const;
	bool operator!=(const FGameplayEffectAttributeCaptureDefinition& Other) const;

	/**
	 * Get type hash for the capture definition; Implemented to allow usage in TMap
	 *
	 * @param CaptureDef Capture definition to get the type hash of
	 */
	friend uint32 GetTypeHash(const FGameplayEffectAttributeCaptureDefinition& CaptureDef)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.AttributeToCapture));
		Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(CaptureDef.AttributeSource)));
		Hash = HashCombine(Hash, GetTypeHash(CaptureDef.bSnapshot));
		return Hash;
	}

	FString ToSimpleString() const;
};

/**
 * FGameplayEffectContext
 *	Data struct for an instigator and related data. This is still being fleshed out. We will want to track actors but also be able to provide some level of tracking for actors that are destroyed.
 *	We may need to store some positional information as well.
 */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayEffectContext
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectContext()
	: AbilityLevel(1)
	, bHasWorldOrigin(false)
	{
	}

	FGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
	: AbilityLevel(1)
	, bHasWorldOrigin(false)
	{
		AddInstigator(InInstigator, InEffectCauser);
	}

	virtual ~FGameplayEffectContext()
	{
	}

	/** Returns the list of gameplay tags applicable to this effect, defaults to the owner's tags */
	virtual void GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const;

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	virtual void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser);

	/** Sets the ability that was used to spawn this */
	virtual void SetAbility(const UGameplayAbility* InGameplayAbility);

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		return Instigator.Get();
	}

	/** returns the CDO of the ability used to instigate this context */
	const UGameplayAbility* GetAbility() const;

	const UGameplayAbility* GetAbilityInstance_NotReplicated() const;

	int32 GetAbilityLevel() const
	{
		return AbilityLevel;
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UAbilitySystemComponent* GetInstigatorAbilitySystemComponent() const
	{
		return InstigatorAbilitySystemComponent.Get();
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		return EffectCauser.Get();
	}

	void SetEffectCauser(AActor* InEffectCauser)
	{
		EffectCauser = InEffectCauser;
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	virtual AActor* GetOriginalInstigator() const
	{
		return Instigator.Get();
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	virtual UAbilitySystemComponent* GetOriginalInstigatorAbilitySystemComponent() const
	{
		return InstigatorAbilitySystemComponent.Get();
	}

	/** Sets the object this effect was created from. */
	virtual void AddSourceObject(const UObject* NewSourceObject)
	{
		SourceObject = NewSourceObject;
	}

	/** Returns the object this effect was created from. */
	virtual UObject* GetSourceObject() const
	{
		return SourceObject.Get();
	}

	virtual void AddActors(const TArray<TWeakObjectPtr<AActor>>& IActor, bool bReset = false);

	virtual void AddHitResult(const FHitResult& InHitResult, bool bReset = false);

	virtual const TArray<TWeakObjectPtr<AActor>>& GetActors() const
	{
		return Actors;
	}

	virtual const FHitResult* GetHitResult() const
	{
		if (HitResult.IsValid())
		{
			return HitResult.Get();
		}
		return NULL;
	}

	virtual void AddOrigin(FVector InOrigin);

	virtual const FVector& GetOrigin() const
	{
		return WorldOrigin;
	}

	virtual bool HasOrigin() const
	{
		return bHasWorldOrigin;
	}

	virtual FString ToString() const
	{
		return Instigator.IsValid() ? Instigator->GetName() : FString(TEXT("NONE"));
	}

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FGameplayEffectContext::StaticStruct();
	}

	/** Creates a copy of this context, used to duplicate for later modifications */
	virtual FGameplayEffectContext* Duplicate() const
	{
		FGameplayEffectContext* NewContext = new FGameplayEffectContext();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual bool IsLocallyControlled() const;

	virtual bool IsLocallyControlledPlayer() const;

	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

protected:
	// The object pointers here have to be weak because contexts aren't necessarily tracked by GC in all cases

	/** Instigator actor, the actor that owns the ability system component */
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;

	/** The physical actor that actually did the damage, can be a weapon or projectile */
	UPROPERTY()
	TWeakObjectPtr<AActor> EffectCauser;

	/** the ability CDO that is responsible for this effect context (replicated) */
	UPROPERTY()
	TWeakObjectPtr<UGameplayAbility> AbilityCDO;

	/** the ability instance that is responsible for this effect context (NOT replicated) */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UGameplayAbility> AbilityInstanceNotReplicated;

	UPROPERTY()
	int32 AbilityLevel;

	/** Object this effect was created from, can be an actor or static object. Useful to bind an effect to a gameplay object */
	UPROPERTY()
	TWeakObjectPtr<UObject> SourceObject;

	/** The ability system component that's bound to instigator */
	UPROPERTY(NotReplicated)
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorAbilitySystemComponent;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Actors;

	/** Trace information - may be NULL in many cases */
	TSharedPtr<FHitResult>	HitResult;

	UPROPERTY()
	FVector	WorldOrigin;

	UPROPERTY()
	bool bHasWorldOrigin;
};

template<>
struct TStructOpsTypeTraits< FGameplayEffectContext > : public TStructOpsTypeTraitsBase2< FGameplayEffectContext >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

/**
 * Handle that wraps a FGameplayEffectContext or subclass, to allow it to be polymorphic and replicate properly
 */
USTRUCT(BlueprintType)
struct FGameplayEffectContextHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectContextHandle()
	{
	}

	virtual ~FGameplayEffectContextHandle()
	{
	}

	/** Constructs from an existing context, should be allocated by new */
	explicit FGameplayEffectContextHandle(FGameplayEffectContext* DataPtr)
	{
		Data = TSharedPtr<FGameplayEffectContext>(DataPtr);
	}

	/** Sets from an existing context, should be allocated by new */
	void operator=(FGameplayEffectContext* DataPtr)
	{
		Data = TSharedPtr<FGameplayEffectContext>(DataPtr);
	}

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	FGameplayEffectContext* Get()
	{
		return IsValid() ? Data.Get() : NULL;
	}

	const FGameplayEffectContext* Get() const
	{
		return IsValid() ? Data.Get() : NULL;
	}

	/** Returns the list of gameplay tags applicable to this effect, defaults to the owner's tags */
	void GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const
	{
		if (IsValid())
		{
			Data->GetOwnedGameplayTags(ActorTagContainer, SpecTagContainer);
		}
	}

	/** Sets the instigator and effect causer. Instigator is who owns the ability that spawned this, EffectCauser is the actor that is the physical source of the effect, such as a weapon. They can be the same. */
	void AddInstigator(class AActor *InInstigator, class AActor *InEffectCauser)
	{
		if (IsValid())
		{
			Data->AddInstigator(InInstigator, InEffectCauser);
		}
	}

	void SetAbility(const UGameplayAbility* InGameplayAbility)
	{
		if (IsValid())
		{
			Data->SetAbility(InGameplayAbility);
		}
	}

	/** Returns the immediate instigator that applied this effect */
	virtual AActor* GetInstigator() const
	{
		if (IsValid())
		{
			return Data->GetInstigator();
		}
		return NULL;
	}

	/** Returns the Ability CDO */
	const UGameplayAbility* GetAbility() const
	{
		if (IsValid())
		{
			return Data->GetAbility();
		}
		return nullptr;
	}

	/** Returns the Ability Instance (never replicated) */
	const UGameplayAbility* GetAbilityInstance_NotReplicated() const
	{
		if (IsValid())
		{
			return Data->GetAbilityInstance_NotReplicated();
		}
		return nullptr;
	}

	int32 GetAbilityLevel() const
	{
		if (IsValid())
		{
			return Data->GetAbilityLevel();
		}
		return 1;
	}

	/** Returns the ability system component of the instigator of this effect */
	virtual UAbilitySystemComponent* GetInstigatorAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetInstigatorAbilitySystemComponent();
		}
		return NULL;
	}

	/** Returns the physical actor tied to the application of this effect */
	virtual AActor* GetEffectCauser() const
	{
		if (IsValid())
		{
			return Data->GetEffectCauser();
		}
		return NULL;
	}

	/** Should always return the original instigator that started the whole chain. Subclasses can override what this does */
	AActor* GetOriginalInstigator() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigator();
		}
		return NULL;
	}

	/** Returns the ability system component of the instigator that started the whole chain */
	UAbilitySystemComponent* GetOriginalInstigatorAbilitySystemComponent() const
	{
		if (IsValid())
		{
			return Data->GetOriginalInstigatorAbilitySystemComponent();
		}
		return NULL;
	}

	/** Sets the object this effect was created from. */
	void AddSourceObject(const UObject* NewSourceObject)
	{
		if (IsValid())
		{
			Data->AddSourceObject(NewSourceObject);
		}
	}

	/** Returns the object this effect was created from. */
	UObject* GetSourceObject() const
	{
		if (IsValid())
		{
			return Data->GetSourceObject();
		}
		return nullptr;
	}

	/** Returns if the instigator is locally controlled */
	bool IsLocallyControlled() const
	{
		if (IsValid())
		{
			return Data->IsLocallyControlled();
		}
		return false;
	}

	bool IsLocallyControlledPlayer() const
	{
		if (IsValid())
		{
			return Data->IsLocallyControlledPlayer();
		}
		return false;
	}

	void AddActors(const TArray<TWeakObjectPtr<AActor>>& InActors, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddActors(InActors, bReset);
		}
	}

	void AddHitResult(const FHitResult& InHitResult, bool bReset = false)
	{
		if (IsValid())
		{
			Data->AddHitResult(InHitResult, bReset);
		}
	}

	const TArray<TWeakObjectPtr<AActor>> GetActors()
	{
		return Data->GetActors();
	}

	const FHitResult* GetHitResult() const
	{
		if (IsValid())
		{
			return Data->GetHitResult();
		}
		return nullptr;
	}

	void AddOrigin(FVector InOrigin)
	{
		if (IsValid())
		{
			Data->AddOrigin(InOrigin);
		}
	}

	virtual const FVector& GetOrigin() const
	{
		if (IsValid())
		{
			return Data->GetOrigin();
		}
		return FVector::ZeroVector;
	}

	virtual bool HasOrigin() const
	{
		if (IsValid())
		{
			return Data->HasOrigin();
		}
		return false;
	}

	FString ToString() const
	{
		return IsValid() ? Data->ToString() : FString(TEXT("NONE"));
	}

	/** Creates a deep copy of this handle, used before modifying */
	FGameplayEffectContextHandle Duplicate() const
	{
		if (IsValid())
		{
			FGameplayEffectContext* NewContext = Data->Duplicate();
			return FGameplayEffectContextHandle(NewContext);
		}
		else
		{
			return FGameplayEffectContextHandle();
		}
	}

	/** Comparison operator */
	bool operator==(FGameplayEffectContextHandle const& Other) const
	{
		if (Data.IsValid() != Other.Data.IsValid())
		{
			return false;
		}
		if (Data.Get() != Other.Data.Get())
		{
			return false;
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(FGameplayEffectContextHandle const& Other) const
	{
		return !(FGameplayEffectContextHandle::operator==(Other));
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:

	TSharedPtr<FGameplayEffectContext> Data;
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectContextHandle> : public TStructOpsTypeTraitsBase2<FGameplayEffectContextHandle>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FGameplayEffectContext> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};


/**
 * FGameplayEffectRemovalInfo
 *	Data struct for containing information pertinent to GameplayEffects as they are removed.
 */
USTRUCT(BlueprintType)
struct FGameplayEffectRemovalInfo
{
	GENERATED_USTRUCT_BODY()

	/** True when the gameplay effect's duration has not expired, meaning the gameplay effect is being forcefully removed.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Removal")
	bool bPrematureRemoval;

	/** Number of Stacks this gameplay effect had before it was removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Removal")
	int32 StackCount;

	/** Actor this gameplay effect was targeting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Removal")
	FGameplayEffectContextHandle EffectContext;
};
// -----------------------------------------------------------


USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayCueParameters
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueParameters()
	: NormalizedMagnitude(0.0f)
	, RawMagnitude(0.0f)
	, Location(ForceInitToZero)
	, Normal(ForceInitToZero)
	, GameplayEffectLevel(1)
	, AbilityLevel(1)
	{}

	/** Projects can override this via UAbilitySystemGlobals */
	FGameplayCueParameters(const struct FGameplayEffectSpecForRPC &Spec);

	FGameplayCueParameters(const struct FGameplayEffectContextHandle& EffectContext);

	/** Magnitude of source gameplay effect, normalzed from 0-1. Use this for "how strong is the gameplay effect" (0=min, 1=,max) */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	float NormalizedMagnitude;

	/** Raw final magnitude of source gameplay effect. Use this is you need to display numbers or for other informational purposes. */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	float RawMagnitude;

	/** Effect context, contains information about hit result, etc */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayEffectContextHandle EffectContext;

	/** The tag name that matched this specific gameplay cue handler */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue, NotReplicated)
	FGameplayTag MatchedTagName;

	/** The original tag of the gameplay cue */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue, NotReplicated)
	FGameplayTag OriginalTag;

	/** The aggregated source tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayTagContainer AggregatedSourceTags;

	/** The aggregated target tags taken from the effect spec */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FGameplayTagContainer AggregatedTargetTags;

	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FVector_NetQuantize10 Location;

	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	FVector_NetQuantizeNormal Normal;

	/** Instigator actor, the actor that owns the ability system component */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	TWeakObjectPtr<AActor> Instigator;

	/** The physical actor that actually did the damage, can be a weapon or projectile */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	TWeakObjectPtr<AActor> EffectCauser;

	/** Object this effect was created from, can be an actor or static object. Useful to bind an effect to a gameplay object */
	UPROPERTY(BlueprintReadWrite, Category=GameplayCue)
	TWeakObjectPtr<const UObject> SourceObject;

	/** PhysMat of the hit, if there was a hit. */
	UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
	TWeakObjectPtr<const UPhysicalMaterial> PhysicalMaterial;

	/** If originating from a GameplayEffect, the level of that GameplayEffect */
	UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
	int32 GameplayEffectLevel;

	/** If originating from an ability, this will be the level of that ability */
	UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
	int32 AbilityLevel;

	/** Could be used to say "attach FX to this component always" */
	UPROPERTY(BlueprintReadWrite, Category = GameplayCue)
	TWeakObjectPtr<USceneComponent> TargetAttachComponent;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	bool IsInstigatorLocallyControlled() const;

	// Fallback actor is used if the parameters have nullptr for instigator and effect causer
	bool IsInstigatorLocallyControlledPlayer(AActor* FallbackActor=nullptr) const;

	AActor* GetInstigator() const;

	AActor* GetEffectCauser() const;

	const UObject* GetSourceObject() const;
};

template<>
struct TStructOpsTypeTraits<FGameplayCueParameters> : public TStructOpsTypeTraitsBase2<FGameplayCueParameters>
{
	enum
	{
		WithNetSerializer = true		
	};
};

UENUM(BlueprintType)
namespace EGameplayCueEvent
{
	enum Type
	{
		OnActive,		// Called when GameplayCue is activated.
		WhileActive,	// Called when GameplayCue is active, even if it wasn't actually just applied (Join in progress, etc)
		Executed,		// Called when a GameplayCue is executed: instant effects or periodic tick
		Removed			// Called when GameplayCue is removed
	};
}

DECLARE_DELEGATE_OneParam(FOnGameplayAttributeEffectExecuted, struct FGameplayModifierEvaluatedData&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayEffectTagCountChanged, const FGameplayTag, int32);

DECLARE_MULTICAST_DELEGATE(FOnActiveGameplayEffectRemoved);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveGameplayEffectRemoved_Info, const FGameplayEffectRemovalInfo&);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGivenActiveGameplayEffectRemoved, const FActiveGameplayEffect&);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActiveGameplayEffectStackChange, FActiveGameplayEffectHandle, int32, int32);

/** FActiveGameplayEffectHandle that is being effect, the start time, duration of the effect */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnActiveGameplayEffectTimeChange, FActiveGameplayEffectHandle, float, float);

// This is deprecated, use FOnGameplayAttributeValueChange
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayAttributeChange, float, const FGameplayEffectModCallbackData*);

struct FOnAttributeChangeData
{
	FGameplayAttribute Attribute;

	float	NewValue;
	float	OldValue;
	const FGameplayEffectModCallbackData* GEModData;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameplayAttributeValueChange, const FOnAttributeChangeData&);

DECLARE_DELEGATE_RetVal(FGameplayTagContainer, FGetGameplayTags);

DECLARE_DELEGATE_RetVal_OneParam(FOnGameplayEffectTagCountChanged&, FRegisterGameplayTagChangeDelegate, FGameplayTag);

// -----------------------------------------------------------


UENUM(BlueprintType)
namespace EGameplayTagEventType
{
	enum Type
	{		
		/** Event only happens when tag is new or completely removed */
		NewOrRemoved,

		/** Event happens any time tag "count" changes */
		AnyCountChange		
	};
}

/**
 * Struct that tracks the number/count of tag applications within it. Explicitly tracks the tags added or removed,
 * while simultaneously tracking the count of parent tags as well. Events/delegates are fired whenever the tag counts
 * of any tag (explicit or parent) are modified.
 */

struct GAMEPLAYABILITIES_API FGameplayTagCountContainer
{	
	FGameplayTagCountContainer()
	{}

	/**
	 * Check if the count container has a gameplay tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the count container has a gameplay tag that matches, false if not
	 */
	FORCEINLINE bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const
	{
		return GameplayTagCountMap.FindRef(TagToCheck) > 0;
	}

	/**
	 * Check if the count container has gameplay tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return true
	 * 
	 * @return True if the count container matches all of the gameplay tags
	 */
	FORCEINLINE bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
	{
		// if the TagContainer count is 0 return bCountEmptyAsMatch;
		if (TagContainer.Num() == 0)
		{
			return true;
		}

		bool AllMatch = true;
		for (const FGameplayTag& Tag : TagContainer)
		{
			if (GameplayTagCountMap.FindRef(Tag) <= 0)
			{
				AllMatch = false;
				break;
			}
		}		
		return AllMatch;
	}
	
	/**
	 * Check if the count container has gameplay tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return false
	 * 
	 * @return True if the count container matches any of the gameplay tags
	 */
	FORCEINLINE bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
	{
		if (TagContainer.Num() == 0)
		{
			return false;
		}

		bool AnyMatch = false;
		for (const FGameplayTag& Tag : TagContainer)
		{
			if (GameplayTagCountMap.FindRef(Tag) > 0)
			{
				AnyMatch = true;
				break;
			}
		}
		return AnyMatch;
	}
	
	/**
	 * Update the specified container of tags by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Container		Container of tags to update
	 * @param CountDelta	Delta of the tag count to apply
	 */
	FORCEINLINE void UpdateTagCount(const FGameplayTagContainer& Container, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
			{
				UpdateTagMap_Internal(*TagIt, CountDelta);
			}
		}
	}
	
	/**
	 * Update the specified tag by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Tag			Tag to update
	 * @param CountDelta	Delta of the tag count to apply
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool UpdateTagCount(const FGameplayTag& Tag, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	 * Set the specified tag count to a specific value
	 * 
	 * @param Tag			Tag to update
	 * @param Count			New count of the tag
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool SetTagCount(const FGameplayTag& Tag, int32 NewCount)
	{
		int32 ExistingCount = 0;
		if (int32* Ptr  = ExplicitTagCountMap.Find(Tag))
		{
			ExistingCount = *Ptr;
		}

		int32 CountDelta = NewCount - ExistingCount;
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	* return the count for a specified tag 
	*
	* @param Tag			Tag to update
	*
	* @return the count of the passed in tag
	*/
	FORCEINLINE int32 GetTagCount(const FGameplayTag& Tag) const
	{
		if (const int32* Ptr = GameplayTagCountMap.Find(Tag))
		{
			return *Ptr;
		}

		return 0;
	}

	/**
	 *	Broadcasts the AnyChange event for this tag. This is called when the stack count of the backing gameplay effect change.
	 *	It is up to the receiver of the broadcasted delegate to decide what to do with this.
	 */
	void Notify_StackCountChange(const FGameplayTag& Tag);

	/**
	 * Return delegate that can be bound to for when the specific tag's count changes to or off of zero
	 *
	 * @param Tag	Tag to get a delegate for
	 * 
	 * @return Delegate for when the specified tag's count changes to or off of zero
	 */
	FOnGameplayEffectTagCountChanged& RegisterGameplayTagEvent(const FGameplayTag& Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);
	
	/**
	 * Return delegate that can be bound to for when the any tag's count changes to or off of zero
	 * 
	 * @return Delegate for when any tag's count changes to or off of zero
	 */
	FOnGameplayEffectTagCountChanged& RegisterGenericGameplayEvent()
	{
		return OnAnyTagChangeDelegate;
	}

	/** Simple accessor to the explicit gameplay tag list */
	const FGameplayTagContainer& GetExplicitGameplayTags() const
	{
		return ExplicitTags;
	}

	void Reset();

private:

	struct FDelegateInfo
	{
		FOnGameplayEffectTagCountChanged	OnNewOrRemove;
		FOnGameplayEffectTagCountChanged	OnAnyChange;
	};

	/** Map of tag to delegate that will be fired when the count for the key tag changes to or away from zero */
	TMap<FGameplayTag, FDelegateInfo> GameplayTagEventMap;

	/** Map of tag to active count of that tag */
	TMap<FGameplayTag, int32> GameplayTagCountMap;

	/** Map of tag to explicit count of that tag. Cannot share with above map because it's not safe to merge explicit and generic counts */	
	TMap<FGameplayTag, int32> ExplicitTagCountMap;

	/** Delegate fired whenever any tag's count changes to or away from zero */
	FOnGameplayEffectTagCountChanged OnAnyTagChangeDelegate;

	/** Container of tags that were explicitly added */
	FGameplayTagContainer ExplicitTags;

	/** Internal helper function to adjust the explicit tag list & corresponding maps/delegates/etc. as necessary */
	bool UpdateTagMap_Internal(const FGameplayTag& Tag, int32 CountDelta);
};


// -----------------------------------------------------------

/** Encapsulate require and ignore tags */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayTagRequirements
{
	GENERATED_USTRUCT_BODY()

	/** All of these tags must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer RequireTags;

	/** None of these tags may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer IgnoreTags;

	bool	RequirementsMet(const FGameplayTagContainer& Container) const;
	bool	IsEmpty() const;

	static FGetGameplayTags	SnapshotTags(FGetGameplayTags TagDelegate);

	FString ToString() const;
};

USTRUCT()
struct GAMEPLAYABILITIES_API FTagContainerAggregator
{
	GENERATED_USTRUCT_BODY()

	FTagContainerAggregator() : CacheIsValid(false) {}

	FTagContainerAggregator(FTagContainerAggregator&& Other)
		: CapturedActorTags(MoveTemp(Other.CapturedActorTags))
		, CapturedSpecTags(MoveTemp(Other.CapturedSpecTags))
		, ScopedTags(MoveTemp(Other.ScopedTags))
		, CachedAggregator(MoveTemp(Other.CachedAggregator))
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator(const FTagContainerAggregator& Other)
		: CapturedActorTags(Other.CapturedActorTags)
		, CapturedSpecTags(Other.CapturedSpecTags)
		, ScopedTags(Other.ScopedTags)
		, CachedAggregator(Other.CachedAggregator)
		, CacheIsValid(Other.CacheIsValid)
	{
	}

	FTagContainerAggregator& operator=(FTagContainerAggregator&& Other)
	{
		CapturedActorTags = MoveTemp(Other.CapturedActorTags);
		CapturedSpecTags = MoveTemp(Other.CapturedSpecTags);
		ScopedTags = MoveTemp(Other.ScopedTags);
		CachedAggregator = MoveTemp(Other.CachedAggregator);
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FTagContainerAggregator& operator=(const FTagContainerAggregator& Other)
	{
		CapturedActorTags = Other.CapturedActorTags;
		CapturedSpecTags = Other.CapturedSpecTags;
		ScopedTags = Other.ScopedTags;
		CachedAggregator = Other.CachedAggregator;
		CacheIsValid = Other.CacheIsValid;
		return *this;
	}

	FGameplayTagContainer& GetActorTags();
	const FGameplayTagContainer& GetActorTags() const;

	FGameplayTagContainer& GetSpecTags();
	const FGameplayTagContainer& GetSpecTags() const;

	const FGameplayTagContainer* GetAggregatedTags() const;

private:

	UPROPERTY()
	FGameplayTagContainer CapturedActorTags;

	UPROPERTY()
	FGameplayTagContainer CapturedSpecTags;

	UPROPERTY()
	FGameplayTagContainer ScopedTags;

	mutable FGameplayTagContainer CachedAggregator;
	mutable bool CacheIsValid;
};


/** Allows blueprints to generate a GameplayEffectSpec once and then reference it by handle, to apply it multiple times/multiple targets. */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEffectSpecHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayEffectSpecHandle();
	FGameplayEffectSpecHandle(FGameplayEffectSpec* DataPtr);

	TSharedPtr<FGameplayEffectSpec>	Data;

	bool IsValidCache;

	void Clear()
	{
		Data.Reset();
	}

	bool IsValid() const
	{
		return Data.IsValid();
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		ABILITY_LOG(Fatal, TEXT("FGameplayEffectSpecHandle should not be NetSerialized"));
		return false;
	}

	/** Comparison operator */
	bool operator==(FGameplayEffectSpecHandle const& Other) const
	{
		// Both invalid structs or both valid and Pointer compare (???) // deep comparison equality
		bool bBothValid = IsValid() && Other.IsValid();
		bool bBothInvalid = !IsValid() && !Other.IsValid();
		return (bBothInvalid || (bBothValid && (Data.Get() == Other.Data.Get())));
	}

	/** Comparison operator */
	bool operator!=(FGameplayEffectSpecHandle const& Other) const
	{
		return !(FGameplayEffectSpecHandle::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayEffectSpecHandle> : public TStructOpsTypeTraitsBase2<FGameplayEffectSpecHandle>
{
	enum
	{
		WithCopy = true,
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

// -----------------------------------------------------------


USTRUCT()
struct GAMEPLAYABILITIES_API FMinimalReplicationTagCountMap
{
	GENERATED_USTRUCT_BODY()

	FMinimalReplicationTagCountMap()
	{
		MapID = 0;
	}

	void AddTag(const FGameplayTag& Tag)
	{
		MapID++;
		TagMap.FindOrAdd(Tag)++;
	}

	void RemoveTag(const FGameplayTag& Tag)
	{
		MapID++;
		int32& Count = TagMap.FindOrAdd(Tag);
		Count--;
		if (Count == 0)
		{
			// Remove from map so that we do not replicate
			TagMap.Remove(Tag);
		}
		else if (Count < 0)
		{
			ABILITY_LOG(Error, TEXT("FMinimapReplicationTagCountMap::RemoveTag called on Tag %s and count is now < 0"), *Tag.ToString());
			Count = 0;
		}
	}

	void AddTags(const FGameplayTagContainer& Container)
	{
		for (const FGameplayTag& Tag : Container)
		{
			AddTag(Tag);
		}
	}

	void RemoveTags(const FGameplayTagContainer& Container)
	{
		for (const FGameplayTag& Tag : Container)
		{
			RemoveTag(Tag);
		}
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	TMap<FGameplayTag, int32>	TagMap;

	UPROPERTY()
	class UAbilitySystemComponent* Owner;

	/** Comparison operator */
	bool operator==(FMinimalReplicationTagCountMap const& Other) const
	{
		return (MapID == Other.MapID);
	}

	/** Comparison operator */
	bool operator!=(FMinimalReplicationTagCountMap const& Other) const
	{
		return !(FMinimalReplicationTagCountMap::operator==(Other));
	}

	int32 MapID;
};

template<>
struct TStructOpsTypeTraits<FMinimalReplicationTagCountMap> : public TStructOpsTypeTraitsBase2<FMinimalReplicationTagCountMap>
{
	enum
	{
		WithCopy = true,
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

DECLARE_MULTICAST_DELEGATE(FOnExternalGameplayModifierDependencyChange);
