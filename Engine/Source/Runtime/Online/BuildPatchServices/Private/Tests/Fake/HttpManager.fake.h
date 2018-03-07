// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Ticker.h"
#include "Tests/Mock/HttpManager.mock.h"
#include "Tests/Mock/HttpRequest.mock.h"
#include "Tests/Mock/HttpResponse.mock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FFakeHttpRequest
		: public FMockHttpRequest
	{
	public:
		virtual bool ProcessRequest() override;

	public:
		class FFakeHttpManager* FakeHttpModule;
	};

	class FFakeHttpResponse
		: public FMockHttpResponse
	{
	public:
		virtual int32 GetResponseCode() override;
		virtual const TArray<uint8>& GetContent() override;
		virtual FString GetContentAsString() override;
		virtual int32 GetContentLength() override;

	public:
		TArray<uint8> Data;
		EHttpResponseCodes::Type Code;
	};

	class FFakeHttpManager
		: public FMockHttpManager
	{
	public:
		FFakeHttpManager(FTicker& Ticker);
		virtual TSharedRef<IHttpRequest> CreateRequest() override;
		virtual bool Tick(float Delta);
		virtual bool OnProcessRequest(FFakeHttpRequest* FakeHttpRequest);

	public:
		TArray<TSharedRef<FFakeHttpRequest>> NewRequests;
		TArray<TSharedRef<FFakeHttpRequest>> RunningRequests;
		TArray<TSharedRef<FFakeHttpRequest>> ProgressedRequests;
		TMap<FString, TArray<uint8>> DataServed;
	};

	bool FFakeHttpRequest::ProcessRequest()
	{
		++RxProcessRequest;
		return FakeHttpModule->OnProcessRequest(this);
	}

	int32 FFakeHttpResponse::GetResponseCode()
	{
		return static_cast<int32>(Code);
	}

	const TArray<uint8>& FFakeHttpResponse::GetContent()
	{
		return Data;
	}

	FString FFakeHttpResponse::GetContentAsString()
	{
		TArray<uint8> ZeroTerminatedPayload(GetContent());
		ZeroTerminatedPayload.Add(0);
		return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
	}

	int32 FFakeHttpResponse::GetContentLength()
	{
		return Data.Num();
	}

	FFakeHttpManager::FFakeHttpManager(FTicker& Ticker)
	{
		Ticker.AddTicker(FTickerDelegate::CreateRaw(this, &FFakeHttpManager::Tick));
	}

	TSharedRef<IHttpRequest> FFakeHttpManager::CreateRequest()
	{
		++RxCreateRequest;
		NewRequests.Emplace(new FFakeHttpRequest());
		NewRequests.Last()->FakeHttpModule = this;
		return NewRequests.Last();
	}

	bool FFakeHttpManager::Tick(float Delta)
	{
		// Complete progressed.
		for (const TSharedRef<FFakeHttpRequest>& ProgressedRequest : ProgressedRequests)
		{
			FFakeHttpResponse* FakeHttpResponse = new FFakeHttpResponse();
			FakeHttpResponse->Code = EHttpResponseCodes::Ok;
			const FString& Url = ProgressedRequest->RxSetURL.Last().Get<0>();
			if (DataServed.Contains(Url))
			{
				FakeHttpResponse->Data = DataServed[Url];
			}
			ProgressedRequest->HttpRequestCompleteDelegate.ExecuteIfBound(
				ProgressedRequest,
				MakeShareable(FakeHttpResponse),
				true);
		}

		// Progress started.
		ProgressedRequests = MoveTemp(RunningRequests);
		for (const TSharedRef<FFakeHttpRequest>& ProgressedRequest : ProgressedRequests)
		{
			const FString& Url = ProgressedRequest->RxSetURL.Last().Get<0>();
			int32 Progress = 0;
			if (DataServed.Contains(Url))
			{
				Progress = DataServed[Url].Num() / 2;
			}
			ProgressedRequest->HttpRequestProgressDelegate.ExecuteIfBound(
				ProgressedRequest,
				0,
				Progress);
		}

		return true;
	}

	bool FFakeHttpManager::OnProcessRequest(FFakeHttpRequest* FakeHttpRequest)
	{
		int32 Index = NewRequests.IndexOfByPredicate([FakeHttpRequest](const TSharedRef<FFakeHttpRequest>& Element)
		{
			return &Element.Get() == FakeHttpRequest;
		});
		bool bValidRequest = Index >= 0;
		if (bValidRequest)
		{
			const TSharedRef<FFakeHttpRequest>& Request = NewRequests[Index];
			bValidRequest = Request->RxSetURL.Num() > 0;
			if (bValidRequest)
			{
				RunningRequests.Add(Request);
				NewRequests.RemoveAtSwap(Index);
			}
		}
		return bValidRequest;
	}
}

#endif //WITH_DEV_AUTOMATION_TESTS
