// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IHttpResponse.h"
#include "Tests/TestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockHttpResponse
		: public IHttpResponse
	{
	public:
		typedef TTuple<FString> FRxSetVerb;
		typedef TTuple<FString> FRxSetURL;

	public:
		virtual int32 GetResponseCode() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetResponseCode");
			return static_cast<int32>(EHttpResponseCodes::Ok);
		}

		virtual FString GetContentAsString() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetContentAsString");
			return FString();
		}

		virtual FString GetURL() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetURL");
			return FString();
		}

		virtual FString GetURLParameter(const FString& ParameterName) override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetURLParameter");
			return FString();
		}

		virtual FString GetHeader(const FString& HeaderName) override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetHeader");
			return FString();
		}

		virtual TArray<FString> GetAllHeaders() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetAllHeaders");
			return TArray<FString>();
		}

		virtual FString GetContentType() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetContentType");
			return FString();
		}

		virtual int32 GetContentLength() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetContentLength");
			return int32();
		}

		virtual const TArray<uint8>& GetContent() override
		{
			MOCK_FUNC_NOT_IMPLEMENTED("FMockHttpRequest::GetContent");
			static TArray<uint8> None;
			return None;
		}
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
