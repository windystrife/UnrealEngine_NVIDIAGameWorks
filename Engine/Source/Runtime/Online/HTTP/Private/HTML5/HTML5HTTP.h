// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

/**
* HTML5 implementation of an Http request
*/
class FHTML5HttpRequest : public IHttpRequest
{
public:
	// implementation friends
	friend class FHTML5HttpResponse;

	//~ Begin IHttpBase Interface
	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;
	//~ End IHttpBase Interface

	//~ Begin IHttpRequest Interface
	virtual FString GetVerb() override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
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
	//~ End IHttpRequest Interface

	/**
	* Marks request as completed.
	*/
	inline void MarkAsCompleted()
	{
		bCompleted = true;
	}

	/**
	* Constructor
	*/
	FHTML5HttpRequest();

	/**
	* Destructor.
	*/
	virtual ~FHTML5HttpRequest();


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

	static void StaticReceiveCallback(void *arg, void *buffer, uint32 size, void* httpHeaders);
	void ReceiveCallback(void *arg, void *buffer, uint32 size, void* httpHeaders);
	static void StaticErrorCallback(void* arg, int httpStatusCode, const char* httpStatusText);
	void ErrorCallback(void* arg, int httpStatusCode, const char* httpStatusText);
	static void StaticProgressCallback(void* arg, int Loaded, int Total);
	void ProgressCallback(void* arg, int Loaded, int Total);

private:
	/** The response object which we will use to pair with this request */
	TSharedPtr<class FHTML5HttpResponse, ESPMode::ThreadSafe> Response;

	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;

	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;

	/** Cached URL */
	FString			URL;
	/** Cached verb */
	FString			Verb;
	/** Set to true if request has been canceled */
	bool			bCanceled;
	/** Set to true when request has been completed */
	bool			bCompleted;
	/** Number of bytes sent already */
	uint32			BytesSent;
	/** Mapping of header section to values. */
	TMap<FString, FString> Headers;
	/** Total elapsed time in seconds since the start of the request */
	float ElapsedTime;
};

// expose HTML5JavaScriptFx.js functions
extern "C" {
	void Register_OnBeforeUnload(void *arg, void(*callback)(void*));
	void UnRegister_OnBeforeUnload(void *arg, void(*callback)(void*));
}


/**
* HTML5 implementation of an Http response
*/
class FHTML5HttpResponse : public IHttpResponse
{

public:
	// implementation friends
	friend class FHTML5HttpRequest;


	//~ Begin IHttpBase Interface
	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;
	//~ End IHttpBase Interface

	//~ Begin IHttpResponse Interface
	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;
	//~ End IHttpResponse Interface

	/**
	* Check whether a response is ready or not.
	*/
	bool IsReady();

	/**
	* Constructor
	*
	* @param InRequest - original request that created this response
	*/
	FHTML5HttpResponse(FHTML5HttpRequest& InRequest);

	/**
	* Destructor
	*/
	virtual ~FHTML5HttpResponse();


private:

	/** Request that owns this response */
	FHTML5HttpRequest& Request;

	/** BYTE array to fill in as the response is read via ReceiveCallback */
	TArray<uint8> Payload;
	/** Caches how many bytes of the response we've read so far */
	int32 TotalBytesRead;
	/** Cached key/value header pairs. Parsed once request completes */
	TMap<FString, FString> Headers;
	/** Cached code from completed response */
	int32 HttpCode;
	/** Cached content length from completed response */
	int32 ContentLength;
	/** True when the response has finished async processing */
	int32 volatile bIsReady;
	/** True if the response was successfully received/processed */
	int32 volatile bSucceeded;

};
