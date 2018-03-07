// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"

/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats
{
	void Add( const TCHAR* StatDescription, const SIZE_T StatValue )
	{
		Data.Add( FString(StatDescription), StatValue );
	}

	TMap<FString, SIZE_T> Data;
};
