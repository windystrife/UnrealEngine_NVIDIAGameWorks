// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaSession.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "IMediaEventSink.h"
#include "Misc/ScopeLock.h"

#include "WmfMediaUtils.h"

#include "AllowWindowsPlatformTypes.h"


#define WMFMEDIASESSION_USE_WINDOWS7FASTFORWARDENDHACK 1


/* Local helpers
 *****************************************************************************/

namespace WmfMediaSession
{
	/** Time span value for RequestedTime indicating a seek to the current time. */
	const FTimespan RequestedTimeCurrent = FTimespan::MaxValue();

	/** Get the human readable string representation of a media player state. */
	const TCHAR* StateToString(EMediaState State)
	{
		switch (State)
		{
		case EMediaState::Closed: return TEXT("Closed");
		case EMediaState::Error: return TEXT("Error");
		case EMediaState::Paused: return TEXT("Paused");
		case EMediaState::Playing: return TEXT("Playing");
		case EMediaState::Preparing: return TEXT("Preparing");
		case EMediaState::Stopped: return TEXT("Stopped");
		default: return TEXT("Unknown");
		}
	}
}


/* FWmfMediaSession structors
 *****************************************************************************/

FWmfMediaSession::FWmfMediaSession()
	: CanScrub(false)
	, Capabilities(0)
	, CurrentDuration(FTimespan::Zero())
	, LastTime(FTimespan::Zero())
	, PendingChanges(false)
	, RefCount(0)
	, SessionRate(0.0f)
	, SessionState(EMediaState::Closed)
	, ShouldLoop(false)
	, Status(EMediaStatus::None)
{
	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Created"), this);
}


FWmfMediaSession::~FWmfMediaSession()
{
	check(RefCount == 0);
	Shutdown();

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Destroyed"), this);
}


/* FWmfMediaSession interface
 *****************************************************************************/

void FWmfMediaSession::GetEvents(TArray<EMediaEvent>& OutEvents)
{
#if WMFMEDIASESSION_USE_WINDOWS7FASTFORWARDENDHACK
	if (CurrentDuration > FTimespan::Zero())
	{
		FTimespan Time = GetTime();

		if ((Time < FTimespan::Zero()) || (Time > CurrentDuration))
		{
			if (!ShouldLoop)
			{
				const HRESULT Result = MediaSession->Stop();

				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Forced media session to stop at end: %s"), this, *WmfMedia::ResultToString(Result));
			}

			HandleSessionEnded();
		}
	}
#endif

	EMediaEvent Event;

	while (DeferredEvents.Dequeue(Event))
	{
		OutEvents.Add(Event);
	}
}


bool FWmfMediaSession::Initialize(bool LowLatency)
{
	Shutdown();

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Initializing media session (LowLatency: %d)"), this, LowLatency);

	// create session attributes
	TComPtr<IMFAttributes> Attributes;
	{
		HRESULT Result = ::MFCreateAttributes(&Attributes, 2);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to create media session attributes: %s"), this, *WmfMedia::ResultToString(Result));
			return false;
		}
	}

	if (LowLatency)
	{
		if (FWindowsPlatformMisc::VerifyWindowsVersion(6, 2))
		{
			HRESULT Result = Attributes->SetUINT32(MF_LOW_LATENCY, TRUE);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to set low latency session attribute: %s"), this, *WmfMedia::ResultToString(Result));
			}
		}
		else
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Low latency media processing requires Windows 8 or newer"), this);
		}
	}

	FScopeLock Lock(&CriticalSection);

	// create media session
	HRESULT Result = ::MFCreateMediaSession(Attributes, &MediaSession);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to create media session: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	// start media event processing
	Result = MediaSession->BeginGetEvent(this, NULL);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to start media session event processing: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	SessionState = EMediaState::Preparing;

	return true;
}


bool FWmfMediaSession::SetTopology(const TComPtr<IMFTopology>& InTopology, FTimespan InDuration)
{
	if (MediaSession == NULL)
	{
		return false;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session: %p: Setting new partial topology (duration = %s)"), this, *InDuration.ToString());

	FScopeLock Lock(&CriticalSection);

	if (SessionState == EMediaState::Preparing)
	{
		// media source resolved
		if (InTopology != NULL)
		{
			// at least one track selected
			HRESULT Result = MediaSession->SetTopology(MFSESSION_SETTOPOLOGY_IMMEDIATE, InTopology);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to set topology: %s"), this, *WmfMedia::ResultToString(Result));
				
				SessionState = EMediaState::Error;
				DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
			}
			else
			{
				// do nothing (Preparing state will exit in MESessionTopologyStatus event)
			}
		}
		else
		{
			// no tracks selected
			UpdateCharacteristics();
			SessionState = EMediaState::Stopped;
			DeferredEvents.Enqueue(EMediaEvent::MediaOpened);
		}
	}
	else
	{
		// topology changed during playback, i.e. track switching
		if (PendingChanges)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting topology change after pending command"), this);
			RequestedTopology = InTopology;
		}
		else
		{
			CommitTopology(InTopology);
		}
	}

	CurrentDuration = InDuration;

	return true;
}


void FWmfMediaSession::Shutdown()
{
	if (MediaSession == NULL)
	{
		return;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session: %p: Shutting down media session"), this);

	FScopeLock Lock(&CriticalSection);

	DiscardPendingChanges();

	MediaSession->Close();
	MediaSession->Shutdown();
	MediaSession.Reset();

	CurrentTopology.Reset();
	PresentationClock.Reset();
	RateControl.Reset();
	RateSupport.Reset();

	CanScrub = false;
	Capabilities = 0;
	CurrentDuration = FTimespan::Zero();
	SessionRate = 0.0f;
	SessionState = EMediaState::Closed;
	LastTime = FTimespan::Zero();
	RequestedRate.Reset();
	Status = EMediaStatus::None;
	ThinnedRates.Empty();
	UnthinnedRates.Empty();
}


/* IMediaControls interface
 *****************************************************************************/

bool FWmfMediaSession::CanControl(EMediaControl Control) const
{
	if (MediaSession == NULL)
	{
		return false;
	}

	FScopeLock Lock(&CriticalSection);

	if (Control == EMediaControl::Pause)
	{
		return ((SessionState == EMediaState::Playing) && (((Capabilities & MFSESSIONCAP_PAUSE) != 0) || UnthinnedRates.Contains(0.0f)));
	}

	if (Control == EMediaControl::Resume)
	{
		return ((SessionState != EMediaState::Playing) && UnthinnedRates.Contains(1.0f));
	}

	if (Control == EMediaControl::Scrub)
	{
		return CanScrub;
	}

	if (Control == EMediaControl::Seek)
	{
		return (((Capabilities & MFSESSIONCAP_SEEK) != 0) && (CurrentDuration > FTimespan::Zero()));
	}

	return false;
}


FTimespan FWmfMediaSession::GetDuration() const
{
	return CurrentDuration;
}


float FWmfMediaSession::GetRate() const
{
	return (SessionState == EMediaState::Playing) ? SessionRate : 0.0f;
}


EMediaState FWmfMediaSession::GetState() const
{
	if ((SessionState == EMediaState::Playing) && (SessionRate == 0.0f))
	{
		return EMediaState::Paused;
	}

	return SessionState;
}


EMediaStatus FWmfMediaSession::GetStatus() const
{
	return Status;
}


TRangeSet<float> FWmfMediaSession::GetSupportedRates(EMediaRateThinning Thinning) const
{
	FScopeLock Lock(&CriticalSection);

	return (Thinning == EMediaRateThinning::Thinned) ? ThinnedRates : UnthinnedRates;
}


FTimespan FWmfMediaSession::GetTime() const
{
	FScopeLock Lock(&CriticalSection);

	MFCLOCK_STATE ClockState;

	if (!PresentationClock.IsValid() || FAILED(PresentationClock->GetState(0, &ClockState)) || (ClockState == MFCLOCK_STATE_INVALID))
	{
		return FTimespan::Zero(); // topology not initialized, or clock not started yet
	}

	if (ClockState == MFCLOCK_STATE_STOPPED)
	{
		return LastTime; // WMF always reports zero when stopped
	}

	MFTIME ClockTime;
	MFTIME SystemTime;

	if (FAILED(PresentationClock->GetCorrelatedTime(0, &ClockTime, &SystemTime)))
	{
		return FTimespan::Zero();
	}

	return FTimespan(ClockTime);
}


bool FWmfMediaSession::IsLooping() const
{
	return ShouldLoop;
}


bool FWmfMediaSession::Seek(const FTimespan& Time)
{
	if (MediaSession == NULL)
	{
		return false;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Seeking to %s"), this, *Time.ToString());

	// validate seek
	if (!CanControl(EMediaControl::Seek))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Media source doesn't support seeking"), this);
		return false;
	}

	FScopeLock Lock(&CriticalSection);

	if ((SessionState == EMediaState::Closed) || (SessionState == EMediaState::Error))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Cannot seek while closed or in error state"), this);
		return false;
	}

	if ((Time < FTimespan::Zero()) || (Time > CurrentDuration))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Invalid seek time %s (media duration is %s)"), this, *Time.ToString(), *CurrentDuration.ToString());
		return false;
	}

	// wait for pending changes to complete
	if (PendingChanges)
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting seek after pending command"), this);
		RequestedTime = Time;

		return true;
	}

	return CommitTime(Time);
}


bool FWmfMediaSession::SetLooping(bool Looping)
{
	ShouldLoop = Looping;

	return true;
}


bool FWmfMediaSession::SetRate(float Rate)
{
	if (MediaSession == NULL)
	{
		return false;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Setting rate to %f"), this, Rate);

	FScopeLock Lock(&CriticalSection);

#if 0
	if (Rate == SessionRate)
	{
		// Note: play rates and play states are handled separately in WMF. The session's initial play
		// rate after opening a media source is 1.0, even though the session is in the Stopped state.

		if ((Rate != 1.0f) || (SessionState == EMediaState::Playing))
		{
			return true; // rate already set
		}
	}
#endif

	// validate rate
	if (!ThinnedRates.Contains(Rate) && !UnthinnedRates.Contains(Rate))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: The rate %f is not supported"), this, Rate);
		return false;
	}

	// wait for pending changes to complete
	if (PendingChanges)
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting rate change after pending command"), this);
		RequestedRate = Rate;

		return true;
	}

	return CommitRate(Rate);
}


/* IMFAsyncCallback interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaSession::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


STDMETHODIMP FWmfMediaSession::GetParameters(unsigned long*, unsigned long*)
{
	return E_NOTIMPL; // default behavior
}


STDMETHODIMP FWmfMediaSession::Invoke(IMFAsyncResult* AsyncResult)
{
	FScopeLock Lock(&CriticalSection);

	if (MediaSession == NULL)
	{
		return S_OK;
	}

	// get event
	TComPtr<IMFMediaEvent> Event;
	{
		const HRESULT Result = MediaSession->EndGetEvent(AsyncResult, &Event);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get event: %s"), this, *WmfMedia::ResultToString(Result));
			return S_OK;
		}
	}

	MediaEventType EventType = MEUnknown;
	{
		const HRESULT Result = Event->GetType(&EventType);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get session event type: %s"), this, *WmfMedia::ResultToString(Result));
			return S_OK;
		}
	}

	HRESULT EventStatus = S_FALSE;
	{
		const HRESULT Result = Event->GetStatus(&EventStatus);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get event status: %s"), this, *WmfMedia::ResultToString(Result));
		}
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Event [%s]: %s"), this, *WmfMedia::MediaEventToString(EventType), *WmfMedia::ResultToString(EventStatus));

	// process event
	switch (EventType)
	{
	case MEBufferingStarted:
		Status |= EMediaStatus::Buffering;
		DeferredEvents.Enqueue(EMediaEvent::MediaBuffering);
		break;

	case MEBufferingStopped:
		Status &= ~EMediaStatus::Buffering;
		break;

	case MEError:
		HandleError(EventStatus);	
		break;

	case MEReconnectEnd:
		Status &= ~EMediaStatus::Connecting;
		break;

	case MEReconnectStart:
		Status |= EMediaStatus::Connecting;
		DeferredEvents.Enqueue(EMediaEvent::MediaConnecting);
		break;

	case MESessionCapabilitiesChanged:
		Capabilities = ::MFGetAttributeUINT32(Event, MF_EVENT_SESSIONCAPS, Capabilities);
		break;

	case MESessionClosed:
		Capabilities = 0;
		LastTime = FTimespan::Zero();
		break;

	case MESessionEnded:
		HandleSessionEnded();
		break;

	case MESessionPaused:
		HandleSessionPaused(EventStatus);
		break;

	case MESessionRateChanged:
		HandleSessionRateChanged(EventStatus, *Event);
		break;

	case MESessionScrubSampleComplete:
		HandleSessionScrubSampleComplete();
		break;

	case MESessionStarted:
		HandleSessionStarted(EventStatus);
		break;

	case MESessionStopped:
		HandleSessionStopped(EventStatus);
		break;

	case MESessionTopologySet:
		HandleSessionTopologySet(EventStatus, *Event);
		break;

	case MESessionTopologyStatus:
		HandleSessionTopologyStatus(EventStatus, *Event);
		break;

	default:
		break; // unsupported event
	}

	// request next event
	if ((EventType != MESessionClosed) && (SessionState != EMediaState::Error) && (MediaSession != NULL))
	{
		const HRESULT Result = MediaSession->BeginGetEvent(this, NULL);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to request next session event; aborting playback: %s"), this, *WmfMedia::ResultToString(Result));

			Capabilities = 0;
			SessionState = EMediaState::Error;
		}
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("Session %p: CurrentState: %s, CurrentRate: %f, CurrentTime: %s, SessionState: %s, SessionRate: %f, PendingChanges: %s"),
		this,
		WmfMediaSession::StateToString(GetState()),
		GetRate(),
		*GetTime().ToString(),
		WmfMediaSession::StateToString(SessionState),
		SessionRate,
		PendingChanges ? TEXT("true") : TEXT("false")
	);
	
	return S_OK;
}


#if _MSC_VER == 1900
	#pragma warning(push)
	#pragma warning(disable:4838)
#endif // _MSC_VER == 1900

STDMETHODIMP FWmfMediaSession::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaSession, IMFAsyncCallback),
		{ 0 }
	};

	return QISearch(this, QITab, RefID, Object);
}

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif // _MSC_VER == 1900


STDMETHODIMP_(ULONG) FWmfMediaSession::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);
	
	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


/* FWmfMediaSession implementation
 *****************************************************************************/

bool FWmfMediaSession::CommitRate(float Rate)
{
	check(MediaSession != NULL);
	check(!PendingChanges);

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Committing rate %f"), this, Rate);

	// Note: if rate control is not available, the session only supports pause and play

	if (RateControl == NULL)
	{
		if (Rate == 0.0f)
		{
			if (SessionState == EMediaState::Playing)
			{
				const HRESULT Result = MediaSession->Pause();

				if (FAILED(Result))
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to pause session: %s"), this, *WmfMedia::ResultToString(Result));
					return false;
				}
			}
		}
		else
		{
			if (SessionState != EMediaState::Playing)
			{
				PROPVARIANT StartPosition;
				PropVariantInit(&StartPosition);

				const HRESULT Result = MediaSession->Start(NULL, &StartPosition);

				if (FAILED(Result))
				{
					UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to start session: %s"), this, *WmfMedia::ResultToString(Result));
					return false;
				}
			}
		}

		PendingChanges = true;

		return true;
	}

	// Note: if rate control is available, things get considerably more complicated
	// as many rate transitions are only allowed from certain session states

	if (((Rate >= 0.0f) && (SessionRate < 0.0f)) || ((Rate < 0.0f) && (SessionRate >= 0.0f)))
	{
		// transitions between negative and zero/positive rates require Stopped state
		if (SessionState != EMediaState::Stopped)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Stopping session for rate change"), this);

			LastTime = GetTime();

			// stop the media session
			const HRESULT Result = MediaSession->Stop();

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to stop for rate change: %s"), this, *WmfMedia::ResultToString(Result));
				return false;
			}

			// defer rate change
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Deferring rate change until after pending stop"), this);

			if (!RequestedTime.IsSet())
			{			
				RequestedTime = LastTime;
			}

			RequestedRate = Rate;
			PendingChanges = true;

			return true;
		}
	}
	
	if (((Rate == 0.0f) && (SessionRate != 0.0f)) || ((Rate != 0.0f) && (SessionRate == 0.0f)))
	{
		// transitions between positive and zero rates require Paused or Stopped state
		if ((SessionState != EMediaState::Paused) && (SessionState != EMediaState::Stopped))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Pausing session for rate change from %f to %f"), this, SessionRate, Rate);

			// pause the media session
			const HRESULT Result = MediaSession->Pause();

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to pause for rate change: %s"), this, *WmfMedia::ResultToString(Result));
				return false;
			}

			// defer rate change
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Deferring rate change until after pending pause"), this);

			RequestedRate = Rate;
			PendingChanges = true;

			return true;
		}
	}

	// Note: The rate control could be updated right after requesting the Pause or Stopped
	// states above, but we wait for these transitions to complete, so that multiple calls
	// to SetRate do not interfere with each other.

	if (Rate != SessionRate)
	{
		// determine thinning mode
		EMediaRateThinning Thinning;

		if (UnthinnedRates.Contains(Rate))
		{
			Thinning = EMediaRateThinning::Unthinned;
		}
		else if (ThinnedRates.Contains(Rate))
		{
			Thinning = EMediaRateThinning::Thinned;
		}
		else
		{
			return false;
		}

		const TCHAR* ThinnedString = (Thinning == EMediaRateThinning::Thinned) ? TEXT("thinned") : TEXT("unthinned");
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Changing rate from %f to %f [%s]"), this, SessionRate, Rate, ThinnedString);

		// set the new rate
		const HRESULT Result = RateControl->SetRate((Thinning == EMediaRateThinning::Thinned) ? TRUE : FALSE, Rate);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to change rate: %s"), this, *WmfMedia::ResultToString(Result));
			return false;
		}

		if (PendingChanges)
		{
			return true; // wait for required state transitions to complete
		}

		PendingChanges = true;
	}

	// Note: no further changes needed if session was playing and direction didn't change

	if (((Rate * SessionRate) > 0.0f) && (SessionState == EMediaState::Playing))
	{
		return true;
	}

	// Note: for non-zero rates, the session must be restarted. If the rate control wasn't
	// updated above, this can be done immediately, otherwise it has to be deferred until
	// after the rate control finished setting the new rate.

	if ((Rate != 0.0f) && ((SessionState != EMediaState::Playing) || PendingChanges))
	{
		// determine restart time
		FTimespan RestartTime;

		if (RequestedTime.IsSet())
		{
			RestartTime = RequestedTime.GetValue();
		}
		else
		{
			RestartTime = GetTime();
		}

		if ((RestartTime == FTimespan::Zero()) && (Rate < 0.0f))
		{
			RestartTime = CurrentDuration; // loop to end
		}
		else if ((RestartTime == CurrentDuration) && (Rate > 0.0f))
		{
			RestartTime = FTimespan::Zero(); // loop to beginning
		}

		// restart session
		if (PendingChanges)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting start after pending rate change"), this);
			RequestedTime = RestartTime;
		}
		else
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Starting session for rate change"), this);
			CommitTime(RestartTime);
		}
	}

	return true;
}


bool FWmfMediaSession::CommitTime(FTimespan Time)
{
	check(MediaSession != NULL);
	check(PendingChanges == false);

	const FString TimeString = (Time == WmfMediaSession::RequestedTimeCurrent) ? TEXT("<current>") : *Time.ToString();
	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Committing time %s"), this, *TimeString);

	// start session at requested time
	PROPVARIANT StartPosition;

	if (!CanControl(EMediaControl::Seek))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Starting from <current>, because media can't seek"), this);
		Time = WmfMediaSession::RequestedTimeCurrent;
	}

	if (Time == WmfMediaSession::RequestedTimeCurrent)
	{
		StartPosition.vt = VT_EMPTY; // current time
	}
	else
	{
		StartPosition.vt = VT_I8;
		StartPosition.hVal.QuadPart = Time.GetTicks();
	}

	HRESULT Result = MediaSession->Start(NULL, &StartPosition);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to start session: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	PendingChanges = true;

	return true;
}


bool FWmfMediaSession::CommitTopology(IMFTopology* Topology)
{
	check(MediaSession != NULL);
	check(PendingChanges == false);

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Committing topology %p"), this, Topology);

	if (SessionState != EMediaState::Stopped)
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Stopping session for topology change"), this);

		LastTime = GetTime();

		// topology change requires transition to Stopped; playback is resumed afterwards
		HRESULT Result = MediaSession->Stop();

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to stop for topology change: %s"), *WmfMedia::ResultToString(Result));
			return false;
		}

		// request deferred playback restart
		if (!RequestedTime.IsSet())
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting restart after pending stop"), this);

			RequestedTime = LastTime;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting topology change after pending stop"), this);

		RequestedTopology = Topology;
		PendingChanges = true;

		return true;
	}

	// set new topology
	HRESULT Result = MediaSession->ClearTopologies();

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to clear queued topologies: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	if (Topology != NULL)
	{
		Result = MediaSession->SetTopology(MFSESSION_SETTOPOLOGY_IMMEDIATE, Topology);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to set topology: %s"), this, *WmfMedia::ResultToString(Result));
			return false;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Committed topology %p"), this, Topology);

		PendingChanges = true;
	}

	return true;
}


void FWmfMediaSession::DiscardPendingChanges()
{
	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Discarding pending changes"), this);

	RequestedRate.Reset();
	RequestedTime.Reset();
	RequestedTopology.Reset();

	PendingChanges = false;
}


void FWmfMediaSession::DoPendingChanges()
{
	if (PendingChanges)
	{
		const FString RequestedRateString = !RequestedRate.IsSet() ? TEXT("<none>") : FString::Printf(TEXT("%f"), RequestedRate.GetValue());
		const FString RequestedTimeString = !RequestedTime.IsSet() ? TEXT("<none>") : (RequestedTime.GetValue() == WmfMediaSession::RequestedTimeCurrent) ? TEXT("<current>") : *RequestedTime.GetValue().ToString();
		const FString RequestedTopologyString = !RequestedTopology ? TEXT("<none>") : FString::Printf(TEXT("%p"), RequestedTopology.Get());

		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Doing pending changes: RequestedRate: %s, RequestedTime: %s, RequestedTopology: %s"), this, *RequestedRateString, *RequestedTimeString, *RequestedTopologyString);
	}
	else
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Doing pending changes: none"), this);
	}

	PendingChanges = false;

	// commit pending topology changes
	if (RequestedTopology != NULL)
	{
		TComPtr<IMFTopology> Topology = RequestedTopology;
		RequestedTopology.Reset();

		CommitTopology(Topology);

		if (PendingChanges)
		{
			return;
		}
	}

	// commit pending rate changes
	if (RequestedRate.IsSet())
	{
		const float Rate = RequestedRate.GetValue();
		RequestedRate.Reset();

		CommitRate(Rate);

		if (PendingChanges)
		{
			return;
		}
	}

	// commit pending seeks/restarts
	if (RequestedTime.IsSet())
	{
		const FTimespan Time = RequestedTime.GetValue();
		RequestedTime.Reset();

		CommitTime(Time);
	}
}


void FWmfMediaSession::UpdateCharacteristics()
{
	// reset characteristics
	PresentationClock.Reset();
	RateControl.Reset();
	RateSupport.Reset();

	ThinnedRates.Empty();
	UnthinnedRates.Empty();

	CanScrub = false;

	if (MediaSession == NULL)
	{
		return;
	}

	// get presentation clock, if available
	TComPtr<IMFClock> Clock;

	HRESULT Result = MediaSession->GetClock(&Clock);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Session clock unavailable: %s"), this, *WmfMedia::ResultToString(Result));
	}
	else
	{
		Result = Clock->QueryInterface(IID_PPV_ARGS(&PresentationClock));

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Presentation clock unavailable: %s"), this, *WmfMedia::ResultToString(Result));
		}
		else
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Presentation clock ready"), this);
		}
	}

	// get rate control & rate support, if available
	Result = ::MFGetService(MediaSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&RateControl));

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Rate control service unavailable: %s"), this, *WmfMedia::ResultToString(Result));
	}
	else
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Rate control ready"), this);

		if (FAILED(RateControl->GetRate(FALSE, &SessionRate)))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to initialize current rate"), this);
			SessionRate = 1.0f; // the session's initial play rate is usually 1.0
		}
	}

	Result = ::MFGetService(MediaSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&RateSupport));

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Rate support service unavailable: %s"), this, *WmfMedia::ResultToString(Result));
	}
	else
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Rate support ready"), this);
	}

	// cache rate control properties
	if (RateSupport.IsValid())
	{
		CanScrub = SUCCEEDED(RateSupport->IsRateSupported(TRUE, 0.0f, NULL));

		float MaxRate = 0.0f;
		float MinRate = 0.0f;

		if (SUCCEEDED(RateSupport->GetSlowestRate(MFRATE_FORWARD, TRUE, &MinRate)) &&
			SUCCEEDED(RateSupport->GetFastestRate(MFRATE_FORWARD, TRUE, &MaxRate)))
		{
			ThinnedRates.Add(TRange<float>::Inclusive(MinRate, MaxRate));
		}

		if (SUCCEEDED(RateSupport->GetSlowestRate(MFRATE_REVERSE, TRUE, &MinRate)) &&
			SUCCEEDED(RateSupport->GetFastestRate(MFRATE_REVERSE, TRUE, &MaxRate)))
		{
			ThinnedRates.Add(TRange<float>::Inclusive(MaxRate, MinRate));
		}

		if (SUCCEEDED(RateSupport->GetSlowestRate(MFRATE_FORWARD, FALSE, &MinRate)) &&
			SUCCEEDED(RateSupport->GetFastestRate(MFRATE_FORWARD, FALSE, &MaxRate)))
		{
			UnthinnedRates.Add(TRange<float>::Inclusive(MinRate, MaxRate));
		}

		if (SUCCEEDED(RateSupport->GetSlowestRate(MFRATE_REVERSE, FALSE, &MinRate)) &&
			SUCCEEDED(RateSupport->GetFastestRate(MFRATE_REVERSE, FALSE, &MaxRate)))
		{
			UnthinnedRates.Add(TRange<float>::Inclusive(MaxRate, MinRate));
		}
	}
}


/* FWmfMediaSession callbacks
*****************************************************************************/

void FWmfMediaSession::HandleError(HRESULT EventStatus)
{
	UE_LOG(LogWmfMedia, Error, TEXT("An error occurred in the media session: %s"), *WmfMedia::ResultToString(EventStatus));

	SessionState = EMediaState::Error;
	DiscardPendingChanges();
	MediaSession->Close();
}


void FWmfMediaSession::HandleSessionEnded()
{
	DeferredEvents.Enqueue(EMediaEvent::PlaybackEndReached);

	SessionState = EMediaState::Stopped;

	if (ShouldLoop)
	{
		// loop back to beginning/end
		RequestedTime = (SessionRate < 0.0f) ? CurrentDuration : FTimespan::Zero();
		DoPendingChanges();
	}
	else
	{
		LastTime = FTimespan::Zero();
		RequestedRate.Reset();
	}
}


void FWmfMediaSession::HandleSessionPaused(HRESULT EventStatus)
{
	if (SUCCEEDED(EventStatus))
	{
		SessionState = EMediaState::Paused;
		DeferredEvents.Enqueue(EMediaEvent::PlaybackSuspended);

		DoPendingChanges();
	}
	else
	{
		DiscardPendingChanges();
	}
}


void FWmfMediaSession::HandleSessionRateChanged(HRESULT EventStatus, IMFMediaEvent& Event)
{
	if (SUCCEEDED(EventStatus))
	{
		// cache current rate
		PROPVARIANT Value;
		::PropVariantInit(&Value);

		const HRESULT Result = Event.GetValue(&Value);

		if (SUCCEEDED(Result) && (Value.vt == VT_R4))
		{
			SessionRate = Value.fltVal;
		}
	}
	else if (RateControl != NULL)
	{
		BOOL Thin = FALSE;
		RateControl->GetRate(&Thin, &SessionRate);
	}

	DoPendingChanges();
}


void FWmfMediaSession::HandleSessionScrubSampleComplete()
{
//	DeferredEvents.Enqueue(EMediaEvent::SeekCompleted);
//	DoPendingChanges();
}


void FWmfMediaSession::HandleSessionStarted(HRESULT EventStatus)
{
	if (SUCCEEDED(EventStatus))
	{
		if ((SessionState == EMediaState::Paused) && (SessionRate == 0.0f))
		{
			SessionState = EMediaState::Playing;

			// Note: pending changes will be processed in MESessionScrubSampleComplete

			DeferredEvents.Enqueue(EMediaEvent::SeekCompleted);
			DoPendingChanges();
		}
		else
		{
			if (RateControl == NULL)
			{
				SessionRate = 1.0f;
			}
			else
			{
				BOOL Thin = FALSE;
				RateControl->GetRate(&Thin, &SessionRate);
			}

			if (SessionState == EMediaState::Playing)
			{
				DeferredEvents.Enqueue(EMediaEvent::SeekCompleted);
			}
			else
			{
				SessionState = EMediaState::Playing;

				if (SessionRate == 0.0f)
				{
					RequestedRate = 0.0f;
					PendingChanges = true;
				}
				else
				{
					DeferredEvents.Enqueue(EMediaEvent::PlaybackResumed);
				}
			}
			
			DoPendingChanges();
		}
	}
	else
	{
		DiscardPendingChanges();
	}
}


void FWmfMediaSession::HandleSessionStopped(HRESULT EventStatus)
{
	if (SUCCEEDED(EventStatus))
	{
		SessionState = EMediaState::Stopped;
		DeferredEvents.Enqueue(EMediaEvent::PlaybackSuspended);
		DoPendingChanges();
	}
	else
	{
		DiscardPendingChanges();
	}
}


void FWmfMediaSession::HandleSessionTopologySet(HRESULT EventStatus, IMFMediaEvent& Event)
{
	if (SUCCEEDED(EventStatus))
	{
		const HRESULT Result = WmfMedia::GetTopologyFromEvent(Event, CurrentTopology);

		if (SUCCEEDED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Topology %p set as current"), this, CurrentTopology.Get());

			if (SessionState != EMediaState::Preparing)
			{
				// Note: track and format changes won't send an MF_TOPOSTATUS_READY event
				// until playback is restarted, so we do pending changes here instead.

				DoPendingChanges();
			}

			return;
		}

		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get topology that was set: %s"), this, *WmfMedia::ResultToString(Result));
	}

	if (SessionState == EMediaState::Preparing)
	{
		SessionState = EMediaState::Error;
		DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
	}

	DiscardPendingChanges();
}


void FWmfMediaSession::HandleSessionTopologyStatus(HRESULT EventStatus, IMFMediaEvent& Event)
{
	// get the status of the topology that generated the event
	MF_TOPOSTATUS TopologyStatus = MF_TOPOSTATUS_INVALID;
	{
		const HRESULT Result = Event.GetUINT32(MF_EVENT_TOPOLOGY_STATUS, (UINT32*)&TopologyStatus);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get topology status: %s"), this, *WmfMedia::ResultToString(Result));
			return;
		}
	}

	// get the topology that generated the event
	TComPtr<IMFTopology> Topology;
	{
		const HRESULT Result = WmfMedia::GetTopologyFromEvent(Event, Topology);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get topology from topology status event: %s"), this, *WmfMedia::ResultToString(Result));
			return;
		}
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Topology %p changed status to %s"), this, Topology.Get(), *WmfMedia::TopologyStatusToString(TopologyStatus));

	// Note: the ordering of topology status events is not guaranteed for two
	// consecutive topologies. we skip events that are not the for current one.

	if (Topology != CurrentTopology)
	{
		return;
	}

	if (SessionState == EMediaState::Error)
	{
		DiscardPendingChanges();

		return;
	}

	if (FAILED(EventStatus))
	{
		if (SessionState == EMediaState::Preparing)
		{
			UE_LOG(LogWmfMedia, Error, TEXT("An error occured when preparing the topology"), this);

			// an error occurred in the topology
			SessionState = EMediaState::Error;
			DeferredEvents.Enqueue(EMediaEvent::MediaOpenFailed);
		}

		DiscardPendingChanges();

		return;
	}

	if (TopologyStatus != MF_TOPOSTATUS_READY)
	{
		return;
	}

#if 0
	TComPtr<IMFTopology> FullTopology;
	{
		const HRESULT Result = MediaSession->GetFullTopology(MFSESSION_GETFULLTOPOLOGY_CURRENT, 0, &FullTopology);

		if (SUCCEEDED(Result))
		{
			if (FullTopology != CurrentTopology)
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Full topology %p from media session doesn't match current topology %p"), this, FullTopology.Get(), CurrentTopology.Get());
			}
		}
		else
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Failed to get full topology from media session"), this, *WmfMedia::ResultToString(Result));
		}
	}
#endif

	// initialize new topology
	UpdateCharacteristics();

	// new media opened successfully
	if (SessionState == EMediaState::Preparing)
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Topology %p ready"), this, CurrentTopology.Get());

		SessionState = EMediaState::Stopped;
		DeferredEvents.Enqueue(EMediaEvent::MediaOpened);
	}
	else if (SessionState == EMediaState::Paused)
	{
		// Note: when paused, the new topology won't apply until the next session start,
		// so we request a scrub to the current time in order to update the video frame.

		if (!RequestedTime.IsSet())
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Session %p: Requesting scrub after topology change"), this);

			RequestedTime = WmfMediaSession::RequestedTimeCurrent;
			PendingChanges = true;
		}
	}

	DoPendingChanges();
}


#include "HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM
