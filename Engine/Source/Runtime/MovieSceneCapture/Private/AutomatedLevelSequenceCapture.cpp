// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AutomatedLevelSequenceCapture.h"
#include "MovieScene.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Slate/SceneViewport.h"
#include "Misc/CommandLine.h"
#include "LevelSequenceActor.h"
#include "JsonObjectConverter.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "MovieSceneCaptureHelpers.h"
#include "EngineUtils.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "TimerManager.h"

UAutomatedLevelSequenceCapture::UAutomatedLevelSequenceCapture(const FObjectInitializer& Init)
	: Super(Init)
{
#if WITH_EDITORONLY_DATA == 0
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		checkf(false, TEXT("Automated level sequence captures can only be used in editor builds."));
	}
#else
	bUseCustomStartFrame = false;
	StartFrame = 0;
	bUseCustomEndFrame = false;
	EndFrame = 1;
	WarmUpFrameCount = 0;
	DelayBeforeWarmUp = 0.0f;
	bWriteEditDecisionList = true;

	RemainingWarmUpFrames = 0;

	NumShots = 0;
	ShotIndex = -1;

	BurnInOptions = Init.CreateDefaultSubobject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
#endif
}

#if WITH_EDITORONLY_DATA

void UAutomatedLevelSequenceCapture::SetLevelSequenceAsset(FString AssetPath)
{
	LevelSequenceAsset = MoveTemp(AssetPath);
}

void UAutomatedLevelSequenceCapture::AddFormatMappings(TMap<FString, FStringFormatArg>& OutFormatMappings, const FFrameMetrics& FrameMetrics) const
{
	OutFormatMappings.Add(TEXT("shot"), CachedState.CurrentShotName.ToString());

	const int32 FrameNumber = FMath::RoundToInt(CachedState.CurrentShotLocalTime * CachedState.Settings.FrameRate);
	OutFormatMappings.Add(TEXT("shot_frame"), FString::Printf(TEXT("%0*d"), Settings.ZeroPadFrameNumbers, FrameNumber));
}

void UAutomatedLevelSequenceCapture::Initialize(TSharedPtr<FSceneViewport> InViewport, int32 PIEInstance)
{
	Viewport = InViewport;

	// Apply command-line overrides from parent class first. This needs to be called before setting up the capture strategy with the desired frame rate.
	Super::Initialize(InViewport);

	// Apply command-line overrides
	{
		FString LevelSequenceAssetPath;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-LevelSequence=" ), LevelSequenceAssetPath ) )
		{
			LevelSequenceAsset.SetPath( LevelSequenceAssetPath );
		}

		int32 StartFrameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieStartFrame=" ), StartFrameOverride ) )
		{
			bUseCustomStartFrame = true;
			StartFrame = StartFrameOverride;
		}

		int32 EndFrameOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieEndFrame=" ), EndFrameOverride ) )
		{
			bUseCustomEndFrame = true;
			EndFrame = EndFrameOverride;
		}

		int32 WarmUpFrameCountOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieWarmUpFrames=" ), WarmUpFrameCountOverride ) )
		{
			WarmUpFrameCount = WarmUpFrameCountOverride;
		}

		float DelayBeforeWarmUpOverride;
		if( FParse::Value( FCommandLine::Get(), TEXT( "-MovieDelayBeforeWarmUp=" ), DelayBeforeWarmUpOverride ) )
		{
			DelayBeforeWarmUp = DelayBeforeWarmUpOverride;
		}
	}

	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	// If we don't have a valid actor, attempt to find a level sequence actor in the world that references this asset
	if( Actor == nullptr )
	{
		if( LevelSequenceAsset.IsValid() )
		{
			ULevelSequence* Asset = Cast<ULevelSequence>( LevelSequenceAsset.TryLoad() );
			if( Asset != nullptr )
			{
				for( auto It = TActorIterator<ALevelSequenceActor>( InViewport->GetClient()->GetWorld() ); It; ++It )
				{
					if( It->LevelSequence == LevelSequenceAsset )
					{
						// Found it!
						Actor = *It;
						this->LevelSequenceActor = Actor;

						break;
					}
				}
			}
		}
	}

	if (!Actor)
	{
		ULevelSequence* Asset = Cast<ULevelSequence>(LevelSequenceAsset.TryLoad());
		if (Asset)
		{
			// Spawn a new actor
			Actor = InViewport->GetClient()->GetWorld()->SpawnActor<ALevelSequenceActor>();
			Actor->SetSequence(Asset);
			// Ensure it doesn't loop (-1 is indefinite)
			Actor->PlaybackSettings.LoopCount = 0;
		
			LevelSequenceActor = Actor;
		}
		else
		{
			//FPlatformMisc::RequestExit(FMovieSceneCaptureExitCodes::AssetNotFound);
		}
	}

	if (Actor)
	{
		if (BurnInOptions)
		{
			Actor->BurnInOptions = BurnInOptions;

			bool bUseBurnIn = false;
			if( FParse::Bool( FCommandLine::Get(), TEXT( "-UseBurnIn=" ), bUseBurnIn ) )
			{
				Actor->BurnInOptions->bUseBurnIn = bUseBurnIn;
			}
		}

		Actor->RefreshBurnIn();

		// Make sure we're not playing yet (in case AutoPlay was called from BeginPlay)
		if( Actor->SequencePlayer != nullptr && Actor->SequencePlayer->IsPlaying() )
		{
			Actor->SequencePlayer->Stop();
		}
		Actor->bAutoPlay = false;

		if (InitializeShots())
		{
			float StartTime, EndTime;
			SetupShot(StartTime, EndTime);
		}
	}

	ExportEDL();

	CaptureState = ELevelSequenceCaptureState::Setup;
	CaptureStrategy = MakeShareable(new FFixedTimeStepCaptureStrategy(Settings.FrameRate));
}

UMovieScene* GetMovieScene(TWeakObjectPtr<ALevelSequenceActor> LevelSequenceActor)
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	if (!Actor)
	{
		return nullptr;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>( Actor->LevelSequence.TryLoad() );
	if (!LevelSequence)
	{
		return nullptr;
	}

	return LevelSequence->GetMovieScene();
}

UMovieSceneCinematicShotTrack* GetCinematicShotTrack(TWeakObjectPtr<ALevelSequenceActor> LevelSequenceActor)
{
	UMovieScene* MovieScene = GetMovieScene(LevelSequenceActor);
	if (!MovieScene)
	{
		return nullptr;
	}

	return MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
}

bool UAutomatedLevelSequenceCapture::InitializeShots()
{
	NumShots = 0;
	ShotIndex = -1;
	CachedShotStates.Empty();

	if (Settings.HandleFrames <= 0)
	{
		return false;
	}

	UMovieScene* MovieScene = GetMovieScene(LevelSequenceActor);
	if (!MovieScene)
	{
		return false;
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = GetCinematicShotTrack(LevelSequenceActor);
	if (!CinematicShotTrack)
	{
		return false;
	}

	NumShots = CinematicShotTrack->GetAllSections().Num();					
	ShotIndex = 0;
	CachedPlaybackRange = MovieScene->GetPlaybackRange();

	float HandleTime = (float)Settings.HandleFrames / (float)Settings.FrameRate;
					
	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(CinematicShotTrack->GetAllSections()[SectionIndex]);
		UMovieScene* ShotMovieScene = ShotSection->GetSequence() ? ShotSection->GetSequence()->GetMovieScene() : nullptr;
		CachedShotStates.Add(FCinematicShotCache(ShotSection->IsActive(), ShotSection->IsLocked(), TRange<float>(ShotSection->GetStartTime(), ShotSection->GetEndTime()), ShotMovieScene ? ShotMovieScene->GetPlaybackRange() : TRange<float>::Empty()));

		if (ShotMovieScene)
		{
			ShotMovieScene->SetPlaybackRange(ShotMovieScene->GetPlaybackRange().GetLowerBoundValue() - HandleTime, ShotMovieScene->GetPlaybackRange().GetUpperBoundValue() + HandleTime, false);
		}
		ShotSection->SetIsLocked(false);
		ShotSection->SetIsActive(false);
		ShotSection->SetStartTime(ShotSection->GetStartTime() - HandleTime);
		ShotSection->SetEndTime(ShotSection->GetEndTime() + HandleTime);
	}
	return NumShots > 0;
}

void UAutomatedLevelSequenceCapture::RestoreShots()
{
	if (Settings.HandleFrames <= 0)
	{
		return;
	}

	UMovieScene* MovieScene = GetMovieScene(LevelSequenceActor);
	if (!MovieScene)
	{
		return;
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = GetCinematicShotTrack(LevelSequenceActor);
	if (!CinematicShotTrack)
	{
		return;
	}

	MovieScene->SetPlaybackRange(CachedPlaybackRange.GetLowerBoundValue(), CachedPlaybackRange.GetUpperBoundValue(), false);

	float HandleTime = (float)Settings.HandleFrames / (float)Settings.FrameRate;
					
	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneCinematicShotSection* ShotSection = Cast<UMovieSceneCinematicShotSection>(CinematicShotTrack->GetAllSections()[SectionIndex]);
		UMovieScene* ShotMovieScene = ShotSection->GetSequence() ? ShotSection->GetSequence()->GetMovieScene() : nullptr;
		if (ShotMovieScene)
		{
			ShotMovieScene->SetPlaybackRange(CachedShotStates[SectionIndex].MovieSceneRange.GetLowerBoundValue(), CachedShotStates[SectionIndex].MovieSceneRange.GetUpperBoundValue(), false);
		}
		ShotSection->SetIsActive(CachedShotStates[SectionIndex].bActive);
		ShotSection->SetStartTime(CachedShotStates[SectionIndex].ShotRange.GetLowerBoundValue());
		ShotSection->SetEndTime(CachedShotStates[SectionIndex].ShotRange.GetUpperBoundValue());
		ShotSection->SetIsLocked(CachedShotStates[SectionIndex].bLocked);
	}
}

bool UAutomatedLevelSequenceCapture::SetupShot(float& StartTime, float& EndTime)
{
	if (Settings.HandleFrames <= 0)
	{
		return false;
	}

	UMovieScene* MovieScene = GetMovieScene(LevelSequenceActor);
	if (!MovieScene)
	{
		return false;
	}

	UMovieSceneCinematicShotTrack* CinematicShotTrack = GetCinematicShotTrack(LevelSequenceActor);
	if (!CinematicShotTrack)
	{
		return false;
	}

	if (ShotIndex > CinematicShotTrack->GetAllSections().Num()-1)
	{
		return false;
	}

	float HandleTime = (float)Settings.HandleFrames / (float)Settings.FrameRate;

	// Disable all shots unless it's the current one being rendered
	for (int32 SectionIndex = 0; SectionIndex < CinematicShotTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneSection* ShotSection = CinematicShotTrack->GetAllSections()[SectionIndex];

		ShotSection->SetIsActive(SectionIndex == ShotIndex);

		if (SectionIndex == ShotIndex)
		{
			StartTime = ShotSection->GetStartTime();
			EndTime = ShotSection->GetEndTime();

			StartTime = FMath::Clamp(StartTime, CachedPlaybackRange.GetLowerBoundValue(), CachedPlaybackRange.GetUpperBoundValue());
			EndTime = FMath::Clamp(EndTime, CachedPlaybackRange.GetLowerBoundValue(), CachedPlaybackRange.GetUpperBoundValue());
			MovieScene->SetPlaybackRange(StartTime, EndTime, false);
		}
	}

	return true;
}

void UAutomatedLevelSequenceCapture::SetupFrameRange()
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	if( Actor )
	{
		ULevelSequence* LevelSequence = Cast<ULevelSequence>( Actor->LevelSequence.TryLoad() );
		if( LevelSequence != nullptr )
		{
			UMovieScene* MovieScene = LevelSequence->GetMovieScene();
			if( MovieScene != nullptr )
			{
				const int32 SequenceStartFrame = FMath::RoundToInt( MovieScene->GetPlaybackRange().GetLowerBoundValue() * Settings.FrameRate );
				const int32 SequenceEndFrame = FMath::Max( SequenceStartFrame, FMath::RoundToInt( MovieScene->GetPlaybackRange().GetUpperBoundValue() * Settings.FrameRate ) );

				// Default to playing back the sequence's stored playback range
				int32 PlaybackStartFrame = SequenceStartFrame;
				int32 PlaybackEndFrame = SequenceEndFrame;

				if( bUseCustomStartFrame )
				{
					PlaybackStartFrame = Settings.bUseRelativeFrameNumbers ? ( SequenceStartFrame + StartFrame ) : StartFrame;
				}

				if( !Settings.bUseRelativeFrameNumbers )
				{
					// NOTE: The frame number will be an offset from the first frame that we start capturing on, not the frame
					// that we start playback at (in the case of WarmUpFrameCount being non-zero).  So we'll cache out frame
					// number offset before adjusting for the warm up frames.
					this->FrameNumberOffset = PlaybackStartFrame;
				}

				if( bUseCustomEndFrame )
				{
					PlaybackEndFrame = FMath::Max( PlaybackStartFrame, Settings.bUseRelativeFrameNumbers ? ( SequenceEndFrame + EndFrame ) : EndFrame );

					// We always add 1 to the number of frames we want to capture, because we want to capture both the start and end frames (which if the play range is 0, would still yield a single frame)
					this->FrameCount = ( PlaybackEndFrame - PlaybackStartFrame ) + 1;
				}
				else
				{
					FrameCount = 0;
				}

				RemainingWarmUpFrames = FMath::Max( WarmUpFrameCount, 0 );
				if( RemainingWarmUpFrames > 0 )
				{
					// We were asked to playback additional frames before we start capturing
					PlaybackStartFrame -= RemainingWarmUpFrames;
				}

				// Override the movie scene's playback range
				Actor->SequencePlayer->SetPlaybackRange(
					(float)PlaybackStartFrame / (float)Settings.FrameRate,
					(float)PlaybackEndFrame / (float)Settings.FrameRate );
				Actor->SequencePlayer->SetPlaybackPosition(0.f);

				const float WarmupTime = WarmUpFrameCount / CachedState.Settings.FrameRate;
				Actor->SequencePlayer->SetSnapshotOffsetTime(WarmupTime);
			}
		}
	}
}

void UAutomatedLevelSequenceCapture::EnableCinematicMode()
{
	if (!GetSettings().bCinematicMode)
	{
		return;
	}

	// iterate through the controller list and set cinematic mode if necessary
	bool bNeedsCinematicMode = !GetSettings().bAllowMovement || !GetSettings().bAllowTurning || !GetSettings().bShowPlayer || !GetSettings().bShowHUD;
	if (!bNeedsCinematicMode)
	{
		return;
	}

	if (Viewport.IsValid())
	{
		for (FConstPlayerControllerIterator Iterator = Viewport.Pin()->GetClient()->GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController *PC = Iterator->Get();
			if (PC->IsLocalController())
			{
				PC->SetCinematicMode(true, !GetSettings().bShowPlayer, !GetSettings().bShowHUD, !GetSettings().bAllowMovement, !GetSettings().bAllowTurning);
			}
		}
	}
}

void UAutomatedLevelSequenceCapture::Tick(float DeltaSeconds)
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	if (!Actor || !Actor->SequencePlayer)
	{
		return;
	}

	// Setup the automated capture
	if (CaptureState == ELevelSequenceCaptureState::Setup)
	{
		SetupFrameRange();

		EnableCinematicMode();
		
		// Bind to the event so we know when to capture a frame
		OnPlayerUpdatedBinding = Actor->SequencePlayer->OnSequenceUpdated().AddUObject( this, &UAutomatedLevelSequenceCapture::SequenceUpdated );

		if (DelayBeforeWarmUp > 0)
		{
			CaptureState = ELevelSequenceCaptureState::DelayBeforeWarmUp;

			Actor->GetWorld()->GetTimerManager().SetTimer(DelayTimer, FTimerDelegate::CreateUObject(this, &UAutomatedLevelSequenceCapture::DelayBeforeWarmupFinished), DelayBeforeWarmUp, false);
		}
		else
		{
			DelayBeforeWarmupFinished();
		}
	}

	// Then we'll just wait a little bit.  We'll delay the specified number of seconds before capturing to allow any
	// textures to stream in or post processing effects to settle.
	if( CaptureState == ELevelSequenceCaptureState::DelayBeforeWarmUp )
	{
		// Ensure evaluation at the start of the sequence/shot
		Actor->SequencePlayer->SetPlaybackPosition(0.f);
	}
	else if( CaptureState == ELevelSequenceCaptureState::ReadyToWarmUp )
	{
		Actor->SequencePlayer->SetSnapshotSettings(FLevelSequenceSnapshotSettings(Settings.ZeroPadFrameNumbers, Settings.FrameRate));
		Actor->SequencePlayer->StartPlayingNextTick();
		// Start warming up
		CaptureState = ELevelSequenceCaptureState::WarmingUp;
	}

	// Count down our warm up frames.
	// The post increment is important - it ensures we capture the very first frame if there are no warm up frames,
	// but correctly skip n frames if there are n warmup frames
	if( CaptureState == ELevelSequenceCaptureState::WarmingUp && RemainingWarmUpFrames-- == 0)
	{
		// Start capturing - this will capture the *next* update from sequencer
		CaptureState = ELevelSequenceCaptureState::FinishedWarmUp;
		UpdateFrameState();
		StartCapture();
	}

	if( bCapturing && !Actor->SequencePlayer->IsPlaying() )
	{
		++ShotIndex;

		float StartTime, EndTime;
		if (SetupShot(StartTime, EndTime))
		{
			Actor->SequencePlayer->SetPlaybackRange(StartTime, EndTime);
			Actor->SequencePlayer->SetPlaybackPosition(0.f);
			Actor->SequencePlayer->StartPlayingNextTick();
			CaptureState = ELevelSequenceCaptureState::FinishedWarmUp;
			UpdateFrameState();
		}
		else
		{
			Actor->SequencePlayer->OnSequenceUpdated().Remove( OnPlayerUpdatedBinding );
			FinalizeWhenReady();
		}
	}
}

void UAutomatedLevelSequenceCapture::DelayBeforeWarmupFinished()
{
	StartWarmup();

	// Wait a frame to go by after we've set the fixed time step, so that the animation starts
	// playback at a consistent time
	CaptureState = ELevelSequenceCaptureState::ReadyToWarmUp;
}

void UAutomatedLevelSequenceCapture::PauseFinished()
{
	CaptureState = ELevelSequenceCaptureState::FinishedWarmUp;

	if (CachedPlayRate.IsSet())
	{
		ALevelSequenceActor* Actor = LevelSequenceActor.Get();
	
		// Force an evaluation to capture this frame
		Actor->SequencePlayer->SetPlaybackPosition(Actor->SequencePlayer->GetPlaybackPosition());
		
		// Continue playing forwards
		Actor->SequencePlayer->SetPlayRate(CachedPlayRate.GetValue());
		CachedPlayRate.Reset();
	}
}

void UAutomatedLevelSequenceCapture::SequenceUpdated(const UMovieSceneSequencePlayer& Player, float CurrentTime, float PreviousTime)
{
	if (bCapturing)
	{
		FLevelSequencePlayerSnapshot PreviousState = CachedState;

		UpdateFrameState();

		ALevelSequenceActor* Actor = LevelSequenceActor.Get();
		if (Actor && Actor->SequencePlayer)
		{
			// If this is a new shot, set the state to shot warm up and pause on this frame until warmed up			
			bool bHasMultipleShots = !PreviousState.CurrentShotName.IdenticalTo(PreviousState.MasterName);
			bool bNewShot = bHasMultipleShots && PreviousState.ShotID != CachedState.ShotID;

			if (bNewShot && Actor->SequencePlayer->IsPlaying() && DelayBeforeWarmUp > 0)
			{
				CaptureState = ELevelSequenceCaptureState::Paused;
				Actor->GetWorld()->GetTimerManager().SetTimer(DelayTimer, FTimerDelegate::CreateUObject(this, &UAutomatedLevelSequenceCapture::PauseFinished), DelayBeforeWarmUp, false);
				CachedPlayRate = Actor->SequencePlayer->GetPlayRate();
				Actor->SequencePlayer->SetPlayRate(0.f);
			}
			else if (CaptureState == ELevelSequenceCaptureState::FinishedWarmUp)
			{
				CaptureThisFrame(CurrentTime - PreviousTime);
			}
		}
	}
}

void UAutomatedLevelSequenceCapture::UpdateFrameState()
{
	ALevelSequenceActor* Actor = LevelSequenceActor.Get();

	if (Actor && Actor->SequencePlayer)
	{
		Actor->SequencePlayer->TakeFrameSnapshot(CachedState);
	}
}

void UAutomatedLevelSequenceCapture::LoadFromConfig()
{
	UMovieSceneCapture::LoadFromConfig();

	BurnInOptions->LoadConfig();
	BurnInOptions->ResetSettings();
	if (BurnInOptions->Settings)
	{
		BurnInOptions->Settings->LoadConfig();
	}
}

void UAutomatedLevelSequenceCapture::SaveToConfig()
{
	int32 CurrentStartFrame = StartFrame;
	int32 CurrentEndFrame = EndFrame;
	bool bRestoreFrameOverrides = RestoreFrameOverrides();

	BurnInOptions->SaveConfig();
	if (BurnInOptions->Settings)
	{
		BurnInOptions->Settings->SaveConfig();
	}

	UMovieSceneCapture::SaveToConfig();

	if (bRestoreFrameOverrides)
	{
		SetFrameOverrides(CurrentStartFrame, CurrentEndFrame);
	}
}

void UAutomatedLevelSequenceCapture::Close()
{
	Super::Close();
			
	RestoreShots();
}

bool UAutomatedLevelSequenceCapture::RestoreFrameOverrides()
{
	bool bAnySet = CachedStartFrame.IsSet() || CachedEndFrame.IsSet() || bCachedUseCustomStartFrame.IsSet() || bCachedUseCustomEndFrame.IsSet();
	if (CachedStartFrame.IsSet())
	{
		StartFrame = CachedStartFrame.GetValue();
		CachedStartFrame.Reset();
	}

	if (CachedEndFrame.IsSet())
	{
		EndFrame = CachedEndFrame.GetValue();
		CachedEndFrame.Reset();
	}

	if (bCachedUseCustomStartFrame.IsSet())
	{
		bUseCustomStartFrame = bCachedUseCustomStartFrame.GetValue();
		bCachedUseCustomStartFrame.Reset();
	}

	if (bCachedUseCustomEndFrame.IsSet())
	{
		bUseCustomEndFrame = bCachedUseCustomEndFrame.GetValue();
		bCachedUseCustomEndFrame.Reset();
	}

	return bAnySet;
}

void UAutomatedLevelSequenceCapture::SetFrameOverrides(int32 InStartFrame, int32 InEndFrame)
{
	CachedStartFrame = StartFrame;
	CachedEndFrame = EndFrame;
	bCachedUseCustomStartFrame = bUseCustomStartFrame;
	bCachedUseCustomEndFrame = bUseCustomEndFrame;

	StartFrame = InStartFrame;
	EndFrame = InEndFrame;
	bUseCustomStartFrame = true;
	bUseCustomEndFrame = true;
}

void UAutomatedLevelSequenceCapture::SerializeAdditionalJson(FJsonObject& Object)
{
	TSharedRef<FJsonObject> OptionsContainer = MakeShareable(new FJsonObject);
	if (FJsonObjectConverter::UStructToJsonObject(BurnInOptions->GetClass(), BurnInOptions, OptionsContainer, 0, 0))
	{
		Object.SetField(TEXT("BurnInOptions"), MakeShareable(new FJsonValueObject(OptionsContainer)));
	}

	if (BurnInOptions->Settings)
	{
		TSharedRef<FJsonObject> SettingsDataObject = MakeShareable(new FJsonObject);
		if (FJsonObjectConverter::UStructToJsonObject(BurnInOptions->Settings->GetClass(), BurnInOptions->Settings, SettingsDataObject, 0, 0))
		{
			Object.SetField(TEXT("BurnInOptionsInitSettings"), MakeShareable(new FJsonValueObject(SettingsDataObject)));
		}
	}
}

void UAutomatedLevelSequenceCapture::DeserializeAdditionalJson(const FJsonObject& Object)
{
	if (!BurnInOptions)
	{
		BurnInOptions = NewObject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
	}

	TSharedPtr<FJsonValue> OptionsContainer = Object.TryGetField(TEXT("BurnInOptions"));
	if (OptionsContainer.IsValid())
	{
		FJsonObjectConverter::JsonAttributesToUStruct(OptionsContainer->AsObject()->Values, BurnInOptions->GetClass(), BurnInOptions, 0, 0);
	}

	BurnInOptions->ResetSettings();
	if (BurnInOptions->Settings)
	{
		TSharedPtr<FJsonValue> SettingsDataObject = Object.TryGetField(TEXT("BurnInOptionsInitSettings"));
		if (SettingsDataObject.IsValid())
		{
			FJsonObjectConverter::JsonAttributesToUStruct(SettingsDataObject->AsObject()->Values, BurnInOptions->Settings->GetClass(), BurnInOptions->Settings, 0, 0);
		}
	}
}

void UAutomatedLevelSequenceCapture::ExportEDL()
{
	if (!bWriteEditDecisionList)
	{
		return;
	}
	
	UMovieScene* MovieScene = GetMovieScene(LevelSequenceActor);
	if (!MovieScene)
	{
		return;
	}

	UMovieSceneCinematicShotTrack* ShotTrack = MovieScene->FindMasterTrack<UMovieSceneCinematicShotTrack>();
	if (!ShotTrack)
	{
		return;
	}

	FString SaveFilename = 	Settings.OutputDirectory.Path / MovieScene->GetOuter()->GetName();
	int32 HandleFrames = Settings.HandleFrames;

	MovieSceneCaptureHelpers::ExportEDL(MovieScene, Settings.FrameRate, SaveFilename, HandleFrames);
}


#endif
