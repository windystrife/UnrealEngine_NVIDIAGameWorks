// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Common/HttpManager.h"
#include "Tests/TestHelpers.h"
#include "Tests/Mock/HttpRequest.mock.h"
#include "Common/StatsCollector.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockHttpManager
		: public IHttpManager
	{
	public:
		FMockHttpManager()
			: RxCreateRequest(0)
		{
		}

		virtual TSharedRef<IHttpRequest> CreateRequest() override
		{
			++RxCreateRequest;
			return TSharedRef<IHttpRequest>(new FMockHttpRequest());
		}

	public:
		int32 RxCreateRequest;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
