// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HttpTests.h"
#include "HttpModule.h"
#include "Http.h"

// FHttpTest

FHttpTest::FHttpTest(const FString& InVerb, const FString& InPayload, const FString& InUrl, int32 InIterations)
	: Verb(InVerb)
	, Payload(InPayload)
	, Url(InUrl)
	, TestsToRun(InIterations)
{
	
}

void FHttpTest::Run(void)
{
	UE_LOG(LogHttp, Log, TEXT("Starting test [%s] Url=[%s]"), 
		*Verb, *Url);

	for (int Idx=0; Idx < TestsToRun; Idx++)
	{
		TSharedPtr<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
		Request->OnProcessRequestComplete().BindRaw(this, &FHttpTest::RequestComplete);
		Request->SetURL(Url);
		if (Payload.Len() > 0)
		{
			Request->SetContentAsString(Payload);
		}
		Request->SetVerb(Verb);
		Request->ProcessRequest();
	}
}

void FHttpTest::RequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (!HttpResponse.IsValid())
	{
		UE_LOG(LogHttp, Log, TEXT("Test failed. NULL response"));
	}
	else
	{
		UE_LOG(LogHttp, Log, TEXT("Completed test [%s] Url=[%s] Response=[%d] [%s]"), 
			*HttpRequest->GetVerb(), 
			*HttpRequest->GetURL(), 
			HttpResponse->GetResponseCode(), 
			*HttpResponse->GetContentAsString());
	}
	
	if ((--TestsToRun) <= 0)
	{
		HttpRequest->OnProcessRequestComplete().Unbind();
		// Done with the test. Delegate should always gets called
		delete this;
	}
}

