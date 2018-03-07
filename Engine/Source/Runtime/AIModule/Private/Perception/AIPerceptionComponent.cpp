// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/Controller.h"
#include "AIController.h"
#include "Perception/AISenseConfig.h"
#include "VisualLogger/VisualLogger.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerTypes.h"
#include "GameplayDebuggerCategory.h"
#endif



DECLARE_CYCLE_STAT(TEXT("Requesting UAIPerceptionComponent::RemoveDeadData call from within a const function"),
	STAT_FSimpleDelegateGraphTask_RequestingRemovalOfDeadPerceptionData,
	STATGROUP_TaskGraphTasks);

//----------------------------------------------------------------------//
// FActorPerceptionInfo
//----------------------------------------------------------------------//
void FActorPerceptionInfo::Merge(const FActorPerceptionInfo& Other)
{
	for (uint32 Index = 0; Index < FAISenseID::GetSize(); ++Index)
	{
		if (LastSensedStimuli[Index].GetAge() > Other.LastSensedStimuli[Index].GetAge())
		{
			LastSensedStimuli[Index] = Other.LastSensedStimuli[Index];
		}
	}
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
FActorPerceptionBlueprintInfo::FActorPerceptionBlueprintInfo(const FActorPerceptionInfo& Info)
{
	Target = Info.Target.Get();
	LastSensedStimuli = Info.LastSensedStimuli;
	bIsHostile = Info.bIsHostile;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
const int32 UAIPerceptionComponent::InitialStimuliToProcessArraySize = 10;

UAIPerceptionComponent::UAIPerceptionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCleanedUp(false)
	, PerceptionListenerId(FPerceptionListenerID::InvalidID())
{
}

void UAIPerceptionComponent::RequestStimuliListenerUpdate()
{
	UAIPerceptionSystem* AIPerceptionSys = UAIPerceptionSystem::GetCurrent(GetWorld());
	if (AIPerceptionSys != NULL)
	{
		AIPerceptionSys->UpdateListener(*this);
	}
}

namespace
{
	struct FConfigOfSenseID
	{
		FConfigOfSenseID(const FAISenseID& InSenseID)
			: SenseID(InSenseID)
		{}

		bool operator()(const UAISenseConfig* SenseConfig) const
		{
			return SenseConfig && SenseConfig->GetSenseID() == SenseID;
		}

		const FAISenseID SenseID;
	};
}

const UAISenseConfig* UAIPerceptionComponent::GetSenseConfig(const FAISenseID& SenseID) const
{
	int32 ConfigIndex = SensesConfig.IndexOfByPredicate(FConfigOfSenseID(SenseID));
	return ConfigIndex != INDEX_NONE ? SensesConfig[ConfigIndex] : nullptr;
}

UAISenseConfig* UAIPerceptionComponent::GetSenseConfig(const FAISenseID& SenseID)
{
	int32 ConfigIndex = SensesConfig.IndexOfByPredicate(FConfigOfSenseID(SenseID));
	return ConfigIndex != INDEX_NONE ? SensesConfig[ConfigIndex] : nullptr;
}

void UAIPerceptionComponent::PostInitProperties() 
{
	Super::PostInitProperties();

	if (DominantSense)
	{
		DominantSenseID = UAISense::GetSenseID(DominantSense);
	}
}

void UAIPerceptionComponent::ConfigureSense(UAISenseConfig& Config)
{
	int32 ConfigIndex = INDEX_NONE;
	// first check if we're reconfiguring a sense
	for (int32 Index = 0; Index < SensesConfig.Num(); ++Index)
	{
		if (SensesConfig[Index]->GetClass() == Config.GetClass())
		{
			ConfigIndex = Index;
			SensesConfig[Index] = &Config;
			SetMaxStimulusAge(Index, Config.GetMaxAge());
			break;
		}
	}

	if (ConfigIndex == INDEX_NONE)
	{
		ConfigIndex = SensesConfig.Add(&Config);
		SetMaxStimulusAge(ConfigIndex, Config.GetMaxAge());
	}

	if (IsRegistered())
	{
		RequestStimuliListenerUpdate();
	}
	// else the sense will be auto-configured during OnRegister
}

void UAIPerceptionComponent::SetMaxStimulusAge(int32 ConfigIndex, float MaxAge)
{
	if (MaxActiveAge.IsValidIndex(ConfigIndex) == false)
	{
		MaxActiveAge.AddUninitialized(ConfigIndex - MaxActiveAge.Num() + 1);
	}
	MaxActiveAge[ConfigIndex] = MaxAge;

	// @todo process all data already gathered and see if any _still_active_ stimuli
	// got it's expiration prolonged, with SetExpirationAge
}

void UAIPerceptionComponent::OnRegister()
{
	Super::OnRegister();

	bCleanedUp = false;

	AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		Owner->OnEndPlay.AddUniqueDynamic(this, &UAIPerceptionComponent::OnOwnerEndPlay);
		AIOwner = Cast<AAIController>(Owner);
	}

	UAIPerceptionSystem* AIPerceptionSys = UAIPerceptionSystem::GetCurrent(GetWorld());
	if (AIPerceptionSys != nullptr)
	{
		PerceptionFilter.Clear();

		if (SensesConfig.Num() > 0)
		{
			// set up perception listener based on SensesConfig
			for (auto SenseConfig : SensesConfig)
			{
				if (SenseConfig)
				{
					TSubclassOf<UAISense> SenseImplementation = SenseConfig->GetSenseImplementation();

					if (SenseImplementation)
					{
						// make sure it's registered with perception system
						AIPerceptionSys->RegisterSenseClass(SenseImplementation);

						if (SenseConfig->IsEnabled())
						{
							PerceptionFilter.AcceptChannel(UAISense::GetSenseID(SenseImplementation));
						}

						const FAISenseID SenseID = UAISense::GetSenseID(SenseImplementation);
						check(SenseID.IsValid());
						SetMaxStimulusAge(SenseID, SenseConfig->GetMaxAge());
					}
				}
			}

			AIPerceptionSys->UpdateListener(*this);
		}
	}

	// this should not be needed but aparently AAIController::PostRegisterAllComponents
	// gets called component's OnRegister
	AIOwner = Cast<AAIController>(GetOwner());
	ensure(AIOwner == nullptr || AIOwner->GetAIPerceptionComponent() == nullptr || AIOwner->GetAIPerceptionComponent() == this
		|| (AIOwner->GetWorld() && AIOwner->GetWorld()->WorldType != EWorldType::Editor));
	if (AIOwner && AIOwner->GetAIPerceptionComponent() == nullptr)
	{
		AIOwner->SetPerceptionComponent(*this);
	}
}

void UAIPerceptionComponent::OnUnregister()
{
	CleanUp();
	Super::OnUnregister();
}

void UAIPerceptionComponent::OnOwnerEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason != EEndPlayReason::EndPlayInEditor && EndPlayReason != EEndPlayReason::Quit)
	{
		CleanUp();
	}
}

void UAIPerceptionComponent::CleanUp()
{
	if (bCleanedUp == false)
	{
		UAIPerceptionSystem* AIPerceptionSys = UAIPerceptionSystem::GetCurrent(GetWorld());
		if (AIPerceptionSys != nullptr)
		{
			AIPerceptionSys->UnregisterListener(*this);
			AActor* MutableBodyActor = GetMutableBodyActor();
			if (MutableBodyActor)
			{
				AIPerceptionSys->UnregisterSource(*MutableBodyActor);
			}
		}

		if (HasAnyFlags(RF_BeginDestroyed) == false)
		{
			AActor* Owner = GetOwner();
			if (Owner != nullptr)
			{
				Owner->OnEndPlay.RemoveDynamic(this, &UAIPerceptionComponent::OnOwnerEndPlay);
			}
		}

		bCleanedUp = true;
	}
}

void UAIPerceptionComponent::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		CleanUp();
	}
	Super::BeginDestroy();
}

void UAIPerceptionComponent::UpdatePerceptionWhitelist(const FAISenseID Channel, const bool bNewValue)
{
	const bool bCurrentValue = PerceptionFilter.ShouldRespondToChannel(Channel);
	if (bNewValue != bCurrentValue)
	{
		bNewValue ? PerceptionFilter.AcceptChannel(Channel) : PerceptionFilter.FilterOutChannel(Channel);
		RequestStimuliListenerUpdate();	
	}
}

void UAIPerceptionComponent::GetHostileActors(TArray<AActor*>& OutActors) const
{
	bool bDeadDataFound = false;

	OutActors.Reserve(PerceptualData.Num());
	for (FActorPerceptionContainer::TConstIterator DataIt = GetPerceptualDataConstIterator(); DataIt; ++DataIt)
	{
		if (DataIt->Value.bIsHostile && DataIt->Value.HasAnyKnownStimulus())
		{
			if (DataIt->Value.Target.IsValid())
			{
				OutActors.Add(DataIt->Value.Target.Get());
			}
			else
			{
				bDeadDataFound = true;
			}
		}
	}

	if (bDeadDataFound)
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UAIPerceptionComponent::RemoveDeadData),
			GET_STATID(STAT_FSimpleDelegateGraphTask_RequestingRemovalOfDeadPerceptionData), NULL, ENamedThreads::GameThread);
	}
}

const FActorPerceptionInfo* UAIPerceptionComponent::GetFreshestTrace(const FAISenseID Sense) const
{
	// @note will stop on first age 0 stimulus
	float BestAge = FAIStimulus::NeverHappenedAge;
	const FActorPerceptionInfo* Result = NULL;

	bool bDeadDataFound = false;
	
	for (FActorPerceptionContainer::TConstIterator DataIt = GetPerceptualDataConstIterator(); DataIt; ++DataIt)
	{
		const FActorPerceptionInfo* Info = &DataIt->Value;
		const float Age = Info->LastSensedStimuli[Sense].GetAge();
		if (Age < BestAge)
		{
			if (Info->Target.IsValid())
			{
				BestAge = Age;
				Result = Info;
				if (BestAge == 0.f)
				{
					// won't find any younger then this
					break;
				}
			}
			else
			{
				bDeadDataFound = true;
			}
		}
	}

	if (bDeadDataFound)
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UAIPerceptionComponent::RemoveDeadData),
			GET_STATID(STAT_FSimpleDelegateGraphTask_RequestingRemovalOfDeadPerceptionData), NULL, ENamedThreads::GameThread);
	}

	return Result;
}

void UAIPerceptionComponent::SetDominantSense(TSubclassOf<UAISense> InDominantSense)
{
	if (DominantSense != InDominantSense)
	{
		DominantSense = InDominantSense;
		DominantSenseID = UAISense::GetSenseID(InDominantSense);
		// update all perceptual info with this info
		for (FActorPerceptionContainer::TIterator DataIt = GetPerceptualDataIterator(); DataIt; ++DataIt)
		{
			DataIt->Value.DominantSense = DominantSenseID;
		}
	}
}

FGenericTeamId UAIPerceptionComponent::GetTeamIdentifier() const
{
	return AIOwner ? FGenericTeamId::GetTeamIdentifier(AIOwner) : FGenericTeamId::NoTeam;
}

FVector UAIPerceptionComponent::GetActorLocation(const AActor& Actor) const 
{ 
	// not that Actor == NULL is valid
	const FActorPerceptionInfo* ActorInfo = GetActorInfo(Actor);
	return ActorInfo ? ActorInfo->GetLastStimulusLocation() : FAISystem::InvalidLocation;
}

void UAIPerceptionComponent::GetLocationAndDirection(FVector& Location, FVector& Direction) const
{
	const AActor* OwnerActor = Cast<AActor>(GetOuter());
	if (OwnerActor != nullptr)
	{
		FRotator ViewRotation(ForceInitToZero);
		OwnerActor->GetActorEyesViewPoint(Location, ViewRotation);
		Direction = ViewRotation.Vector();
	}
}

const AActor* UAIPerceptionComponent::GetBodyActor() const
{
	AController* OwnerController = Cast<AController>(GetOuter());
	if (OwnerController != NULL)
	{
		return OwnerController->GetPawn();
	}

	return Cast<AActor>(GetOuter());
}

AActor* UAIPerceptionComponent::GetMutableBodyActor()
{
	return const_cast<AActor*>(GetBodyActor());
}

void UAIPerceptionComponent::RegisterStimulus(AActor* Source, const FAIStimulus& Stimulus)
{
	FStimulusToProcess& StimulusToProcess = StimuliToProcess[StimuliToProcess.Add(FStimulusToProcess(Source, Stimulus))];
	StimulusToProcess.Stimulus.SetExpirationAge(MaxActiveAge[int32(Stimulus.Type)]);
}

void UAIPerceptionComponent::ProcessStimuli()
{
	if(StimuliToProcess.Num() == 0)
	{
		UE_VLOG(GetOwner(), LogAIPerception, Warning, TEXT("UAIPerceptionComponent::ProcessStimuli called without any Stimuli to process"));
		return;
	}

	const bool bBroadcastEveryTargetUpdate = OnTargetPerceptionUpdated.IsBound();
	
	FStimulusToProcess* SourcedStimulus = StimuliToProcess.GetData();
	TArray<AActor*> UpdatedActors;
	UpdatedActors.Reserve(StimuliToProcess.Num());

	for (int32 StimulusIndex = 0; StimulusIndex < StimuliToProcess.Num(); ++StimulusIndex, ++SourcedStimulus)
	{
		FActorPerceptionInfo* PerceptualInfo = PerceptualData.Find(SourcedStimulus->Source);

		if (PerceptualInfo == NULL)
		{
			if (SourcedStimulus->Stimulus.WasSuccessfullySensed() == false)
			{
				// this means it's a failed perception of an actor our owner is not aware of
				// at all so there's no point in creating perceptual data for a failed stimulus
				continue;
			}
			else
			{
				// create an entry
				PerceptualInfo = &PerceptualData.Add(SourcedStimulus->Source, FActorPerceptionInfo(SourcedStimulus->Source));
				// tell it what's our dominant sense
				PerceptualInfo->DominantSense = DominantSenseID;

				PerceptualInfo->bIsHostile = AIOwner != NULL && FGenericTeamId::GetAttitude(AIOwner, SourcedStimulus->Source) == ETeamAttitude::Hostile;
			}
		}

		if (PerceptualInfo->LastSensedStimuli.Num() <= SourcedStimulus->Stimulus.Type)
		{
			const int32 NumberToAdd = SourcedStimulus->Stimulus.Type - PerceptualInfo->LastSensedStimuli.Num() + 1;
			for (int32 Index = 0; Index < NumberToAdd; ++Index)
			{
				PerceptualInfo->LastSensedStimuli.Add(FAIStimulus());
			}
		}

		check(SourcedStimulus->Stimulus.Type.IsValid());

		FAIStimulus& StimulusStore = PerceptualInfo->LastSensedStimuli[SourcedStimulus->Stimulus.Type];
		const bool bActorInfoUpdated = SourcedStimulus->Stimulus.WantsToNotifyOnlyOnPerceptionChange() == false 
			|| SourcedStimulus->Stimulus.WasSuccessfullySensed() != StimulusStore.WasSuccessfullySensed();

		if (SourcedStimulus->Stimulus.WasSuccessfullySensed())
		{
			RefreshStimulus(StimulusStore, SourcedStimulus->Stimulus);
		}
		else if (StimulusStore.IsExpired() == false)
		{	
			if (bActorInfoUpdated)
			{
				// @note there some more valid info in SourcedStimulus->Stimulus regarding test that failed
				// may be useful in future
				StimulusStore.MarkNoLongerSensed();
				StimulusStore.SetStimulusAge(0);
			}
		}
		else
		{
			HandleExpiredStimulus(StimulusStore);
		}

		// if the new stimulus is "valid" or it's info that "no longer sensed" and it used to be sensed successfully
		if (bActorInfoUpdated)
		{
			UpdatedActors.AddUnique(SourcedStimulus->Source);
			if (bBroadcastEveryTargetUpdate)
			{
				OnTargetPerceptionUpdated.Broadcast(SourcedStimulus->Source, StimulusStore);
			}
		}
	}

	StimuliToProcess.Reset();

	if (UpdatedActors.Num() > 0)
	{
		if (AIOwner != NULL)
		{
			AIOwner->ActorsPerceptionUpdated(UpdatedActors);
		}

		OnPerceptionUpdated.Broadcast(UpdatedActors);
	}
}

void UAIPerceptionComponent::RefreshStimulus(FAIStimulus& StimulusStore, const FAIStimulus& NewStimulus)
{
	// if new stimulus is younger or stronger
	// note that stimulus Age depends on PerceptionSystem::PerceptionAgingRate. It's possible that 
	// both already stored and the new stimulus have Age of 0, but stored stimulus' acctual age is in [0, PerceptionSystem::PerceptionAgingRate)
	if (NewStimulus.GetAge() <= StimulusStore.GetAge() || StimulusStore.Strength < NewStimulus.Strength)
	{
		StimulusStore = NewStimulus;
		// update stimulus 
	}
}

void UAIPerceptionComponent::HandleExpiredStimulus(FAIStimulus& StimulusStore)
{
	ensure(StimulusStore.IsExpired() == true);
}

bool UAIPerceptionComponent::AgeStimuli(const float ConstPerceptionAgingRate)
{
	bool bExpiredStimuli = false;

	for (FActorPerceptionContainer::TIterator It(PerceptualData); It; ++It)
	{
		FActorPerceptionInfo& ActorPerceptionInfo = It->Value;

		for (FAIStimulus& Stimulus : ActorPerceptionInfo.LastSensedStimuli)
		{
			// Age the stimulus. If it is active but has just expired, mark it as such
			if (Stimulus.AgeStimulus(ConstPerceptionAgingRate) == false 
				&& (Stimulus.IsActive() || Stimulus.WantsToNotifyOnlyOnPerceptionChange())
				&& Stimulus.IsExpired() == false)
			{
				AActor* TargetActor = ActorPerceptionInfo.Target.Get();
				if (TargetActor)
				{
					Stimulus.MarkExpired();
					RegisterStimulus(TargetActor, Stimulus);
					bExpiredStimuli = true;
				}
			}
		}
	}

	return bExpiredStimuli;
}

void UAIPerceptionComponent::ForgetActor(AActor* ActorToForget)
{
	if (PerceptualData.Num() > 0)
	{
		UAIPerceptionSystem* AIPerceptionSys = UAIPerceptionSystem::GetCurrent(GetWorld());
		if (AIPerceptionSys != nullptr && ActorToForget != nullptr)
		{
			AIPerceptionSys->OnListenerForgetsActor(*this, *ActorToForget);
		}

		PerceptualData.Remove(ActorToForget);
	}
}

void UAIPerceptionComponent::ForgetAll()
{
	if (PerceptualData.Num() > 0)
	{
		UAIPerceptionSystem* AIPerceptionSys = UAIPerceptionSystem::GetCurrent(GetWorld());
		if (AIPerceptionSys != nullptr)
		{
			AIPerceptionSys->OnListenerForgetsAll(*this);
		}

		PerceptualData.Reset();
	}
}

float UAIPerceptionComponent::GetYoungestStimulusAge(const AActor& Source) const
{
	const FActorPerceptionInfo* Info = GetActorInfo(Source);
	if (Info == NULL)
	{
		return FAIStimulus::NeverHappenedAge;
	}

	float SmallestAge = FAIStimulus::NeverHappenedAge;
	for (int32 SenseID = 0; SenseID < Info->LastSensedStimuli.Num(); ++SenseID)
	{
		if (Info->LastSensedStimuli[SenseID].WasSuccessfullySensed())
		{
			float SenseAge = Info->LastSensedStimuli[SenseID].GetAge();
			if (SenseAge < SmallestAge)
			{
				SmallestAge = SenseAge;
			}
		}
	}

	return SmallestAge;
}

bool UAIPerceptionComponent::HasAnyActiveStimulus(const AActor& Source) const
{
	const FActorPerceptionInfo* Info = GetActorInfo(Source);
	if (Info == NULL)
	{
		return false;
	}

	return Info->HasAnyKnownStimulus();
}

bool UAIPerceptionComponent::HasAnyCurrentStimulus(const AActor& Source) const
{
	const FActorPerceptionInfo* Info = GetActorInfo(Source);
	if (Info == NULL)
	{
		return false;
	}

	return Info->HasAnyCurrentStimulus();
}

bool UAIPerceptionComponent::HasActiveStimulus(const AActor& Source, FAISenseID Sense) const
{
	const FActorPerceptionInfo* Info = GetActorInfo(Source);
	return (Info 
		&& Info->LastSensedStimuli.IsValidIndex(Sense) 
		&& Info->LastSensedStimuli[Sense].WasSuccessfullySensed()
		&& Info->LastSensedStimuli[Sense].GetAge() < FAIStimulus::NeverHappenedAge
		&& (Info->LastSensedStimuli[Sense].GetAge() <= MaxActiveAge[Sense] || MaxActiveAge[Sense] == 0.f));
}

void UAIPerceptionComponent::RemoveDeadData()
{
	for (FActorPerceptionContainer::TIterator It(PerceptualData); It; ++It)
	{
		if (It->Value.Target.IsValid() == false)
		{
			It.RemoveCurrent();
		}
	}
}

//----------------------------------------------------------------------//
// blueprint interface
//----------------------------------------------------------------------//
void UAIPerceptionComponent::GetPerceivedHostileActors(TArray<AActor*>& OutActors) const
{
	GetHostileActors(OutActors);
}

void UAIPerceptionComponent::GetCurrentlyPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const
{
	const FAISenseID SenseID = UAISense::GetSenseID(SenseToUse);

	OutActors.Reserve(PerceptualData.Num());
	for (FActorPerceptionContainer::TConstIterator DataIt = GetPerceptualDataConstIterator(); DataIt; ++DataIt)
	{
		const bool bCurrentlyPerceived = (SenseToUse == nullptr) ? DataIt->Value.HasAnyCurrentStimulus() : DataIt->Value.IsSenseRegistered(SenseID);
		if (bCurrentlyPerceived)
		{
			if (DataIt->Value.Target.IsValid())
			{
				OutActors.Add(DataIt->Value.Target.Get());
			}
		}
	}
}

void UAIPerceptionComponent::GetKnownPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const
{
	const FAISenseID SenseID = UAISense::GetSenseID(SenseToUse);

	OutActors.Reserve(PerceptualData.Num());
	for (FActorPerceptionContainer::TConstIterator DataIt = GetPerceptualDataConstIterator(); DataIt; ++DataIt)
	{
		const bool bWasEverPerceived = (SenseToUse == nullptr) ? DataIt->Value.HasAnyKnownStimulus() : DataIt->Value.HasKnownStimulusOfSense(SenseID);
		if (bWasEverPerceived)
		{
			if (DataIt->Value.Target.IsValid())
			{
				OutActors.Add(DataIt->Value.Target.Get());
			}
		}
	}
}

bool UAIPerceptionComponent::GetActorsPerception(AActor* Actor, FActorPerceptionBlueprintInfo& Info)
{
	bool bInfoFound = false;
	if (Actor != nullptr && Actor->IsPendingKillPending() == false)
	{
		const FActorPerceptionInfo* PerceivedInfo = GetActorInfo(*Actor);
		if (PerceivedInfo)
		{
			Info = FActorPerceptionBlueprintInfo(*PerceivedInfo);
			bInfoFound = true;
		}
	}

	return bInfoFound;
}

void UAIPerceptionComponent::SetSenseEnabled(TSubclassOf<UAISense> SenseClass, const bool bEnable)
{
	const FAISenseID SenseID = UAISense::GetSenseID(SenseClass);
	if (SenseID.IsValid())
	{
		UpdatePerceptionWhitelist(SenseID, bEnable);
	}
}

//----------------------------------------------------------------------//
// debug
//----------------------------------------------------------------------//
#if WITH_GAMEPLAY_DEBUGGER
void UAIPerceptionComponent::DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const
{
	if (DebuggerCategory == nullptr)
	{
		return;
	}

	for (UAIPerceptionComponent::FActorPerceptionContainer::TConstIterator It(GetPerceptualDataConstIterator()); It; ++It)
	{
		const FActorPerceptionInfo& ActorPerceptionInfo = It->Value;
		if (ActorPerceptionInfo.Target.IsValid() && It->Key)
		{
			const FVector TargetLocation = ActorPerceptionInfo.Target->GetActorLocation();
			for (const FAIStimulus& Stimulus : ActorPerceptionInfo.LastSensedStimuli)
			{
				const UAISenseConfig* SenseConfig = GetSenseConfig(Stimulus.Type);
				if (Stimulus.IsValid() && (Stimulus.IsExpired() == false) && SenseConfig)
				{
					const FString Description = FString::Printf(TEXT("%s: %.2f age:%.2f"), *SenseConfig->GetSenseName(), Stimulus.Strength, Stimulus.GetAge());
					const FColor DebugColor = SenseConfig->GetDebugColor();

					DebuggerCategory->AddShape(FGameplayDebuggerShape::MakePoint(Stimulus.StimulusLocation + FVector(0, 0, 30), 30.0f, DebugColor, Description));
					DebuggerCategory->AddShape(FGameplayDebuggerShape::MakeSegment(Stimulus.ReceiverLocation, Stimulus.StimulusLocation, DebugColor));
					DebuggerCategory->AddShape(FGameplayDebuggerShape::MakeSegment(TargetLocation, Stimulus.StimulusLocation, FColor::Black));
				}
			}
		}
	}

	for (UAISenseConfig* SenseConfig : SensesConfig)
	{
		if (SenseConfig)
		{
			SenseConfig->DescribeSelfToGameplayDebugger(this, DebuggerCategory);
		}
	}
}
#endif // WITH_GAMEPLAY_DEBUGGER

#if ENABLE_VISUAL_LOG
void UAIPerceptionComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{

}
#endif // ENABLE_VISUAL_LOG

//----------------------------------------------------------------------//
// deprecated
//----------------------------------------------------------------------//
void UAIPerceptionComponent::UpdatePerceptionFilter(FAISenseID Channel, bool bNewValue)
{
	UpdatePerceptionWhitelist(Channel, bNewValue);
}

void UAIPerceptionComponent::GetPerceivedActors(TSubclassOf<UAISense> SenseToUse, TArray<AActor*>& OutActors) const
{
	GetCurrentlyPerceivedActors(SenseToUse, OutActors);
}
