// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaStreamSink.h"


#if WMFMEDIA_SUPPORTED_PLATFORM

#include "Misc/ScopeLock.h"

#include "WmfMediaSink.h"
#include "WmfMediaUtils.h"


/* FWmfMediaStreamSink static functions
 *****************************************************************************/

bool FWmfMediaStreamSink::Create(const GUID& MajorType, TComPtr<FWmfMediaStreamSink>& OutSink)
{
	TComPtr<FWmfMediaStreamSink> StreamSink = new FWmfMediaStreamSink(MajorType, 1);
	TComPtr<FWmfMediaSink> MediaSink = new FWmfMediaSink();

	if (!MediaSink->Initialize(*StreamSink))
	{
		return false;
	}

	OutSink = StreamSink;

	return true;
}


/* FWmfMediaStreamSink structors
 *****************************************************************************/

FWmfMediaStreamSink::FWmfMediaStreamSink(const GUID& InMajorType, DWORD InStreamId)
	: Prerolling(false)
	, RefCount(0)
	, StreamId(InStreamId)
	, StreamType(InMajorType)
{
	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Created with stream type %s"), this, *WmfMedia::MajorTypeToString(StreamType));
}


FWmfMediaStreamSink::~FWmfMediaStreamSink()
{
	check(RefCount == 0);

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Destroyed"), this);
}


/* FWmfMediaStreamSink interface
 *****************************************************************************/

bool FWmfMediaStreamSink::GetNextSample(const TRange<FTimespan>& SampleRange, FWmfMediaStreamSinkSample& OutSample)
{
	if (SampleRange.IsEmpty())
	{
		return false; // nothing to play
	}

	FScopeLock Lock(&CriticalSection);

	FQueuedSample QueuedSample;

	while (SampleQueue.Peek(QueuedSample))
	{
		if (QueuedSample.MediaType.IsValid())
		{
			check(QueuedSample.Sample.IsValid());

			if (!SampleRange.Contains(FTimespan(QueuedSample.Time)))
			{
				return false; // no new sample needed
			}

			check(SampleQueue.Pop());

			OutSample.MediaType = QueuedSample.MediaType;
			OutSample.Sample = QueuedSample.Sample;

			return true;
		}

		SampleQueue.Pop();

		// process pending marker
		QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, QueuedSample.MarkerContext);
		PropVariantClear(QueuedSample.MarkerContext);
		delete QueuedSample.MarkerContext;

		UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Processed marker (%s)"), this, *WmfMedia::MarkerTypeToString(QueuedSample.MarkerType));

		continue;
	}

	return false;
}


bool FWmfMediaStreamSink::Initialize(FWmfMediaSink& InOwner)
{
	FScopeLock Lock(&CriticalSection);

	const HRESULT Result = ::MFCreateEventQueue(&EventQueue);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Failed to create event queue for stream sink: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	Owner = &InOwner;

	return true;
}


HRESULT FWmfMediaStreamSink::Pause()
{
	FScopeLock Lock(&CriticalSection);

	return QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Preroll()
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	Prerolling = true;

	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Restart()
{
	FScopeLock Lock(&CriticalSection);

	return QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, NULL);
}


void FWmfMediaStreamSink::Shutdown()
{
	FScopeLock Lock(&CriticalSection);

	if (EventQueue.IsValid())
	{
		EventQueue->Shutdown();
		EventQueue.Reset();
	}
}


HRESULT FWmfMediaStreamSink::Start()
{
	FScopeLock Lock(&CriticalSection);

	HRESULT Result = QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, NULL);

	if (FAILED(Result))
	{
		return Result;
	}

	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Stop()
{
	Flush();

	FScopeLock Lock(&CriticalSection);

	return QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, NULL);
}


/* IMFGetService interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
	return Owner->GetService(guidService, riid, ppvObject);
}


/* IMFMediaEventGenerator interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->BeginGetEvent(pCallback, pState);
}


STDMETHODIMP FWmfMediaStreamSink::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->EndGetEvent(pResult, ppEvent);
}


STDMETHODIMP FWmfMediaStreamSink::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
	TComPtr<IMFMediaEventQueue> TempQueue;
	{
		FScopeLock Lock(&CriticalSection);

		if (!EventQueue.IsValid())
		{
			return MF_E_SHUTDOWN;
		}

		TempQueue = EventQueue;
	}

	return TempQueue->GetEvent(dwFlags, ppEvent);
}


STDMETHODIMP FWmfMediaStreamSink::QueueEvent(MediaEventType met, REFGUID extendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->QueueEventParamVar(met, extendedType, hrStatus, pvValue);
}


/* IMFMediaTypeHandler interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::GetCurrentMediaType(_Outptr_ IMFMediaType** ppMediaType)
{
	FScopeLock Lock(&CriticalSection);

	if (ppMediaType == NULL)
	{
		return E_POINTER;
	}

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		return MF_E_NOT_INITIALIZED;
	}

	*ppMediaType = CurrentMediaType;
	(*ppMediaType)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMajorType(__RPC__out GUID* pguidMajorType)
{
	if (pguidMajorType == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		return MF_E_NOT_INITIALIZED;
	}

	return CurrentMediaType->GetGUID(MF_MT_MAJOR_TYPE, pguidMajorType);
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeByIndex(DWORD dwIndex, _Outptr_ IMFMediaType** ppType)
{
	if (ppType == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	// get supported media type
	TArray<TComPtr<IMFMediaType>> SupportedTypes = WmfMedia::GetSupportedMediaTypes(StreamType);

	if (!SupportedTypes.IsValidIndex(dwIndex))
	{
		return MF_E_NO_MORE_TYPES;
	}

	TComPtr<IMFMediaType> SupportedType = SupportedTypes[dwIndex];

	if (!SupportedType.IsValid())
	{
		return MF_E_INVALIDMEDIATYPE;
	}

	// create result type
	TComPtr<IMFMediaType> MediaType;
	{
		HRESULT Result = ::MFCreateMediaType(&MediaType);

		if (FAILED(Result))
		{
			return Result;
		}

		Result = SupportedType->CopyAllItems(MediaType);

		if (FAILED(Result))
		{
			return Result;
		}
	}

	*ppType = MediaType;
	(*ppType)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeCount(__RPC__out DWORD* pdwTypeCount)
{
	if (pdwTypeCount == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pdwTypeCount = (DWORD)WmfMedia::GetSupportedMediaTypes(StreamType).Num();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::IsMediaTypeSupported(IMFMediaType* pMediaType, _Outptr_opt_result_maybenull_ IMFMediaType** ppMediaType)
{
	if (ppMediaType != NULL)
	{
		*ppMediaType = NULL;
	}

	if (pMediaType == NULL)
	{
		return E_POINTER;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Checking if media type is supported:\n%s"), this, *WmfMedia::DumpAttributes(*pMediaType));

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	// get requested major type
	GUID MajorType;
	{
		const HRESULT Result = pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &MajorType);

		if (FAILED(Result))
		{
			return Result;
		}
	}

	if (MajorType != StreamType)
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type doesn't match stream type %s"), this, *WmfMedia::MajorTypeToString(StreamType));
		return MF_E_INVALIDMEDIATYPE;
	}

	// compare media type
	const DWORD CompareFlags = MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_DATA;

	for (const TComPtr<IMFMediaType>& MediaType : WmfMedia::GetSupportedMediaTypes(StreamType))
	{
		if (!MediaType.IsValid())
		{
			continue;
		}

		DWORD OutFlags = 0;
		const HRESULT Result = MediaType->IsEqual(pMediaType, &OutFlags);

		if (SUCCEEDED(Result) && ((OutFlags & CompareFlags) == CompareFlags))
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type is supported"), this, *WmfMedia::MajorTypeToString(MajorType));
			return S_OK;
		}
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type is not supported"), this);

	return MF_E_INVALIDMEDIATYPE;
}


STDMETHODIMP FWmfMediaStreamSink::SetCurrentMediaType(IMFMediaType* pMediaType)
{
	if (pMediaType == NULL)
	{
		return E_POINTER;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Setting current media type:\n%s"), this, *WmfMedia::DumpAttributes(*pMediaType));

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	const HRESULT Result = IsMediaTypeSupported(pMediaType, NULL);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Tried to set unsupported media type"), this);
		return Result;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Current media type set"), this);

	CurrentMediaType = pMediaType;

	return S_OK;
}


/* IMFStreamSink interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::Flush()
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Flushing samples & markers"), this);

	FQueuedSample QueuedSample;

	while (SampleQueue.Dequeue(QueuedSample))
	{
		if (QueuedSample.MediaType.IsValid())
		{
			continue;
		}

		// notify WMF that flushed markers haven't been processed
		QueueEvent(MEStreamSinkMarker, GUID_NULL, E_ABORT, QueuedSample.MarkerContext);
		PropVariantClear(QueuedSample.MarkerContext);
		delete QueuedSample.MarkerContext;
	}

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetIdentifier(__RPC__out DWORD* pdwIdentifier)
{
	if (pdwIdentifier == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pdwIdentifier = StreamId;

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaSink(__RPC__deref_out_opt IMFMediaSink** ppMediaSink)
{
	if (ppMediaSink == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*ppMediaSink = Owner;
	(*ppMediaSink)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeHandler(__RPC__deref_out_opt IMFMediaTypeHandler** ppHandler)
{
	if (ppHandler == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return QueryInterface(IID_IMFMediaTypeHandler, (void**)ppHandler);
}


STDMETHODIMP FWmfMediaStreamSink::PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, __RPC__in const PROPVARIANT* pvarMarkerValue, __RPC__in const PROPVARIANT* pvarContextValue)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Placing marker (%s)"), this, *WmfMedia::MarkerTypeToString(eMarkerType));

	PROPVARIANT* MarkerContext = new PROPVARIANT;

	if (pvarContextValue != NULL)
	{
		HRESULT Result = ::PropVariantCopy(MarkerContext, pvarContextValue);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Failed to copy marker context: %s"), this, *WmfMedia::ResultToString(Result));
			delete MarkerContext;

			return Result;
		}
	}

	SampleQueue.Enqueue({ eMarkerType, MarkerContext, NULL, NULL, 0 });

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::ProcessSample(__RPC__in_opt IMFSample* pSample)
{
	if (pSample == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Stream received a sample while not having a valid media type set"), this);
		return MF_E_INVALIDMEDIATYPE;
	}

	// get sample time
	LONGLONG Time = 0;
	{
		const HRESULT Result = pSample->GetSampleTime(&Time);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("Failed to get time from sink sample: %s"), *WmfMedia::ResultToString(Result));
			return Result;
		}
	}

	SampleQueue.Enqueue({ MFSTREAMSINK_MARKER_DEFAULT, NULL, CurrentMediaType, pSample, Time });

	// finish pre-rolling
	if (Prerolling)
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Preroll complete"), this);
		Prerolling = false;

		return QueueEvent(MEStreamSinkPrerolled, GUID_NULL, S_OK, NULL);
	}

	// request another sample
	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


/* IUnknown interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaStreamSink::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


#if _MSC_VER == 1900
	#pragma warning(push)
	#pragma warning(disable:4838)
#endif

STDMETHODIMP FWmfMediaStreamSink::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaStreamSink, IMFGetService),
		QITABENT(FWmfMediaStreamSink, IMFMediaTypeHandler),
		QITABENT(FWmfMediaStreamSink, IMFStreamSink),
		{ 0 }
	};

	return QISearch(this, QITab, RefID, Object);
}

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif


STDMETHODIMP_(ULONG) FWmfMediaStreamSink::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);

	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


#endif
