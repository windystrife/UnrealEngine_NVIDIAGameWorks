// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/ShapedTextFwd.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Textures/TextureAtlas.h"
#include "Fonts/FontTypes.h"
#include "FontCache.generated.h"

class FCompositeFontCache;
class FFreeTypeAdvanceCache;
class FFreeTypeFace;
class FFreeTypeGlyphCache;
class FFreeTypeKerningPairCache;
class FFreeTypeLibrary;
class FShapedGlyphFaceData;
class FSlateFontCache;
class FSlateFontRenderer;
class FSlateShaderResource;
class FSlateTextShaper;

enum class EFontCacheAtlasDataType : uint8
{
	/** Data was cached for a regular non-outline font */
	Regular = 0,

	/** Data was cached for a outline (stroked) font */
	Outline,

	/** Must be last */
	Num,
};


/** 
 * Methods that can be used to shape text.
 * @note If you change this enum, make sure and update CVarDefaultTextShapingMethod and GetDefaultTextShapingMethod.
 */
UENUM(BlueprintType)
enum class ETextShapingMethod : uint8
{
	/**
	 * Automatically picks the fastest possible shaping method (either KerningOnly or FullShaping) based on the reading direction of the text.
	 * Left-to-right text uses the KerningOnly method, and right-to-left text uses the FullShaping method.
	 */
	 Auto = 0,

	/** 
	 * Provides fake shaping using only kerning data.
	 * This can be faster than full shaping, but won't render complex right-to-left or bi-directional glyphs (such as Arabic) correctly.
	 * This can be useful as an optimization when you know your text block will only show simple glyphs (such as numbers).
	 */
	KerningOnly,

	/**
	 * Provides full text shaping, allowing accurate rendering of complex right-to-left or bi-directional glyphs (such as Arabic).
	 * This mode will perform ligature replacement for all languages (such as the combined "fi" glyph in English).
	 */
	FullShaping,
};

/** Get the default shaping method (from the "Slate.DefaultTextShapingMethod" CVar) */
SLATECORE_API ETextShapingMethod GetDefaultTextShapingMethod();

/** The font atlas data for a single glyph in a shaped text sequence */
struct FShapedGlyphFontAtlasData
{
	/** The vertical distance from the baseline to the topmost border of the glyph bitmap */
	int16 VerticalOffset;
	/** The horizontal distance from the origin to the leftmost border of the glyph bitmap */
	int16 HorizontalOffset;
	/** Start X location of the glyph in the texture */
	uint16 StartU;
	/** Start Y location of the glyph in the texture */
	uint16 StartV;
	/** X Size of the glyph in the texture */
	uint16 USize;
	/** Y Size of the glyph in the texture */
	uint16 VSize;
	/** Index to a specific texture in the font cache. */
	uint8 TextureIndex;
	/** True if this entry is valid, false otherwise. */
	bool Valid;

	FShapedGlyphFontAtlasData()
		: VerticalOffset(0)
		, HorizontalOffset(0)
		, StartU(0.0f)
		, StartV(0.0f)
		, USize(0.0f)
		, VSize(0.0f)
		, TextureIndex(0)
		, Valid(false)
	{
	}
};

/** Information for rendering one glyph in a shaped text sequence */
struct FShapedGlyphEntry
{
	friend class FSlateFontCache;

	/** Provides access to the FreeType face for this glyph (not available publicly) */
	TSharedPtr<FShapedGlyphFaceData> FontFaceData;
	/** The index of this glyph in the FreeType face */
	uint32 GlyphIndex;
	/** The index of this glyph from the source text. The source indices may skip characters if the sequence contains ligatures, additionally, some characters produce multiple glyphs leading to duplicate source indices */
	int32 SourceIndex;
	/** The amount to advance in X before drawing the next glyph in the sequence */
	int16 XAdvance;
	/** The amount to advance in Y before drawing the next glyph in the sequence */
	int16 YAdvance;
	/** The offset to apply in X when drawing this glyph */
	int16 XOffset;
	/** The offset to apply in Y when drawing this glyph */
	int16 YOffset;
	/** 
	 * The "kerning" between this glyph and the next one in the sequence
	 * @note This value is included in the XAdvance so you never usually need it unless you're manually combining two sets of glyphs together.
	 * @note This value isn't strictly the kerning value - it's simply the difference between the glyphs horizontal advance, and the shaped horizontal advance (so will contain any accumulated advance added by the shaper)
	 */
	int8 Kerning;
	/**
	 * The number of source characters represented by this glyph
	 * This is typically 1, however will be greater for ligatures, or may be 0 if a single character produces multiple glyphs
	 */
	uint8 NumCharactersInGlyph;
	/**
	 * The number of source grapheme clusters represented by this glyph
	 * This is typically 1, however will be greater for ligatures, or may be 0 if a single character produces multiple glyphs
	 */
	uint8 NumGraphemeClustersInGlyph;
	/**
	 * The reading direction of the text this glyph was shaped from
	 */
	TextBiDi::ETextDirection TextDirection;
	/**
	 * True if this is a visible glyph that should be drawn.
	 * False if the glyph is invisible (eg, whitespace or a control code) and should skip drawing, but still include its advance amount.
	 */
	bool bIsVisible;

	FShapedGlyphEntry()
		: FontFaceData()
		, GlyphIndex(0)
		, SourceIndex(0)
		, XAdvance(0)
		, YAdvance(0)
		, XOffset(0)
		, YOffset(0)
		, Kerning(0)
		, NumCharactersInGlyph(0)
		, NumGraphemeClustersInGlyph(0)
		, TextDirection(TextBiDi::ETextDirection::LeftToRight)
		, bIsVisible(false)
	{
	}

private:
	/** 
	 * Pointer to the cached atlas data for this glyph entry.
	 * This is cached on the glyph by FSlateFontCache::GetShapedGlyphFontAtlasData to avoid repeated map look ups.
	 * First index is to determine if this is a cached outline glyph or a regular glyph
	 * Second index is the index of the thread dependent font cache. Index 0 is the cached value for the game thread font cache. Index 1 is the cached value for the render thread font cache.
	 */
	mutable TWeakPtr<FShapedGlyphFontAtlasData> CachedAtlasData[(uint8)EFontCacheAtlasDataType::Num][2];
};

/** Minimal FShapedGlyphEntry key information used for map lookups */
struct FShapedGlyphEntryKey
{
public:
	FShapedGlyphEntryKey(const FShapedGlyphFaceData& InFontFaceData, uint32 InGlyphIndex, const FFontOutlineSettings& InOutlineSettings);

	FORCEINLINE bool operator==(const FShapedGlyphEntryKey& Other) const
	{
		return FontFace == Other.FontFace 
			&& FontSize == Other.FontSize
			&& OutlineSize == Other.OutlineSize
			&& OutlineSizeSeparateFillAlpha == Other.OutlineSizeSeparateFillAlpha
			&& FontScale == Other.FontScale
			&& GlyphIndex == Other.GlyphIndex;
	}

	FORCEINLINE bool operator!=(const FShapedGlyphEntryKey& Other) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash(const FShapedGlyphEntryKey& Key)
	{
		return Key.KeyHash;
	}

private:
	/** Weak pointer to the FreeType face to render with */
	TWeakPtr<FFreeTypeFace> FontFace;
	/** Provides the point size used to render the font */
	int32 FontSize;
	/** The size in pixels of the outline to render for the font */
	float OutlineSize;
	/** If checked, the outline will be completely translucent where the filled area will be. @see FFontOutlineSettings */
	bool OutlineSizeSeparateFillAlpha;
	/** Provides the final scale used to render to the font */
	float FontScale;
	/** The index of this glyph in the FreeType face */
	uint32 GlyphIndex;
	/** Cached hash value used for map lookups */
	uint32 KeyHash;
};

/** Information for rendering a shaped text sequence */
class SLATECORE_API FShapedGlyphSequence
{
public:
	struct FSourceTextRange
	{
		FSourceTextRange(const int32 InTextStart, const int32 InTextLen)
			: TextStart(InTextStart)
			, TextLen(InTextLen)
		{
		}

		int32 TextStart;
		int32 TextLen;
	};

	FShapedGlyphSequence(TArray<FShapedGlyphEntry> InGlyphsToRender, const int16 InTextBaseline, const uint16 InMaxTextHeight, const UObject* InFontMaterial, const FFontOutlineSettings& InOutlineSettings, const FSourceTextRange& InSourceTextRange);
	~FShapedGlyphSequence();

	/** Get the amount of memory allocated to this sequence */
	uint32 GetAllocatedSize() const;

	/** Get the array of glyphs in this sequence. This data will be ordered so that you can iterate and draw left-to-right, which means it will be backwards for right-to-left languages */
	const TArray<FShapedGlyphEntry>& GetGlyphsToRender() const
	{
		return GlyphsToRender;
	}

	/** Get the baseline to use when drawing the glyphs in this sequence */
	int16 GetTextBaseline() const
	{
		return TextBaseline;
	}

	/** Get the maximum height of any glyph in the font we're using */
	uint16 GetMaxTextHeight() const
	{
		return MaxTextHeight;
	}

	/** Get the material to use when rendering these glyphs */
	const UObject* GetFontMaterial() const
	{
		return FontMaterial;
	}

	/** Get the font outline settings to use when rendering these glyphs */
	const FFontOutlineSettings& GetFontOutlineSettings() const
	{
		return OutlineSettings;
	}

	/** Check to see whether this glyph sequence is dirty (ie, contains glyphs with invalid font pointers) */
	bool IsDirty() const;

	/**
	 * Get the measured width of the entire shaped text
	 * @return The measured width
	 */
	int32 GetMeasuredWidth() const;

	/**
	 * Get the measured width of the specified range of this shaped text
	 * @note The indices used here are relative to the start of the text we were shaped from, even if we were only shaped from a sub-section of that text
	 * @return The measured width, or an unset value if the text couldn't be measured (eg, because you started or ended on a merged ligature, or because the range is out-of-bounds)
	 */
	TOptional<int32> GetMeasuredWidth(const int32 InStartIndex, const int32 InEndIndex, const bool InIncludeKerningWithPrecedingGlyph = true) const;

	/** Return data used by GetGlyphAtOffset */
	struct FGlyphOffsetResult
	{
		FGlyphOffsetResult()
			: Glyph(nullptr)
			, GlyphOffset(0)
			, CharacterIndex(0)
		{
		}

		explicit FGlyphOffsetResult(const int32 InCharacterIndex)
			: Glyph(nullptr)
			, GlyphOffset(0)
			, CharacterIndex(InCharacterIndex)
		{
		}

		FGlyphOffsetResult(const FShapedGlyphEntry* InGlyph, const int32 InGlyphOffset)
			: Glyph(InGlyph)
			, GlyphOffset(InGlyphOffset)
			, CharacterIndex(InGlyph->SourceIndex)
		{
		}

		/** The glyph that was hit. May be null if we hit outside the range of any glyph */
		const FShapedGlyphEntry* Glyph;
		/** The offset to the left edge of the hit glyph */
		int32 GlyphOffset;
		/** The character index that was hit (set to the start or end index if we fail to hit a glyph) */
		int32 CharacterIndex;
	};

	/**
	 * Get the information for the glyph at the specified position in pixels along the string horizontally
	 * @return The result data (see FGlyphOffsetResult)
	 */
	FGlyphOffsetResult GetGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InHorizontalOffset, const int32 InStartOffset = 0) const;

	/**
	 * Get the information for the glyph at the specified position in pixels along the string horizontally
	 * @note The indices used here are relative to the start of the text we were shaped from, even if we were only shaped from a sub-section of that text
	 * @return The result data (see FGlyphOffsetResult), or an unset value if we couldn't find the character (eg, because you started or ended on a merged ligature, or because the range is out-of-bounds)
	 */
	TOptional<FGlyphOffsetResult> GetGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InStartIndex, const int32 InEndIndex, const int32 InHorizontalOffset, const int32 InStartOffset = 0, const bool InIncludeKerningWithPrecedingGlyph = true) const;

	/**
	 * Get the kerning value between the given entry and the next entry in the sequence
	 * @note The index used here is relative to the start of the text we were shaped from, even if we were only shaped from a sub-section of that text
	 * @return The kerning, or an unset value if we couldn't get the kerning (eg, because you specified a merged ligature, or because the index is out-of-bounds)
	 */
	TOptional<int8> GetKerning(const int32 InIndex) const;

	/**
	 * Get a sub-sequence of the specified range
	 * @note The indices used here are relative to the start of the text we were shaped from, even if we were only shaped from a sub-section of that text
	 * @return The sub-sequence, or an null if the sub-sequence couldn't be created (eg, because you started or ended on a merged ligature, or because the range is out-of-bounds)
	 */
	FShapedGlyphSequencePtr GetSubSequence(const int32 InStartIndex, const int32 InEndIndex) const;

private:
	/** Non-copyable */
	FShapedGlyphSequence(const FShapedGlyphSequence&);
	FShapedGlyphSequence& operator=(const FShapedGlyphSequence&);

	/** Helper function to share some common logic between the bound and unbound GetGlyphAtOffset functions */
	bool HasFoundGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InHorizontalOffset, const FShapedGlyphEntry& InCurrentGlyph, const int32 InCurrentGlyphIndex, int32& InOutCurrentOffset, const FShapedGlyphEntry*& OutMatchedGlyph) const;

	/**
	 * Enumerate all of the glyphs within the given source index range (enumerates either visually or logically)
	 * @note The indices used here are relative to the start of the text we were shaped from, even if we were only shaped from a sub-section of that text
	 * @return EnumerationComplete if we found the start and end point and enumerated the glyphs, EnumerationAborted if the callback returned false, or EnumerationFailed (eg, because you started or ended on a merged ligature, or because the range is out-of-bounds)
	 */
	enum class EEnumerateGlyphsResult : uint8 { EnumerationFailed, EnumerationAborted, EnumerationComplete };
	typedef TFunctionRef<bool(const FShapedGlyphEntry&, int32)> FForEachShapedGlyphEntryCallback;
	EEnumerateGlyphsResult EnumerateLogicalGlyphsInSourceRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const;
	EEnumerateGlyphsResult EnumerateVisualGlyphsInSourceRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const;

	/** Contains the information needed when performing a reverse look-up from a source index to the corresponding shaped glyph */
	struct FSourceIndexToGlyphData
	{
		FSourceIndexToGlyphData()
			: GlyphIndex(INDEX_NONE)
			, AdditionalGlyphIndices()
		{
		}

		explicit FSourceIndexToGlyphData(const int32 InGlyphIndex)
			: GlyphIndex(InGlyphIndex)
			, AdditionalGlyphIndices()
		{
		}

		bool IsValid() const
		{
			return GlyphIndex != INDEX_NONE;
		}

		int32 GetLowestGlyphIndex() const
		{
			return GlyphIndex;
		};

		int32 GetHighestGlyphIndex() const
		{
			return (AdditionalGlyphIndices.Num() > 0) ? AdditionalGlyphIndices.Last() : GlyphIndex;
		}

		int32 GlyphIndex;
		TArray<int32> AdditionalGlyphIndices;
	};

	/** A map of source indices to their shaped glyph data indices. Stored internally as an array so we can perform a single allocation */
	struct FSourceIndicesToGlyphData
	{
	public:
		explicit FSourceIndicesToGlyphData(const FSourceTextRange& InSourceTextRange)
			: SourceTextRange(InSourceTextRange)
			, GlyphDataArray()
		{
			GlyphDataArray.SetNum(InSourceTextRange.TextLen);
		}

		FORCEINLINE int32 GetSourceTextStartIndex() const
		{
			return SourceTextRange.TextStart;
		}

		FORCEINLINE int32 GetSourceTextEndIndex() const
		{
			return SourceTextRange.TextStart + SourceTextRange.TextLen;
		}

		FORCEINLINE FSourceIndexToGlyphData* GetGlyphData(const int32 InSourceTextIndex)
		{
			const int32 InternalIndex = InSourceTextIndex - SourceTextRange.TextStart;
			return (GlyphDataArray.IsValidIndex(InternalIndex)) ? &GlyphDataArray[InternalIndex] : nullptr;
		}

		FORCEINLINE const FSourceIndexToGlyphData* GetGlyphData(const int32 InSourceTextIndex) const
		{
			const int32 InternalIndex = InSourceTextIndex - SourceTextRange.TextStart;
			return (GlyphDataArray.IsValidIndex(InternalIndex)) ? &GlyphDataArray[InternalIndex] : nullptr;
		}

		FORCEINLINE uint32 GetAllocatedSize() const
		{
			return GlyphDataArray.GetAllocatedSize();
		}

	private:
		FSourceTextRange SourceTextRange;
		TArray<FSourceIndexToGlyphData> GlyphDataArray;
	};

	/** Array of glyphs in this sequence. This data will be ordered so that you can iterate and draw left-to-right, which means it will be backwards for right-to-left languages */
	TArray<FShapedGlyphEntry> GlyphsToRender;
	/** The baseline to use when drawing the glyphs in this sequence */
	int16 TextBaseline;
	/** The maximum height of any glyph in the font we're using */
	uint16 MaxTextHeight;
	/** The material to use when rendering these glyphs */
	const UObject* FontMaterial;
	/** Outline settings to use when rendering these glyphs */
	FFontOutlineSettings OutlineSettings;
	/** The cached width of the entire sequence */
	int32 SequenceWidth;
	/** The set of fonts being used by the glyphs within this sequence */
	TArray<TWeakPtr<FFreeTypeFace>> GlyphFontFaces;
	/** A map of source indices to their shaped glyph data indices - used to perform efficient reverse look-up */
	FSourceIndicesToGlyphData SourceIndicesToGlyphData;
};

/** Information for rendering one non-shaped character */
struct SLATECORE_API FCharacterEntry
{
	/** The character this entry is for */
	TCHAR Character;
	/** The index of the glyph from the FreeType face that this entry is for */
	uint32 GlyphIndex;
	/** The raw font data this character was rendered with */
	const FFontData* FontData;
	/** Scale that was applied when rendering this character */
	float FontScale;
	/** Start X location of the character in the texture */
	uint16 StartU;
	/** Start Y location of the character in the texture */
	uint16 StartV;
	/** X Size of the character in the texture */
	uint16 USize;
	/** Y Size of the character in the texture */
	uint16 VSize;
	/** The vertical distance from the baseline to the topmost border of the character */
	int16 VerticalOffset;
	/** The vertical distance from the origin to the left most border of the character */
	int16 HorizontalOffset;
	/** The largest vertical distance below the baseline for any character in the font */
	int16 GlobalDescender;
	/** The amount to advance in X before drawing the next character in a string */
	int16 XAdvance;
	/** Index to a specific texture in the font cache. */
	uint8 TextureIndex;
	/** 1 if this entry has kerning, 0 otherwise. */
	bool HasKerning;
	/** The fallback level this character represents */
	EFontFallback FallbackLevel;
	/** 1 if this entry is valid, 0 otherwise. */
	bool Valid;

	FCharacterEntry()
	{
		FMemory::Memzero( this, sizeof(FCharacterEntry) );
	}
};

/**
 * Manages a potentially large list of non-shaped characters
 * Uses a directly indexed by TCHAR array until space runs out and then maps the rest to conserve memory
 * Every character indexed by TCHAR could potentially cost a lot of memory of a lot of empty entries are created
 * because characters being used are far apart
 */
class SLATECORE_API FCharacterList
{
public:
	FCharacterList( const FSlateFontKey& InFontKey, FSlateFontCache& InFontCache );

	/* @return Is the character in this list */
	bool IsValidIndex( TCHAR Character ) const
	{
		return DirectIndexEntries.IsValidIndex( Character ) || ( Character >= MaxDirectIndexedEntries && MappedEntries.Contains( Character ) );
	}

	/**
	 * Gets data about how to render and measure a character 
	 * Caching and atlasing it if needed
	 *
	 * @param Character			The character to get
	 * @param MaxFontFallback	The maximum fallback level that can be used when resolving glyphs
	 * @return				Data about the character
	 */
	FCharacterEntry GetCharacter(TCHAR Character, const EFontFallback MaxFontFallback);

#if WITH_EDITORONLY_DATA
	/** Check to see if our cached data is potentially stale for our font */
	bool IsStale() const;
#endif	// WITH_EDITORONLY_DATA

	/**
	 * Gets a kerning value for a pair of characters
	 *
	 * @param FirstChar			The first character in the pair
	 * @param SecondChar		The second character in the pair
	 * @param MaxFontFallback	The maximum fallback level that can be used when resolving glyphs
	 * @return The kerning value
	 */
	int8 GetKerning(TCHAR FirstChar, TCHAR SecondChar, const EFontFallback MaxFontFallback);

	/**
	 * Gets a kerning value for a pair of character entries
	 *
	 * @param FirstCharacterEntry	The first character entry in the pair
	 * @param SecondCharacterEntry	The second character entry in the pair
	 * @return The kerning value
	 */
	int8 GetKerning( const FCharacterEntry& FirstCharacterEntry, const FCharacterEntry& SecondCharacterEntry );

	/**
	 * @return The global max height for any character in this font
	 */
	uint16 GetMaxHeight() const;

	/** 
	 * Returns the baseline for the font used by this character 
	 *
	 * @return The offset from the bottom of the max character height to the baseline. Be aware that the value will be negative.
	 */
	int16 GetBaseline() const;

private:
	/** Maintains a fake shaped glyph for each character in the character list */
	struct FCharacterListEntry
	{
		FCharacterListEntry()
			: ShapedGlyphEntry()
			, FontData(nullptr)
			, FallbackLevel(EFontFallback::FF_Max)
			, HasKerning(false)
			, Valid(false)
		{
		}

		/** The shaped glyph data for this character */
		FShapedGlyphEntry ShapedGlyphEntry;
		/** Font data this character was rendered with */
		const FFontData* FontData;
		/** The fallback level this character represents */
		EFontFallback FallbackLevel;
		/** Does this character have kerning? */
		bool HasKerning;
		/** Has this entry been initialized? */
		bool Valid;
	};

	/**
	 * Returns whether the specified character is valid for caching (i.e. whether it matches the FontFallback level)
	 *
	 * @param Character			The character to check
	 * @param MaxFontFallback	The maximum fallback level that can be used when resolving glyphs
	 */
	bool CanCacheCharacter(TCHAR Character, const EFontFallback MaxFontFallback) const;

	/**
	 * Caches a new character
	 * 
	 * @param Character	The character to cache
	 */
	FCharacterListEntry CacheCharacter(TCHAR Character);

	/**
	 * Convert the cached internal entry to the external data for the old non-shaped API
	 */
	FCharacterEntry MakeCharacterEntry(TCHAR Character, const FCharacterListEntry& InternalEntry) const;

private:
	/** Entries for larger character sets to conserve memory */
	TMap<TCHAR, FCharacterListEntry> MappedEntries; 
	/** Directly indexed entries for fast lookup */
	TArray<FCharacterListEntry> DirectIndexEntries;
	/** Font for this character list */
	FSlateFontKey FontKey;
	/** Reference to the font cache for accessing new unseen characters */
	FSlateFontCache& FontCache;
#if WITH_EDITORONLY_DATA
	/** The history revision of the cached composite font */
	int32 CompositeFontHistoryRevision;
#endif	// WITH_EDITORONLY_DATA
	/** Number of directly indexed entries */
	int32 MaxDirectIndexedEntries;
	/** The global max height for any character in this font */
	mutable uint16 MaxHeight;
	/** The offset from the bottom of the max character height to the baseline. */
	mutable int16 Baseline;
};

/**
 * Font caching implementation
 * Caches characters into textures as needed
 */
class SLATECORE_API FSlateFontCache : public ISlateAtlasProvider
{
	friend FCharacterList;

public:
	/**
	 * Constructor
	 *
	 * @param InTextureSize The size of the atlas texture
	 * @param InFontAlas	Platform specific font atlas resource
	 */
	FSlateFontCache( TSharedRef<ISlateFontAtlasFactory> InFontAtlasFactory );
	virtual ~FSlateFontCache();

	/** ISlateAtlasProvider */
	virtual int32 GetNumAtlasPages() const override;
	virtual FIntPoint GetAtlasPageSize() const override;
	virtual FSlateShaderResource* GetAtlasPageResource(const int32 InIndex) const override;
	virtual bool IsAtlasPageResourceAlphaOnly() const override;

	/** 
	 * Performs text shaping on the given string using the given font info. Returns you the shaped text sequence to use for text rendering via FSlateDrawElement::MakeShapedText.
	 * When using the version which takes a start point and length, the text outside of the given range won't be shaped, but will provide context information to allow the shaping to function correctly.
	 * ShapeBidirectionalText is used when you have text that may contain a mixture of LTR and RTL text runs.
	 * 
	 * @param InText				The string to shape
	 * @param InTextStart			The start position of the text to shape
	 * @param InTextLen				The length of the text to shape
	 * @param InFontInfo			Information about the font that the string is drawn with
	 * @param InFontScale			The scale to apply to the font
	 * @param InBaseDirection		The overall reading direction of the text (see TextBiDi::ComputeBaseDirection). This will affect where some characters (such as brackets and quotes) are placed within the resultant shaped text
	 * @param InTextShapingMethod	The text shaping method to use
	 */
	FShapedGlyphSequenceRef ShapeBidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const;
	FShapedGlyphSequenceRef ShapeBidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const;

	/** 
	 * Performs text shaping on the given range of the string using the given font info. Returns you the shaped text sequence to use for text rendering via FSlateDrawElement::MakeShapedText.
	 * When using the version which takes a start point and length, the text outside of the given range won't be shaped, but will provide context information to allow the shaping to function correctly.
	 * ShapeUnidirectionalText is used when you have text that all reads in the same direction (either LTR or RTL).
	 * 
	 * @param InText				The string containing the sub-string to shape
	 * @param InTextStart			The start position of the text to shape
	 * @param InTextLen				The length of the text to shape
	 * @param InFontInfo			Information about the font that the string is drawn with
	 * @param InFontScale			The scale to apply to the font
	 * @param InTextDirection		The reading direction of the text to shape (valid values are LeftToRight or RightToLeft)
	 * @param InTextShapingMethod	The text shaping method to use
	 */
	FShapedGlyphSequenceRef ShapeUnidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const;
	FShapedGlyphSequenceRef ShapeUnidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const;

	/** 
	 * Gets information for how to draw all non-shaped characters in the specified string. Caches characters as they are found
	 * 
	 * @param InFontInfo		Information about the font that the string is drawn with
	 * @param FontScale			The scale to apply to the font
	 * @param OutCharacterEntries	Populated array of character entries. Indices of characters in Text match indices in this array
	 */
	class FCharacterList& GetCharacterList( const FSlateFontInfo &InFontInfo, float FontScale, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings::NoOutline);

	/**
	 * Get the atlas information for the given shaped glyph. This information will be cached if required 
	 */
	FShapedGlyphFontAtlasData GetShapedGlyphFontAtlasData( const FShapedGlyphEntry& InShapedGlyph, const FFontOutlineSettings& InOutlineSettings);

	/** 
	 * Add a new entries into a cache atlas
	 *
	 * @param InFontInfo	Information about the font being used for the characters
	 * @param Characters	The characters to cache
	 * @param FontScale		The font scale to use
	 * @return true if the characters could be cached. false if the cache is full
	 */
	bool AddNewEntry( const FShapedGlyphEntry& InShapedGlyph, const FFontOutlineSettings& InOutlineSettings, FShapedGlyphFontAtlasData& OutAtlasData );

	bool AddNewEntry( const FCharacterRenderData InRenderData, uint8& OutTextureIndex, uint16& OutGlyphX, uint16& OutGlyphY, uint16& OutGlyphWidth, uint16& OutGlyphHeight );

	/**
	 * Flush the given object out of the cache
	 */
	void FlushObject( const UObject* const InObject );

	/**
	 * Flush the given composite font out of the cache
	 */
	void FlushCompositeFont(const FCompositeFont& InCompositeFont);

	/** 
	 * Flush the cache if needed
	 */
	bool ConditionalFlushCache();

	/**
	 * Updates the texture used for rendering
	 */
	void UpdateCache();

	/**
	 * Releases rendering resources
	 */
	void ReleaseResources();

	/**
	 * Get the texture resource for a font atlas at a given index
	 * 
	 * @param Index	The index of the texture 
	 * @return Handle to the texture resource
	 */
	class FSlateShaderResource* GetSlateTextureResource( uint32 Index ) { return AllFontTextures[Index]->GetSlateTexture(); }
	class FTextureResource* GetEngineTextureResource( uint32 Index ) { return AllFontTextures[Index]->GetEngineTexture(); }

	/**
	 * Returns the font to use from the default typeface
	 *
	 * @param InFontInfo	A descriptor of the font to get the default typeface for
	 * 
	 * @return The raw font data
	 */
	const FFontData& GetDefaultFontData( const FSlateFontInfo& InFontInfo ) const;

	/**
	 * Returns the font to use from the typeface associated with the given character
	 *
	 * @param InFontInfo		A descriptor of the font to get the typeface for
	 * @param InChar			The character to get the typeface associated with
	 * @param OutScalingFactor	The scaling factor applied to characters rendered with the given font
	 * 
	 * @return The raw font data
	 */
	const FFontData& GetFontDataForCharacter( const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor ) const;

	/**
	 * Returns the height of the largest character in the font. 
	 *
	 * @param InFontInfo	A descriptor of the font to get character size for 
	 * @param FontScale		The scale to apply to the font
	 * 
	 * @return The largest character height
	 */
	uint16 GetMaxCharacterHeight( const FSlateFontInfo& InFontInfo, float FontScale ) const;

	/**
	 * Returns the baseline for the specified font.
	 *
	 * @param InFontInfo	A descriptor of the font to get character size for 
	 * @param FontScale		The scale to apply to the font
	 * 
	 * @return The offset from the bottom of the max character height to the baseline.
	 */
	int16 GetBaseline( const FSlateFontInfo& InFontInfo, float FontScale ) const;

	/**
	 * Get the underline metrics for the specified font.
	 *
	 * @param InFontInfo			A descriptor of the font to get character size for
	 * @param FontScale				The scale to apply to the font
	 * @param OutUnderlinePos		The offset from the baseline to the center of the underline bar
	 * @param OutUnderlineThickness	The thickness of the underline bar
	 */
	void GetUnderlineMetrics( const FSlateFontInfo& InFontInfo, const float FontScale, int16& OutUnderlinePos, int16& OutUnderlineThickness ) const;

	/**
	 * Calculates the kerning amount for a pair of characters
	 *
	 * @param InFontData	The font that used to draw the string with the first and second characters
	 * @param InSize		The size of the font to draw
	 * @param First			The first character in the pair
	 * @param Second		The second character in the pair
	 * @return The kerning amount, 0 if no kerning
	 */
	int8 GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, float Scale ) const;

	/**
	 * @return Whether or not the font used has kerning information
	 */
	bool HasKerning( const FFontData& InFontData ) const;

	/**
	 * Returns the font attributes for the specified font.
	 *
	 * @param InFontData	The font to get attributes for 
	 * 
	 * @return The font attributes for the specified font.
	 */
	const TSet<FName>& GetFontAttributes( const FFontData& InFontData ) const;

	/**
	 * Get the revision index of the currently active localized fallback font.
	 */
	uint16 GetLocalizedFallbackFontRevision() const;

	/**
	 * Issues a request to clear all cached data from the cache
	 */
	void RequestFlushCache();

	/**
	 * Clears just the cached font data, but leaves the atlases alone
	 */
	void FlushData();

private:
	// Non-copyable
	FSlateFontCache(const FSlateFontCache&);
	FSlateFontCache& operator=(const FSlateFontCache&);

	/**
	 * Clears all cached data from the cache
	 */
	void FlushCache();

	/**
	 * Clears out any pending UFont objects that were requested to be flushed
	 */
	void FlushFontObjects();

	/** Called after the active culture has changed */
	void HandleCultureChanged();

private:

	/** FreeType library instance (owned by this font cache) */
	TUniquePtr<FFreeTypeLibrary> FTLibrary;

	/** FreeType low-level glyph cache (owned by this font cache) */
	TUniquePtr<FFreeTypeGlyphCache> FTGlyphCache;

	/** FreeType low-level advance cache (owned by this font cache) */
	TUniquePtr<FFreeTypeAdvanceCache> FTAdvanceCache;

	/** FreeType low-level kerning pair cache (owned by this font cache) */
	TUniquePtr<FFreeTypeKerningPairCache> FTKerningPairCache;

	/** High-level composite font cache (owned by this font cache) */
	TUniquePtr<FCompositeFontCache> CompositeFontCache;

	/** FreeType font renderer (owned by this font cache) */
	TUniquePtr<FSlateFontRenderer> FontRenderer;

	/** HarfBuzz text shaper (owned by this font cache) */
	TUniquePtr<FSlateTextShaper> TextShaper;

	/** Mapping Font keys to cached data */
	TMap<FSlateFontKey, TSharedRef< class FCharacterList > > FontToCharacterListCache;

	/** Mapping shaped glyphs to their cached atlas data */
	TMap<FShapedGlyphEntryKey, TSharedRef<FShapedGlyphFontAtlasData>> ShapedGlyphToAtlasData;

	/** Array of all font atlases */
	TArray< TSharedRef<FSlateFontAtlas> > FontAtlases;

	/** Array of any non-atlased font textures */
	TArray< TSharedRef<ISlateFontTexture> > NonAtlasedTextures;

	/** Array of all font textures - both atlased and non-atlased */
	TArray< TSharedRef<ISlateFontTexture> > AllFontTextures;

	/** Factory for creating new font atlases */
	TSharedRef<ISlateFontAtlasFactory> FontAtlasFactory;

	/** Whether or not we have a pending request to flush the cache when it is safe to do so */
	volatile bool bFlushRequested;

	/** Number of atlas pages we can have before we request that the cache be flushed */
	int32 MaxAtlasPagesBeforeFlushRequest;

	/** Number of non-atlased textures we can have before we request that the cache be flushed */
	int32 MaxNonAtlasedTexturesBeforeFlushRequest;

	/** The frame counter the last time the font cache was asked to be flushed */
	uint64 FrameCounterLastFlushRequest;

	/** Critical section preventing concurrent access to FontObjectsToFlush */
	mutable FCriticalSection FontObjectsToFlushCS;

	/** Array of UFont objects that the font cache has been requested to flush. Since GC can happen while the loading screen is running, the request may be deferred until the next call to ConditionalFlushCache */
	TArray<const UObject*> FontObjectsToFlush;
};
