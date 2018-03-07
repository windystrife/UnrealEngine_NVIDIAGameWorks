// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorder.h"
#include "ISequenceAudioRecorder.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "AssetData.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "EngineGlobals.h"
#include "Toolkits/AssetEditorManager.h"
#include "LevelEditor.h"
#include "AnimationRecorder.h"
#include "ActorRecording.h"
#include "AssetRegistryModule.h"
#include "SequenceRecorderUtils.h"
#include "SequenceRecorderSettings.h"
#include "ObjectTools.h"
#include "Features/IModularFeatures.h"
#include "Engine/DemoNetDriver.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "LevelSequenceActor.h"
#include "ILevelViewport.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundWave.h"

#define LOCTEXT_NAMESPACE "SequenceRecorder"

FSequenceRecorder::FSequenceRecorder()
	: bQueuedRecordingsDirty(false)
	, bWasImmersive(false)
	, CurrentDelay(0.0f)
	, CurrentTime(0.0f)
{
}

void FSequenceRecorder::Initialize()
{
	// load textures we use for the countdown/recording display
	CountdownTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/SequenceRecorder/Countdown.Countdown"), nullptr, LOAD_None, nullptr);
	CountdownTexture->AddToRoot();
	RecordingIndicatorTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EditorResources/SequenceRecorder/RecordingIndicator.RecordingIndicator"), nullptr, LOAD_None, nullptr);
	RecordingIndicatorTexture->AddToRoot();

	// register built-in recorders
	IModularFeatures::Get().RegisterModularFeature("MovieSceneSectionRecorderFactory", &AnimationSectionRecorderFactory);
	IModularFeatures::Get().RegisterModularFeature("MovieSceneSectionRecorderFactory", &TransformSectionRecorderFactory);
	IModularFeatures::Get().RegisterModularFeature("MovieSceneSectionRecorderFactory", &MultiPropertySectionRecorder);

	RefreshNextSequence();
}

void FSequenceRecorder::Shutdown()
{
	// unregister built-in recorders
	IModularFeatures::Get().UnregisterModularFeature("MovieSceneSectionRecorderFactory", &AnimationSectionRecorderFactory);
	IModularFeatures::Get().UnregisterModularFeature("MovieSceneSectionRecorderFactory", &TransformSectionRecorderFactory);
	IModularFeatures::Get().UnregisterModularFeature("MovieSceneSectionRecorderFactory", &MultiPropertySectionRecorder);

	if(CountdownTexture.IsValid())
	{
		CountdownTexture->RemoveFromRoot();
		CountdownTexture.Reset();
	}
	if(RecordingIndicatorTexture.IsValid())
	{
		RecordingIndicatorTexture->RemoveFromRoot();
		RecordingIndicatorTexture.Reset();
	}
}

FSequenceRecorder& FSequenceRecorder::Get()
{
	static FSequenceRecorder SequenceRecorder;
	return SequenceRecorder;
}


bool FSequenceRecorder::IsRecordingQueued(AActor* Actor) const
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		if (QueuedRecording->GetActorToRecord() == Actor)
		{
			return true;
		}
	}

	return false;
}

UActorRecording* FSequenceRecorder::FindRecording(AActor* Actor) const
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		if (QueuedRecording->GetActorToRecord() == Actor)
		{
			return QueuedRecording;
		}
	}

	return nullptr;
}

void FSequenceRecorder::StartAllQueuedRecordings()
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		QueuedRecording->StartRecording(CurrentSequence.Get(), CurrentTime);
	}
}

void FSequenceRecorder::StopAllQueuedRecordings()
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		QueuedRecording->StopRecording(CurrentSequence.Get());
	}
}

UActorRecording* FSequenceRecorder::AddNewQueuedRecording(AActor* Actor, UAnimSequence* AnimSequence, float Length)
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	UActorRecording* ActorRecording = NewObject<UActorRecording>();
	ActorRecording->AddToRoot();
	ActorRecording->SetActorToRecord(Actor);
	ActorRecording->TargetAnimation = AnimSequence;
	ActorRecording->AnimationSettings.Length = Length;

	// We always record in world space as we need animations to record root motion
	ActorRecording->AnimationSettings.bRecordInWorldSpace = true;

	UMovieScene3DTransformSectionRecorderSettings* TransformSettings = ActorRecording->ActorSettings.GetSettingsObject<UMovieScene3DTransformSectionRecorderSettings>();
	check(TransformSettings);
	TransformSettings->bRecordTransforms = true;

	// auto-save assets in non-editor runtime
	if(GEditor == nullptr)
	{
		ActorRecording->AnimationSettings.bAutoSaveAsset = true;
	}

	QueuedRecordings.Add(ActorRecording);

	bQueuedRecordingsDirty = true;

	return ActorRecording;
}

void FSequenceRecorder::RemoveQueuedRecording(AActor* Actor)
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		if (QueuedRecording->GetActorToRecord() == Actor)
		{
			QueuedRecording->RemoveFromRoot();
			QueuedRecordings.Remove(QueuedRecording);
			break;
		}
	}

	bQueuedRecordingsDirty = true;
}

void FSequenceRecorder::RemoveQueuedRecording(class UActorRecording* Recording)
{
	for (UActorRecording* QueuedRecording : QueuedRecordings)
	{
		if (QueuedRecording == Recording)
		{
			QueuedRecording->RemoveFromRoot();
			QueuedRecordings.Remove(QueuedRecording);
			break;
		}
	}

	bQueuedRecordingsDirty = true;
}

void FSequenceRecorder::ClearQueuedRecordings()
{
	if(IsRecording())
	{
		UE_LOG(LogAnimation, Display, TEXT("Couldnt clear queued recordings while recording is in progress"));
	}
	else
	{
		for (UActorRecording* QueuedRecording : QueuedRecordings)
		{
			QueuedRecording->RemoveFromRoot();
		}
		QueuedRecordings.Empty();

		bQueuedRecordingsDirty = true;
	}
}

bool FSequenceRecorder::HasQueuedRecordings() const
{
	return QueuedRecordings.Num() > 0;
}


void FSequenceRecorder::Tick(float DeltaSeconds)
{
	const float FirstFrameTickLimit = 1.0f / 30.0f;

	// in-editor we can get a long frame update because of the searching we need to do to filter actors
	if(DeltaSeconds > FirstFrameTickLimit && CurrentTime < FirstFrameTickLimit * 2.0f && IsRecording())
	{
		DeltaSeconds = FirstFrameTickLimit;
	}

	// if a replay recording is in progress and channels are paused, wait until we have data again before recording
	if(CurrentReplayWorld.IsValid() && CurrentReplayWorld->DemoNetDriver != nullptr)
	{
		if(CurrentReplayWorld->DemoNetDriver->bChannelsArePaused)
		{
			return;
		}
	}

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	// check for spawned actors and if they have begun playing yet
	for(TWeakObjectPtr<AActor>& QueuedSpawnedActor : QueuedSpawnedActors)
	{
		if(AActor* Actor = QueuedSpawnedActor.Get())
		{
			if(Actor->HasActorBegunPlay())
			{
				if(UActorRecording::IsRelevantForRecording(Actor) && IsActorValidForRecording(Actor))
				{
					UActorRecording* NewRecording = AddNewQueuedRecording(Actor);
					NewRecording->bWasSpawnedPostRecord = true;
					NewRecording->StartRecording(CurrentSequence.Get(), CurrentTime);
				}

				QueuedSpawnedActor.Reset();
			}
		}
	}

	QueuedSpawnedActors.RemoveAll([](TWeakObjectPtr<AActor>& QueuedSpawnedActor) { return !QueuedSpawnedActor.IsValid(); } );

	FAnimationRecorderManager::Get().Tick(DeltaSeconds);

	for(UActorRecording* Recording : QueuedRecordings)
	{
		Recording->Tick(DeltaSeconds, CurrentSequence.Get(), CurrentTime);
	}

	if(CurrentDelay > 0.0f)
	{
		CurrentDelay -= DeltaSeconds;
		if(CurrentDelay <= 0.0f)
		{
			CurrentDelay = 0.0f;
			StartRecordingInternal(nullptr);
		}
	}

	if(Settings->bCreateLevelSequence && CurrentSequence.IsValid())
	{
		CurrentTime += DeltaSeconds;

		// check if all our actor recordings are finished or we timed out
		if(QueuedRecordings.Num() > 0)
		{
			bool bAllFinished = true;
			for(UActorRecording* Recording : QueuedRecordings)
			{
				if(Recording->IsRecording())
				{
					bAllFinished = false;
					break;
				}
			}

			if(bAllFinished || (Settings->SequenceLength > 0.0f && CurrentTime >= Settings->SequenceLength))
			{
				StopRecording();
			}
		}

		auto RemoveDeadActorPredicate = 
			[&](UActorRecording* Recording)
			{
				if(!Recording->GetActorToRecord())
				{
					DeadRecordings.Add(Recording);
					return true;
				}
				return false; 
			};

		// remove all dead actors
		int32 Removed = QueuedRecordings.RemoveAll(RemoveDeadActorPredicate);
		if(Removed > 0)
		{
			bQueuedRecordingsDirty = true;
		}

		UpdateSequencePlaybackRange();
	}
}

void FSequenceRecorder::DrawDebug(UCanvas* InCanvas, APlayerController* InPlayerController)
{
	const float NumFrames = 9.0f;
	const bool bCountingDown = CurrentDelay > 0.0f && CurrentDelay < NumFrames;

	if(bCountingDown)
	{
		const FVector2D IconSize(128.0f, 128.0f);
		const FVector2D HalfIconSize(64.0f, 64.0f);
		const float LineThickness = 2.0f;

		FVector2D Center;
		InCanvas->GetCenter(Center.X, Center.Y);
		FVector2D IconPosition = Center - HalfIconSize;

		InCanvas->SetDrawColor(FColor::White);

		FCanvasIcon Icon = UCanvas::MakeIcon(CountdownTexture.Get(), FMath::FloorToFloat(NumFrames - CurrentDelay) * IconSize.X, 0.0f, IconSize.X, IconSize.Y);
		InCanvas->DrawIcon(Icon, IconPosition.X, IconPosition.Y);
		
		// draw 'clock' line
		const float Angle = 2.0f * PI * FMath::Fmod(CurrentDelay, 1.0f);
		const FVector2D AxisX(0.f, -1.f);
		const FVector2D AxisY(-1.f, 0.f);
		const FVector2D EndPos = Center + (AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle)) * (InCanvas->SizeX + InCanvas->SizeY);
		FCanvasLineItem LineItem(Center, EndPos);
		LineItem.LineThickness = LineThickness;
		LineItem.SetColor(FLinearColor::Black);
		InCanvas->DrawItem(LineItem);

		// draw 'crosshairs'
		LineItem.Origin = FVector(0.0f, Center.Y, 0.0f);
		LineItem.EndPos = FVector(InCanvas->SizeX, Center.Y, 0.0f);
		InCanvas->DrawItem(LineItem);

		LineItem.Origin = FVector(Center.X, 0.0f, 0.0f);
		LineItem.EndPos = FVector(Center.X, InCanvas->SizeY, 0.0f);
		InCanvas->DrawItem(LineItem);
	}

	if(bCountingDown || IsRecording())
	{
		FText LabelText;
		if(IsRecording())
		{
			LabelText = FText::Format(LOCTEXT("RecordingIndicatorFormat", "{0}"), FText::FromName(CurrentSequence.Get()->GetFName()));
		}
		else
		{
			LabelText = FText::Format(LOCTEXT("RecordingIndicatorPending", "Pending recording: {0}"), FText::FromString(NextSequenceName));
		}


		float TimeAccumulator = CurrentTime;
		float Hours = FMath::FloorToFloat(TimeAccumulator / (60.0f * 60.0f));
		TimeAccumulator -= Hours * 60.0f * 60.0f;
		float Minutes = FMath::FloorToFloat(TimeAccumulator / 60.0f);
		TimeAccumulator -= Minutes * 60.0f;
		float Seconds = FMath::FloorToFloat(TimeAccumulator);
		TimeAccumulator -= Seconds;
		float Frames = FMath::FloorToFloat(TimeAccumulator * GetDefault<USequenceRecorderSettings>()->DefaultAnimationSettings.SampleRate);

		FNumberFormattingOptions Options;
		Options.MinimumIntegralDigits = 2;
		Options.MaximumIntegralDigits = 2;

		FFormatNamedArguments NamedArgs;
		NamedArgs.Add(TEXT("Hours"), FText::AsNumber((int32)Hours, &Options));
		NamedArgs.Add(TEXT("Minutes"), FText::AsNumber((int32)Minutes, &Options));
		NamedArgs.Add(TEXT("Seconds"), FText::AsNumber((int32)Seconds, &Options));
		NamedArgs.Add(TEXT("Frames"), FText::AsNumber((int32)Frames, &Options));
		FText TimeText = FText::Format(LOCTEXT("RecordingTimerFormat", "{Hours}:{Minutes}:{Seconds}:{Frames}"), NamedArgs);

		const FVector2D IconSize(32.0f, 32.0f);
		const FVector2D Offset(8.0f, 32.0f);

		InCanvas->SetDrawColor(FColor::White);

		FVector2D IconPosition(Offset.X, InCanvas->SizeY - (Offset.Y + IconSize.Y));
		FCanvasIcon Icon = UCanvas::MakeIcon(RecordingIndicatorTexture.Get(), FMath::FloorToFloat(NumFrames - CurrentDelay) * IconSize.X, 0.0f, IconSize.X, IconSize.Y);
		InCanvas->DrawIcon(Icon, IconPosition.X, IconPosition.Y);

		const float TextScale = 1.2f;
		float TextPositionY = 0.0f;
		// draw label
		{
			float XSize, YSize;
			InCanvas->TextSize(GEngine->GetLargeFont(), LabelText.ToString(), XSize, YSize, TextScale, TextScale);

			TextPositionY = (IconPosition.Y + (IconSize.Y * 0.5f)) - (YSize * 0.5f);

			FFontRenderInfo Info;
			Info.bEnableShadow = true;
			InCanvas->DrawText(GEngine->GetLargeFont(), LabelText, IconPosition.X + IconSize.X + 4.0f, TextPositionY, TextScale, TextScale, Info);
		}
		// draw time
		{
			float XSize, YSize;
			InCanvas->TextSize(GEngine->GetLargeFont(), TimeText.ToString(), XSize, YSize, TextScale, TextScale);

			FVector2D TimePosition(InCanvas->SizeX - (Offset.X + XSize), TextPositionY);

			FFontRenderInfo Info;
			Info.bEnableShadow = true;
			InCanvas->DrawText(GEngine->GetLargeFont(), TimeText, TimePosition.X, TimePosition.Y, TextScale, TextScale, Info);
		}
	}
}

bool FSequenceRecorder::StartRecording(const FOnRecordingStarted& OnRecordingStarted, const FOnRecordingFinished& OnRecordingFinished, const FString& InPathToRecordTo, const FString& InSequenceName)
{
	OnRecordingStartedDelegate = OnRecordingStarted;
	OnRecordingFinishedDelegate = OnRecordingFinished;

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	if(InPathToRecordTo.Len() > 0)
	{
		PathToRecordTo = InPathToRecordTo;
	}
	else
	{
		PathToRecordTo = Settings->SequenceRecordingBasePath.Path;
	}

	if(InSequenceName.Len() > 0)
	{
		SequenceName = InSequenceName;
	}
	else
	{
		SequenceName = Settings->SequenceName.Len() > 0 ? Settings->SequenceName : TEXT("RecordedSequence");
	}

	CurrentTime = 0.0f;

	if (Settings->bImmersiveMode)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr< ILevelViewport > ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

		if( ActiveLevelViewport.IsValid() )
		{
			bWasImmersive = ActiveLevelViewport->IsImmersive();

			if (!ActiveLevelViewport->IsImmersive())
			{
				const bool bWantImmersive = true;
				const bool bAllowAnimation = false;
				ActiveLevelViewport->MakeImmersive( bWantImmersive, bAllowAnimation );
			}
		}
	}

	RefreshNextSequence();

	if(Settings->RecordingDelay > 0.0f)
	{
		CurrentDelay = Settings->RecordingDelay;

		UE_LOG(LogAnimation, Display, TEXT("Starting sequence recording with delay of %f seconds"), CurrentDelay);

		return QueuedRecordings.Num() > 0;
	}
	
	return StartRecordingInternal(nullptr);
}

bool FSequenceRecorder::StartRecordingForReplay(UWorld* World, const FSequenceRecorderActorFilter& ActorFilter)
{
	// set up our recording settings
	USequenceRecorderSettings* Settings = GetMutableDefault<USequenceRecorderSettings>();
	
	Settings->bCreateLevelSequence = true;
	Settings->SequenceLength = 0.0f;
	Settings->RecordingDelay = 0.0f;
	Settings->bRecordNearbySpawnedActors = true;
	Settings->NearbyActorRecordingProximity = 0.0f;
	Settings->bRecordWorldSettingsActor = true;
	Settings->ActorFilter = ActorFilter;

	CurrentReplayWorld = World;
	
	return StartRecordingInternal(World);
}

bool FSequenceRecorder::StartRecordingInternal(UWorld* World)
{
	CurrentTime = 0.0f;

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	UWorld* ActorWorld = nullptr;
	if(Settings->bRecordWorldSettingsActor)
	{
		if(World != nullptr || (QueuedRecordings.Num() > 0 && QueuedRecordings[0]->GetActorToRecord() != nullptr))
		{
			ActorWorld = World != nullptr ? World : QueuedRecordings[0]->GetActorToRecord()->GetWorld();
			if(ActorWorld)
			{
				AWorldSettings* WorldSettings = ActorWorld->GetWorldSettings();
				AddNewQueuedRecording(WorldSettings);
			}
		}
	}

	// kick off level sequence actors we are syncing to
	for(TLazyObjectPtr<ALevelSequenceActor> LevelSequenceActor : Settings->LevelSequenceActorsToTrigger)
	{
		if(LevelSequenceActor.IsValid())
		{
			// find a counterpart in the PIE world if this actor is not
			ALevelSequenceActor* ActorToTrigger = LevelSequenceActor.Get();
			if(ActorWorld && ActorWorld->IsPlayInEditor() && !ActorToTrigger->GetWorld()->IsPlayInEditor())
			{
				ActorToTrigger = Cast<ALevelSequenceActor>(EditorUtilities::GetSimWorldCounterpartActor(ActorToTrigger));
			}

			if(ActorToTrigger && ActorToTrigger->SequencePlayer)
			{
				ActorToTrigger->SequencePlayer->Play();
			}
		}
	}

	if(QueuedRecordings.Num() > 0)
	{
		ULevelSequence* LevelSequence = nullptr;
		
		if(Settings->bCreateLevelSequence)
		{
			LevelSequence = SequenceRecorderUtils::MakeNewAsset<ULevelSequence>(PathToRecordTo, SequenceName);
			if(LevelSequence)
			{
				LevelSequence->Initialize();
				CurrentSequence = LevelSequence;

				FAssetRegistryModule::AssetCreated(LevelSequence);

				RefreshNextSequence();
			}
		}

		// register for spawning delegate in the world(s) of recorded actors
		for(UActorRecording* Recording : QueuedRecordings)
		{
			if(Recording->GetActorToRecord() != nullptr)
			{
				UWorld* ActorToRecordWorld = Recording->GetActorToRecord()->GetWorld();
				if(ActorToRecordWorld != nullptr)
				{
					FDelegateHandle* FoundHandle = ActorSpawningDelegateHandles.Find(ActorToRecordWorld);
					if(FoundHandle == nullptr)
					{
						FDelegateHandle NewHandle = ActorToRecordWorld->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateStatic(&FSequenceRecorder::HandleActorSpawned));
						ActorSpawningDelegateHandles.Add(ActorToRecordWorld, NewHandle);
					}
				}
			}
		}

		// start recording 
		bool bStartedRecordingAllActors = true;
		for(UActorRecording* Recording : QueuedRecordings)
		{
			if(!Recording->StartRecording(CurrentSequence.Get(), CurrentTime))
			{
				bStartedRecordingAllActors = false;
				break;
			}
		}

		if(!bStartedRecordingAllActors)
		{
			// if we couldnt start a recording, stop all others
			TArray<FAssetData> AssetsToCleanUp;
			if(LevelSequence)
			{
				AssetsToCleanUp.Add(LevelSequence);
			}

			for(UActorRecording* Recording : QueuedRecordings)
			{
				Recording->StopRecording(CurrentSequence.Get());
			}

			// clean up any assets that we can
			if(AssetsToCleanUp.Num() > 0)
			{
				ObjectTools::DeleteAssets(AssetsToCleanUp, false);
			}
		}

#if WITH_EDITOR
		// if recording via PIE, be sure to stop recording cleanly when PIE ends
		if (ActorWorld && ActorWorld->IsPlayInEditor())
		{
			FEditorDelegates::EndPIE.AddRaw(this, &FSequenceRecorder::HandleEndPIE);
		}
#endif

		if (LevelSequence)
		{
			UE_LOG(LogAnimation, Display, TEXT("Started recording sequence %s"), *LevelSequence->GetPathName());
		}

		// If we created an audio recorder at the start of the count down, then start recording
		// Create the audio recorder now before the count down finishes
		if (Settings->RecordAudio != EAudioRecordingMode::None)
		{
			if (LevelSequence)
			{
				FDirectoryPath AudioDirectory;
				AudioDirectory.Path = Settings->SequenceRecordingBasePath.Path;
				if (Settings->AudioSubDirectory.Len())
				{
					AudioDirectory.Path /= Settings->AudioSubDirectory;
				}

				ISequenceRecorder& Recorder = FModuleManager::Get().LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");

				FAudioRecorderSettings AudioSettings;
				AudioSettings.Directory = AudioDirectory;
				AudioSettings.AssetName = FText::Format(LOCTEXT("AudioFormatStr", "{0}_Audio"), FText::FromString(LevelSequence->GetName())).ToString();
				AudioSettings.RecordingDurationSec = Settings->SequenceLength;
				AudioSettings.GainDB = Settings->AudioGain;
				AudioSettings.InputBufferSize = Settings->AudioInputBufferSize;

				AudioRecorder = Recorder.CreateAudioRecorder();
				if (AudioRecorder)
				{
					AudioRecorder->Start(AudioSettings);
				}
			}
			else
			{
				UE_LOG(LogAnimation, Display, TEXT("'Create Level Sequence' needs to be enabled for audio recording"));
			}
		}

		OnRecordingStartedDelegate.ExecuteIfBound(CurrentSequence.Get());
		return true;
	}
	else
	{
		UE_LOG(LogAnimation, Display, TEXT("No recordings queued, aborting recording"));
	}

	return false;
} 

void FSequenceRecorder::HandleEndPIE(bool bSimulating)
{
	StopRecording();

#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif
}

bool FSequenceRecorder::StopRecording()
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	if (Settings->bImmersiveMode)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr< ILevelViewport > ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

		if( ActiveLevelViewport.IsValid() )
		{
			if (ActiveLevelViewport->IsImmersive() != bWasImmersive)
			{
				const bool bAllowAnimation = false;
				ActiveLevelViewport->MakeImmersive(bWasImmersive, bAllowAnimation);
			}
		}
	}

	// 1 step for the audio processing
	static const uint8 NumAdditionalSteps = 1;

	FScopedSlowTask SlowTask((float)(QueuedRecordings.Num() + DeadRecordings.Num() + NumAdditionalSteps), LOCTEXT("ProcessingRecording", "Processing Recording"));
	SlowTask.MakeDialog(false, true);

	// Process audio first so it doesn't record while we're processing the other captured state
	ULevelSequence* LevelSequence = CurrentSequence.Get();

	SlowTask.EnterProgressFrame(1.f, LOCTEXT("ProcessingAudio", "Processing Audio"));
	if (AudioRecorder && LevelSequence)
	{
		USoundWave* RecordedAudio = AudioRecorder->Stop();
		AudioRecorder.Reset();

		if (RecordedAudio)
		{
			// Add a new master audio track to the level sequence
			
			UMovieScene* MovieScene = LevelSequence->GetMovieScene();

			UMovieSceneAudioTrack* AudioTrack = MovieScene->FindMasterTrack<UMovieSceneAudioTrack>();
			if (!AudioTrack)
			{
				AudioTrack = MovieScene->AddMasterTrack<UMovieSceneAudioTrack>();
				AudioTrack->SetDisplayName(LOCTEXT("DefaultAudioTrackName", "Recorded Audio"));
			}

			int32 RowIndex = -1;
			for (UMovieSceneSection* Section : AudioTrack->GetAllSections())
			{
				RowIndex = FMath::Max(RowIndex, Section->GetRowIndex());
			}

			UMovieSceneAudioSection* NewAudioSection = NewObject<UMovieSceneAudioSection>(AudioTrack, UMovieSceneAudioSection::StaticClass());

			NewAudioSection->SetRowIndex(RowIndex + 1);
			NewAudioSection->SetSound(RecordedAudio);
			NewAudioSection->SetStartTime(0);
			NewAudioSection->SetEndTime(RecordedAudio->GetDuration());

			AudioTrack->AddSection(*NewAudioSection);
		}
	}



	CurrentDelay = 0.0f;

	// remove spawn delegates
	for(auto It = ActorSpawningDelegateHandles.CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<UWorld> World = It->Key;
		if(World.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(It->Value);
		}
	}

	ActorSpawningDelegateHandles.Empty();

	// also stop all dead animation recordings, i.e. ones that use GCed components
	const bool bShowMessage = false;
	FAnimationRecorderManager::Get().StopRecordingDeadAnimations(bShowMessage);

	for(UActorRecording* Recording : QueuedRecordings)
	{
		SlowTask.EnterProgressFrame();

		Recording->StopRecording(CurrentSequence.Get());
	}

	for(UActorRecording* Recording : DeadRecordings)
	{
		SlowTask.EnterProgressFrame();

		Recording->StopRecording(CurrentSequence.Get());
	}

	DeadRecordings.Empty();

	if(Settings->bCreateLevelSequence)
	{
		if(LevelSequence)
		{
			// set movie scene playback range to encompass all sections
			UpdateSequencePlaybackRange();

			// Stop referencing the sequence so we are listed as 'not recording'
			CurrentSequence = nullptr;

			if(GEditor == nullptr)
			{
				// auto-save asset outside of the editor
				UPackage* const Package = LevelSequence->GetOutermost();
				FString const PackageName = Package->GetName();
				FString const PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

				UPackage::SavePackage(Package, NULL, RF_Standalone, *PackageFileName, GError, nullptr, false, true, SAVE_NoError);
			}

			if(FSlateApplication::IsInitialized())
			{
				const FText NotificationText = FText::Format(LOCTEXT("RecordSequence", "'{0}' has been successfully recorded."), FText::FromString(LevelSequence->GetName()));
					
				FNotificationInfo Info(NotificationText);
				Info.ExpireDuration = 8.0f;
				Info.bUseLargeFont = false;
				Info.Hyperlink = FSimpleDelegate::CreateLambda([=]()
				{
					TArray<UObject*> Assets;
					Assets.Add(LevelSequence);
					FAssetEditorManager::Get().OpenEditorForAssets(Assets);
				});
				Info.HyperlinkText = FText::Format(LOCTEXT("OpenNewAnimationHyperlink", "Open {0}"), FText::FromString(LevelSequence->GetName()));
				TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
				if ( Notification.IsValid() )
				{
					Notification->SetCompletionState( SNotificationItem::CS_Success );
				}
			}

			UE_LOG(LogAnimation, Display, TEXT("Stopped recording sequence %s"), *LevelSequence->GetPathName());

			OnRecordingFinishedDelegate.ExecuteIfBound(LevelSequence);

			OnRecordingFinishedDelegate = FOnRecordingFinished();
			OnRecordingStartedDelegate = FOnRecordingStarted();

			return true;
		}
	}
	else
	{
		UE_LOG(LogAnimation, Display, TEXT("Stopped recording, no sequence created"));
		return true;
	}

	return false;
}

void FSequenceRecorder::UpdateSequencePlaybackRange()
{
	if(CurrentSequence.IsValid())
	{
		UMovieScene* MovieScene = CurrentSequence->GetMovieScene();
		if(MovieScene)
		{
			float MaxRange = 0.0f;
			float MinRange = 0.0f;

			TArray<UMovieSceneSection*> MovieSceneSections = MovieScene->GetAllSections();
			for(UMovieSceneSection* Section : MovieSceneSections)
			{
				MaxRange = FMath::Max(MaxRange, Section->GetEndTime());
				MinRange = FMath::Min(MinRange, Section->GetStartTime());
			}

			MovieScene->SetPlaybackRange(MinRange, MaxRange);

			// Initialize the working and view range with a little bit more space
			const float OutputViewSize = MaxRange - MinRange;
			const float OutputChange = OutputViewSize * 0.1f;

			MovieScene->SetWorkingRange(MinRange - OutputChange, MaxRange + OutputChange);
			MovieScene->SetViewRange(MinRange - OutputChange, MaxRange + OutputChange);
		}
	}
}

bool FSequenceRecorder::IsDelaying() const
{
	return CurrentDelay > 0.0f;
}

float FSequenceRecorder::GetCurrentDelay() const
{
	return CurrentDelay;
}

bool FSequenceRecorder::IsActorValidForRecording(AActor* Actor)
{
	check(Actor);

	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	float Distance = Settings->NearbyActorRecordingProximity;

	// check distance if valid
	if(Settings->bRecordNearbySpawnedActors && Distance > 0.0f)
	{
		const FTransform ActorTransform = Actor->GetTransform();
		const FVector ActorTranslation = ActorTransform.GetTranslation();

		for(UActorRecording* Recording : QueuedRecordings)
		{
			if(AActor* OtherActor = Recording->GetActorToRecord())
			{
				if(OtherActor != Actor)
				{
					const FTransform OtherActorTransform = OtherActor->GetTransform();
					const FVector OtherActorTranslation = OtherActorTransform.GetTranslation();

					if((OtherActorTranslation - ActorTranslation).Size() < Distance)
					{
						return true;
					}
				}
			}
		}
	}

	// check class if any
	for(const TSubclassOf<AActor>& ActorClass : Settings->ActorFilter.ActorClassesToRecord)
	{
		if(*ActorClass != nullptr && Actor->IsA(*ActorClass))
		{
			return true;
		}
	}

	return false;
}

void FSequenceRecorder::HandleActorSpawned(AActor* Actor)
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();

	if(Actor && FSequenceRecorder::Get().IsRecording())
	{
		// Queue a possible recording - we need to wait until the actor has begun playing
		FSequenceRecorder::Get().QueuedSpawnedActors.Add(Actor);
	}
}

void FSequenceRecorder::HandleActorDespawned(AActor* Actor)
{
	if(Actor && FSequenceRecorder::Get().IsRecording())
	{
		for(int32 Index = 0; Index < QueuedRecordings.Num(); ++Index)
		{
			UActorRecording* Recording = QueuedRecordings[Index];
			if(Recording->GetActorToRecord() == Actor)
			{
				Recording->InvalidateObjectToRecord();
				DeadRecordings.Add(Recording);
				QueuedRecordings.RemoveAt(Index);
				break;
			}
		}
	}
}

void FSequenceRecorder::RefreshNextSequence()
{
	const USequenceRecorderSettings* Settings = GetDefault<USequenceRecorderSettings>();
	if (SequenceName.IsEmpty())
	{
		SequenceName = Settings->SequenceName.Len() > 0 ? Settings->SequenceName : TEXT("RecordedSequence");
	}

	// Cache the name of the next sequence we will try to record to
	NextSequenceName = SequenceRecorderUtils::MakeNewAssetName<ULevelSequence>(Settings->SequenceRecordingBasePath.Path, SequenceName);
}

#undef LOCTEXT_NAMESPACE
