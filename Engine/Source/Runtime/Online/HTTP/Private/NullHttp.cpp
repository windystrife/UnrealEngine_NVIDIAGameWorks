// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NullHttp.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "Http.h"

// FNullHttpRequest

FString FNullHttpRequest::GetURL()
{
	return Url;
}

FString FNullHttpRequest::GetURLParameter(const FString& ParameterName)
{
	return FString();
}

FString FNullHttpRequest::GetHeader(const FString& HeaderName)
{
	FString* Header = Headers.Find(HeaderName);
	if (Header != NULL)
	{
		return *Header;
	}
	return FString();
}

TArray<FString> FNullHttpRequest::GetAllHeaders()
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FNullHttpRequest::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FNullHttpRequest::GetContentLength()
{
	return Payload.Num();
}

const TArray<uint8>& FNullHttpRequest::GetContent()
{
	return Payload;
}

FString FNullHttpRequest::GetVerb()
{
	return Verb;
}

void FNullHttpRequest::SetVerb(const FString& InVerb)
{
	Verb = InVerb;
}

void FNullHttpRequest::SetURL(const FString& InURL)
{
	Url = InURL;
}

void FNullHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	Payload = ContentPayload;
}

void FNullHttpRequest::SetContentAsString(const FString& ContentString)
{
	FTCHARToUTF8 Converter(*ContentString);
	Payload.SetNum(Converter.Length());
	FMemory::Memcpy(Payload.GetData(), (uint8*)(ANSICHAR*)Converter.Get(), Payload.Num());
}

void FNullHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	Headers.Add(HeaderName, HeaderValue);
}

void FNullHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (HeaderName.Len() > 0 && AdditionalHeaderValue.Len() > 0)
	{
		FString* PreviousValue = Headers.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != NULL && PreviousValue->Len() > 0)
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

bool FNullHttpRequest::ProcessRequest()
{
	ElapsedTime = 0;
	CompletionStatus = EHttpRequestStatus::Processing;

	UE_LOG(LogHttp, Log, TEXT("Start request. %p %s url=%s"), this, *GetVerb(), *GetURL());

	FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	return true;
}

FHttpRequestCompleteDelegate& FNullHttpRequest::OnProcessRequestComplete()
{
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FNullHttpRequest::OnRequestProgress()
{
	return ReuestProgressDelegate;
}

void FNullHttpRequest::CancelRequest()
{
	FinishedRequest();
}

EHttpRequestStatus::Type FNullHttpRequest::GetStatus()
{
	return CompletionStatus;
}

const FHttpResponsePtr FNullHttpRequest::GetResponse() const
{
	return FHttpResponsePtr(nullptr);
}

void FNullHttpRequest::Tick(float DeltaSeconds)
{
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		ElapsedTime += DeltaSeconds;
		const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
		if (HttpTimeout > 0 && ElapsedTime >= HttpTimeout)
		{
			UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"),
				this);

			FinishedRequest();
		}
	}
}

float FNullHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}

void FNullHttpRequest::FinishedRequest()
{
	CompletionStatus = EHttpRequestStatus::Failed;
	TSharedRef<IHttpRequest> Request = SharedThis(this);
	FHttpModule::Get().GetHttpManager().RemoveRequest(Request);

	UE_LOG(LogHttp, Log, TEXT("Finished request %p. no response %s url=%s elapsed=%.3f"),
		this, *GetVerb(), *GetURL(), ElapsedTime);

	OnProcessRequestComplete().ExecuteIfBound(Request, NULL, false);
}

// FNullHttpResponse

FString FNullHttpResponse::GetURL()
{
	return FString();
}

FString FNullHttpResponse::GetURLParameter(const FString& ParameterName)
{
	return FString();
}

FString FNullHttpResponse::GetHeader(const FString& HeaderName)
{
	return FString();
}

TArray<FString> FNullHttpResponse::GetAllHeaders()
{
	return TArray<FString>();
}

FString FNullHttpResponse::GetContentType()
{
	return FString();
}

int32 FNullHttpResponse::GetContentLength()
{
	return 0;
}

const TArray<uint8>& FNullHttpResponse::GetContent()
{
	return Payload;
}

int32 FNullHttpResponse::GetResponseCode()
{
	return 0;
}

FString FNullHttpResponse::GetContentAsString()
{
	return FString();
}


