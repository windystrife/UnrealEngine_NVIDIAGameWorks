// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameplayPrediction.h"
#include "Components/MeshComponent.h"
#include "GameplayAbilityTargetTypes.generated.h"

class UGameplayAbility;
class UGameplayEffect;
struct FGameplayEffectSpec;

UENUM(BlueprintType)
namespace EGameplayTargetingConfirmation
{
	enum Type
	{
		Instant,			// The targeting happens instantly without special logic or user input deciding when to 'fire'.
		UserConfirmed,		// The targeting happens when the user confirms the targeting.
		Custom,				// The GameplayTargeting Ability is responsible for deciding when the targeting data is ready. Not supported by all TargetingActors.
		CustomMulti,		// The GameplayTargeting Ability is responsible for deciding when the targeting data is ready. Not supported by all TargetingActors. Should not destroy upon data production.
	};
}


/**
 *	A generic structure for targeting data. We want generic functions to produce this data and other generic
 *	functions to consume this data.
 *
 *	We expect this to be able to hold specific actors/object reference and also generic location/direction/origin
 *	information.
 *
 *	Some example producers:
 *		-Overlap/Hit collision event generates TargetData about who was hit in a melee attack
 *		-A mouse input causes a hit trace and the actor infront of the crosshair is turned into TargetData
 *		-A mouse input causes TargetData to be generated from the owner's crosshair view origin/direction
 *		-An AOE/aura pulses and all actors in a radius around the instigator are added to TargetData
 *		-Panzer Dragoon style 'painting' targeting mode
 *		-MMORPG style ground AOE targeting style (potentially both a location on the ground and actors that were targeted)
 *
 *	Some example consumers:
 *		-Apply a GameplayEffect to all actors in TargetData
 *		-Find closest actor from all in TargetData
 *		-Call some function on all actors in TargetData
 * 		-Filter or merge TargetDatas
 *		-Spawn a new actor at a TargetData location
 *
 *
 *
 *	Maybe it is better to distinguish between actor list targeting vs positional targeting data?
 *		-AOE/aura type of targeting data blurs the line
 *
 *
 */

USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	virtual ~FGameplayAbilityTargetData() { }

	TArray<FActiveGameplayEffectHandle> ApplyGameplayEffect(const UGameplayEffect* GameplayEffect, const FGameplayEffectContextHandle& InEffectContext, float Level, FPredictionKey PredictionKey = FPredictionKey());

	virtual TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectSpec(FGameplayEffectSpec& Spec, FPredictionKey PredictionKey = FPredictionKey());

	virtual void AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const;

	virtual void AddTargetDataToGameplayCueParameters(FGameplayCueParameters& Parameters) const;

	virtual TArray<TWeakObjectPtr<AActor>> GetActors() const
	{
		return TArray<TWeakObjectPtr<AActor>>();
	}

	virtual bool SetActors(TArray<TWeakObjectPtr<AActor>> NewActorArray)
	{
		//By default, we don't keep this data, and therefore can't set it.
		return false;
	}

	// -------------------------------------

	virtual bool HasHitResult() const
	{
		return false;
	}

	virtual const FHitResult* GetHitResult() const
	{
		return nullptr;
	}

	// -------------------------------------

	virtual bool HasOrigin() const
	{
		return false;
	}

	virtual FTransform GetOrigin() const
	{
		return FTransform::Identity;
	}

	// -------------------------------------

	virtual bool HasEndPoint() const
	{
		return false;
	}

	virtual FVector GetEndPoint() const
	{
		return FVector::ZeroVector;
	}

	virtual FTransform GetEndPointTransform() const
	{
		return FTransform(GetEndPoint());
	}

	// -------------------------------------

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FGameplayAbilityTargetData::StaticStruct();
	}

	virtual FString ToString() const;

	// -------------------------------------
	//..See notes on delegate definition FOnTargetActorSwapped.
	virtual bool ShouldCheckForTargetActorSwap() const
	{
		return false;
	}

};


UENUM(BlueprintType)
namespace EGameplayAbilityTargetingLocationType
{
	/**
	*	What type of location calculation to use when an ability asks for our transform.
	*/

	enum Type
	{
		LiteralTransform		UMETA(DisplayName = "Literal Transform"),		// We report an actual raw transform. This is also the final fallback if other methods fail.
		ActorTransform			UMETA(DisplayName = "Actor Transform"),			// We pull the transform from an associated actor directly.
		SocketTransform			UMETA(DisplayName = "Socket Transform"),		// We aim from a named socket on the player's skeletal mesh component.
	};
}

/**
*	Handle for Targeting Data. This servers two main purposes:
*		-Avoid us having to copy around the full targeting data structure in Blueprints
*		-Allows us to leverage polymorphism in the target data structure
*		-Allows us to implement NetSerialize and replicate by value between clients/server
*
*		-Avoid using UObjects could be used to give us polymorphism and by reference passing in blueprints.
*		-However we would still be screwed when it came to replication
*
*		-Replication by value
*		-Pass by reference in blueprints
*		-Polymophism in TargetData structure
*
*/

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetDataHandle
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityTargetDataHandle() { }
	FGameplayAbilityTargetDataHandle(struct FGameplayAbilityTargetData* DataPtr)
	{
		Data.Add(TSharedPtr<FGameplayAbilityTargetData>(DataPtr));
	}

	TArray<TSharedPtr<FGameplayAbilityTargetData>, TInlineAllocator<1> >	Data;

	void Clear()
	{
		Data.Reset();
	}

	int32 Num() const
	{
		return Data.Num();
	}

	bool IsValid(int32 Index) const
	{
		return (Index < Data.Num() && Data[Index].IsValid());
	}

	const FGameplayAbilityTargetData* Get(int32 Index) const
	{
		return IsValid(Index) ? Data[Index].Get() : nullptr;
	}

	FGameplayAbilityTargetData* Get(int32 Index)
	{
		return IsValid(Index) ? Data[Index].Get() : nullptr;
	}

	void Add(struct FGameplayAbilityTargetData* DataPtr)
	{
		Data.Add(TSharedPtr<FGameplayAbilityTargetData>(DataPtr));
	}

	DEPRECATED(4.11, "Pass Handle by reference, not pointer")
	void Append(FGameplayAbilityTargetDataHandle* OtherHandle)
	{
		Append(*OtherHandle);
	}

	void Append(const FGameplayAbilityTargetDataHandle& OtherHandle)
	{
		Data.Append(OtherHandle.Data);
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Comparison operator */
	bool operator==(const FGameplayAbilityTargetDataHandle& Other) const
	{
		// Both invalid structs or both valid and Pointer compare (???) // deep comparison equality
		if (Data.Num() != Other.Data.Num())
		{
			return false;
		}
		for (int32 i = 0; i < Data.Num(); ++i)
		{
			if (Data[i].IsValid() != Other.Data[i].IsValid())
			{
				return false;
			}
			if (Data[i].Get() != Other.Data[i].Get())
			{
				return false;
			}
		}
		return true;
	}

	/** Comparison operator */
	bool operator!=(const FGameplayAbilityTargetDataHandle& Other) const
	{
		return !(FGameplayAbilityTargetDataHandle::operator==(Other));
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetDataHandle> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetDataHandle>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FGameplayAbilityTargetData> Data is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
	};
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetingLocationInfo
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityTargetingLocationInfo()
	: LocationType(EGameplayAbilityTargetingLocationType::LiteralTransform)
	, SourceActor(nullptr)
	, SourceComponent(nullptr)
	, SourceAbility(nullptr)
	{
	};

	virtual ~FGameplayAbilityTargetingLocationInfo() {};

	void operator=(const FGameplayAbilityTargetingLocationInfo& Other)
	{
		LocationType = Other.LocationType;
		LiteralTransform = Other.LiteralTransform;
		SourceActor = Other.SourceActor;
		SourceComponent = Other.SourceComponent;
		SourceAbility = Other.SourceAbility;
		SourceSocketName = Other.SourceSocketName;
	}

public:
	FTransform GetTargetingTransform() const
	{
		//Return or calculate based on LocationType.
		switch (LocationType)
		{
		case EGameplayAbilityTargetingLocationType::ActorTransform:
			if (SourceActor)
			{
				return SourceActor->GetTransform();
			}
			break;
		case EGameplayAbilityTargetingLocationType::SocketTransform:
			if (SourceComponent)
			{
				return SourceComponent->GetSocketTransform(SourceSocketName);		//Bad socket name will just return component transform anyway, so we're safe
			}
			break;
		case EGameplayAbilityTargetingLocationType::LiteralTransform:
			return LiteralTransform;
		default:
			check(false);		//This case should not happen
			break;
		}
		//Error
		return FTransform::Identity;
	}

	FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromHitResult(TWeakObjectPtr<UGameplayAbility> Ability, const FHitResult& HitResult) const;
	FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromHitResults(TWeakObjectPtr<UGameplayAbility> Ability, const TArray<FHitResult>& HitResults) const;
	FGameplayAbilityTargetDataHandle MakeTargetDataHandleFromActors(const TArray<TWeakObjectPtr<AActor>>& TargetActors, bool OneActorPerHandle = false) const;

	/** Type of location used - will determine what data is transmitted over the network and what fields are used when calculating position. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	TEnumAsByte<EGameplayAbilityTargetingLocationType::Type> LocationType;

	/** A literal world transform can be used, if one has been calculated outside of the actor using the ability. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	FTransform LiteralTransform;

	/** A source actor is needed for Actor-based targeting, but not for Socket-based targeting. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	AActor* SourceActor;

	/** Socket-based targeting requires a skeletal mesh component to check for the named socket. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	UMeshComponent* SourceComponent;

	/** Ability that will be using the targeting data */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	UGameplayAbility* SourceAbility;

	/** If SourceComponent is valid, this is the name of the socket transform that will be used. If no Socket is provided, SourceComponent's transform will be used. */
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = Targeting)
	FName SourceSocketName;

	// -------------------------------------

	virtual FString ToString() const
	{
		return TEXT("FGameplayAbilityTargetingLocationInfo");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FGameplayAbilityTargetingLocationInfo::StaticStruct();
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetingLocationInfo> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetingLocationInfo>
{
	enum
	{
		WithNetSerializer = true	// For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetData_LocationInfo : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	/** Generic location data for source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Targeting)
	FGameplayAbilityTargetingLocationInfo SourceLocation;

	/** Generic location data for target */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Targeting)
	FGameplayAbilityTargetingLocationInfo TargetLocation;

	// -------------------------------------

	virtual bool HasOrigin() const override
	{
		return true;
	}

	virtual FTransform GetOrigin() const override
	{
		return SourceLocation.GetTargetingTransform();
	}

	// -------------------------------------

	virtual bool HasEndPoint() const override
	{
		return true;
	}

	virtual FVector GetEndPoint() const override
	{
		return TargetLocation.GetTargetingTransform().GetLocation();
	}

	// -------------------------------------

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_LocationInfo::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FGameplayAbilityTargetData_LocationInfo");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_LocationInfo> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_LocationInfo>
{
	enum
	{
		WithNetSerializer = true	// For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetData_ActorArray : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	/** We could be selecting this group of actors from any type of location, so use a generic location type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Targeting)
	FGameplayAbilityTargetingLocationInfo SourceLocation;

	/** Rather than targeting a single point, this type of targeting selects multiple actors. */
	UPROPERTY(EditAnywhere, Category = Targeting)
	TArray<TWeakObjectPtr<AActor> > TargetActorArray;

	virtual TArray<TWeakObjectPtr<AActor> >	GetActors() const override
	{
		return TargetActorArray;
	}

	virtual bool SetActors(TArray<TWeakObjectPtr<AActor>> NewActorArray) override
	{
		TargetActorArray = NewActorArray;
		return true;
	}

	// -------------------------------------

	virtual bool HasOrigin() const override
	{
		return true;
	}

	virtual FTransform GetOrigin() const override
	{
		FTransform ReturnTransform = SourceLocation.GetTargetingTransform();

		//Aim at first valid target, if we have one. Duplicating GetEndPoint() code here so we don't iterate through the target array twice.
		for (int32 i = 0; i < TargetActorArray.Num(); ++i)
		{
			if (TargetActorArray[i].IsValid())
			{
				FVector Direction = (TargetActorArray[i].Get()->GetActorLocation() - ReturnTransform.GetLocation()).GetSafeNormal();
				if (Direction.IsNormalized())
				{
					ReturnTransform.SetRotation(Direction.Rotation().Quaternion());
					break;
				}
			}
		}
		return ReturnTransform;
	}

	// -------------------------------------

	virtual bool HasEndPoint() const override
	{
		//We have an endpoint if we have at least one valid actor in our target array
		for (int32 i = 0; i < TargetActorArray.Num(); ++i)
		{
			if (TargetActorArray[i].IsValid())
			{
				return true;
			}
		}
		return false;
	}

	virtual FVector GetEndPoint() const override
	{
		for (int32 i = 0; i < TargetActorArray.Num(); ++i)
		{
			if (TargetActorArray[i].IsValid())
			{
				return TargetActorArray[i].Get()->GetActorLocation();
			}
		}
		return FVector::ZeroVector;
	}

	// -------------------------------------

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_ActorArray::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FGameplayAbilityTargetData_ActorArray");
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_ActorArray> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_ActorArray>
{
	enum
	{
		WithNetSerializer = true	// For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};

USTRUCT(BlueprintType)
struct GAMEPLAYABILITIES_API FGameplayAbilityTargetData_SingleTargetHit : public FGameplayAbilityTargetData
{
	GENERATED_USTRUCT_BODY()

	FGameplayAbilityTargetData_SingleTargetHit()
	{ }

	FGameplayAbilityTargetData_SingleTargetHit(FHitResult InHitResult)
		: HitResult(MoveTemp(InHitResult))
	{ }

	// -------------------------------------

	virtual TArray<TWeakObjectPtr<AActor> >	GetActors() const override
	{
		TArray<TWeakObjectPtr<AActor> >	Actors;
		if (HitResult.Actor.IsValid())
		{
			Actors.Push(HitResult.Actor);
		}
		return Actors;
	}

	// SetActors() will not work here because the actor "array" is drawn from the hit result data, and changing that doesn't make sense.

	// -------------------------------------

	virtual bool HasHitResult() const override
	{
		return true;
	}

	virtual const FHitResult* GetHitResult() const override
	{
		return &HitResult;
	}

	virtual bool HasOrigin() const override
	{
		return true;
	}

	virtual FTransform GetOrigin() const override
	{
		return FTransform((HitResult.TraceEnd - HitResult.TraceStart).Rotation(), HitResult.TraceStart);
	}

	virtual bool HasEndPoint() const override
	{
		return true;
	}

	virtual FVector GetEndPoint() const override
	{
		return HitResult.Location;
	}

	// -------------------------------------

	UPROPERTY()
	FHitResult	HitResult;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FGameplayAbilityTargetData_SingleTargetHit::StaticStruct();
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_SingleTargetHit> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_SingleTargetHit>
{
	enum
	{
		WithNetSerializer = true	// For now this is REQUIRED for FGameplayAbilityTargetDataHandle net serialization to work
	};
};

/** Generic callback for returning when target data is available */
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilityTargetData, const FGameplayAbilityTargetDataHandle&);


// ----------------------------------------------------

/** Generic callback for returning when target data is available */
DECLARE_MULTICAST_DELEGATE_TwoParams(FAbilityTargetDataSetDelegate, const FGameplayAbilityTargetDataHandle&, FGameplayTag);

/** These are generic, nonpayload carrying events that are replicated between the client and server */
UENUM()
namespace EAbilityGenericReplicatedEvent
{
	enum Type
	{	
		/** A generic confirmation to commit the ability */
		GenericConfirm = 0,
		/** A generic cancellation event. Not necessarily a canellation of the ability or targeting. Could be used to cancel out of a channelling portion of ability. */
		GenericCancel,
		/** Additional input presses of the ability (Press X to activate ability, press X again while it is active to do other things within the GameplayAbility's logic) */
		InputPressed,	
		/** Input release event of the ability */
		InputReleased,
		/** A generic event from the client */
		GenericSignalFromClient,
		/** A generic event from the server */
		GenericSignalFromServer,
		/** Custom events for game use */
		GameCustom1,
		GameCustom2,
		GameCustom3,
		GameCustom4,
		GameCustom5,
		GameCustom6,
		MAX
	};
}

struct FAbilityReplicatedData
{
	FAbilityReplicatedData() : bTriggered(false), VectorPayload(ForceInitToZero) {}
	/** Event has triggered */
	bool bTriggered;

	/** Optional Vector payload for event */
	FVector_NetQuantize100 VectorPayload;

	FSimpleMulticastDelegate Delegate;
};

/** Struct defining the cached data for a specific gameplay ability. This data is generally syncronized client->server in a network game. */
struct FAbilityReplicatedDataCache
{
	/** What elements this activation is targeting */
	FGameplayAbilityTargetDataHandle TargetData;

	/** What tag to pass through when doing an application */
	FGameplayTag ApplicationTag;

	/** True if we've been positively confirmed our targeting, false if we don't know */
	bool bTargetConfirmed;

	/** True if we've been positively cancelled our targeting, false if we don't know */
	bool bTargetCancelled;

	/** Delegate to call whenever this is modified */
	FAbilityTargetDataSetDelegate TargetSetDelegate;

	/** Delegate to call whenever this is confirmed (without target data) */
	FSimpleMulticastDelegate TargetCancelledDelegate;

	/** Generic events that contain no payload data */
	FAbilityReplicatedData	GenericEvents[EAbilityGenericReplicatedEvent::MAX];

	/** Prediction Key when this data was set */
	FPredictionKey PredictionKey;

	FAbilityReplicatedDataCache() : bTargetConfirmed(false), bTargetCancelled(false) {}

	/** Resets any cached data, leaves delegates up */
	void Reset()
	{
		bTargetConfirmed = bTargetCancelled = false;
		TargetData = FGameplayAbilityTargetDataHandle();
		ApplicationTag = FGameplayTag();
		PredictionKey = FPredictionKey();
		for (int32 i=0; i < (int32) EAbilityGenericReplicatedEvent::MAX; ++i)
		{
			GenericEvents[i].bTriggered = false;
			GenericEvents[i].VectorPayload = FVector::ZeroVector;
		}
	}
};
