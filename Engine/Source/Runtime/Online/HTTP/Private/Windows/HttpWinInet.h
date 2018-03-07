// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "HAL/ThreadSafeCounter.h"
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
	#include <wininet.h>
#include "HideWindowsPlatformTypes.h"

/**
 * Class that encapsulates the logic for using WinInet to parse a URL string.
 * Shared by FHttpRequestWinInet and FHttpResponseWinInet classes to handle retrieving info about the URL.
 * Main feature is lazy evaluation of the WinInet structs used to parse the URL.
 * When the first field is accessed, it parses the string and caches the info it needs.
 * WARNING: There is a heavyweight map of URL parameters that is built, so if the URL contains
 * parameters, this class can be somewhat heavyweight.
 */
class FURLWinInet
{
public: 
	/** Default Ctor */
	FURLWinInet();
	/** Ctor from a string that contains the full URL */
	explicit FURLWinInet(const FString& InURL);
	/** Copy Ctor */
	FURLWinInet(const FURLWinInet& InURL);
	/** Assignment operator */
	FURLWinInet& operator=(const FURLWinInet& InURL);
	/** gets the full URL as a string. */
	const FString& GetURL() const;
	/** Extracts the host from the URL (empty if it can't be parsed). */
	FString GetHost() const;
	/** Extracts the port from the URL (0 if it can't be parsed). */
	WORD GetPort() const;
	/** Extracts the Path from the URL (empty if it can't be parsed). */
	FString GetPath() const;
	/** Extracts the extra info (parameters) from the URL (empty if it can't be parsed). */
	FString GetExtraInfo() const;
	/** Gets the full components object from windows. */
	const URL_COMPONENTS& GetURLComponents() const;
	/**
	 * Extract a URL parameter from the URL. If the URL doesn't use '?key=value&key2=value2'
	 * format, this will return NULL, or NULL if the parameter is not present.
	 * If the parameter has no value (?ParamWithNoValue), will return an empty string.
	 */
	const FString* GetParameter(const FString& ParameterName) const;
private:
	/** Shared function to clear the cached URL info. */
	void ClearCachedData() const;
	/** Does the real work of cracking the URL parameters on the first call. */
	void CrackUrlParameters() const;
	/** The full URL String.
	 ** WARNING: The data below uses pointers into this string, so it cannot be
	 ** modified without clearing the cache. */
	FString RequestURL;
	/** Cached data. Points into the string. */
	mutable const TCHAR* URLPtr;
	/** Cached data. Points into the string. */
	mutable URL_COMPONENTS URLParts;
	/** Cached parameter data. */
	mutable TMap<FString, FString> URLParameters;
};

/**
 * WinInet implementation of an Http request
 */
class FHttpRequestWinInet : public IHttpRequest
{

public:

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpRequest

	virtual FString GetVerb() override;
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() override;

	// FHttpRequestWinInet

	/**
	 * Constructor
	 */
	FHttpRequestWinInet();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FHttpRequestWinInet();

private:

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();

	/**
	 * Process state for a finished request that no longer needs to be ticked
	 * Calls the completion delegate
	 */
	void FinishedRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();

	/**
	 * Create the header buffer string from all the key/value pairs
	 * Also add the "Content-Length" header entry
	 *
	 * @param ContentLength - used to add the "Content-Length" header entry
	 */
	FString GenerateHeaderBuffer(uint32 ContentLength);

	/**
	 * Resets the timeout mechanism. This is used to indicate that there was activity from the server.
	 */
	void ResetRequestTimeout();

	// implementation friends
	friend class FHttpResponseWinInet;
	friend void CALLBACK InternetStatusCallbackWinInet(HINTERNET, DWORD_PTR,::DWORD,LPVOID,::DWORD);

private:

	/** URL address to connect request to */
	FURLWinInet RequestURL;
	/** Verb for making request (GET,POST,etc) */
	FString RequestVerb;
	/** Mapping of header section to values. Used to generate final header string for request */
	TMap<FString, FString> RequestHeaders;
	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;
	/** Holds response data that comes back from a successful request.  NULL if request can't connect */
	TSharedPtr<class FHttpResponseWinInet,ESPMode::ThreadSafe> Response;
	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;
	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;
	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;
	/** WinInet handle to a session connection opened via InternetConnect */
	HINTERNET ConnectionHandle;
	/** WinInet handle to a request opened via HttpOpenRequest */
	HINTERNET RequestHandle;
	/** Total elapsed time in seconds since the last server response, in milliseconds */
	int32 volatile ElapsedTimeSinceLastServerResponse;
	/** Number of bytes sent to progress update */
	int32 ProgressBytesSent;
	/** Used to calculate total elapsed time for the request */
	double StartRequestTime;
	/** Elapsed time for hte request in seconds. */
	float ElapsedTime;
	/** enables verbose logging for any http request that exceeds the total timeout */
	bool bDebugVerbose;
};

/**
 * WinInet implementation of an Http response
 */
class FHttpResponseWinInet : public IHttpResponse
{

public:

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpResponse

	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;

	// FHttpResponseWinInet

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FHttpResponseWinInet(FHttpRequestWinInet& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FHttpResponseWinInet();

private:

	/**
	 * Process response that has been received. Copy content to payload buffer via async reads
	 */
	void ProcessResponse();

	/**
	 * Query header info from the response and cache the results to ResponseHeaders
	 */
	void ProcessResponseHeaders();

	/**
	 * Query for the response code that has been received and cache to ResponseCode
	 */
	void ProcessResponseCode();

	/**
	 * Query header info from the response for a specific header value
	 *
	 * @param HttpQueryInfoLevel - see HttpQueryInfo flags
	 * @param HeaderName - header entry to find value for
	 * @return value of the header entry or "" if not found
	 */
	FString QueryHeaderString(uint32 HttpQueryInfoLevel, const FString& HeaderName) const;

	/**
	 * Query header info for "Content-Length"
	 *
	 * @return "Content-Length" header entry as an int
	 */
	int32 QueryContentLength() const;

	// implementation friends
	friend class FHttpRequestWinInet;
	friend void CALLBACK InternetStatusCallbackWinInet(HINTERNET, DWORD_PTR,::DWORD,LPVOID,::DWORD);

private:

	/** Request that owns this response */
	FHttpRequestWinInet& Request;
	/** Original URL used for the request */
	FURLWinInet RequestURL;
	/** Last bytes read from async call to InternetReadFile */
	int32 AsyncBytesRead;
	/** Caches how many bytes of the response we've read so far */
	int32 TotalBytesRead;
	/** Cached key/value header pairs. Parsed from HttpQueryInfo results once request completes */
	TMap<FString, FString> ResponseHeaders;
	/** Cached code from completed response */
	int32 ResponseCode;
	/** Cached content length from completed response */
	int32 ContentLength;
	/** BYTE array to fill in as the response is read via InternetReadFile */
	TArray<uint8> ResponsePayload;
	/** True when the response has finished async processing */
	int32 volatile bIsReady;
	/** True if the response was successfully received/processed */
	int32 volatile bResponseSucceeded;
	/** Whether or not the request was actually sent */
	int32 volatile bRequestSent;
	/** Threadsafe counter for exchanging bytes read so far with main thread tick */
	FThreadSafeCounter ProgressBytesRead;
	/** Max buffer size for individual http reads */
	int32 MaxReadBufferSize;
};

/**
 * Helper for setting up a valid Internet connection for use by Http request
 */
class FWinInetConnection
{
public:

	/**
	 * Constructor
	 */
	FWinInetConnection()
	:	InternetHandle(NULL)
	,	HttpManager(&FHttpModule::Get().GetHttpManager())
	{
	}

	/**
	 * Singleton accessor
	 */
	static FWinInetConnection& Get()
	{
		static FWinInetConnection Singleton;
		return Singleton;
	}

	inline FHttpManager* GetHttpManager()
	{
		return HttpManager;
	}

	/**
	 * Attempt to open an Internet connection
	 * Caches the ConnectionHandle if successful
	 *
	 * @return true if the connection was created
	 */
	bool InitConnection();

	/**
	 * Close the internet connection handle
	 *
	 * @return true if succeeded
	 */
	bool ShutdownConnection();

	/**
	 * Determine if internet connection handle is valid
	 *
	 * @return true if valid
	 */
	bool IsConnectionValid()
	{
		return InternetHandle != NULL;
	}

	/** Handle created via InternetOpen. NULL if not valid */
	HINTERNET InternetHandle;
	/** holding a pointer to this since it will be needed by the internetstatus callback which is called from another thread */
	FHttpManager* HttpManager;
	/** A bool to determine statically if this singleton is initialized. This is set back to false at commention shutdown to prevent issues at module shutdown time. */
	static bool bStaticConnectionInitialized;
};
