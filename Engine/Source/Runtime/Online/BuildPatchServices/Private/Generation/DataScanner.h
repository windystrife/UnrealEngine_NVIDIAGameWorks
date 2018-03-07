// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Generation/CloudEnumeration.h"

namespace BuildPatchServices
{
	struct FChunkMatch
	{
		FChunkMatch(const uint64& InDataOffset, const FGuid& InChunkGuid)
			: DataOffset(InDataOffset)
			, ChunkGuid(InChunkGuid)
		{}

		// Offset into provided data.
		uint64 DataOffset;
		// The chunk matched.
		FGuid ChunkGuid;
	};

	class IDataScanner
	{
	public:
		virtual bool IsComplete() = 0;
		virtual TArray<FChunkMatch> GetResultWhenComplete() = 0;
	};

	typedef TSharedRef<IDataScanner, ESPMode::ThreadSafe> IDataScannerRef;
	typedef TSharedPtr<IDataScanner, ESPMode::ThreadSafe> IDataScannerPtr;

	class FDataScannerCounter
	{
	public:
		static int32 GetNumIncompleteScanners();
		static int32 GetNumRunningScanners();
	};

	class FDataScannerFactory
	{
	public:
		static IDataScannerRef Create(const TArray<uint8>& Data, const ICloudEnumerationRef& CloudEnumeration, const FStatsCollectorRef& StatsCollector);
	};
}
