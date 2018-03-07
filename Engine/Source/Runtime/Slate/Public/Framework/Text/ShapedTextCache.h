// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/ShapedTextFwd.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/IRun.h"

class FSlateFontCache;

/** Information representing a piece of shaped text */
struct FCachedShapedTextKey
{
public:
	FCachedShapedTextKey(const FTextRange& InTextRange, const float InScale, const FShapedTextContext& InTextContext, const FSlateFontInfo& InFontInfo)
		: TextRange(InTextRange)
		, Scale(InScale)
		, TextContext(InTextContext)
		, FontInfo(InFontInfo)
	{
	}

	FORCEINLINE bool operator==(const FCachedShapedTextKey& Other) const
	{
		return TextRange == Other.TextRange 
			&& Scale == Other.Scale
			&& TextContext == Other.TextContext
			&& FontInfo == Other.FontInfo;
	}

	FORCEINLINE bool operator!=(const FCachedShapedTextKey& Other) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash(const FCachedShapedTextKey& Key)
	{
		uint32 KeyHash = 0;
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.TextRange));
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.Scale));
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.TextContext));
		KeyHash = HashCombine(KeyHash, GetTypeHash(Key.FontInfo));
		return KeyHash;
	}

	FTextRange TextRange;
	float Scale;
	FShapedTextContext TextContext;
	FSlateFontInfo FontInfo;
};

/** Cache of shaped text */
class SLATE_API FShapedTextCache
{
public:
	/** Create a new shaped text cache */
	static FShapedTextCacheRef Create(FSlateFontCache& InFontCache)
	{
		return MakeShareable(new FShapedTextCache(InFontCache));
	}

	/**
	 * Try and find an existing shaped text instance
	 *
	 * @param InKey					The key identifying the shaped text instance to find
	 *
	 * @return The shaped text instance, or null if it wasn't found or was stale
	 */
	FShapedGlyphSequencePtr FindShapedText(const FCachedShapedTextKey& InKey) const;

	/**
	 * Add the given shaped text instance to the cache, or generate a new instance and add that based on the parameters provided
	 *
	 * @param InKey					The key identifying the shaped text instance to add
	 * @param InText				The text to shape. InKey may specify a sub-section of the entire text
	 * @param InTextDirection		The text direction of all of the text to be shaped. If present we do a unidirectional shape, otherwise we do a bidirectional shape
	 * @param InShapedText			The shaped text instance to add
	 *
	 * @return The shaped text instance
	 */
	FShapedGlyphSequenceRef AddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText);
	FShapedGlyphSequenceRef AddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const TextBiDi::ETextDirection InTextDirection);
	FShapedGlyphSequenceRef AddShapedText(const FCachedShapedTextKey& InKey, FShapedGlyphSequenceRef InShapedText);

	/**
	 * Try and find an existing shaped text instance, or add a new entry to the cache if one cannot be found
	 *
	 * @param InKey					The key identifying the shaped text instance to find or add
	 * @param InText				The text to shape if we can't find the shaped text in the cache. InKey may specify a sub-section of the entire text
	 * @param InTextDirection		The text direction of all of the text to be shaped. If present we do a unidirectional shape, otherwise we do a bidirectional shape
	 *
	 * @return The shaped text instance
	 */
	FShapedGlyphSequenceRef FindOrAddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText);
	FShapedGlyphSequenceRef FindOrAddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const TextBiDi::ETextDirection InTextDirection);

	/**
	 * Clear this cache
	 */
	void Clear();

	/**
	 * Get the font cache used by this instance
	 */
	FSlateFontCache& GetFontCache() const
	{
		return FontCache;
	}

private:
	/** Constructor */
	FShapedTextCache(FSlateFontCache& InFontCache)
		: FontCache(InFontCache)
	{
	}

	/** Font cache to use when shaping text */
	FSlateFontCache& FontCache;

	/** Mapping between a cache key and the corresponding shaped text */
	TMap<FCachedShapedTextKey, FShapedGlyphSequencePtr> CachedShapedText;
};

/** Utility functions that can provide efficient caching of common operations */
namespace ShapedTextCacheUtil
{

/**
 * Measure a sub-section of a run of text
 *
 * @param InShapedTextCache		The shaped text cache to use
 * @param InRunKey				The key identifying the cached shaped text for the run
 * @param InMeasureRange		The range of text that should be measured
 * @param InText				The text to shape if we can't find the shaped text in the cache. InMeasureRange may specify a sub-section of the entire text
 *
 * @return The measured size of the shaped text
 */
SLATE_API FVector2D MeasureShapedText(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const FTextRange& InMeasureRange, const TCHAR* InText);

/**
 * Find the character index at the specified position in pixels along the string horizontally
 *
 * @param InShapedTextCache		The shaped text cache to use
 * @param InRunKey				The key identifying the cached shaped text for the run
 * @param InTextRange			The range of text that should be extracted into its own shaped glyph sequence
 * @param InText				The text to shape if we can't find the shaped text in the cache. InTextRange may specify a sub-section of the entire text
 * @param InHorizontalOffset	The horizontal offset to get the character index for
 *
 * @return The index of the character closest to the specified horizontal offset
 */
SLATE_API int32 FindCharacterIndexAtOffset(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const FTextRange& InTextRange, const TCHAR* InText, const int32 InHorizontalOffset);

/**
 * Get the kerning between two shaped glyphs
 *
 * @param InShapedTextCache		The shaped text cache to use
 * @param InRunKey				The key identifying the cached shaped text for the run
 * @param InGlyphIndex			The index of the glyph to get the kerning for (will get it between the given glyph, and it's next glyph)
 * @param InText				The text to shape if we can't find the shaped text in the cache. (InGlyphIndex, InGlyphIndex+1) will be the range used
 *
 * @return The kerning
 */
SLATE_API int8 GetShapedGlyphKerning(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const int32 InGlyphIndex, const TCHAR* InText);

/**
 * Extract a sub-section of a run of text into its own shaped glyph sequence
 *
 * @param InShapedTextCache		The shaped text cache to use
 * @param InRunKey				The key identifying the cached shaped text for the run
 * @param InTextRange			The range of text that should be extracted into its own shaped glyph sequence
 * @param InText				The text to shape if we can't find the shaped text in the cache. InTextRange may specify a sub-section of the entire text
 * @param InTextDirection		The text direction of all of the text to be shaped
 *
 * @return The shaped text
 */
SLATE_API FShapedGlyphSequenceRef GetShapedTextSubSequence(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const FTextRange& InTextRange, const TCHAR* InText, const TextBiDi::ETextDirection InTextDirection);

}
