// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "GameplayAbilitySpec.h"
#include "UObject/Package.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayAbilityTargetTypes.h"
#include "GameplayAbilityTypes.generated.h"

class APlayerController;
class UAbilitySystemComponent;
class UAnimInstance;
class UAnimMontage;
class UGameplayAbility;
class UMovementComponent;

GAMEPLAYABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(LogAbilitySystemComponent, Log, All);

#define ENABLE_ABILITYTASK_DEBUGMSG !(UE_BUILD_SHIPPING | UE_BUILD_TEST)

UENUM(BlueprintType)
namespace EGameplayAbilityInstancingPolicy
{
	/**
	 *	How the ability is instanced when executed. This limits what an ability can do in its implementation. For example, a NonInstanced
	 *	Ability cannot have state. It is probably unsafe for an InstancedPerActor ability to have latent actions, etc.
	 */

	enum Type
	{
		// This ability is never instanced. Anything that executes the ability is operating on the CDO.
		NonInstanced,

		// Each actor gets their own instance of this ability. State can be saved, replication is possible.
		InstancedPerActor,

		// We instance this ability each time it is executed. Replication possible but not recommended.
		InstancedPerExecution,
	};
}

UENUM(BlueprintType)
namespace EGameplayAbilityNetExecutionPolicy
{
	/**
	 *	How does an ability execute on the network. Does a client "ask and predict", "ask and wait", "don't ask (just do it)"
	 */

	enum Type
	{
		// Part of this ability runs predictively on the local client if there is one
		LocalPredicted		UMETA(DisplayName = "Local Predicted"),

		// This ability will only run on the client or server that has local control
		LocalOnly			UMETA(DisplayName = "Local Only"),

		// This ability is initiated by the server, but will also run on the local client if one exists
		ServerInitiated		UMETA(DisplayName = "Server Initiated"),

		// This ability will only run on the server
		ServerOnly			UMETA(DisplayName = "Server Only"),
	};
}

UENUM(BlueprintType)
namespace EGameplayAbilityReplicationPolicy
{
	/**
	 *	How an ability replicates state/events to everyone on the network.
	 */

	enum Type
	{
		// We don't replicate the instance of the ability to anyone.
		ReplicateNo			UMETA(DisplayName = "Do Not Replicate"),

		// We replicate the instance of the ability to the owner.
		ReplicateYes		UMETA(DisplayName = "Replicate"),
	};
}


UENUM(BlueprintType)
namespace EGameplayAbilityTriggerSource
{
	/**	Defines what type of trigger will activate the ability, paired to a tag */

	enum Type
	{
		// Triggered from a gameplay event, will come with payload
		GameplayEvent,

		// Triggered if the ability's owner gets a tag added, triggered once whenever it's added
		OwnedTagAdded,

		// Triggered if the ability's owner gets tag added, removed when the tag is removed
		OwnedTagPresent,
	};
}


// ----------------------------------------------------

/**
 *	FGameplayAbilityActorInfo
 *
 *	Cached data associated with an Actor using an Ability.
 *		-Initialized from an AActor* in InitFromActor
 *		-Abilities use this to know what to actor upon. E.g., instead of being coupled to a specific actor class.
 *		-These are generally passed around as pointers to support polymorphism.
 *		-Projects can override UAbilitySystemGlobals::AllocAbilityActorInfo to override the default struct type that is created.
 *
 */
USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityActorInfo
{
	GENERATED_USTRUCT_BODY()

	virtual ~FGameplayAbilityActorInfo() {}

	/** The actor that owns the abilities, shouldn't be null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<AActor>	OwnerActor;

	/** The physical representation of the owner, used for targeting and animation. This will often be null! */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<AActor>	AvatarActor;

	/** PlayerController associated with the owning actor. This will often be null! */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<APlayerController>	PlayerController;

	/** Ability System component associated with the owner actor, shouldn't be null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UAbilitySystemComponent>	AbilitySystemComponent;

	/** Skeletal mesh of the avatar actor. Often null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<USkeletalMeshComponent>	SkeletalMeshComponent;

	/** Anim instance of the avatar actor. Often null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UAnimInstance>	AnimInstance;

	/** Movement component of the avatar actor. Often null */
	UPROPERTY(BlueprintReadOnly, Category = "ActorInfo")
	TWeakObjectPtr<UMovementComponent>	MovementComponent;

	/** Accessor to get the current anim instance from the SkeletalMeshComponent */
	UAnimInstance* GetAnimInstance() const { return (SkeletalMeshComponent.IsValid() ? SkeletalMeshComponent->GetAnimInstance() : nullptr); }

	/** Returns true if this actor is locally controlled. Only true for players on the client that owns them */
	bool IsLocallyControlled() const;

	bool IsLocallyControlledPlayer() const;

	/** Returns true if the owning actor has net authority */
	bool IsNetAuthority() const;

	/** Initializes the info from an owning actor. Will set both owner and avatar */
	virtual void InitFromActor(AActor *OwnerActor, AActor *AvatarActor, UAbilitySystemComponent* InAbilitySystemComponent);

	/** Sets a new avatar actor, keeps same owner and ability system component */
	virtual void SetAvatarActor(AActor *AvatarActor);

	/** Clears out any actor info, both owner and avatar */
	virtual void ClearActorInfo();
};

// ----------------------------------------------------

USTRUCT()
struct FGameplayAbilitySpecHandleAndPredictionKey
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilitySpecHandleAndPredictionKey()
	: PredictionKeyAtCreation(0)
	{}

	FGameplayAbilitySpecHandleAndPredictionKey(const FGameplayAbilitySpecHandle& HandleRef, const FPredictionKey& PredictionKeyAtCreationRef)
		: AbilityHandle(HandleRef), PredictionKeyAtCreation(PredictionKeyAtCreationRef.Current)
	{}

	bool operator==(const FGameplayAbilitySpecHandleAndPredictionKey& Other) const
	{
		return AbilityHandle == Other.AbilityHandle && PredictionKeyAtCreation == Other.PredictionKeyAtCreation;
	}

	bool operator!=(const FGameplayAbilitySpecHandleAndPredictionKey& Other) const
	{
		return AbilityHandle != Other.AbilityHandle || PredictionKeyAtCreation != Other.PredictionKeyAtCreation;
	}

	friend uint32 GetTypeHash(const FGameplayAbilitySpecHandleAndPredictionKey& Handle)
	{
		return GetTypeHash(Handle.AbilityHandle) ^ Handle.PredictionKeyAtCreation;
	}

private:

	UPROPERTY()
	FGameplayAbilitySpecHandle AbilityHandle;

	UPROPERTY()
	int32 PredictionKeyAtCreation;
};


// ----------------------------------------------------


/** Data about montages that is replicated to simulated clients */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityRepAnimMontage
{
	GENERATED_USTRUCT_BODY()

	/** AnimMontage ref */
	UPROPERTY()
	UAnimMontage* AnimMontage;

	/** Play Rate */
	UPROPERTY()
	float PlayRate;

	/** Montage position */
	UPROPERTY()
	float Position;

	/** Montage current blend time */
	UPROPERTY()
	float BlendTime;

	/** NextSectionID */
	UPROPERTY()
	uint8 NextSectionID;

	/** Bit set when montage has been stopped. */
	UPROPERTY()
	uint8 IsStopped : 1;
	
	/** Bit flipped every time a new Montage is played. To trigger replication when the same montage is played again. */
	UPROPERTY()
	uint8 ForcePlayBit : 1;

	FGameplayAbilityRepAnimMontage()
		: AnimMontage(NULL),
		PlayRate(0.f),
		Position(0.f),
		BlendTime(0.f),
		NextSectionID(0),
		IsStopped(true),
		ForcePlayBit(0)
	{
	}

	UPROPERTY()
	FPredictionKey PredictionKey;
};

/** Data about montages that were played locally (all montages in case of server. predictive montages in case of client). Never replicated directly. */
USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityLocalAnimMontage
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityLocalAnimMontage()
	: AnimMontage(nullptr), PlayBit(false), AnimatingAbility(nullptr)
	{
	}

	UPROPERTY()
	UAnimMontage* AnimMontage;

	UPROPERTY()
	bool PlayBit;

	UPROPERTY()
	FPredictionKey PredictionKey;

	/** The ability, if any, that instigated this montage */
	UPROPERTY()
	UGameplayAbility* AnimatingAbility;
};

// ----------------------------------------------------

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayEventData
{
	GENERATED_USTRUCT_BODY()

	FGameplayEventData()
	: Instigator(nullptr)
	, Target(nullptr)
	, OptionalObject(nullptr)
	, OptionalObject2(nullptr)
	, EventMagnitude(0.f)
	{
	}
	
	// Tag of the event that triggered this
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	FGameplayTag EventTag;

	// The instigator of the event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	const AActor* Instigator;

	// The target of the event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	const AActor* Target;

	// An optional ability-specific object to be passed though the event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	const UObject* OptionalObject;

	// A second optional ability-specific object to be passed though the event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	const UObject* OptionalObject2;

	// Polymorphic context information
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	FGameplayEffectContextHandle ContextHandle;

	// Tags that the instigator has
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	FGameplayTagContainer InstigatorTags;

	// Tags that the target has
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	FGameplayTagContainer TargetTags;

	// The magnitude of the triggering event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	float EventMagnitude;

	// The magnitude of the triggering event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayAbilityTriggerPayload)
	FGameplayAbilityTargetDataHandle	TargetData;
};

/** Used for cleaning up predicted data on network clients */
DECLARE_MULTICAST_DELEGATE_OneParam(FGameplayEventMulticastDelegate, const FGameplayEventData*);

/** Ability Ended Data */
USTRUCT()
struct FAbilityEndedData
{
	GENERATED_USTRUCT_BODY()
	FAbilityEndedData()
		: AbilityThatEnded(nullptr)
		, bReplicateEndAbility(false)
		, bWasCancelled(false)
	{
	}

	FAbilityEndedData(UGameplayAbility* InAbility, FGameplayAbilitySpecHandle InHandle, bool bInReplicateEndAbility, bool bInWasCancelled)
		: AbilityThatEnded(InAbility)
		, AbilitySpecHandle(InHandle)
		, bReplicateEndAbility(bInReplicateEndAbility)
		, bWasCancelled(bInWasCancelled)
	{
	}

	UPROPERTY()
	UGameplayAbility* AbilityThatEnded;

	UPROPERTY()
	FGameplayAbilitySpecHandle AbilitySpecHandle;

	UPROPERTY()
	bool bReplicateEndAbility;

	UPROPERTY()
	bool bWasCancelled;
};

/** Notification delegate definition for when the gameplay ability ends */
DECLARE_MULTICAST_DELEGATE_OneParam(FGameplayAbilityEndedDelegate, const FAbilityEndedData&);

/** 
 *	Structure that tells AbilitySystemComponent what to bind to an InputComponent (see BindAbilityActivationToInputComponent) 
 *	
 */
struct FGameplayAbiliyInputBinds
{
	FGameplayAbiliyInputBinds(FString InConfirmTargetCommand, FString InCancelTargetCommand, FString InEnumName, int32 InConfirmTargetInputID = INDEX_NONE, int32 InCancelTargetInputID = INDEX_NONE)
	: ConfirmTargetCommand(InConfirmTargetCommand)
	, CancelTargetCommand(InCancelTargetCommand)
	, EnumName(InEnumName)
	, ConfirmTargetInputID(InConfirmTargetInputID)
	, CancelTargetInputID(InCancelTargetInputID)
	{ }

	/** Defines command string that will be bound to Confirm Targeting */
	FString ConfirmTargetCommand;

	/** Defines command string that will be bound to Cancel Targeting */
	FString CancelTargetCommand;

	/** Returns enum to use for ability binds. E.g., "Ability1"-"Ability9" input commands will be bound to ability activations inside the AbiltiySystemComponent */
	FString	EnumName;

	/** If >=0, Confirm is bound to an entry in the enum */
	int32 ConfirmTargetInputID;

	/** If >=0, Confirm is bound to an entry in the enum */
	int32 CancelTargetInputID;

	UEnum* GetBindEnum() { return FindObject<UEnum>(ANY_PACKAGE, *EnumName); }
};

USTRUCT()
struct GAMEPLAYABILITIES_API FAttributeDefaults
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "AttributeTest")
	TSubclassOf<UAttributeSet> Attributes;

	UPROPERTY(EditAnywhere, Category = "AttributeTest")
	class UDataTable*	DefaultStartingTable;
};

USTRUCT()
struct GAMEPLAYABILITIES_API FAbilityTaskDebugMessage
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	class UGameplayTask*	FromTask;

	FString Message;
};

/** Used for cleaning up predicted data on network clients */
DECLARE_MULTICAST_DELEGATE(FAbilitySystemComponentPredictionKeyClear);

/** Generic delegate for ability 'events'/notifies */
DECLARE_MULTICAST_DELEGATE_OneParam(FGenericAbilityDelegate, UGameplayAbility*);
