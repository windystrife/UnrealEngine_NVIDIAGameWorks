// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 *  Blackboard - holds AI's world knowledge, easily accessible for behavior trees
 *
 *  Properties are stored in byte array, and should be accessed only though
 *  GetValue* / SetValue* functions. They will handle broadcasting change events
 *  for registered observers.
 *
 *  Keys are defined by BlackboardData data asset.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "AISystem.h"
#include "BehaviorTree/BlackboardData.h"
#include "BlackboardComponent.generated.h"

class UBrainComponent;

namespace EBlackboardDescription
{
	enum Type
	{
		OnlyValue,
		KeyWithValue,
		DetailedKeyWithValue,
		Full,
	};
}

UCLASS(ClassGroup = AI, meta = (BlueprintSpawnableComponent))
class AIMODULE_API UBlackboardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlackboardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** BEGIN UActorComponent overrides */
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	/** END UActorComponent overrides */

	/** @return name of key */
	FName GetKeyName(FBlackboard::FKey KeyID) const;

	/** @return key ID from name */
	FBlackboard::FKey GetKeyID(const FName& KeyName) const;

	/** @return class of value for given key */
	TSubclassOf<UBlackboardKeyType> GetKeyType(FBlackboard::FKey KeyID) const;

	/** @return true if the key is marked as instance synced */
	bool IsKeyInstanceSynced(FBlackboard::FKey KeyID) const;
	
	/** @return number of entries in data asset */
	int32 GetNumKeys() const;

	/** @return true if blackboard have valid data asset */
	bool HasValidAsset() const;

	/** register observer for blackboard key */
	FDelegateHandle RegisterObserver(FBlackboard::FKey KeyID, UObject* NotifyOwner, FOnBlackboardChangeNotification ObserverDelegate);

	/** unregister observer from blackboard key */
	void UnregisterObserver(FBlackboard::FKey KeyID, FDelegateHandle ObserverHandle);

	/** unregister all observers associated with given owner */
	void UnregisterObserversFrom(UObject* NotifyOwner);

	/** pause observer change notifications, any new ones will be added to a queue */
	void PauseObserverNotifications();

	/** resume observer change notifications and, optionally, process the queued observation list */
	void ResumeObserverNotifications(bool bSendQueuedObserverNotifications);

	/** pause change notifies and add them to queue */
	DEPRECATED(4.15, "Please call PauseObserverUpdates.")
	void PauseUpdates();

	/** resume change notifies and process queued list */
	DEPRECATED(4.15, "Please call ResumeObserverNotifications.")
	void ResumeUpdates();

	/** @return associated behavior tree component */
	UBrainComponent* GetBrainComponent() const;

	/** @return blackboard data asset */
	UBlackboardData* GetBlackboardAsset() const;

	/** caches UBrainComponent pointer to be used in communication */
	void CacheBrainComponent(UBrainComponent& BrainComponent);

	/** setup component for using given blackboard asset */
	bool InitializeBlackboard(UBlackboardData& NewAsset);
	
	/** @return true if component can be used with specified blackboard asset */
	bool IsCompatibleWith(UBlackboardData* TestAsset) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	UObject* GetValueAsObject(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	UClass* GetValueAsClass(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	uint8 GetValueAsEnum(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	int32 GetValueAsInt(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	float GetValueAsFloat(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	bool GetValueAsBool(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	FString GetValueAsString(const FName& KeyName) const;
	
	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	FName GetValueAsName(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	FVector GetValueAsVector(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	FRotator GetValueAsRotator(const FName& KeyName) const;

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsObject(const FName& KeyName, UObject* ObjectValue);
	
	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsClass(const FName& KeyName, UClass* ClassValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsEnum(const FName& KeyName, uint8 EnumValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsInt(const FName& KeyName, int32 IntValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsFloat(const FName& KeyName, float FloatValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsBool(const FName& KeyName, bool BoolValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsString(const FName& KeyName, FString StringValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsName(const FName& KeyName, FName NameValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	void SetValueAsVector(const FName& KeyName, FVector VectorValue);

	UFUNCTION(BlueprintCallable, Category = "AI|Components|Blackboard")
	void SetValueAsRotator(const FName& KeyName, FRotator VectorValue);

	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard", Meta=(
		Tooltip="If the vector value has been set (and not cleared), this function returns true (indicating that the value should be valid).  If it's not set, the vector value is invalid and this function will return false.  (Also returns false if the key specified does not hold a vector.)"))
	bool IsVectorValueSet(const FName& KeyName) const;
	bool IsVectorValueSet(FBlackboard::FKey KeyID) const;
		
	/** return false if call failed (most probably no such entry in BB) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	bool GetLocationFromEntry(const FName& KeyName, FVector& ResultLocation) const;
	bool GetLocationFromEntry(FBlackboard::FKey KeyID, FVector& ResultLocation) const;

	/** return false if call failed (most probably no such entry in BB) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|Blackboard")
	bool GetRotationFromEntry(const FName& KeyName, FRotator& ResultRotation) const;
	bool GetRotationFromEntry(FBlackboard::FKey KeyID, FRotator& ResultRotation) const;

	UFUNCTION(BlueprintCallable, Category = "AI|Components|Blackboard")
	void ClearValue(const FName& KeyName);
	void ClearValue(FBlackboard::FKey KeyID);

	template<class TDataClass>
	bool IsKeyOfType(FBlackboard::FKey KeyID) const;

	template<class TDataClass>
	bool SetValue(const FName& KeyName, typename TDataClass::FDataType Value);

	template<class TDataClass>
	bool SetValue(FBlackboard::FKey KeyID, typename TDataClass::FDataType Value);

	template<class TDataClass>
	typename TDataClass::FDataType GetValue(const FName& KeyName) const;

	template<class TDataClass>
	typename TDataClass::FDataType GetValue(FBlackboard::FKey KeyID) const;

	/** get pointer to raw data for given key */
	FORCEINLINE uint8* GetKeyRawData(const FName& KeyName) { return GetKeyRawData(GetKeyID(KeyName)); }
	FORCEINLINE uint8* GetKeyRawData(FBlackboard::FKey KeyID) { return ValueMemory.Num() && ValueOffsets.IsValidIndex(KeyID) ? (ValueMemory.GetData() + ValueOffsets[KeyID]) : NULL; }

	FORCEINLINE const uint8* GetKeyRawData(const FName& KeyName) const { return GetKeyRawData(GetKeyID(KeyName)); }
	FORCEINLINE const uint8* GetKeyRawData(FBlackboard::FKey KeyID) const { return ValueMemory.Num() && ValueOffsets.IsValidIndex(KeyID) ? (ValueMemory.GetData() + ValueOffsets[KeyID]) : NULL; }

	FORCEINLINE bool IsValidKey(FBlackboard::FKey KeyID) const { check(BlackboardAsset); return KeyID != FBlackboard::InvalidKey && BlackboardAsset->Keys.IsValidIndex(KeyID); }

	/** compares blackboard's values under specified keys */
	EBlackboardCompare::Type CompareKeyValues(TSubclassOf<UBlackboardKeyType> KeyType, FBlackboard::FKey KeyA, FBlackboard::FKey KeyB) const;

	FString GetDebugInfoString(EBlackboardDescription::Type Mode) const;

	/** get description of value under given key */
	FString DescribeKeyValue(const FName& KeyName, EBlackboardDescription::Type Mode) const;
	FString DescribeKeyValue(FBlackboard::FKey KeyID, EBlackboardDescription::Type Mode) const;

#if ENABLE_VISUAL_LOG
	/** prepare blackboard snapshot for logs */
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif
	
protected:

	/** cached behavior tree component */
	UPROPERTY(transient)
	UBrainComponent* BrainComp;

	/** data asset defining entries */
	UPROPERTY(transient)
	UBlackboardData* BlackboardAsset;

	/** memory block holding all values */
	TArray<uint8> ValueMemory;

	/** offsets in ValueMemory for each key */
	TArray<uint16> ValueOffsets;

	/** instanced keys with custom data allocations */
	UPROPERTY(transient)
	TArray<UBlackboardKeyType*> KeyInstances;

protected:
	/** observers registered for blackboard keys */
	mutable TMultiMap<uint8, FOnBlackboardChangeNotification> Observers;
	
	/** observers registered from owner objects */
	TMultiMap<UObject*, FDelegateHandle> ObserverHandles;

	/** queued key change notification, will be processed on ResumeUpdates call */
	mutable TArray<uint8> QueuedUpdates;

	/** set when observation notifies are paused and shouldn't be passed to observers */
	uint32 bPausedNotifies : 1;

	/** reset to false every time a new BB asset is assigned to this component */
	uint32 bSynchronizedKeyPopulated : 1;

	/** notifies behavior tree decorators about change in blackboard */
	void NotifyObservers(FBlackboard::FKey KeyID) const;

	/** initializes parent chain in asset */
	void InitializeParentChain(UBlackboardData* NewAsset);

	/** destroy allocated values */
	void DestroyValues();

	/** populates BB's synchronized entries */
	void PopulateSynchronizedKeys();

	bool ShouldSyncWithBlackboard(UBlackboardComponent& OtherBlackboardComponent) const;

	friend UBlackboardKeyType;
};

//////////////////////////////////////////////////////////////////////////
// Inlines

FORCEINLINE bool UBlackboardComponent::HasValidAsset() const
{
	return BlackboardAsset && BlackboardAsset->IsValid();
}

template<class TDataClass>
bool UBlackboardComponent::IsKeyOfType(FBlackboard::FKey KeyID) const
{
	const FBlackboardEntry* EntryInfo = BlackboardAsset ? BlackboardAsset->GetKey(KeyID) : nullptr;
	return (EntryInfo != nullptr) && (EntryInfo->KeyType != nullptr) && (EntryInfo->KeyType->GetClass() == TDataClass::StaticClass());
}

template<class TDataClass>
bool UBlackboardComponent::SetValue(const FName& KeyName, typename TDataClass::FDataType Value)
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	return SetValue<TDataClass>(KeyID, Value);
}

template<class TDataClass>
bool UBlackboardComponent::SetValue(FBlackboard::FKey KeyID, typename TDataClass::FDataType Value)
{
	const FBlackboardEntry* EntryInfo = BlackboardAsset ? BlackboardAsset->GetKey(KeyID) : nullptr;
	if ((EntryInfo == nullptr) || (EntryInfo->KeyType == nullptr) || (EntryInfo->KeyType->GetClass() != TDataClass::StaticClass()))
	{
		return false;
	}

	const uint16 DataOffset = EntryInfo->KeyType->HasInstance() ? sizeof(FBlackboardInstancedKeyMemory) : 0;
	uint8* RawData = GetKeyRawData(KeyID) + DataOffset;
	if (RawData)
	{
		UBlackboardKeyType* KeyOb = EntryInfo->KeyType->HasInstance() ? KeyInstances[KeyID] : EntryInfo->KeyType;
		const bool bChanged = TDataClass::SetValue((TDataClass*)KeyOb, RawData, Value);
		if (bChanged)
		{
			NotifyObservers(KeyID);
			if (BlackboardAsset->HasSynchronizedKeys() && IsKeyInstanceSynced(KeyID))
			{
				UAISystem* AISystem = UAISystem::GetCurrentSafe(GetWorld());
				for (auto Iter = AISystem->CreateBlackboardDataToComponentsIterator(*BlackboardAsset); Iter; ++Iter)
				{
					UBlackboardComponent* OtherBlackboard = Iter.Value();
					if (OtherBlackboard != nullptr && ShouldSyncWithBlackboard(*OtherBlackboard))
					{
						UBlackboardData* const OtherBlackboardAsset = OtherBlackboard->GetBlackboardAsset();
						const int32 OtherKeyID = OtherBlackboardAsset ? OtherBlackboardAsset->GetKeyID(EntryInfo->EntryName) : FBlackboard::InvalidKey;
						if (OtherKeyID != FBlackboard::InvalidKey)
						{
							UBlackboardKeyType* OtherKeyOb = EntryInfo->KeyType->HasInstance() ? OtherBlackboard->KeyInstances[OtherKeyID] : EntryInfo->KeyType;
							uint8* OtherRawData = OtherBlackboard->GetKeyRawData(OtherKeyID) + DataOffset;

							TDataClass::SetValue((TDataClass*)OtherKeyOb, OtherRawData, Value);
							OtherBlackboard->NotifyObservers(OtherKeyID);
						}
					}
				}
			}
		}

		return true;
	}

	return false;
}

template<class TDataClass>
typename TDataClass::FDataType UBlackboardComponent::GetValue(const FName& KeyName) const
{
	const FBlackboard::FKey KeyID = GetKeyID(KeyName);
	return GetValue<TDataClass>(KeyID);
}

template<class TDataClass>
typename TDataClass::FDataType UBlackboardComponent::GetValue(FBlackboard::FKey KeyID) const
{
	const FBlackboardEntry* EntryInfo = BlackboardAsset ? BlackboardAsset->GetKey(KeyID) : nullptr;
	if ((EntryInfo == nullptr) || (EntryInfo->KeyType == nullptr) || (EntryInfo->KeyType->GetClass() != TDataClass::StaticClass()))
	{
		return TDataClass::InvalidValue;
	}

	UBlackboardKeyType* KeyOb = EntryInfo->KeyType->HasInstance() ? KeyInstances[KeyID] : EntryInfo->KeyType;
	const uint16 DataOffset = EntryInfo->KeyType->HasInstance() ? sizeof(FBlackboardInstancedKeyMemory) : 0;

	const uint8* RawData = GetKeyRawData(KeyID) + DataOffset;
	return RawData ? TDataClass::GetValue((TDataClass*)KeyOb, RawData) : TDataClass::InvalidValue;
}
