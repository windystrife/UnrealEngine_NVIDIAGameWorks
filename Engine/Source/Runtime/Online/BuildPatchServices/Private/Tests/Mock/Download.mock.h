// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/DownloadService.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockDownload
		: public IDownload
	{
	public:
		virtual bool WasSuccessful() const override
		{
			return bSuccess;
		}

		virtual int32 GetResponseCode() const override
		{
			return ResponseCode;
		}

		virtual const TArray<uint8>& GetData() const override
		{
			return Data;
		}

	public:
		bool bSuccess;
		int32 ResponseCode;
		TArray<uint8> Data;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
