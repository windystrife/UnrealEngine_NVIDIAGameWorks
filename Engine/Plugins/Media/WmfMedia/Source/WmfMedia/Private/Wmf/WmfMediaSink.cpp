// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaSink.h"


#if WMFMEDIA_SUPPORTED_PLATFORM

#include "Misc/ScopeLock.h"

#include "WmfMediaStreamSink.h"
#include "WmfMediaUtils.h"


/* FWmfMediaStreamSink structors
 *****************************************************************************/

FWmfMediaSink::FWmfMediaSink()
	: RefCount(0)
{ }


FWmfMediaSink::~FWmfMediaSink()
{
	check(RefCount == 0);
}


/* FWmfMediaStreamSink interface
 *****************************************************************************/

bool FWmfMediaSink::Initialize(FWmfMediaStreamSink& InStreamSink)
{
	FScopeLock Lock(&CriticalSection);

	DWORD StreamId = 0;

	if (!InStreamSink.Initialize(*this))
	{
		return false;
	}

	StreamSink = &InStreamSink;

	return true;
}


/* IMFClockStateSink interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaSink::OnClockPause(MFTIME hnsSystemTime)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return StreamSink->Pause();
}


STDMETHODIMP FWmfMediaSink::OnClockRestart(MFTIME hnsSystemTime)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return StreamSink->Restart();
}


STDMETHODIMP FWmfMediaSink::OnClockSetRate(MFTIME hnsSystemTime, float flRate)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return StreamSink->Start();
}


STDMETHODIMP FWmfMediaSink::OnClockStop(MFTIME hnsSystemTime)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return StreamSink->Stop();
}


/* IMFGetService interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaSink::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
	if (guidService == MF_RATE_CONTROL_SERVICE)
	{
		return QueryInterface(riid, ppvObject);
	}
	
	if (guidService == MR_VIDEO_ACCELERATION_SERVICE)
	{
		// @todo gmp: implement WmfMedia video acceleration
	}

	return MF_E_UNSUPPORTED_SERVICE;
}


/* IMFMediaSink interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaSink::AddStreamSink(DWORD dwStreamSinkIdentifier, __RPC__in_opt IMFMediaType* pMediaType, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
{
	return MF_E_STREAMSINKS_FIXED;
}


STDMETHODIMP FWmfMediaSink::GetCharacteristics(__RPC__out DWORD* pdwCharacteristics)
{
	if (pdwCharacteristics == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pdwCharacteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_CAN_PREROLL;

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::GetPresentationClock(__RPC__deref_out_opt IMFPresentationClock** ppPresentationClock)
{
	if (ppPresentationClock == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!PresentationClock.IsValid())
	{
		return MF_E_NO_CLOCK; // presentation clock not set yet
	}

	*ppPresentationClock = PresentationClock;
	(*ppPresentationClock)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::GetStreamSinkById(DWORD dwIdentifier, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
{
	if (ppStreamSink == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	DWORD Identifier = 0;

	if (FAILED(StreamSink->GetIdentifier(&Identifier)) || (Identifier != dwIdentifier))
	{
		return MF_E_INVALIDSTREAMNUMBER;
	}

	*ppStreamSink = StreamSink;
	(*ppStreamSink)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::GetStreamSinkByIndex(DWORD dwIndex, __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
{
	if (ppStreamSink == NULL)
	{
		return E_POINTER;
	}

	if (dwIndex > 0)
	{
		return MF_E_INVALIDINDEX; // stream count is fixed at 1
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*ppStreamSink = StreamSink;
	(*ppStreamSink)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::GetStreamSinkCount(__RPC__out DWORD* pcStreamSinkCount)
{
	if (pcStreamSinkCount == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pcStreamSinkCount = 1; // stream count is fixed at 1

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::RemoveStreamSink(DWORD dwStreamSinkIdentifier)
{
	return MF_E_STREAMSINKS_FIXED;
}


STDMETHODIMP FWmfMediaSink::SetPresentationClock(__RPC__in_opt IMFPresentationClock* pPresentationClock)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	// remove ourselves from old clock
	if (PresentationClock.IsValid())
	{
		const HRESULT Result = PresentationClock->RemoveClockStateSink(this);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to remove media sink from presentation clock: %s"), *WmfMedia::ResultToString(Result));
			return Result;
		}
	}

	// Register ourselves to get state notifications from the new clock.
	if (pPresentationClock != NULL)
	{
		const HRESULT Result = pPresentationClock->AddClockStateSink(this);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Error, TEXT("Failed to add media sink to presentation clock: %s"), *WmfMedia::ResultToString(Result));
			return Result;
		}
	}

	PresentationClock = pPresentationClock;

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::Shutdown()
{
	FScopeLock Lock(&CriticalSection);

	if (StreamSink.IsValid())
	{
		StreamSink->Shutdown();
		StreamSink.Reset();
	}

	PresentationClock.Reset();

	return MF_E_SHUTDOWN;
}


/* IMFMediaSinkPreroll interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaSink::NotifyPreroll(MFTIME hnsUpcomingStartTime)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return StreamSink->Preroll();
}


/* IMFRateSupport interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaSink::GetFastestRate(MFRATE_DIRECTION eDirection, BOOL fThin, _Out_ float* pflRate)
{
	if (pflRate == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pflRate = (eDirection == MFRATE_FORWARD) ? FLT_MAX : FLT_MIN;

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::GetSlowestRate(MFRATE_DIRECTION eDirection, BOOL fThin, _Out_ float* pflRate)
{
	if (pflRate == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pflRate = 0.0;

	return S_OK;
}


STDMETHODIMP FWmfMediaSink::IsRateSupported(BOOL fThin, float flRate, __RPC__inout_opt float* pflNearestSupportedRate)
{
	FScopeLock Lock(&CriticalSection);

	if (!StreamSink.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return S_OK;
}


/* IUnknown interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaSink::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


#if _MSC_VER == 1900
	#pragma warning(push)
	#pragma warning(disable:4838)
#endif

STDMETHODIMP FWmfMediaSink::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaSink, IMFClockStateSink),
		QITABENT(FWmfMediaSink, IMFGetService),
		QITABENT(FWmfMediaSink, IMFMediaSink),
		QITABENT(FWmfMediaSink, IMFMediaSinkPreroll),
		QITABENT(FWmfMediaSink, IMFRateSupport),
		{ 0 }
	};

	return QISearch(this, QITab, RefID, Object);
}

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif


STDMETHODIMP_(ULONG) FWmfMediaSink::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);

	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


#endif
