// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayer.h"
#include "MediaAssetsPrivate.h"

#include "IMediaClock.h"
#include "IMediaControls.h"
#include "IMediaModule.h"
#include "IMediaPlayer.h"
#include "IMediaPlayerFactory.h"
#include "IMediaTicker.h"
#include "IMediaTracks.h"
#include "MediaPlayerFacade.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "MediaPlaylist.h"
#include "MediaSource.h"
#include "StreamMediaSource.h"


/* UMediaPlayer structors
 *****************************************************************************/

UMediaPlayer::UMediaPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CacheAhead(FTimespan::FromMilliseconds(100))
	, CacheBehind(FTimespan::FromMilliseconds(3000))
	, CacheBehindGame(FTimespan::FromMilliseconds(100))
	, PlayOnOpen(true)
	, Shuffle(false)
	, Loop(false)
	, PlaylistIndex(INDEX_NONE)
	, HorizontalFieldOfView(90.0f)
	, VerticalFieldOfView(60.0f)
	, ViewRotation(FRotator::ZeroRotator)
	, PlayerFacade(MakeShareable(new FMediaPlayerFacade))
	, PlayerGuid(FGuid::NewGuid())
	, PlayOnNext(false)
#if WITH_EDITOR
	, WasPlayingInPIE(false)
#endif
{
	PlayerFacade->OnMediaEvent().AddUObject(this, &UMediaPlayer::HandlePlayerMediaEvent);
	Playlist = NewObject<UMediaPlaylist>(GetTransientPackage(), NAME_None, RF_Transactional | RF_Transient);
}


/* UMediaPlayer interface
 *****************************************************************************/

bool UMediaPlayer::CanPause() const
{
	return PlayerFacade->CanPause();
}


bool UMediaPlayer::CanPlaySource(UMediaSource* MediaSource)
{
	if ((MediaSource == nullptr) || !MediaSource->Validate())
	{
		return false;
	}

	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.CanPlaySource"), *GetFName().ToString(), *MediaSource->GetFName().ToString());

	return PlayerFacade->CanPlayUrl(MediaSource->GetUrl(), MediaSource);
}


bool UMediaPlayer::CanPlayUrl(const FString& Url)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.CanPlayUrl"), *GetFName().ToString(), *Url);

	if (Url.IsEmpty())
	{
		return false;
	}

	return PlayerFacade->CanPlayUrl(Url, GetDefault<UMediaSource>());
}


void UMediaPlayer::Close()
{
	UE_LOG(LogMediaAssets, VeryVerbose, TEXT("%s.Close"), *GetFName().ToString());

	PlayerFacade->Close();

	Playlist = NewObject<UMediaPlaylist>(GetTransientPackage(), NAME_None, RF_Transactional | RF_Transient);
	PlaylistIndex = INDEX_NONE;
	PlayOnNext = false;
}


int32 UMediaPlayer::GetAudioTrackChannels(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetAudioTrackChannels(TrackIndex, FormatIndex);
}


int32 UMediaPlayer::GetAudioTrackSampleRate(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetAudioTrackSampleRate(TrackIndex, FormatIndex);
}


FString UMediaPlayer::GetAudioTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetAudioTrackType(TrackIndex, FormatIndex);
}


FName UMediaPlayer::GetDesiredPlayerName() const
{
	return PlayerFacade->DesiredPlayerName;
}


FTimespan UMediaPlayer::GetDuration() const
{
	return PlayerFacade->GetDuration();
}


float UMediaPlayer::GetHorizontalFieldOfView() const
{
	float OutHorizontal = 0.0f;
	float OutVertical = 0.0f;

	if (!PlayerFacade->GetViewField(OutHorizontal, OutVertical))
	{
		return 0.0f;
	}

	return OutHorizontal;
}


FText UMediaPlayer::GetMediaName() const
{
	return PlayerFacade->GetMediaName();
}


int32 UMediaPlayer::GetNumTracks(EMediaPlayerTrack TrackType) const
{
	return PlayerFacade->GetNumTracks((EMediaTrackType)TrackType);
}


int32 UMediaPlayer::GetNumTrackFormats(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return PlayerFacade->GetNumTrackFormats((EMediaTrackType)TrackType, TrackIndex);
}


TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> UMediaPlayer::GetPlayerFacade() const
{
	return PlayerFacade.ToSharedRef();
}


FName UMediaPlayer::GetPlayerName() const
{
	return PlayerFacade->GetPlayerName();
}


float UMediaPlayer::GetRate() const
{
	return PlayerFacade->GetRate();
}


int32 UMediaPlayer::GetSelectedTrack(EMediaPlayerTrack TrackType) const
{
	return PlayerFacade->GetSelectedTrack((EMediaTrackType)TrackType);
}


void UMediaPlayer::GetSupportedRates(TArray<FFloatRange>& OutRates, bool Unthinned) const
{
	const TRangeSet<float> Rates = PlayerFacade->GetSupportedRates(Unthinned);
	Rates.GetRanges((TArray<TRange<float>>&)OutRates);
}


FTimespan UMediaPlayer::GetTime() const
{
	return PlayerFacade->GetTime();
}


FText UMediaPlayer::GetTrackDisplayName(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return PlayerFacade->GetTrackDisplayName((EMediaTrackType)TrackType, TrackIndex);
}


int32 UMediaPlayer::GetTrackFormat(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return PlayerFacade->GetTrackFormat((EMediaTrackType)TrackType, TrackIndex);
}


FString UMediaPlayer::GetTrackLanguage(EMediaPlayerTrack TrackType, int32 TrackIndex) const
{
	return PlayerFacade->GetTrackLanguage((EMediaTrackType)TrackType, TrackIndex);
}


const FString& UMediaPlayer::GetUrl() const
{
	return PlayerFacade->GetUrl();
}


float UMediaPlayer::GetVerticalFieldOfView() const
{
	float OutHorizontal = 0.0f;
	float OutVertical = 0.0f;

	if (!PlayerFacade->GetViewField(OutHorizontal, OutVertical))
	{
		return 0.0f;
	}

	return OutVertical;
}


float UMediaPlayer::GetVideoTrackAspectRatio(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetVideoTrackAspectRatio(TrackIndex, FormatIndex);
}


FIntPoint UMediaPlayer::GetVideoTrackDimensions(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetVideoTrackDimensions(TrackIndex, FormatIndex);
}


float UMediaPlayer::GetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetVideoTrackFrameRate(TrackIndex, FormatIndex);
}


FFloatRange UMediaPlayer::GetVideoTrackFrameRates(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetVideoTrackFrameRates(TrackIndex, FormatIndex);
}


FString UMediaPlayer::GetVideoTrackType(int32 TrackIndex, int32 FormatIndex) const
{
	return PlayerFacade->GetVideoTrackType(TrackIndex, FormatIndex);
}


FRotator UMediaPlayer::GetViewRotation() const
{
	FQuat OutOrientation;

	if (!PlayerFacade->GetViewOrientation(OutOrientation))
	{
		return FRotator::ZeroRotator;
	}

	return OutOrientation.Rotator();
}


bool UMediaPlayer::HasError() const
{
	return PlayerFacade->HasError();
}


bool UMediaPlayer::IsBuffering() const
{
	return PlayerFacade->IsBuffering();
}


bool UMediaPlayer::IsConnecting() const
{
	return PlayerFacade->IsConnecting();
}


bool UMediaPlayer::IsLooping() const
{
	return PlayerFacade->IsLooping();
}


bool UMediaPlayer::IsPaused() const
{
	return PlayerFacade->IsPaused();
}


bool UMediaPlayer::IsPlaying() const
{
	return PlayerFacade->IsPlaying();
}


bool UMediaPlayer::IsPreparing() const
{
	return PlayerFacade->IsPreparing();
}


bool UMediaPlayer::IsReady() const
{
	UE_LOG(LogMediaAssets, VeryVerbose, TEXT("%s.IsReady"), *GetFName().ToString());
	return PlayerFacade->IsReady();
}


bool UMediaPlayer::Next()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Next"), *GetFName().ToString());

	check(Playlist != nullptr);
	int32 RemainingAttempts = Playlist->Num();

	if (RemainingAttempts == 0)
	{
		return false;
	}

	PlayOnNext |= PlayerFacade->IsPlaying();

	while (RemainingAttempts-- > 0)
	{
		UMediaSource* NextSource = Shuffle
			? Playlist->GetRandom(PlaylistIndex)
			: Playlist->GetNext(PlaylistIndex);

		if ((NextSource != nullptr) && NextSource->Validate() && PlayerFacade->Open(NextSource->GetUrl(), NextSource))
		{
			return true;
		}
	}

	return false;
}


bool UMediaPlayer::OpenFile(const FString& FilePath)
{
	Close();

	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.OpenFile %s"), *GetFName().ToString(), *FilePath);

	check(Playlist != nullptr);

	if (!Playlist->AddFile(FilePath))
	{
		return false;
	}

	return Next();
}


bool UMediaPlayer::OpenPlaylistIndex(UMediaPlaylist* InPlaylist, int32 Index)
{
	Close();

	if (InPlaylist == nullptr)
	{
		UE_LOG(LogMediaAssets, Warning, TEXT("%s.OpenPlaylistIndex called with null MediaPlaylist"), *GetFName().ToString());
		return false;
	}

	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.OpenSource %s %i"), *GetFName().ToString(), *InPlaylist->GetFName().ToString(), Index);

	Playlist = InPlaylist;

	if (Index == INDEX_NONE)
	{
		return true;
	}

	UMediaSource* MediaSource = Playlist->Get(Index);

	if (MediaSource == nullptr)
	{
		UE_LOG(LogMediaAssets, Warning, TEXT("%s.OpenPlaylistIndex called with invalid PlaylistIndex %i"), *GetFName().ToString(), Index);
		return false;
	}

	PlaylistIndex = Index;

	if (!MediaSource->Validate())
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s (%s)"), *MediaSource->GetName(), *MediaSource->GetUrl());
		return false;
	}

	return PlayerFacade->Open(MediaSource->GetUrl(), MediaSource);
}


bool UMediaPlayer::OpenSource(UMediaSource* MediaSource)
{
	Close();

	if (MediaSource == nullptr)
	{
		UE_LOG(LogMediaAssets, Warning, TEXT("%s.OpenSource called with null MediaSource"), *GetFName().ToString());
		return false;
	}

	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.OpenSource %s"), *GetFName().ToString(), *MediaSource->GetFName().ToString());

	if (!MediaSource->Validate())
	{
		UE_LOG(LogMediaAssets, Error, TEXT("Failed to validate media source %s (%s)"), *MediaSource->GetName(), *MediaSource->GetUrl());
		return false;
	}

	check(Playlist != nullptr);
	Playlist->Add(MediaSource);

	return Next();
}


bool UMediaPlayer::OpenUrl(const FString& Url)
{
	Close();

	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.OpenUrl %s"), *GetFName().ToString(), *Url);

	check(Playlist != nullptr);

	if (!Playlist->AddUrl(Url))
	{
		return false;
	}

	return Next();
}


bool UMediaPlayer::Pause()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Pause"), *GetFName().ToString());
	return PlayerFacade->SetRate(0.0f);
}


bool UMediaPlayer::Play()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Play"), *GetFName().ToString());
	return PlayerFacade->SetRate(1.0f);
}


bool UMediaPlayer::Previous()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Previous"), *GetFName().ToString());

	check(Playlist != nullptr);
	int32 RemainingAttempts = Playlist->Num();

	if (RemainingAttempts == 0)
	{
		return false;
	}

	PlayOnNext |= PlayerFacade->IsPlaying();

	while (--RemainingAttempts >= 0)
	{
		UMediaSource* PrevSource = Shuffle
			? Playlist->GetRandom(PlaylistIndex)
			: Playlist->GetPrevious(PlaylistIndex);

		if ((PrevSource != nullptr) && PrevSource->Validate() && PlayerFacade->Open(PrevSource->GetUrl(), PrevSource))
		{
			return true;
		}
	}

	return false;
}


bool UMediaPlayer::Reopen()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Reopen"), *GetFName().ToString());
	return OpenPlaylistIndex(Playlist, PlaylistIndex);
}


bool UMediaPlayer::Rewind()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.Rewind"), *GetFName().ToString());
	return Seek(FTimespan::Zero());
}


bool UMediaPlayer::Seek(const FTimespan& Time)
{
	UE_LOG(LogMediaAssets, VeryVerbose, TEXT("%s.Seek %s"), *GetFName().ToString(), *Time.ToString());
	return PlayerFacade->Seek(Time);
}


bool UMediaPlayer::SelectTrack(EMediaPlayerTrack TrackType, int32 TrackIndex)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SelectTrack %s %i"), *GetFName().ToString(), *UEnum::GetValueAsString(TEXT("MediaAssets.EMediaPlayerTrack"), TrackType), TrackIndex);
	return PlayerFacade->SelectTrack((EMediaTrackType)TrackType, TrackIndex);
}


void UMediaPlayer::SetDesiredPlayerName(FName PlayerName)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetDesiredPlayerName %s"), *GetFName().ToString(), *PlayerName.ToString());
	PlayerFacade->DesiredPlayerName = PlayerName;
}


bool UMediaPlayer::SetLooping(bool Looping)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetLooping %s"), *GetFName().ToString(), *(Looping ? GTrue : GFalse).ToString());

	Loop = Looping;

	return PlayerFacade->SetLooping(Looping);
}


bool UMediaPlayer::SetRate(float Rate)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetRate %f"), *GetFName().ToString(), Rate);
	return PlayerFacade->SetRate(Rate);
}


bool UMediaPlayer::SetTrackFormat(EMediaPlayerTrack TrackType, int32 TrackIndex, int32 FormatIndex)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetTrackFormat %s %i %i"), *GetFName().ToString(), *UEnum::GetValueAsString(TEXT("MediaAssets.EMediaPlayerTrack"), TrackType), TrackIndex, FormatIndex);
	return PlayerFacade->SetTrackFormat((EMediaTrackType)TrackType, TrackIndex, FormatIndex);
}


bool UMediaPlayer::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetVideoTrackFrameRate %i %i %f"), *GetFName().ToString(), TrackIndex, FormatIndex, FrameRate);
	return PlayerFacade->SetVideoTrackFrameRate(TrackIndex, FormatIndex, FrameRate);
}

bool UMediaPlayer::SetViewField(float Horizontal, float Vertical, bool Absolute)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetViewField %f %f %s"), *GetFName().ToString(), Horizontal, Vertical, *(Absolute ? GTrue : GFalse).ToString());
	return PlayerFacade->SetViewField(Horizontal, Vertical, Absolute);
}


bool UMediaPlayer::SetViewRotation(const FRotator& Rotation, bool Absolute)
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.SetViewRotation %s %s"), *GetFName().ToString(), *Rotation.ToString(), *(Absolute ? GTrue : GFalse).ToString());
	return PlayerFacade->SetViewOrientation(FQuat(Rotation), Absolute);
}


bool UMediaPlayer::SupportsRate(float Rate, bool Unthinned) const
{
	return PlayerFacade->SupportsRate(Rate, Unthinned);
}


bool UMediaPlayer::SupportsScrubbing() const
{
	return PlayerFacade->CanScrub();
}


bool UMediaPlayer::SupportsSeeking() const
{
	return PlayerFacade->CanSeek();
}


#if WITH_EDITOR

void UMediaPlayer::PausePIE()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.PausePIE"), *GetFName().ToString());

	WasPlayingInPIE = IsPlaying();

	if (WasPlayingInPIE)
	{
		Pause();
	}
}


void UMediaPlayer::ResumePIE()
{
	UE_LOG(LogMediaAssets, Verbose, TEXT("%s.ResumePIE"), *GetFName().ToString());

	if (WasPlayingInPIE)
	{
		Play();
	}
}

#endif


/* UObject overrides
 *****************************************************************************/

void UMediaPlayer::BeginDestroy()
{
	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->GetClock().RemoveSink(PlayerFacade.ToSharedRef());
		MediaModule->GetTicker().RemoveTickable(PlayerFacade.ToSharedRef());
	}

	PlayerFacade->Close();

	Super::BeginDestroy();
}


bool UMediaPlayer::CanBeInCluster() const
{
	return false;
}


FString UMediaPlayer::GetDesc()
{
	return TEXT("UMediaPlayer");
}


void UMediaPlayer::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	PlayerGuid = FGuid::NewGuid();
	PlayerFacade->SetGuid(PlayerGuid);
}


void UMediaPlayer::PostInitProperties()
{
	Super::PostInitProperties();

	// Set the player GUID - required for UMediaPlayers dynamically allocated at runtime
	PlayerFacade->SetGuid(PlayerGuid);

	IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->GetClock().AddSink(PlayerFacade.ToSharedRef());
		MediaModule->GetTicker().AddTickable(PlayerFacade.ToSharedRef());
	}
}

void UMediaPlayer::PostLoad()
{
	Super::PostLoad();

	// Set the player GUID - required for UMediaPlayer assets
	PlayerFacade->SetGuid(PlayerGuid);
}


#if WITH_EDITOR

void UMediaPlayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr)
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMediaPlayer, Loop))
	{
		SetLooping(Loop);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif


/* UMediaPlayer callbacks
 *****************************************************************************/

void UMediaPlayer::HandlePlayerMediaEvent(EMediaEvent Event)
{
	MediaEvent.Broadcast(Event);

	switch(Event)
	{
	case EMediaEvent::MediaClosed:
		OnMediaClosed.Broadcast();
		break;

	case EMediaEvent::MediaOpened:
		PlayerFacade->SetCacheWindow(CacheAhead, FApp::IsGame() ? CacheBehindGame : CacheBehind);
		PlayerFacade->SetLooping(Loop && (Playlist->Num() == 1));
		PlayerFacade->SetViewField(HorizontalFieldOfView, VerticalFieldOfView, true);
		PlayerFacade->SetViewOrientation(FQuat(ViewRotation), true);

		OnMediaOpened.Broadcast(PlayerFacade->GetUrl());

		if (PlayOnOpen || PlayOnNext)
		{
			PlayOnNext = false;
			Play();
		}
		break;

	case EMediaEvent::MediaOpenFailed:
		OnMediaOpenFailed.Broadcast(PlayerFacade->GetUrl());

		if ((Loop && (Playlist->Num() != 1)) || (PlaylistIndex + 1 < Playlist->Num()))
		{
			Next();
		}
		break;

	case EMediaEvent::PlaybackEndReached:
		OnEndReached.Broadcast();

		check(Playlist != nullptr);

		if ((Loop && (Playlist->Num() != 1)) || (PlaylistIndex + 1 < Playlist->Num()))
		{
			PlayOnNext = true;
			Next();
		}
		break;

	case EMediaEvent::PlaybackResumed:
		OnPlaybackResumed.Broadcast();
		break;

	case EMediaEvent::PlaybackSuspended:
		OnPlaybackSuspended.Broadcast();
		break;

	case EMediaEvent::SeekCompleted:
		OnSeekCompleted.Broadcast();
		break;

	case EMediaEvent::TracksChanged:
		OnTracksChanged.Broadcast();
		break;
	}
}
