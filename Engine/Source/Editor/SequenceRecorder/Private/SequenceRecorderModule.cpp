// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/Guid.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/DebugDrawService.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "EngineGlobals.h"
#include "LevelEditor.h"
#include "PersonaModule.h"
#include "Animation/AnimationRecordingSettings.h"
#include "AnimationRecorder.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "PropertyEditorModule.h"
#include "ActorRecording.h"
#include "SSequenceRecorder.h"
#include "Widgets/Docking/SDockTab.h"
#include "SequenceRecorderCommands.h"
#include "Containers/ArrayView.h"
#include "ISequenceAudioRecorder.h"
#include "ISequenceRecorder.h"
#include "SequenceRecorder.h"
#include "SequenceRecorderSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ActorRecordingDetailsCustomization.h"
#include "SequenceRecorderDetailsCustomization.h"
#include "PropertiesToRecordForClassDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "SequenceRecorder"

static const FName SequenceRecorderTabName(TEXT("SequenceRecorder"));

static TAutoConsoleVariable<float> CVarDefaultRecordedAnimLength(
	TEXT("AnimRecorder.AnimLength"),
	FAnimationRecordingSettings::DefaultMaximumLength,
	TEXT("Sets default animation length for the animation recorder system."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAnimRecorderSampleRate(
	TEXT("AnimRecorder.SampleRate"),
	FAnimationRecordingSettings::DefaultSampleRate,
	TEXT("Sets the sample rate for the animation recorder system"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAnimRecorderWorldSpace(
	TEXT("AnimRecorder.RecordInWorldSpace"),
	1,
	TEXT("True to record anim keys in world space, false to record only in local space."),
	ECVF_Default);

class FSequenceRecorderModule : public ISequenceRecorder, private FSelfRegisteringExec
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		GetMutableDefault<USequenceRecorderSettings>()->LoadConfig();

		// set cvar defaults
		CVarDefaultRecordedAnimLength.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			GetMutableDefault<USequenceRecorderSettings>()->DefaultAnimationSettings.Length = CVarDefaultRecordedAnimLength.GetValueOnGameThread();
		}));

		CVarAnimRecorderSampleRate.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			GetMutableDefault<USequenceRecorderSettings>()->DefaultAnimationSettings.SampleRate = CVarAnimRecorderSampleRate.GetValueOnGameThread();
		}));

		CVarAnimRecorderWorldSpace.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			GetMutableDefault<USequenceRecorderSettings>()->DefaultAnimationSettings.bRecordInWorldSpace = (CVarAnimRecorderWorldSpace.GetValueOnGameThread() != 0);
		}));

		FSequenceRecorderCommands::Register();

		// init sequence recorder
		FSequenceRecorder::Get().Initialize();

		// register main tick
		if(GEngine)
		{
			PostEditorTickHandle = GEngine->OnPostEditorTick().AddStatic(&FSequenceRecorderModule::TickSequenceRecorder);
		}

		if (GEditor)
		{
			// register Persona recorder
			FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>(TEXT("Persona"));
			PersonaModule.OnIsRecordingActive().BindStatic(&FSequenceRecorderModule::HandlePersonaIsRecordingActive);
			PersonaModule.OnRecord().BindStatic(&FSequenceRecorderModule::HandlePersonaRecord);
			PersonaModule.OnStopRecording().BindStatic(&FSequenceRecorderModule::HandlePersonaStopRecording);
			PersonaModule.OnGetCurrentRecording().BindStatic(&FSequenceRecorderModule::HandlePersonaCurrentRecording);
			PersonaModule.OnGetCurrentRecordingTime().BindStatic(&FSequenceRecorderModule::HandlePersonaCurrentRecordingTime);

			// register 'keep simulation changes' recorder
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			LevelEditorModule.OnCaptureSingleFrameAnimSequence().BindStatic(&FSequenceRecorderModule::HandleCaptureSingleFrameAnimSequence);

			// register standalone UI
			LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda([]()
			{
				FLevelEditorModule& LocalLevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
				LocalLevelEditorModule.GetLevelEditorTabManager()->RegisterTabSpawner(SequenceRecorderTabName, FOnSpawnTab::CreateStatic(&FSequenceRecorderModule::SpawnSequenceRecorderTab))
				.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
				.SetDisplayName(LOCTEXT("SequenceRecorderTabTitle", "Sequence Recorder"))
				.SetTooltipText(LOCTEXT("SequenceRecorderTooltipText", "Open the Sequence Recorder tab."))
				.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "SequenceRecorder.TabIcon"));
			});

			// register for debug drawing
			DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("Decals"), FDebugDrawDelegate::CreateStatic(&FSequenceRecorderModule::DrawDebug));

			// register details customization
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout(UActorRecording::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FActorRecordingDetailsCustomization::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(USequenceRecorderSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSequenceRecorderDetailsCustomization::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout(FPropertiesToRecordForClass::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertiesToRecordForClassDetailsCustomization::MakeInstance));
		}
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR

		FSequenceRecorder::Get().Shutdown();

		if (GEditor)
		{
			UDebugDrawService::Unregister(DrawDebugDelegateHandle);

			if (FSlateApplication::IsInitialized())
			{
				FGlobalTabmanager::Get()->UnregisterTabSpawner(SequenceRecorderTabName);
			}

			if(FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
			{
				FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
				LevelEditorModule.OnCaptureSingleFrameAnimSequence().Unbind();
				LevelEditorModule.OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
			}

			if (FModuleManager::Get().IsModuleLoaded(TEXT("Persona")))
			{
				FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>(TEXT("Persona"));
				PersonaModule.OnIsRecordingActive().Unbind();
				PersonaModule.OnRecord().Unbind();
				PersonaModule.OnStopRecording().Unbind();
				PersonaModule.OnGetCurrentRecording().Unbind();
				PersonaModule.OnGetCurrentRecordingTime().Unbind();
			}

			if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
			{
				FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

				if (UObjectInitialized())
				{
					PropertyModule.UnregisterCustomClassLayout(UActorRecording::StaticClass()->GetFName());
					PropertyModule.UnregisterCustomClassLayout(USequenceRecorderSettings::StaticClass()->GetFName());
					PropertyModule.UnregisterCustomPropertyTypeLayout(FPropertiesToRecordForClass::StaticStruct()->GetFName());
				}
			}
		}

		if(GEngine)
		{
			GEngine->OnPostEditorTick().Remove(PostEditorTickHandle);
		}
#endif
	}

	// FSelfRegisteringExec implementation
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
#if WITH_EDITOR
		if (FParse::Command(&Cmd, TEXT("RecordAnimation")))
		{
			return HandleRecordAnimationCommand(InWorld, Cmd, Ar);
		}
		else if (FParse::Command(&Cmd, TEXT("StopRecordingAnimation")))
		{
			return HandleStopRecordAnimationCommand(InWorld, Cmd, Ar);
		}
		else if (FParse::Command(&Cmd, TEXT("RecordSequence")))
		{
			return HandleRecordSequenceCommand(InWorld, Cmd, Ar);
		}
		else if (FParse::Command(&Cmd, TEXT("StopRecordingSequence")))
		{
			return HandleStopRecordSequenceCommand(InWorld, Cmd, Ar);
		}
#endif
		return false;
	}

	static AActor* FindActorByName(const FString& ActorNameStr, UWorld* InWorld)
	{
		for (ULevel const* Level : InWorld->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor)
					{
						if (Actor->GetName() == ActorNameStr)
						{
							return Actor;
						}
					}
				}
			}
		}

		return nullptr;
	}

	static bool HandleRecordAnimationCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		const TCHAR* Str = InStr;
		// parse actor name
		TCHAR ActorName[128];
		AActor* FoundActor = nullptr;
		if (FParse::Token(Str, ActorName, ARRAY_COUNT(ActorName), 0))
		{
			FoundActor = FindActorByName(FString(ActorName), InWorld);
		}

		if (FoundActor)
		{
			USkeletalMeshComponent* const SkelComp = FoundActor->FindComponentByClass<USkeletalMeshComponent>();
			if (SkelComp)
			{
				TCHAR AssetPath[256];
				FParse::Token(Str, AssetPath, ARRAY_COUNT(AssetPath), 0);
				FString const AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
				return FAnimationRecorderManager::Get().RecordAnimation(SkelComp, AssetPath, AssetName, GetDefault<USequenceRecorderSettings>()->DefaultAnimationSettings);
			}
		}
#endif
		return false;
	}

	static AActor* FindActorByLabel(const FString& ActorNameStr, UWorld* InWorld, bool bFuzzy = false)
	{
		// search for the actor by name
		for (ULevel* Level : InWorld->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor)
					{
						if (Actor->GetActorLabel() == ActorNameStr)
						{
							return Actor;
						}
					}
				}
			}
		}

		// if we want to do a fuzzy search then we return the first actor whose name that starts 
		// the specified string
		if(bFuzzy)
		{
			for (ULevel* Level : InWorld->GetLevels())
			{
				if (Level)
				{
					for (AActor* Actor : Level->Actors)
					{
						if (Actor)
						{
							if (Actor->GetActorLabel().StartsWith(ActorNameStr))
							{
								return Actor;
							}
						}
					}
				}
			}			
		}

		return nullptr;
	}

	static bool HandleStopRecordAnimationCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		const TCHAR* Str = InStr;

		// parse actor name
		TCHAR ActorName[128];
		AActor* FoundActor = nullptr;
		bool bStopAll = false;
		if (FParse::Token(Str, ActorName, ARRAY_COUNT(ActorName), 0))
		{
			FString const ActorNameStr = FString(ActorName);

			if (ActorNameStr.ToLower() == TEXT("all"))
			{
				bStopAll = true;
			}
			else if (InWorld)
			{
				FoundActor = FindActorByName(ActorNameStr, InWorld);
			}
		}

		if (bStopAll)
		{
			FAnimationRecorderManager::Get().StopRecordingAllAnimations();
			return true;
		}
		else if (FoundActor)
		{
			USkeletalMeshComponent* const SkelComp = FoundActor->FindComponentByClass<USkeletalMeshComponent>();
			if (SkelComp)
			{
				FAnimationRecorderManager::Get().StopRecordingAnimation(SkelComp);
				return true;
			}
		}

#endif
		return false;
	}

	static void FindActorsOfClass(UClass* Class, UWorld* InWorld, TArray<AActor*>& OutActors)
	{
		for (ULevel* Level : InWorld->GetLevels())
		{
			if (Level)
			{
				for (AActor* Actor : Level->Actors)
				{
					if (Actor && Actor->IsA(Class) && UActorRecording::IsRelevantForRecording(Actor))
					{
						OutActors.AddUnique(Actor);
					}
				}
			}
		}
	}

	static bool HandleRecordSequenceCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		USequenceRecorderSettings* Settings = GetMutableDefault<USequenceRecorderSettings>();

		enum class EFilterType : int32
		{
			None,
			All,
			Actor,
			Class
		};

		const TCHAR* Str = InStr;
		EFilterType FilterType = EFilterType::None;
		TCHAR Filter[128];
		if(FParse::Token(Str, Filter, ARRAY_COUNT(Filter), 0))
		{
			FString const FilterStr = Filter;
			if (FilterStr == TEXT("all"))
			{
				FilterType = EFilterType::All;
			}
			else if(FilterStr == TEXT("actor"))
			{
				FilterType = EFilterType::Actor;
			}
			else if(FilterStr == TEXT("class"))
			{
				FilterType = EFilterType::Class;
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("Couldnt parse recording filter, using actor filters from settings."));
			}
		}

		if(FilterType == EFilterType::Actor || FilterType == EFilterType::Class)
		{
			TCHAR Specifier[128];
			if(FParse::Token(Str, Specifier, ARRAY_COUNT(Specifier), 0))
			{
				FString const SpecifierStr = FString(Specifier).TrimStart();
				if(FilterType == EFilterType::Actor)
				{
					AActor* FoundActor = FindActorByLabel(SpecifierStr, InWorld, true);
					if(FoundActor)
					{
						Settings->ActorFilter.ActorClassesToRecord.Empty();
						FSequenceRecorder::Get().ClearQueuedRecordings();
						FSequenceRecorder::Get().AddNewQueuedRecording(FoundActor);
						FSequenceRecorder::Get().StartRecording();					
					}
					return true;
				}
				else if(FilterType == EFilterType::Class)
				{
					UClass* FoundClass = FindObject<UClass>(ANY_PACKAGE, *SpecifierStr);
					if(FoundClass != nullptr)
					{
						Settings->ActorFilter.ActorClassesToRecord.Empty();
						Settings->ActorFilter.ActorClassesToRecord.Add(FoundClass);
						Settings->bRecordNearbySpawnedActors = false;
						Settings->NearbyActorRecordingProximity = 0.0f;

						FSequenceRecorder::Get().ClearQueuedRecordings();

						TArray<AActor*> ActorsToRecord;
						FindActorsOfClass(FoundClass, InWorld, ActorsToRecord);

						for(AActor* ActorToRecord : ActorsToRecord)
						{
							FSequenceRecorder::Get().AddNewQueuedRecording(ActorToRecord);
						}

						FSequenceRecorder::Get().StartRecording();
						return true;
					}
					else
					{
						UE_LOG(LogAnimation, Warning, TEXT("Couldnt parse class filter, aborting recording."));
					}
				}
			}
		}
		else
		{
			FSequenceRecorder::Get().ClearQueuedRecordings();

			TArray<AActor*> ActorsToRecord;
			if(FilterType == EFilterType::None)
			{
				for(TSubclassOf<AActor>& SubClass : Settings->ActorFilter.ActorClassesToRecord)
				{
					FindActorsOfClass(*SubClass, InWorld, ActorsToRecord);
				}
			}
			else
			{
				Settings->bRecordNearbySpawnedActors = false;
				Settings->NearbyActorRecordingProximity = 0.0f;

				Settings->ActorFilter.ActorClassesToRecord.Empty();
				Settings->ActorFilter.ActorClassesToRecord.Add(AActor::StaticClass());

				FindActorsOfClass(AActor::StaticClass(), InWorld, ActorsToRecord);
			}

			for(AActor* ActorToRecord : ActorsToRecord)
			{
				FSequenceRecorder::Get().AddNewQueuedRecording(ActorToRecord);
			}

			FSequenceRecorder::Get().StartRecording();
			return true;
		}
#endif
		return false;
	}

	bool HandleStopRecordSequenceCommand(UWorld* InWorld, const TCHAR* InStr, FOutputDevice& Ar)
	{
#if WITH_EDITOR
		FSequenceRecorder::Get().StopRecording();
		FSequenceRecorder::Get().ClearQueuedRecordings();
		return true;
#else
		return false;
#endif
	}

	// ISequenceRecorder interface
	virtual bool StartRecording(UWorld* World, const FSequenceRecorderActorFilter& ActorFilter) override
	{
		return FSequenceRecorder::Get().StartRecordingForReplay(World, ActorFilter);
	}

	virtual void StopRecording() override
	{
		FSequenceRecorder::Get().StopRecording();
	}

	virtual bool IsRecording() override
	{
		return FSequenceRecorder::Get().IsRecording();
	}

	virtual float GetCurrentRecordingLength() override
	{
		TWeakObjectPtr<ULevelSequence> CurrentSequence = FSequenceRecorder::Get().GetCurrentSequence();

		return CurrentSequence.IsValid() ? CurrentSequence->GetMovieScene()->GetPlaybackRange().Size<float>() : 0.0f;
	}

	virtual bool StartRecording(TArrayView<AActor* const> ActorsToRecord, const FOnRecordingStarted& OnRecordingStarted, const FOnRecordingFinished& OnRecordingFinished, const FString& PathToRecordTo, const FString& SequenceName) override
	{
		if(ActorsToRecord.Num() != 0)
		{
			FSequenceRecorder::Get().ClearQueuedRecordings();
			for (AActor* Actor : ActorsToRecord)
			{
				FSequenceRecorder::Get().AddNewQueuedRecording(Actor);
			}
		}
		else
		{
			if(FSlateApplication::IsInitialized())
			{
				FNotificationInfo Info(LOCTEXT("SequenceRecordingErrorActor", "Couldn't find actor to record"));
				Info.bUseLargeFont = false;

				FSlateNotificationManager::Get().AddNotification(Info);
			}

			UE_LOG(LogAnimation, Display, TEXT("Couldn't find actor to record"));
		}

		return FSequenceRecorder::Get().StartRecording(OnRecordingStarted, OnRecordingFinished, PathToRecordTo, SequenceName);
	}

	virtual void NotifyActorStartRecording(AActor* Actor)
	{
		FSequenceRecorder::HandleActorSpawned(Actor);
	}

	virtual void NotifyActorStopRecording(AActor* Actor)
	{
		FSequenceRecorder::Get().HandleActorDespawned(Actor);
	}

	virtual FGuid GetRecordingGuid(AActor* Actor) const
	{
		UActorRecording* Recording = FSequenceRecorder::Get().FindRecording(Actor);
		if (Recording != nullptr)
		{
			return Recording->GetSpawnableGuid();
		}

		return FGuid();
	}

	virtual FDelegateHandle RegisterAudioRecorder(const TFunction<TUniquePtr<ISequenceAudioRecorder>()>& FactoryFunction) override
	{
		ensureMsgf(!AudioFactory, TEXT("Audio recorder already registered."));

		AudioFactory = FactoryFunction;
		AudioFactoryHandle = FDelegateHandle(FDelegateHandle::GenerateNewHandle);
		return AudioFactoryHandle;
	}

	virtual void UnregisterAudioRecorder(FDelegateHandle Handle) override
	{
		if (Handle == AudioFactoryHandle)
		{
			AudioFactory = nullptr;
			AudioFactoryHandle = FDelegateHandle();
		}
	}

	virtual bool HasAudioRecorder() const override
	{
		return AudioFactoryHandle.IsValid();
	}

	virtual TUniquePtr<ISequenceAudioRecorder> CreateAudioRecorder() const
	{
		return AudioFactory ? AudioFactory() : TUniquePtr<ISequenceAudioRecorder>();
	}

	static void TickSequenceRecorder(float DeltaSeconds)
	{
		if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
		{
			FSequenceRecorder::Get().Tick(DeltaSeconds);
		}
	}

#if WITH_EDITOR
	static UAnimSequence* HandleCaptureSingleFrameAnimSequence(USkeletalMeshComponent* Component)
	{
		FAnimationRecorder Recorder;
		if (Recorder.TriggerRecordAnimation(Component))
		{
			class UAnimSequence * Sequence = Recorder.GetAnimationObject();
			if (Sequence)
			{
				Recorder.StopRecord(false);
				return Sequence;
			}
		}

		return nullptr;
	}

	static void HandlePersonaIsRecordingActive(USkeletalMeshComponent* Component, bool& bIsRecording)
	{
		bIsRecording = FAnimationRecorderManager::Get().IsRecording(Component);
	}

	static void HandlePersonaRecord(USkeletalMeshComponent* Component)
	{
		FAnimationRecorderManager::Get().RecordAnimation(Component);
	}

	static void HandlePersonaStopRecording(USkeletalMeshComponent* Component)
	{
		FAnimationRecorderManager::Get().StopRecordingAnimation(Component);
	}

	static void HandlePersonaTickRecording(USkeletalMeshComponent* Component, float DeltaSeconds)
	{
	//	FAnimationRecorderManager::Get().Tick(Component, DeltaSeconds);
	}

	static void HandlePersonaCurrentRecording(USkeletalMeshComponent* Component, UAnimSequence*& OutSequence)
	{
		OutSequence = FAnimationRecorderManager::Get().GetCurrentlyRecordingSequence(Component);
	}

	static void HandlePersonaCurrentRecordingTime(USkeletalMeshComponent* Component, float& OutTime)
	{
		OutTime = FAnimationRecorderManager::Get().GetCurrentRecordingTime(Component);
	}

	static TSharedRef<SDockTab> SpawnSequenceRecorderTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> MajorTab =
			SNew(SDockTab)
			.Icon(FEditorStyle::Get().GetBrush("SequenceRecorder.TabIcon"))
			.TabRole(ETabRole::NomadTab);

		MajorTab->SetContent(SNew(SSequenceRecorder));

		return MajorTab;
	}

	static void DrawDebug(UCanvas* InCanvas, APlayerController* InPlayerController)
	{
		FSequenceRecorder::Get().DrawDebug(InCanvas, InPlayerController);
	}
#endif
	FDelegateHandle PostEditorTickHandle;

	FDelegateHandle DrawDebugDelegateHandle;

	FDelegateHandle LevelEditorTabManagerChangedHandle;

	TFunction<TUniquePtr<ISequenceAudioRecorder>()> AudioFactory;

	FDelegateHandle AudioFactoryHandle;
};

IMPLEMENT_MODULE( FSequenceRecorderModule, SequenceRecorder )

#undef LOCTEXT_NAMESPACE
