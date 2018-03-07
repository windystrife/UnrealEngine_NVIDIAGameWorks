// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

/**
 * Null (mock) implementation of an HTTP request
 */
class FNullHttpRequest : public IHttpRequest
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

	FNullHttpRequest()
		: CompletionStatus(EHttpRequestStatus::NotStarted)
		, ElapsedTime(0)
	{}
	virtual ~FNullHttpRequest() {}

private:
	void FinishedRequest();

	FString Url;
	FString Verb;
	TArray<uint8> Payload;
	FHttpRequestCompleteDelegate RequestCompleteDelegate;
	FHttpRequestProgressDelegate ReuestProgressDelegate;
	EHttpRequestStatus::Type CompletionStatus;
	TMap<FString, FString> Headers;
	float ElapsedTime;
};

/**
 * Null (mock) implementation of an HTTP request
 */
class FNullHttpResponse : public IHttpResponse
{
	// IHttpBase 
	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;
	//~ Begin IHttpResponse Interface
	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;

	FNullHttpResponse() {}
	virtual ~FNullHttpResponse() {}

private:
	TArray<uint8> Payload;
};
