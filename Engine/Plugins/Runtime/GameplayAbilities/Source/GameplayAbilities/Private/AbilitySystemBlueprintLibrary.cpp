// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectAggregator.h"
#include "GameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

UAbilitySystemBlueprintLibrary::UAbilitySystemBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UAbilitySystemComponent* UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AActor *Actor)
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
}

void UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload)
{
	if (Actor && !Actor->IsPendingKill())
	{
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor);
		if (AbilitySystemInterface != NULL)
		{
			UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent();
			if (AbilitySystemComponent != NULL)
			{
				FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
				AbilitySystemComponent->HandleGameplayEvent(EventTag, &Payload);
			}
		}
	}
}

bool UAbilitySystemBlueprintLibrary::IsValid(FGameplayAttribute Attribute)
{
	return Attribute.IsValid();
}

float UAbilitySystemBlueprintLibrary::GetFloatAttribute(const class AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	const UAbilitySystemComponent* const AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);

	return GetFloatAttributeFromAbilitySystemComponent(AbilitySystem, Attribute, bSuccessfullyFoundAttribute);
}

float UAbilitySystemBlueprintLibrary::GetFloatAttributeFromAbilitySystemComponent(const class UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	bSuccessfullyFoundAttribute = true;

	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccessfullyFoundAttribute = false;
		return 0.f;
	}

	const float Result = AbilitySystem->GetNumericAttribute(Attribute);
	return Result;
}

float UAbilitySystemBlueprintLibrary::GetFloatAttributeBase(const AActor* Actor, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	const UAbilitySystemComponent* const AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return GetFloatAttributeBaseFromAbilitySystemComponent(AbilitySystem, Attribute, bSuccessfullyFoundAttribute);
}

float UAbilitySystemBlueprintLibrary::GetFloatAttributeBaseFromAbilitySystemComponent(const UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bSuccessfullyFoundAttribute)
{
	float Result = 0.f;
	bSuccessfullyFoundAttribute = false;

	if (AbilitySystemComponent && AbilitySystemComponent->HasAttributeSetForAttribute(Attribute))
	{
		bSuccessfullyFoundAttribute = true;
		Result = AbilitySystemComponent->GetNumericAttributeBase(Attribute);
	}

	return Result;
}

float UAbilitySystemBlueprintLibrary::EvaluateAttributeValueWithTags(UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, bool& bSuccess)
{
	float RetVal = 0.f;
	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccess = false;
		return RetVal;
	}

	FGameplayEffectAttributeCaptureDefinition Capture(Attribute, EGameplayEffectAttributeCaptureSource::Source, true);

	FGameplayEffectAttributeCaptureSpec CaptureSpec(Capture);
	AbilitySystem->CaptureAttributeForGameplayEffect(CaptureSpec);

	FAggregatorEvaluateParameters EvalParams;

	EvalParams.SourceTags = &SourceTags;
	EvalParams.TargetTags = &TargetTags;

	bSuccess = CaptureSpec.AttemptCalculateAttributeMagnitude(EvalParams, RetVal);

	return RetVal;
}

float UAbilitySystemBlueprintLibrary::EvaluateAttributeValueWithTagsAndBase(UAbilitySystemComponent* AbilitySystem, FGameplayAttribute Attribute, const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, float BaseValue, bool& bSuccess)
{
	float RetVal = 0.f;
	if (!AbilitySystem || !AbilitySystem->HasAttributeSetForAttribute(Attribute))
	{
		bSuccess = false;
		return RetVal;
	}

	FGameplayEffectAttributeCaptureDefinition Capture(Attribute, EGameplayEffectAttributeCaptureSource::Source, true);

	FGameplayEffectAttributeCaptureSpec CaptureSpec(Capture);
	AbilitySystem->CaptureAttributeForGameplayEffect(CaptureSpec);

	FAggregatorEvaluateParameters EvalParams;

	EvalParams.SourceTags = &SourceTags;
	EvalParams.TargetTags = &TargetTags;

	bSuccess = CaptureSpec.AttemptCalculateAttributeMagnitudeWithBase(EvalParams, BaseValue, RetVal);

	return RetVal;
}

bool UAbilitySystemBlueprintLibrary::EqualEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB)
{
	return (AttributeA == AttributeB);
}

bool UAbilitySystemBlueprintLibrary::NotEqual_GameplayAttributeGameplayAttribute(FGameplayAttribute AttributeA, FGameplayAttribute AttributeB)
{
	return (AttributeA != AttributeB);
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AppendTargetDataHandle(FGameplayAbilityTargetDataHandle TargetHandle, const FGameplayAbilityTargetDataHandle& HandleToAdd)
{
	TargetHandle.Append(HandleToAdd);
	return TargetHandle;
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromLocations(const FGameplayAbilityTargetingLocationInfo& SourceLocation, const FGameplayAbilityTargetingLocationInfo& TargetLocation)
{
	// Construct TargetData
	FGameplayAbilityTargetData_LocationInfo*	NewData = new FGameplayAbilityTargetData_LocationInfo();
	NewData->SourceLocation = SourceLocation;
	NewData->TargetLocation = TargetLocation;

	// Give it a handle and return
	FGameplayAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData_LocationInfo>(NewData));
	return Handle;
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActor(AActor* Actor)
{
	// Construct TargetData
	FGameplayAbilityTargetData_ActorArray*	NewData = new FGameplayAbilityTargetData_ActorArray();
	NewData->TargetActorArray.Add(Actor);
	FGameplayAbilityTargetDataHandle		Handle(NewData);
	return Handle;
}
FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(const TArray<AActor*>& ActorArray, bool OneTargetPerHandle)
{
	// Construct TargetData
	if (OneTargetPerHandle)
	{
		FGameplayAbilityTargetDataHandle Handle;
		for (int32 i = 0; i < ActorArray.Num(); ++i)
		{
			if (::IsValid(ActorArray[i]))
			{
				FGameplayAbilityTargetDataHandle TempHandle = AbilityTargetDataFromActor(ActorArray[i]);
				Handle.Append(TempHandle);
			}
		}
		return Handle;
	}
	else
	{
		FGameplayAbilityTargetData_ActorArray*	NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Reset();
		for (auto Actor : ActorArray)
		{
			NewData->TargetActorArray.Add(Actor);
		}
		FGameplayAbilityTargetDataHandle		Handle(NewData);
		return Handle;
	}
}

FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::FilterTargetData(const FGameplayAbilityTargetDataHandle& TargetDataHandle, FGameplayTargetDataFilterHandle FilterHandle)
{
	FGameplayAbilityTargetDataHandle ReturnDataHandle;
	
	for (int32 i = 0; TargetDataHandle.IsValid(i); ++i)
	{
		const FGameplayAbilityTargetData* UnfilteredData = TargetDataHandle.Get(i);
		check(UnfilteredData);
		if (UnfilteredData->GetActors().Num() > 0)
		{
			TArray<TWeakObjectPtr<AActor>> FilteredActors = UnfilteredData->GetActors().FilterByPredicate(FilterHandle);
			if (FilteredActors.Num() > 0)
			{
				//Copy the data first, since we don't understand the internals of it
				const UScriptStruct* ScriptStruct = UnfilteredData->GetScriptStruct();
				FGameplayAbilityTargetData* NewData = (FGameplayAbilityTargetData*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
				ScriptStruct->InitializeStruct(NewData);
				ScriptStruct->CopyScriptStruct(NewData, UnfilteredData);
				ReturnDataHandle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(NewData));
				if (FilteredActors.Num() < UnfilteredData->GetActors().Num())
				{
					//We have lost some, but not all, of our actors, so replace the array. This should only be possible with targeting types that permit actor-array setting.
					if (!NewData->SetActors(FilteredActors))
					{
						//This is an error, though we could ignore it. We somehow filtered out part of a list, but the class doesn't support changing the list, so now it's all or nothing.
						check(false);
					}
				}
			}
		}
	}

	return ReturnDataHandle;
}

FGameplayTargetDataFilterHandle UAbilitySystemBlueprintLibrary::MakeFilterHandle(FGameplayTargetDataFilter Filter, AActor* FilterActor)
{
	FGameplayTargetDataFilterHandle FilterHandle;
	FGameplayTargetDataFilter* NewFilter = new FGameplayTargetDataFilter(Filter);
	NewFilter->InitializeFilterContext(FilterActor);
	FilterHandle.Filter = TSharedPtr<FGameplayTargetDataFilter>(NewFilter);
	return FilterHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::MakeSpecHandle(UGameplayEffect* InGameplayEffect, AActor* InInstigator, AActor* InEffectCauser, float InLevel)
{
	FGameplayEffectContext* EffectContext = new FGameplayEffectContext(InInstigator, InEffectCauser);
	return FGameplayEffectSpecHandle(new FGameplayEffectSpec(InGameplayEffect, FGameplayEffectContextHandle(EffectContext), InLevel));
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::CloneSpecHandle(AActor* InNewInstigator, AActor* InEffectCauser, FGameplayEffectSpecHandle GameplayEffectSpecHandle_Clone)
{
	FGameplayEffectContext* EffectContext = new FGameplayEffectContext(InNewInstigator, InEffectCauser);

	return FGameplayEffectSpecHandle(new FGameplayEffectSpec(*GameplayEffectSpecHandle_Clone.Data.Get(), FGameplayEffectContextHandle(EffectContext)));
}


FGameplayAbilityTargetDataHandle UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(const FHitResult& HitResult)
{
	// Construct TargetData
	FGameplayAbilityTargetData_SingleTargetHit* TargetData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);

	// Give it a handle and return
	FGameplayAbilityTargetDataHandle	Handle;
	Handle.Data.Add(TSharedPtr<FGameplayAbilityTargetData>(TargetData));

	return Handle;
}

int32 UAbilitySystemBlueprintLibrary::GetDataCountFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData)
{
	return TargetData.Data.Num();
}

TArray<AActor*> UAbilitySystemBlueprintLibrary::GetActorsFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		TArray<AActor*>	ResolvedArray;
		if (Data)
		{
			TArray<TWeakObjectPtr<AActor> > WeakArray = Data->GetActors();
			for (TWeakObjectPtr<AActor> WeakPtr : WeakArray)
			{
				ResolvedArray.Add(WeakPtr.Get());
			}
		}
		return ResolvedArray;
	}
	return TArray<AActor*>();
}

bool UAbilitySystemBlueprintLibrary::DoesTargetDataContainActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index, AActor* Actor)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			for (auto DataActorIter : Data->GetActors())
			{
				if (DataActorIter == Actor)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasActor(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->GetActors().Num() > 0);
		}
	}
	return false;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasHitResult(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->HasHitResult();
		}
	}
	return false;
}

FHitResult UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return *HitResultPtr;
			}
		}
	}

	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index) == false)
	{
		return false;
	}

	FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		return (Data->HasHitResult() || Data->HasOrigin());
	}
	return false;
}

FTransform UAbilitySystemBlueprintLibrary::GetTargetDataOrigin(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index) == false)
	{
		return FTransform::Identity;
	}

	FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
	if (Data)
	{
		if (Data->HasOrigin())
		{
			return Data->GetOrigin();
		}
		if (Data->HasHitResult())
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			FTransform ReturnTransform;
			ReturnTransform.SetLocation(HitResultPtr->TraceStart);
			ReturnTransform.SetRotation((HitResultPtr->Location - HitResultPtr->TraceStart).GetSafeNormal().Rotation().Quaternion());
			return ReturnTransform;
		}
	}

	return FTransform::Identity;
}

bool UAbilitySystemBlueprintLibrary::TargetDataHasEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return (Data->HasHitResult() || Data->HasEndPoint());
		}
	}
	return false;
}

FVector UAbilitySystemBlueprintLibrary::GetTargetDataEndPoint(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			const FHitResult* HitResultPtr = Data->GetHitResult();
			if (HitResultPtr)
			{
				return HitResultPtr->Location;
			}
			else if (Data->HasEndPoint())
			{
				return Data->GetEndPoint();
			}
		}
	}

	return FVector::ZeroVector;
}

FTransform UAbilitySystemBlueprintLibrary::GetTargetDataEndPointTransform(const FGameplayAbilityTargetDataHandle& TargetData, int32 Index)
{
	if (TargetData.Data.IsValidIndex(Index))
	{
		FGameplayAbilityTargetData* Data = TargetData.Data[Index].Get();
		if (Data)
		{
			return Data->GetEndPointTransform();
		}
	}

	return FTransform::Identity;
}


// -------------------------------------------------------------------------------------

bool UAbilitySystemBlueprintLibrary::EffectContextIsValid(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.IsValid();
}

bool UAbilitySystemBlueprintLibrary::EffectContextIsInstigatorLocallyControlled(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.IsLocallyControlled();
}

FHitResult UAbilitySystemBlueprintLibrary::EffectContextGetHitResult(FGameplayEffectContextHandle EffectContext)
{
	if (EffectContext.GetHitResult())
	{
		return *EffectContext.GetHitResult();
	}

	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::EffectContextHasHitResult(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetHitResult() != NULL;
}

void UAbilitySystemBlueprintLibrary::EffectContextAddHitResult(FGameplayEffectContextHandle EffectContext, FHitResult HitResult, bool bReset)
{
	EffectContext.AddHitResult(HitResult, bReset);
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetInstigatorActor(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetInstigator();
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetOriginalInstigatorActor(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetOriginalInstigator();
}

AActor*	UAbilitySystemBlueprintLibrary::EffectContextGetEffectCauser(FGameplayEffectContextHandle EffectContext)
{
	return EffectContext.GetEffectCauser();
}

UObject* UAbilitySystemBlueprintLibrary::EffectContextGetSourceObject(FGameplayEffectContextHandle EffectContext)
{
	return const_cast<UObject*>( EffectContext.GetSourceObject() );
}

FVector UAbilitySystemBlueprintLibrary::EffectContextGetOrigin(FGameplayEffectContextHandle EffectContext)
{
	if (EffectContext.HasOrigin())
	{
		return EffectContext.GetOrigin();
	}

	return FVector::ZeroVector;
}

void UAbilitySystemBlueprintLibrary::EffectContextSetOrigin(FGameplayEffectContextHandle EffectContext, FVector Origin)
{
	EffectContext.AddOrigin(Origin);
}

bool UAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlled(FGameplayCueParameters Parameters)
{
	return Parameters.IsInstigatorLocallyControlled();
}

bool UAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlledPlayer(FGameplayCueParameters Parameters)
{
	return Parameters.IsInstigatorLocallyControlledPlayer();
}

int32 UAbilitySystemBlueprintLibrary::GetActorCount(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.GetActors().Num();
}

AActor* UAbilitySystemBlueprintLibrary::GetActorByIndex(FGameplayCueParameters Parameters, int32 Index)
{
	if (Parameters.EffectContext.GetActors().IsValidIndex(Index))
	{
		return Parameters.EffectContext.GetActors()[Index].Get();
	}
	return NULL;
}

FHitResult UAbilitySystemBlueprintLibrary::GetHitResult(FGameplayCueParameters Parameters)
{
	if (Parameters.EffectContext.GetHitResult())
	{
		return *Parameters.EffectContext.GetHitResult();
	}
	
	return FHitResult();
}

bool UAbilitySystemBlueprintLibrary::HasHitResult(FGameplayCueParameters Parameters)
{
	return Parameters.EffectContext.GetHitResult() != NULL;
}

void UAbilitySystemBlueprintLibrary::ForwardGameplayCueToTarget(TScriptInterface<IGameplayCueInterface> TargetCueInterface, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	AActor* ActorTarget = Cast<AActor>(TargetCueInterface.GetObject());
	if (TargetCueInterface && ActorTarget)
	{
		TargetCueInterface->HandleGameplayCue(ActorTarget, Parameters.OriginalTag, EventType, Parameters);
	}
}

AActor*	UAbilitySystemBlueprintLibrary::GetInstigatorActor(FGameplayCueParameters Parameters)
{
	return Parameters.GetInstigator();
}

FTransform UAbilitySystemBlueprintLibrary::GetInstigatorTransform(FGameplayCueParameters Parameters)
{
	AActor* InstigatorActor = GetInstigatorActor(Parameters);
	if (InstigatorActor)
	{
		return InstigatorActor->GetTransform();
	}

	ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::GetInstigatorTransform called on GameplayCue with no valid instigator"));
	return FTransform::Identity;
}

FVector UAbilitySystemBlueprintLibrary::GetOrigin(FGameplayCueParameters Parameters)
{
	if (Parameters.EffectContext.HasOrigin())
	{
		return Parameters.EffectContext.GetOrigin();
	}

	return Parameters.Location;
}

bool UAbilitySystemBlueprintLibrary::GetGameplayCueEndLocationAndNormal(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Location, FVector& Normal)
{
	FGameplayEffectContext* Data = Parameters.EffectContext.Get();
	if (Parameters.Location.IsNearlyZero() == false)
	{
		Location = Parameters.Location;
		Normal = Parameters.Normal;
	}
	else if (Data && Data->GetHitResult())
	{
		Location = Data->GetHitResult()->Location;
		Normal = Data->GetHitResult()->Normal;
		return true;
	}
	else if(TargetActor)
	{
		Location = TargetActor->GetActorLocation();
		Normal = TargetActor->GetActorForwardVector();
		return true;
	}

	return false;
}

bool UAbilitySystemBlueprintLibrary::GetGameplayCueDirection(AActor* TargetActor, FGameplayCueParameters Parameters, FVector& Direction)
{
	if (Parameters.Normal.IsNearlyZero() == false)
	{
		Direction = -Parameters.Normal;
		return true;
	}

	if (FGameplayEffectContext* Ctx = Parameters.EffectContext.Get())
	{
		if (Ctx->GetHitResult())
		{
			// Most projectiles and melee attacks will use this
			Direction = (-1.f * Ctx->GetHitResult()->Normal);
			return true;
		}
		else if (TargetActor && Ctx->HasOrigin())
		{
			// Fallback to trying to use the target location and the origin of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetOrigin());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
		else if (TargetActor && Ctx->GetEffectCauser())
		{
			// Finally, try to use the direction between the causer of the effect and the target of the effect
			FVector NewVec = (TargetActor->GetActorLocation() - Ctx->GetEffectCauser()->GetActorLocation());
			NewVec.Normalize();
			Direction = NewVec;
			return true;
		}
	}

	Direction = FVector::ZeroVector;
	return false;
}

bool UAbilitySystemBlueprintLibrary::DoesGameplayCueMeetTagRequirements(FGameplayCueParameters Parameters, FGameplayTagRequirements& SourceTagReqs, FGameplayTagRequirements& TargetTagReqs)
{
	return SourceTagReqs.RequirementsMet(Parameters.AggregatedSourceTags)
		&& TargetTagReqs.RequirementsMet(Parameters.AggregatedSourceTags);
}

// ---------------------------------------------------------------------------------------

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FName DataName, float Magnitude)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		
		Spec->SetSetByCallerMagnitude(DataName, Magnitude);
		
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayTag DataTag, float Magnitude)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(DataTag, Magnitude);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AssignSetByCallerTagMagnitude called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetDuration(FGameplayEffectSpecHandle SpecHandle, float Duration)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->SetDuration(Duration, true);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::SetDuration called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddGrantedTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AddTag(NewGameplayTag);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddGrantedTag called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddGrantedTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicGrantedTags.AppendTags(NewGameplayTags);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddGrantedTags called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddAssetTag(FGameplayEffectSpecHandle SpecHandle, FGameplayTag NewGameplayTag)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicAssetTags.AddTag(NewGameplayTag);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddEffectTag called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddAssetTags(FGameplayEffectSpecHandle SpecHandle, FGameplayTagContainer NewGameplayTags)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->DynamicAssetTags.AppendTags(NewGameplayTags);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddEffectTags called with invalid SpecHandle"));
	}

	return SpecHandle;
}
	
FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec(FGameplayEffectSpecHandle SpecHandle, FGameplayEffectSpecHandle LinkedGameplayEffectSpec)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->TargetEffectSpecs.Add(LinkedGameplayEffectSpec);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffect(FGameplayEffectSpecHandle SpecHandle, TSubclassOf<UGameplayEffect> LinkedGameplayEffect)
{
	FGameplayEffectSpecHandle LinkedSpecHandle;
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		FGameplayEffectSpec* LinkedSpec = new FGameplayEffectSpec();
		LinkedSpec->InitializeFromLinkedSpec(LinkedGameplayEffect->GetDefaultObject<UGameplayEffect>(), *Spec);

		LinkedSpecHandle = FGameplayEffectSpecHandle(LinkedSpec);
		Spec->TargetEffectSpecs.Add(LinkedSpecHandle);
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}

	return LinkedSpecHandle;
}


FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetStackCount(FGameplayEffectSpecHandle SpecHandle, int32 StackCount)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec)
	{
		Spec->StackCount = StackCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}
	
FGameplayEffectSpecHandle UAbilitySystemBlueprintLibrary::SetStackCountToMax(FGameplayEffectSpecHandle SpecHandle)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (Spec && Spec->Def)
	{
		Spec->StackCount = Spec->Def->StackLimitCount;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::AddLinkedGameplayEffectSpec called with invalid SpecHandle"));
	}
	return SpecHandle;
}

FGameplayEffectContextHandle UAbilitySystemBlueprintLibrary::GetEffectContext(FGameplayEffectSpecHandle SpecHandle)
{
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		return Spec->GetEffectContext();
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::GetEffectContext called with invalid SpecHandle"));
	}

	return FGameplayEffectContextHandle();
}

TArray<FGameplayEffectSpecHandle> UAbilitySystemBlueprintLibrary::GetAllLinkedGameplayEffectSpecHandles(FGameplayEffectSpecHandle SpecHandle)
{
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		return Spec->TargetEffectSpecs;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilitySystemBlueprintLibrary::GetEffectContext called with invalid SpecHandle"));
	}

	TArray<FGameplayEffectSpecHandle> Handles;
	return Handles;
}

int32 UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectStackCount(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		return ASC->GetCurrentStackCount(ActiveHandle);
	}
	return 0;
}

int32 UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectStackLimitCount(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		const UGameplayEffect* ActiveGE = ASC->GetGameplayEffectDefForHandle(ActiveHandle);
		if (ActiveGE)
		{
			return ActiveGE->StackLimitCount;
		}
	}
	return 0;
}

float UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectStartTime(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveHandle))
		{
			return ActiveGE->StartWorldTime;
		}
	}
	return 0;
}
	
float UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectExpectedEndTime(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveHandle))
		{
			return ActiveGE->GetEndTime();
		}
	}
	return 0;
}

float UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectTotalDuration(FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveHandle))
		{
			return ActiveGE->GetDuration();
		}
	}
	return 0;
}
float UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectRemainingDuration(UObject* WorldContextObject, FActiveGameplayEffectHandle ActiveHandle)
{
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveHandle))
		{
			if (WorldContextObject)
			{
				if (UWorld* World = WorldContextObject->GetWorld())
				{
					return ActiveGE->GetTimeRemaining(World->GetTimeSeconds());
				}
			}
		}
	}
	return 0;
}

float UAbilitySystemBlueprintLibrary::GetModifiedAttributeMagnitude(const FGameplayEffectSpec& Spec, FGameplayAttribute Attribute)
{
	float Delta = 0.f;
	for (const FGameplayEffectModifiedAttribute &Mod : Spec.ModifiedAttributes)
	{
		if (Mod.Attribute == Attribute)
		{
			Delta += Mod.TotalMagnitude;
		}
	}
	return Delta;
}

float UAbilitySystemBlueprintLibrary::GetModifiedAttributeMagnitude(FGameplayEffectSpecHandle SpecHandle, FGameplayAttribute Attribute)
{
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	float Delta = 0.f;
	if (Spec)
	{
		return GetModifiedAttributeMagnitude(*Spec, Attribute);
	}
	return 0;
}

FString UAbilitySystemBlueprintLibrary::GetActiveGameplayEffectDebugString(FActiveGameplayEffectHandle ActiveHandle)
{
	FString Str;
	UAbilitySystemComponent* ASC = ActiveHandle.GetOwningAbilitySystemComponent();
	if (ASC)
	{
		Str = ASC->GetActiveGEDebugString(ActiveHandle);
	}
	return Str;
}
