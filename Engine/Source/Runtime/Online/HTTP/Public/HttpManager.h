// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Runtime/Online/HTTP/Private/IHttpThreadedRequest.h"
#include "Containers/Ticker.h"
#include "HttpPackage.h"

class FHttpThread;

/**
 * Manages Http request that are currently being processed
 */
class HTTP_API FHttpManager
	: public FTickerObjectBase
{
public:

	// FHttpManager

	/**
	 * Constructor
	 */
	FHttpManager();

	/**
	 * Destructor
	 */
	~FHttpManager();

	/**
	 * Initialize
	 */
	void Initialize();

	/**
	 * Adds an Http request instance to the manager for tracking/ticking
	 * Manager should always have a list of requests currently being processed
	 *
	 * @param Request - the request object to add
	 */
	void AddRequest(const TSharedRef<IHttpRequest>& Request);

	/**
	 * Removes an Http request instance from the manager
	 * Presumably it is done being processed
	 *
	 * @param Request - the request object to remove
	 */
	void RemoveRequest(const TSharedRef<IHttpRequest>& Request);

	/**
	* Find an Http request in the lists of current valid requests
	*
	* @param RequestPtr - ptr to the http request object to find
	*
	* @return true if the request is being tracked, false if not
	*/
	bool IsValidRequest(const IHttpRequest* RequestPtr) const;

	/**
	 * Block until all pending requests are finished processing
	 *
	 * @param bShutdown true if final flush during shutdown
	 */
	void Flush(bool bShutdown);

	/**
	 * FTicker callback
	 *
	 * @param DeltaSeconds - time in seconds since the last tick
	 *
	 * @return false if no longer needs ticking
	 */
	bool Tick(float DeltaSeconds) override;

	/** 
	 * Add a http request to be executed on the http thread
	 *
	 * @param Request - the request object to add
	 */
	void AddThreadedRequest(const TSharedRef<IHttpThreadedRequest>& Request);

	/**
	 * Mark a threaded http request as cancelled to be removed from the http thread
	 *
	 * @param Request - the request object to cancel
	 */
	void CancelThreadedRequest(const TSharedRef<IHttpThreadedRequest>& Request);

	/**
	 * List all of the Http requests currently being processed
	 *
	 * @param Ar - output device to log with
	 */
	void DumpRequests(FOutputDevice& Ar) const;

protected:
	/** 
	 * Create HTTP thread object
	 *
	 * @return the HTTP thread object
	 */
	virtual FHttpThread* CreateHttpThread();


protected:
	/** List of Http requests that are actively being processed */
	TArray<TSharedRef<IHttpRequest>> Requests;

	/** Keep track of a request that should be deleted later */
	class FRequestPendingDestroy
	{
	public:
		FRequestPendingDestroy(float InTimeLeft, const TSharedPtr<IHttpRequest>& InHttpRequest)
			: TimeLeft(InTimeLeft)
			, HttpRequest(InHttpRequest)
		{}

		FORCEINLINE bool operator==(const FRequestPendingDestroy& Other) const
		{
			return Other.HttpRequest == HttpRequest;
		}

		float TimeLeft;
		TSharedPtr<IHttpRequest> HttpRequest;
	};

	/** Dead requests that need to be destroyed */
	TArray<FRequestPendingDestroy> PendingDestroyRequests;

	FHttpThread* Thread;

PACKAGE_SCOPE:

	/** Used to lock access to add/remove/find requests */
	static FCriticalSection RequestLock;
	/** Delay in seconds to defer deletion of requests */
	float DeferredDestroyDelay;
};
