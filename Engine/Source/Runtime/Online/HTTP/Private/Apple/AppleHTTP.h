// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "PlatformHttp.h"

class FHttpManager;
class FString;
class IHttpRequest;

/**
 * Apple implementation of an Http request
 */
class FAppleHttpRequest : public IHttpRequest
{
private:
	// This is the NSMutableURLRequest, all our Apple functionality will deal with this.
	NSMutableURLRequest* Request;

	// This is the connection our request is sent along.
	NSURLConnection* Connection;


public:
	// implementation friends
	friend class FAppleHttpResponse;


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
	//~ End IHttpRequest Interface


	/**
	 * Constructor
	 */
	FAppleHttpRequest();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FAppleHttpRequest();


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


private:
	/** The response object which we will use to pair with this request */
	TSharedPtr<class FAppleHttpResponse,ESPMode::ThreadSafe> Response;

	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;

	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;

	/** Number of bytes sent to progress update */
	int32 ProgressBytesSent;

	/** Start of the request */
	double StartRequestTime;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;
};


/**
 * Apple Response Wrapper which will be used for it's delegates to receive responses.
 */
@interface FHttpResponseAppleWrapper : NSObject
{
	/** Holds the payload as we receive it. */
	TArray<uint8> Payload;
}
/** A handle for the response */
@property(retain) NSHTTPURLResponse* Response;
/** Flag whether the response is ready */
@property BOOL bIsReady;
/** When the response is complete, indicates whether the response was received without error. */
@property BOOL bHadError;
/** The total number of bytes written out during the request/response */
@property int32 BytesWritten;

/** Delegate called when we send data. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite;
/** Delegate called with we receive a response. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
/** Delegate called with we receive data. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
/** Delegate called with we complete with an error. See Apple docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error;
/** Delegate called with we complete successfully. See Apple docs for when/how this should be used. */
-(void) connectionDidFinishLoading:(NSURLConnection *)connection;

- (TArray<uint8>&)getPayload;
- (int32)getBytesWritten;
@end


/**
 * Apple implementation of an Http response
 */
class FAppleHttpResponse : public IHttpResponse
{
private:
	// This is the NSHTTPURLResponse, all our functionality will deal with.
	FHttpResponseAppleWrapper* ResponseWrapper;

	/** Request that owns this response */
	const FAppleHttpRequest& Request;


public:
	// implementation friends
	friend class FAppleHttpRequest;


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

	NSHTTPURLResponse* GetResponseObj();

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady();
	
	/**
	 * Check whether a response had an error.
	 */
	bool HadError();

	/**
	 * Get the number of bytes received so far
	 */
	const int32 GetNumBytesReceived() const;

	/**
	* Get the number of bytes sent so far
	*/
	const int32 GetNumBytesWritten() const;

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FAppleHttpResponse(const FAppleHttpRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FAppleHttpResponse();


private:

	/** BYTE array to fill in as the response is read via didReceiveData */
	TArray<uint8> Payload;
};