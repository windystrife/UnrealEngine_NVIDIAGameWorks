// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WmfMediaPrivate.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "CoreTypes.h"
#include "Containers/Queue.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "Misc/Timespan.h"

class FWmfMediaSink;


/**
 * Structure for media samples queued in a stream sink.
 */
struct FWmfMediaStreamSinkSample
{
	/** The sample's media type. */
	TComPtr<IMFMediaType> MediaType;

	/** The media sample. */
	TComPtr<IMFSample> Sample;
};


/**
 * Implements a stream sink object for the WMF pipeline.
 */
class FWmfMediaStreamSink
	: public IMFGetService
	, public IMFMediaTypeHandler
	, public IMFStreamSink
{
public:

	/**
	 * Creates a stream sink for the specified major type.
	 *
	 * @param MajorType The sink's major type.
	 * @param OutSink Will contain the created sink.
	 * @return true on success, false otherwise.
	 */
	static bool Create(const GUID& MajorType, TComPtr<FWmfMediaStreamSink>& OutSink);

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InMajorType The sink's major media type, i.e. audio or video.
	 * @param InStreamId The sink's identifier.
	 */
	FWmfMediaStreamSink(const GUID& InMajorType, DWORD InStreamId);

public:

	/**
	 * Get the next sample in the queue.
	 *
	 * @param SampleRange Time range of samples that should be returned.
	 * @param OutSample Will contain the sample.
	 * @return true if a sample was returned, false if the queue is empty.
	 */
	bool GetNextSample(const TRange<FTimespan>& SampleRange, FWmfMediaStreamSinkSample& OutSample);

	/**
	 * Initialize this sink.
	 *
	 * @param InOwner The media sink that owns this stream sink.
	 * @return true on success, false otherwise.
	 * @see Shutdown
	 */
	bool Initialize(FWmfMediaSink& InOwner);

	/**
	 * Pause the stream.
	 *
	 * This method is called by the owner media sink.
	 *
	 * @return Result code.
	 * @see Preroll, Restart, Start, Stop
	 */
	HRESULT Pause();

	/**
	 * Preroll the sink.
	 *
	 * This method is called by the owner media sink.
	 *
	 * @return Result code.
	 * @see Pause, Restart, Start, Stop
	 */
	HRESULT Preroll();

	/**
	 * Restart the stream.
	 *
	 * This method is called by the owner media sink.
	 *
	 * @return Result code.
	 * @see Pause, Preroll, Start, Stop
	 */
	HRESULT Restart();

	/**
	 * Shut down this sink.
	 *
	 * @see Initialize
	 */
	void Shutdown();

	/**
	 * Start the sink.
	 *
	 * This method is called by the owner media sink.
	 *
	 * @return Result code.
	 * @see Pause, Preroll, Restart, Stop
	 */
	HRESULT Start();

	/**
	 * Stop the stream.
	 *
	 * This method is called by the owner media sink.
	 *
	 * @return Result code.
	 * @see Pause, Preroll, Restart, Start
	 */
	HRESULT Stop();

public:

	//~ IMFGetService interface

	STDMETHODIMP GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject);

public:

	//~ IMFMediaEventGenerator interface

	STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState);
	STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
	STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);
	STDMETHODIMP QueueEvent(MediaEventType met, REFGUID extendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

public:

	//~ IMFMediaTypeHandler

	STDMETHODIMP GetCurrentMediaType(_Outptr_ IMFMediaType** ppMediaType);
	STDMETHODIMP GetMajorType(__RPC__out GUID* pguidMajorType);
	STDMETHODIMP GetMediaTypeByIndex(DWORD dwIndex, _Outptr_ IMFMediaType** ppType);
	STDMETHODIMP GetMediaTypeCount(__RPC__out DWORD* pdwTypeCount);
	STDMETHODIMP IsMediaTypeSupported(IMFMediaType* pMediaType, _Outptr_opt_result_maybenull_ IMFMediaType** ppMediaType);
	STDMETHODIMP SetCurrentMediaType(IMFMediaType* pMediaType);

public:

	//~ IMFStreamSink interface

	STDMETHODIMP Flush();
	STDMETHODIMP GetIdentifier(__RPC__out DWORD* pdwIdentifier);
	STDMETHODIMP GetMediaSink(__RPC__deref_out_opt IMFMediaSink** ppMediaSink);
	STDMETHODIMP GetMediaTypeHandler(__RPC__deref_out_opt IMFMediaTypeHandler** ppHandler);
	STDMETHODIMP PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, __RPC__in const PROPVARIANT* pvarMarkerValue, __RPC__in const PROPVARIANT* pvarContextValue);
	STDMETHODIMP ProcessSample(__RPC__in_opt IMFSample* pSample);

public:

	//~ IUnknown interface

	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object);
	STDMETHODIMP_(ULONG) Release();

private:

	/** Hidden destructor (this class is reference counted). */
	virtual ~FWmfMediaStreamSink();

private:

	/** Critical section for synchronizing access to this sink. */
	FCriticalSection CriticalSection;

	/** The event queue. */
	TComPtr<IMFMediaEventQueue> EventQueue;

	/** The sink's current media type. */
	TComPtr<IMFMediaType> CurrentMediaType;

	/** The media sink that owns this stream sink. */
	TComPtr<FWmfMediaSink> Owner;

	/** Whether the sink is currently prerolling samples. */
	bool Prerolling;

	/** Holds a reference counter for this instance. */
	int32 RefCount;

	/** The stream identifier (currently fixed). */
	DWORD StreamId;

	/** The sink's major media type. */
	const GUID StreamType;

private:

	/** Structure for queued media samples & markers. */
	struct FQueuedSample
	{
		/** Stream marker type. */
		MFSTREAMSINK_MARKER_TYPE MarkerType;

		/** Stream marker context. */
		PROPVARIANT* MarkerContext;

		/** The sample's media type. */
		TComPtr<IMFMediaType> MediaType;

		/** The media sample. */
		TComPtr<IMFSample> Sample;

		/** Sample time. */
		LONGLONG Time;
	};

	/** Media sample queue. */
	TQueue<FQueuedSample> SampleQueue;
};

#endif
