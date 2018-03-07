// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Misc/Guid.h"

/** Version information for compiled LocMeta (Localization MetaData Resource) and LocRes (Localization Resource) files */
struct CORE_API FTextLocalizationResourceVersion
{
	/**
	 * Magic number identifying a LocMeta file.
	 */
	static const FGuid LocMetaMagic;

	/**
	 * Magic number identifying a LocRes file.
	 * @note Legacy LocRes files will be missing this as it wasn't added until version 1.
	 */
	static const FGuid LocResMagic;

	/**
	 * Data versions for LocMeta files.
	 */
	enum class ELocMetaVersion : uint8
	{
		/** Initial format. */
		Initial = 0,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	/**
	 * Data versions for LocRes files.
	 */
	enum class ELocResVersion : uint8
	{
		/** Legacy format file - will be missing the magic number. */
		Legacy = 0,
		/** Compact format file - strings are stored in a LUT to avoid duplication. */
		Compact,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};
};
