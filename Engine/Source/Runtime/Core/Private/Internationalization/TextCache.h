// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Internationalization/Text.h"

/** Caches FText instances generated via the LOCTEXT macro to avoid repeated constructions */
class FTextCache
{
public:
	/** 
	 * Get the singleton instance of the text cache.
	 */
	static FTextCache& Get();

	/**
	 * Try and find an existing cached entry for the given data, or construct and cache a new entry if one cannot be found.
	 */
	FText FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey);

	/**
	 * Flush all the instances currently stored in this cache and free any allocated data.
	 */
	void Flush();

private:
	/** The key used to identify an FText instance */
	struct FCacheKey
	{
	public:
		/**
		 * Make a key that references the given strings.
		 * This key can only be used while the given values exist, but will be faster than copying the strings.
		 */
		static FCacheKey MakeReference(const TCHAR* InNamespace, const TCHAR* InKey)
		{
			return FCacheKey(InNamespace, InKey);
		}

		/**
		 * Make a key that copies the given strings.
		 * This key can be stored in the map and safely used again later.
		 */
		static FCacheKey MakePersistent(FString InNamespace, FString InKey)
		{
			return FCacheKey(MoveTemp(InNamespace), MoveTemp(InKey));
		}

		/**
		 * Ensure that the current key is persistent and can be safely used with the map.
		 */
		void Persist()
		{
			if (KeyType == EKeyType::Reference)
			{
				PersistentData = FPersistentKeyData(FString(ReferenceData.Namespace), FString(ReferenceData.Key));
				ReferenceData = FReferenceKeyData();
				KeyType = EKeyType::Persistent;
			}
		}

		FORCEINLINE const TCHAR* GetNamespace() const
		{
			return (KeyType == EKeyType::Reference) ? ReferenceData.Namespace : *PersistentData.Namespace;
		}

		FORCEINLINE const TCHAR* GetKey() const
		{
			return (KeyType == EKeyType::Reference) ? ReferenceData.Key : *PersistentData.Key;
		}

		FORCEINLINE bool operator==(const FCacheKey& Other) const
		{
			return FCString::Strcmp(GetNamespace(), Other.GetNamespace()) == 0
				&& FCString::Strcmp(GetKey(), Other.GetKey()) == 0;
		}

		FORCEINLINE bool operator!=(const FCacheKey& Other) const
		{
			return !(*this == Other);
		}

		friend inline uint32 GetTypeHash(const FCacheKey& Key)
		{
			return Key.KeyHash;
		}

	private:
		enum class EKeyType : uint8
		{
			Reference,
			Persistent,
		};

		struct FReferenceKeyData
		{
			FReferenceKeyData()
				: Namespace(nullptr)
				, Key(nullptr)
			{
			}

			FReferenceKeyData(const TCHAR* InNamespace, const TCHAR* InKey)
				: Namespace(InNamespace)
				, Key(InKey)
			{
			}

			const TCHAR* Namespace;
			const TCHAR* Key;
		};

		struct FPersistentKeyData
		{
			FPersistentKeyData()
				: Namespace()
				, Key()
			{
			}

			FPersistentKeyData(FString&& InNamespace, FString&& InKey)
				: Namespace(MoveTemp(InNamespace))
				, Key(MoveTemp(InKey))
			{
			}

			FString Namespace;
			FString Key;
		};

		FCacheKey(const TCHAR* InNamespace, const TCHAR* InKey)
			: ReferenceData(InNamespace, InKey)
			, PersistentData()
			, KeyType(EKeyType::Reference)
			, KeyHash(0)
		{
			HashKey();
		}

		FCacheKey(FString&& InNamespace, FString&& InKey)
			: ReferenceData()
			, PersistentData(MoveTemp(InNamespace), MoveTemp(InKey))
			, KeyType(EKeyType::Persistent)
			, KeyHash(0)
		{
			HashKey();
		}

		void HashKey()
		{
			// We use MemCrc32 rather than StrCrc32 as MemCrc32 is faster since we don't need to worry about 
			// hash differences between ANSI and WIDE strings (as we only deal with TCHAR strings).
			KeyHash = 0;
			{
				const TCHAR* NamespaceStr = GetNamespace();
				KeyHash = FCrc::MemCrc32(NamespaceStr, FCString::Strlen(NamespaceStr) * sizeof(TCHAR), KeyHash);
			}
			{
				const TCHAR* KeyStr = GetKey();
				KeyHash = FCrc::MemCrc32(KeyStr, FCString::Strlen(KeyStr) * sizeof(TCHAR), KeyHash);
			}
		}

		FReferenceKeyData ReferenceData;
		FPersistentKeyData PersistentData;
		EKeyType KeyType;
		uint32 KeyHash;
	};

	TMap<FCacheKey, FText> CachedText;
	FCriticalSection CachedTextCS;
};
