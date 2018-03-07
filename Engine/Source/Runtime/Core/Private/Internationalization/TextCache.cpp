// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextCache.h"
#include "Misc/ScopeLock.h"

FTextCache& FTextCache::Get()
{
	static FTextCache Instance;
	return Instance;
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	FCacheKey CacheKey = FCacheKey::MakeReference(InNamespace, InKey);

	// First try and find a cached instance
	{
		FScopeLock Lock(&CachedTextCS);
	
		const FText* FoundText = CachedText.Find(CacheKey);
		if (FoundText)
		{
			const FString* FoundTextLiteral = FTextInspector::GetSourceString(*FoundText);
			if (FoundTextLiteral && FCString::Strcmp(**FoundTextLiteral, InTextLiteral) == 0)
			{
				return *FoundText;
			}
		}
	}

	// Not currently cached, make a new instance...
	FText NewText = FText(InTextLiteral, InNamespace, InKey, ETextFlag::Immutable);

	// ... and add it to the cache
	{
		CacheKey.Persist(); // Persist the key as we'll be adding it to the map

		FScopeLock Lock(&CachedTextCS);

		CachedText.Add(CacheKey, NewText);
	}

	return NewText;
}

void FTextCache::Flush()
{
	FScopeLock Lock(&CachedTextCS);

	CachedText.Empty();
}
