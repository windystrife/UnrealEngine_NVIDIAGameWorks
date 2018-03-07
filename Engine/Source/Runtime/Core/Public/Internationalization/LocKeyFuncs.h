// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Set.h"
#include "Containers/Map.h"

/** Case sensitive hashing function for TSet */
struct FLocKeySetFuncs : BaseKeyFuncs<FString, FString>
{
	static FORCEINLINE const FString& GetSetKey(const FString& Element)
	{
		return Element;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

/** Case sensitive hashing function for TMap */
template <typename ValueType>
struct FLocKeyMapFuncs : BaseKeyFuncs<ValueType, FString, /*bInAllowDuplicateKeys*/false>
{
	static FORCEINLINE const FString& GetSetKey(const TPair<FString, ValueType>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

/** Case sensitive hashing function for TMultiMap */
template <typename ValueType>
struct FLocKeyMultiMapFuncs : BaseKeyFuncs<ValueType, FString, /*bInAllowDuplicateKeys*/true>
{
	static FORCEINLINE const FString& GetSetKey(const TPair<FString, ValueType>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return A.Equals(B, ESearchCase::CaseSensitive);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};
