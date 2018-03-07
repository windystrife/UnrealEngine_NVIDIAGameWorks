// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DownloadService.h"
#include "Common/StatsCollector.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockDownloadService
		: public IDownloadService
	{
	public:
		typedef TTuple<double, int32, FString, FDownloadCompleteDelegate, FDownloadProgressDelegate> FRequestFile;
		typedef TTuple<double, int32> FRequestCancel;

	public:
		FMockDownloadService()
			: Count(0)
		{
		}

		virtual int32 RequestFile(const FString& FileUri, const FDownloadCompleteDelegate& OnCompleteDelegate, const FDownloadProgressDelegate& OnProgressDelegate) override
		{
			FScopeLock ScopeLock(&ThreadLock);
			int32 ReturnId;
			if (RequestFileFunc)
			{
				ReturnId = RequestFileFunc(FileUri, OnCompleteDelegate, OnProgressDelegate);
			}
			else
			{
				ReturnId = ++Count;
			}
			RxRequestFile.Emplace(FStatsCollector::GetSeconds(), ReturnId, FileUri, OnCompleteDelegate, OnProgressDelegate);
			return ReturnId;
		}

		virtual void RequestCancel(int32 RequestId) override
		{
			FScopeLock ScopeLock(&ThreadLock);
			if (RequestCancelFunc)
			{
				RequestCancelFunc(RequestId);
			}
			RxRequestCancel.Emplace(FStatsCollector::GetSeconds(), RequestId);
		}

	public:
		FCriticalSection ThreadLock;
		int32 Count;
		TArray<FRequestFile> RxRequestFile;
		TArray<FRequestCancel> RxRequestCancel;
		TFunction<int32(const FString&, const FDownloadCompleteDelegate&, const FDownloadProgressDelegate&)> RequestFileFunc;
		TFunction<void(int32)> RequestCancelFunc;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
