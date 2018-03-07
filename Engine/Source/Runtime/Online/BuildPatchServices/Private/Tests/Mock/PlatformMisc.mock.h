// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FMockPlatformMisc
	{
	public:
		static uint32 GetLastError()
		{
			return 0;
		}
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
