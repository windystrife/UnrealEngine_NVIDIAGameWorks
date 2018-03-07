// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DownloadService.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockDownloadServiceStat
		: public IDownloadServiceStat
	{
	public:
		typedef TTuple<double, FDownloadRecord> FDownloadComplete;

	public:
		virtual void OnDownloadComplete(FDownloadRecord DownloadRecord) override
		{
			RxDownloadComplete.Emplace(FStatsCollector::GetSeconds(), DownloadRecord);
		}

	public:
		TArray<FDownloadComplete> RxDownloadComplete;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
