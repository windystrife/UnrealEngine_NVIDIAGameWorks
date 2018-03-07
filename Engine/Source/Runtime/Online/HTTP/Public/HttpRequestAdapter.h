// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"

/** 
  * Adapter class for IHttpRequest abstract interface
  * does not fully expose the wrapped interface in the base. This allows client defined marshalling of the requests when end point permissions are at issue.
  */

class FHttpRequestAdapterBase : public IHttpRequest
{
public:
    FHttpRequestAdapterBase(const TSharedRef<IHttpRequest>& InHttpRequest) 
		: HttpRequest(InHttpRequest)
    {}

	// IHttpRequest interface
    virtual FString                       GetURL() override   	                                                   { return HttpRequest->GetURL(); }
	virtual FString                       GetURLParameter(const FString& ParameterName) override                   { return HttpRequest->GetURLParameter(ParameterName); }
	virtual FString                       GetHeader(const FString& HeaderName) override                            { return HttpRequest->GetHeader(HeaderName); }
	virtual TArray<FString>               GetAllHeaders() override                                                 { return HttpRequest->GetAllHeaders(); }
	virtual FString                       GetContentType() override                                                { return HttpRequest->GetContentType(); }
	virtual int32                         GetContentLength() override                                              { return HttpRequest->GetContentLength(); }
	virtual const TArray<uint8>&          GetContent() override                                                    { return HttpRequest->GetContent(); }
	virtual FString                       GetVerb() override						                               { return HttpRequest->GetVerb(); }
	virtual void                          SetVerb(const FString& Verb) override	                                   { HttpRequest->SetVerb(Verb); }
	virtual void                          SetURL(const FString& URL) override		                               { HttpRequest->SetURL(URL); }
	virtual void                          SetContent(const TArray<uint8>& ContentPayload) override                 { HttpRequest->SetContent(ContentPayload); }
	virtual void                          SetContentAsString(const FString& ContentString) override                 { HttpRequest->SetContentAsString(ContentString); }
	virtual void                          SetHeader(const FString& HeaderName, const FString& HeaderValue) override { HttpRequest->SetHeader(HeaderName, HeaderValue); }
	virtual void                          AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override { HttpRequest->AppendToHeader(HeaderName, AdditionalHeaderValue); }
	virtual const FHttpResponsePtr        GetResponse() const override                                             { return HttpRequest->GetResponse(); }
	virtual float                         GetElapsedTime() override                                                { return HttpRequest->GetElapsedTime(); }
	virtual EHttpRequestStatus::Type	  GetStatus() override { return HttpRequest->GetStatus(); }
	virtual void Tick(float DeltaSeconds) override { HttpRequest->Tick(DeltaSeconds); }

protected:
    TSharedRef<IHttpRequest> HttpRequest;
};

