// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/CompositeFont.h"

class FFreeTypeFace;
class FFreeTypeLibrary;
struct FSlateFontInfo;

/**
 * Cached data for a given typeface
 */
class FCachedTypefaceData
{
public:
	/** Default constructor */
	FCachedTypefaceData();

	/** Construct the cache from the given typeface */
	FCachedTypefaceData(const FTypeface& InTypeface, const float InScalingFactor = 1.0f);

	/** Get the typeface we cached data from */
	FORCEINLINE const FTypeface& GetTypeface() const
	{
		check(Typeface);
		return *Typeface;
	}

	/** Find the font associated with the given name */
	const FFontData* GetFontData(const FName& InName) const;

	/** Get the scaling factor for this typeface */
	FORCEINLINE float GetScalingFactor() const
	{
		return ScalingFactor;
	}

	/** Get all the font data cached by this entry */
	void GetCachedFontData(TArray<const FFontData*>& OutFontData) const;

private:
	/** Typeface we cached data from */
	const FTypeface* Typeface;

	/** Singular entry, used when we don't have enough data to warrant using a map */
	const FFontData* SingularFontData;

	/** Mapping between a font name, and its data */
	TMap<FName, const FFontData*> NameToFontDataMap;

	/** Scaling factor to apply to this typeface */
	float ScalingFactor;
};


/**
 * Cached data for a given composite font
 */
class FCachedCompositeFontData
{
public:
	/** Default constructor */
	FCachedCompositeFontData();

	/** Construct the cache from the given composite font */
	FCachedCompositeFontData(const FCompositeFont& InCompositeFont);

	/** Get the composite font we cached data from */
	FORCEINLINE const FCompositeFont& GetCompositeFont() const
	{
		check(CompositeFont);
		return *CompositeFont;
	}

	/** Get the default typeface for this composite font */
	FORCEINLINE const FCachedTypefaceData* GetDefaultTypeface() const
	{
		return CachedTypefaces[0].Get();
	}

	/** Get the typeface that should be used for the given character */
	const FCachedTypefaceData* GetTypefaceForCharacter(const TCHAR InChar) const;

	/** Get all the font data cached by this entry */
	void GetCachedFontData(TArray<const FFontData*>& OutFontData) const;

private:
	/** Entry containing a range and the typeface associated with that range */
	struct FCachedFontRange
	{
		/** Default constructor */
		FCachedFontRange()
			: Range(FInt32Range::Empty())
			, CachedTypeface()
		{
		}

		/** Construct from the given range and typeface */
		FCachedFontRange(const FInt32Range& InRange, TSharedPtr<FCachedTypefaceData> InCachedTypeface)
			: Range(InRange)
			, CachedTypeface(MoveTemp(InCachedTypeface))
		{
		}

		/** Range to use for the typeface */
		FInt32Range Range;

		/** Typeface to which the range applies */
		TSharedPtr<FCachedTypefaceData> CachedTypeface;
	};

	/** Composite font we cached data from */
	const FCompositeFont* CompositeFont;

	/** Array of cached typefaces - 0 is the default typeface, and the remaining entries are sub-typefaces */
	TArray<TSharedPtr<FCachedTypefaceData>> CachedTypefaces;

	/** Array of font ranges paired with their associated typefaces - this is sorted in ascending order */
	TArray<FCachedFontRange> CachedFontRanges;
};


/**
 * High-level caching of composite fonts and FreeType font faces
 */
class FCompositeFontCache
{
public:
	/** Constructor */
	FCompositeFontCache(const FFreeTypeLibrary* InFTLibrary);

	/** Get the default font data to use for the given font info */
	const FFontData& GetDefaultFontData(const FSlateFontInfo& InFontInfo);

	/** Get the font data to use for the given font info and character */
	const FFontData& GetFontDataForCharacter(const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor);

	/** Gets or loads a FreeType font face */
	TSharedPtr<FFreeTypeFace> GetFontFace(const FFontData& InFontData);

	/** Get the attributes associated with the given font data */
	const TSet<FName>& GetFontAttributes(const FFontData& InFontData);

	/** Flush a single composite font entry from this cache */
	void FlushCompositeFont(const FCompositeFont& InCompositeFont);

	/** Flush this cache */
	void FlushCache();

private:
	/** Get the cached composite font data for the given composite font */
	const FCachedCompositeFontData* GetCachedCompositeFont(const FCompositeFont* const InCompositeFont);

	/** Get the default typeface for the given composite font */
	FORCEINLINE const FCachedTypefaceData* GetDefaultCachedTypeface(const FCompositeFont* const InCompositeFont)
	{
		const FCachedCompositeFontData* const CachedCompositeFont = GetCachedCompositeFont(InCompositeFont);
		return (CachedCompositeFont) ? CachedCompositeFont->GetDefaultTypeface() : nullptr;
	}

	/** Get the typeface that should be used for the given character */
	FORCEINLINE const FCachedTypefaceData* GetCachedTypefaceForCharacter(const FCompositeFont* const InCompositeFont, const TCHAR InChar)
	{
		const FCachedCompositeFontData* const CachedCompositeFont = GetCachedCompositeFont(InCompositeFont);
		return (CachedCompositeFont) ? CachedCompositeFont->GetTypefaceForCharacter(InChar) : nullptr;
	}

	/** Try and find some font data that best matches the given attributes */
	const FFontData* GetBestMatchFontForAttributes(const FCachedTypefaceData* const InCachedTypefaceData, const TSet<FName>& InFontAttributes);

	/** FreeType library instance to use */
	const FFreeTypeLibrary* FTLibrary;

	/** Mapping of composite fonts to their cached lookup data */
	TMap<const FCompositeFont*, TSharedPtr<FCachedCompositeFontData>> CompositeFontToCachedDataMap;

	/** Mapping of font data to FreeType faces */
	TMap<FFontData, TSharedPtr<FFreeTypeFace>> FontFaceMap;
};
