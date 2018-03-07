// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Data/ChunkData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FFakeChunkDataAccess
		: public IChunkDataAccess
	{
	public:
		virtual void GetDataLock(const uint8** OutChunkData, const FChunkHeader** OutChunkHeader) const override
		{
			(*OutChunkData) = ChunkData;
			(*OutChunkHeader) = &ChunkHeader;
		}

		virtual void GetDataLock(uint8** OutChunkData, FChunkHeader** OutChunkHeader) override
		{
			(*OutChunkData) = ChunkData;
			(*OutChunkHeader) = &ChunkHeader;
		}

		virtual void ReleaseDataLock() const override
		{
		}

		const FGuid& GetGuid() const
		{
			return ChunkHeader.Guid;
		}

	public:
		FChunkHeader ChunkHeader;
		uint8* ChunkData;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
