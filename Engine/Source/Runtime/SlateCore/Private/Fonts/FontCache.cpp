// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCache.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include "Application/SlateApplicationBase.h"
#include "Fonts/FontCacheFreeType.h"
#include "Fonts/FontCacheHarfBuzz.h"
#include "Fonts/FontCacheCompositeFont.h"
#include "Fonts/SlateFontRenderer.h"
#include "Fonts/SlateTextShaper.h"
#include "Fonts/LegacySlateFontInfoCache.h"
#include "Fonts/FontCacheUtils.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Font Atlases"), STAT_SlateNumFontAtlases, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Font Non-Atlased Textures"), STAT_SlateNumFontNonAtlasedTextures, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Shaped Glyph Sequence Memory"), STAT_SlateShapedGlyphSequenceMemory, STATGROUP_SlateMemory);
DEFINE_STAT(STAT_SlateFontMeasureCacheMemory);


namespace FontCacheConstants
{
	/** Number of characters that can be indexed directly in the cache */
	const int32 DirectAccessSize = 256;
}


static TAutoConsoleVariable<int32> CVarDefaultTextShapingMethod(
	TEXT("Slate.DefaultTextShapingMethod"),
	static_cast<int32>(ETextShapingMethod::Auto),
	TEXT("0: Auto (default), 1: KerningOnly, 2: FullShaping."),
	ECVF_Default
	);

ETextShapingMethod GetDefaultTextShapingMethod()
{
	const int32 DefaultTextShapingMethodAsInt = CVarDefaultTextShapingMethod.AsVariable()->GetInt();
	if (DefaultTextShapingMethodAsInt >= static_cast<int32>(ETextShapingMethod::Auto) && DefaultTextShapingMethodAsInt <= static_cast<int32>(ETextShapingMethod::FullShaping))
	{
		return static_cast<ETextShapingMethod>(DefaultTextShapingMethodAsInt);
	}
	return ETextShapingMethod::Auto;
}


FShapedGlyphEntryKey::FShapedGlyphEntryKey(const FShapedGlyphFaceData& InFontFaceData, uint32 InGlyphIndex, const FFontOutlineSettings& InOutlineSettings)
	: FontFace(InFontFaceData.FontFace)
	, FontSize(InFontFaceData.FontSize)
	, OutlineSize(InOutlineSettings.OutlineSize)
	, OutlineSizeSeparateFillAlpha(InOutlineSettings.bSeparateFillAlpha)
	, FontScale(InFontFaceData.FontScale)
	, GlyphIndex(InGlyphIndex)
	, KeyHash(0)
{
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontFace));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontSize));
	KeyHash = HashCombine(KeyHash, GetTypeHash(OutlineSize));
	KeyHash = HashCombine(KeyHash, GetTypeHash(OutlineSizeSeparateFillAlpha));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontScale));
	KeyHash = HashCombine(KeyHash, GetTypeHash(GlyphIndex));
}

FShapedGlyphSequence::FShapedGlyphSequence(TArray<FShapedGlyphEntry> InGlyphsToRender, const int16 InTextBaseline, const uint16 InMaxTextHeight, const UObject* InFontMaterial, const FFontOutlineSettings& InOutlineSettings, const FSourceTextRange& InSourceTextRange)
	: GlyphsToRender(MoveTemp(InGlyphsToRender))
	, TextBaseline(InTextBaseline)
	, MaxTextHeight(InMaxTextHeight)
	, FontMaterial(InFontMaterial)
	, OutlineSettings(InOutlineSettings)
	, SequenceWidth(0)
	, GlyphFontFaces()
	, SourceIndicesToGlyphData(InSourceTextRange)
{
	const int32 NumGlyphsToRender = GlyphsToRender.Num();
	for (int32 CurrentGlyphIndex = 0; CurrentGlyphIndex < NumGlyphsToRender; ++CurrentGlyphIndex)
	{
		const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

		// Track unique font faces
		if (CurrentGlyph.FontFaceData->FontFace.IsValid())
		{
			GlyphFontFaces.AddUnique(CurrentGlyph.FontFaceData->FontFace);
		}

		// Update the measured width
		SequenceWidth += CurrentGlyph.XAdvance;

		// Track reverse look-up data
		FSourceIndexToGlyphData* SourceIndexToGlyphData = SourceIndicesToGlyphData.GetGlyphData(CurrentGlyph.SourceIndex);
		checkSlow(SourceIndexToGlyphData);
		if (SourceIndexToGlyphData->IsValid())
		{
			// If this data already exists then it means a single character produced multiple glyphs and we need to track it as an additional glyph (these are always within the same cluster block)
			SourceIndexToGlyphData->AdditionalGlyphIndices.Add(CurrentGlyphIndex);
		}
		else
		{
			*SourceIndexToGlyphData = FSourceIndexToGlyphData(CurrentGlyphIndex);
		}
	}

	// Track memory usage
	INC_MEMORY_STAT_BY(STAT_SlateShapedGlyphSequenceMemory, GetAllocatedSize());
}

FShapedGlyphSequence::~FShapedGlyphSequence()
{
	// Untrack memory usage
	DEC_MEMORY_STAT_BY(STAT_SlateShapedGlyphSequenceMemory, GetAllocatedSize());
}

uint32 FShapedGlyphSequence::GetAllocatedSize() const
{
	return GlyphsToRender.GetAllocatedSize() + GlyphFontFaces.GetAllocatedSize() + SourceIndicesToGlyphData.GetAllocatedSize();
}

bool FShapedGlyphSequence::IsDirty() const
{
	for (const auto& GlyphFontFace : GlyphFontFaces)
	{
		if (!GlyphFontFace.IsValid())
		{
			return true;
		}
	}

	return false;
}

int32 FShapedGlyphSequence::GetMeasuredWidth() const
{
	return SequenceWidth;
}

TOptional<int32> FShapedGlyphSequence::GetMeasuredWidth(const int32 InStartIndex, const int32 InEndIndex, const bool InIncludeKerningWithPrecedingGlyph) const
{
	int32 MeasuredWidth = 0;

	if (InIncludeKerningWithPrecedingGlyph && InStartIndex > 0)
	{
		const TOptional<int8> Kerning = GetKerning(InStartIndex - 1);
		MeasuredWidth += Kerning.Get(0);
	}

	auto GlyphCallback = [&](const FShapedGlyphEntry& CurrentGlyph, int32 CurrentGlyphIndex) -> bool
	{
		MeasuredWidth += CurrentGlyph.XAdvance;
		return true;
	};

	if (EnumerateLogicalGlyphsInSourceRange(InStartIndex, InEndIndex, GlyphCallback) == EEnumerateGlyphsResult::EnumerationComplete)
	{
		return MeasuredWidth;
	}

	return TOptional<int32>();
}

FShapedGlyphSequence::FGlyphOffsetResult FShapedGlyphSequence::GetGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InHorizontalOffset, const int32 InStartOffset) const
{
	if (GlyphsToRender.Num() == 0)
	{
		return FGlyphOffsetResult();
	}

	int32 CurrentOffset = InStartOffset;
	const FShapedGlyphEntry* MatchedGlyph = nullptr;

	const int32 NumGlyphsToRender = GlyphsToRender.Num();
	for (int32 CurrentGlyphIndex = 0; CurrentGlyphIndex < NumGlyphsToRender; ++CurrentGlyphIndex)
	{
		const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

		if (HasFoundGlyphAtOffset(InFontCache, InHorizontalOffset, CurrentGlyph, CurrentGlyphIndex, /*InOut*/CurrentOffset, /*Out*/MatchedGlyph))
		{
			break;
		}
	}

	// Found a valid glyph?
	if (MatchedGlyph)
	{
		return FGlyphOffsetResult(MatchedGlyph, CurrentOffset);
	}

	// Hit was outside of our measure boundary, so return the start or end source index, depending on the reading direction of the right-most glyph
	if (GlyphsToRender.Last().TextDirection == TextBiDi::ETextDirection::LeftToRight)
	{
		return FGlyphOffsetResult(SourceIndicesToGlyphData.GetSourceTextEndIndex());
	}
	else
	{
		return FGlyphOffsetResult(SourceIndicesToGlyphData.GetSourceTextStartIndex());
	}
}

TOptional<FShapedGlyphSequence::FGlyphOffsetResult> FShapedGlyphSequence::GetGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InStartIndex, const int32 InEndIndex, const int32 InHorizontalOffset, const int32 InStartOffset, const bool InIncludeKerningWithPrecedingGlyph) const
{
	int32 CurrentOffset = InStartOffset;
	const FShapedGlyphEntry* MatchedGlyph = nullptr;
	const FShapedGlyphEntry* RightmostGlyph = nullptr;

	if (InIncludeKerningWithPrecedingGlyph && InStartIndex > 0)
	{
		const TOptional<int8> Kerning = GetKerning(InStartIndex - 1);
		CurrentOffset += Kerning.Get(0);
	}

	auto GlyphCallback = [&](const FShapedGlyphEntry& CurrentGlyph, int32 CurrentGlyphIndex) -> bool
	{
		if (HasFoundGlyphAtOffset(InFontCache, InHorizontalOffset, CurrentGlyph, CurrentGlyphIndex, /*InOut*/CurrentOffset, /*Out*/MatchedGlyph))
		{
			return false; // triggers the enumeration to abort
		}

		RightmostGlyph = &CurrentGlyph;
		return true;
	};

	if (EnumerateVisualGlyphsInSourceRange(InStartIndex, InEndIndex, GlyphCallback) != EEnumerateGlyphsResult::EnumerationFailed)
	{
		// Found a valid glyph?
		if (MatchedGlyph)
		{
			return FGlyphOffsetResult(MatchedGlyph, CurrentOffset);
		}

		// Hit was outside of our measure boundary, so return the start or end index (if valid), depending on the reading direction of the right-most glyph we tested
		if (!RightmostGlyph || RightmostGlyph->TextDirection == TextBiDi::ETextDirection::LeftToRight)
		{
			if (InEndIndex >= SourceIndicesToGlyphData.GetSourceTextStartIndex() && InEndIndex <= SourceIndicesToGlyphData.GetSourceTextEndIndex())
			{
				return FGlyphOffsetResult(InEndIndex);
			}
		}
		else
		{
			if (InStartIndex >= SourceIndicesToGlyphData.GetSourceTextStartIndex() && InStartIndex <= SourceIndicesToGlyphData.GetSourceTextEndIndex())
			{
				return FGlyphOffsetResult(InStartIndex);
			}
		}
	}

	return TOptional<FGlyphOffsetResult>();
}

bool FShapedGlyphSequence::HasFoundGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InHorizontalOffset, const FShapedGlyphEntry& InCurrentGlyph, const int32 InCurrentGlyphIndex, int32& InOutCurrentOffset, const FShapedGlyphEntry*& OutMatchedGlyph) const
{
	// Skip any glyphs that don't represent any characters (these are additional glyphs when a character produces multiple glyphs, and we process them below when we find their primary glyph, so can ignore them now)
	if (InCurrentGlyph.NumCharactersInGlyph == 0)
	{
		return false;
	}

	// A single character may produce multiple glyphs which must be treated as a single logic unit
	int32 TotalGlyphSpacing = 0;
	int32 TotalGlyphAdvance = 0;
	for (int32 SubGlyphIndex = InCurrentGlyphIndex;; ++SubGlyphIndex)
	{
		const FShapedGlyphEntry& SubGlyph = GlyphsToRender[SubGlyphIndex];
		const FShapedGlyphFontAtlasData SubGlyphAtlasData = InFontCache.GetShapedGlyphFontAtlasData(SubGlyph,FFontOutlineSettings::NoOutline);
		TotalGlyphSpacing += SubGlyphAtlasData.HorizontalOffset + SubGlyph.XAdvance;
		TotalGlyphAdvance += SubGlyph.XAdvance;

		const bool bIsWithinGlyphCluster = GlyphsToRender.IsValidIndex(SubGlyphIndex + 1) && SubGlyph.SourceIndex == GlyphsToRender[SubGlyphIndex + 1].SourceIndex;
		if (!bIsWithinGlyphCluster)
		{
			break;
		}
	}

	// Round our test toward the glyphs center position, but don't do this for ligatures as they're handled outside of this function
	const int32 GlyphWidthToTest = (InCurrentGlyph.NumGraphemeClustersInGlyph > 1) ? TotalGlyphSpacing : TotalGlyphSpacing / 2;

	// Did we reach our desired hit-point?
	if (InHorizontalOffset < (InOutCurrentOffset + GlyphWidthToTest))
	{
		if (InCurrentGlyph.TextDirection == TextBiDi::ETextDirection::LeftToRight)
		{
			OutMatchedGlyph = &InCurrentGlyph;
		}
		else
		{
			// Right-to-left text needs to return the previous glyph index, since that is the logical "next" glyph
			const int32 PreviousGlyphIndex = InCurrentGlyphIndex - 1;
			if (GlyphsToRender.IsValidIndex(PreviousGlyphIndex))
			{
				OutMatchedGlyph = &GlyphsToRender[PreviousGlyphIndex];
			}
			else
			{
				OutMatchedGlyph = &InCurrentGlyph;
			}
		}

		return true;
	}

	InOutCurrentOffset += TotalGlyphAdvance;
	return false;
}

TOptional<int8> FShapedGlyphSequence::GetKerning(const int32 InIndex) const
{
	const FSourceIndexToGlyphData* SourceIndexToGlyphData = SourceIndicesToGlyphData.GetGlyphData(InIndex);
	if (SourceIndexToGlyphData && SourceIndexToGlyphData->IsValid())
	{
		const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[SourceIndexToGlyphData->GlyphIndex];
		checkSlow(CurrentGlyph.SourceIndex == InIndex);
		return CurrentGlyph.Kerning;
	}

	// If we got here it means we couldn't find the glyph
	return TOptional<int8>();
}

FShapedGlyphSequencePtr FShapedGlyphSequence::GetSubSequence(const int32 InStartIndex, const int32 InEndIndex) const
{
	TArray<FShapedGlyphEntry> SubGlyphsToRender;
	SubGlyphsToRender.Reserve(InEndIndex - InStartIndex);

	auto GlyphCallback = [&](const FShapedGlyphEntry& CurrentGlyph, int32 CurrentGlyphIndex) -> bool
	{
		SubGlyphsToRender.Add(CurrentGlyph);
		return true;
	};

	if (EnumerateVisualGlyphsInSourceRange(InStartIndex, InEndIndex, GlyphCallback) == EEnumerateGlyphsResult::EnumerationComplete)
	{
		return MakeShareable(new FShapedGlyphSequence(MoveTemp(SubGlyphsToRender), TextBaseline, MaxTextHeight, FontMaterial, OutlineSettings, FSourceTextRange(InStartIndex, InEndIndex - InStartIndex)));
	}

	return nullptr;
}

FShapedGlyphSequence::EEnumerateGlyphsResult FShapedGlyphSequence::EnumerateLogicalGlyphsInSourceRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const
{
	if (InStartIndex == InEndIndex)
	{
		// Nothing to enumerate, but don't say we failed
		return EEnumerateGlyphsResult::EnumerationComplete;
	}

	// Enumerate the corresponding glyph for each source index in the given range
	int32 SourceIndex = InStartIndex;
	while (SourceIndex < InEndIndex)
	{
		// Get the glyph(s) that correspond to this source index
		const FSourceIndexToGlyphData* SourceIndexToGlyphData = SourceIndicesToGlyphData.GetGlyphData(SourceIndex);
		if (!(SourceIndexToGlyphData && SourceIndexToGlyphData->IsValid()))
		{
			return EEnumerateGlyphsResult::EnumerationFailed;
		}

		// Enumerate each glyph generated by the given source index
		const int32 StartGlyphIndex = SourceIndexToGlyphData->GetLowestGlyphIndex();
		const int32 EndGlyphIndex = SourceIndexToGlyphData->GetHighestGlyphIndex();
		for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
		{
			const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

			if (!InGlyphCallback(CurrentGlyph, CurrentGlyphIndex))
			{
				return EEnumerateGlyphsResult::EnumerationAborted;
			}

			// Advance the source index by the number of characters within this glyph
			SourceIndex += CurrentGlyph.NumCharactersInGlyph;
		}
	}

	return (SourceIndex == InEndIndex) ? EEnumerateGlyphsResult::EnumerationComplete : EEnumerateGlyphsResult::EnumerationFailed;
}

FShapedGlyphSequence::EEnumerateGlyphsResult FShapedGlyphSequence::EnumerateVisualGlyphsInSourceRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const
{
	if (InStartIndex == InEndIndex)
	{
		// Nothing to enumerate, but don't say we failed
		return EEnumerateGlyphsResult::EnumerationComplete;
	}

	// The given range is exclusive, but we use an inclusive range when performing all the bounds testing below (as it makes things simpler)
	const FSourceIndexToGlyphData* StartSourceIndexToGlyphData = SourceIndicesToGlyphData.GetGlyphData(InStartIndex);
	const FSourceIndexToGlyphData* EndSourceIndexToGlyphData = SourceIndicesToGlyphData.GetGlyphData(InEndIndex - 1);

	// If we found a start glyph but no end glyph, test to see whether the start glyph spans to the end glyph (as may happen with a ligature)
	if ((StartSourceIndexToGlyphData && StartSourceIndexToGlyphData->IsValid()) && !(EndSourceIndexToGlyphData && EndSourceIndexToGlyphData->IsValid()))
	{
		const FShapedGlyphEntry& StartGlyph = GlyphsToRender[StartSourceIndexToGlyphData->GlyphIndex];

		const int32 GlyphEndSourceIndex = StartGlyph.SourceIndex + StartGlyph.NumCharactersInGlyph;
		if (GlyphEndSourceIndex == InEndIndex)
		{
			EndSourceIndexToGlyphData = StartSourceIndexToGlyphData;
		}
	}

	// Found valid glyphs to enumerate between?
	if (!(StartSourceIndexToGlyphData && StartSourceIndexToGlyphData->IsValid() && EndSourceIndexToGlyphData && EndSourceIndexToGlyphData->IsValid()))
	{
		return EEnumerateGlyphsResult::EnumerationFailed;
	}

	// Find the real start and end glyph indices - taking into account characters that may have produced multiple glyphs when shaped
	int32 StartGlyphIndex = INDEX_NONE;
	int32 EndGlyphIndex = INDEX_NONE;
	if (StartSourceIndexToGlyphData->GlyphIndex <= EndSourceIndexToGlyphData->GlyphIndex)
	{
		StartGlyphIndex = StartSourceIndexToGlyphData->GetLowestGlyphIndex();
		EndGlyphIndex = EndSourceIndexToGlyphData->GetHighestGlyphIndex();
	}
	else
	{
		StartGlyphIndex = EndSourceIndexToGlyphData->GetLowestGlyphIndex();
		EndGlyphIndex = StartSourceIndexToGlyphData->GetHighestGlyphIndex();
	}
	check(StartGlyphIndex <= EndGlyphIndex);

	bool bStartIndexInRange = SourceIndicesToGlyphData.GetSourceTextStartIndex() == InStartIndex;
	bool bEndIndexInRange = SourceIndicesToGlyphData.GetSourceTextEndIndex() == InEndIndex;

	// Enumerate everything in the found range
	for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
	{
		const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

		if (!bStartIndexInRange || !bEndIndexInRange)
		{
			const int32 GlyphStartSourceIndex = CurrentGlyph.SourceIndex;
			const int32 GlyphEndSourceIndex = CurrentGlyph.SourceIndex + CurrentGlyph.NumCharactersInGlyph;

			if (!bStartIndexInRange && GlyphStartSourceIndex == InStartIndex)
			{
				bStartIndexInRange = true;
			}

			if (!bEndIndexInRange && GlyphEndSourceIndex == InEndIndex)
			{
				bEndIndexInRange = true;
			}
		}

		if (!InGlyphCallback(CurrentGlyph, CurrentGlyphIndex))
		{
			return EEnumerateGlyphsResult::EnumerationAborted;
		}
	}

	return (bStartIndexInRange && bEndIndexInRange) ? EEnumerateGlyphsResult::EnumerationComplete : EEnumerateGlyphsResult::EnumerationFailed;
}

FCharacterList::FCharacterList( const FSlateFontKey& InFontKey, FSlateFontCache& InFontCache )
	: FontKey( InFontKey )
	, FontCache( InFontCache )
#if WITH_EDITORONLY_DATA
	, CompositeFontHistoryRevision( INDEX_NONE )
#endif	// WITH_EDITORONLY_DATA
	, MaxDirectIndexedEntries( FontCacheConstants::DirectAccessSize )
	, MaxHeight( 0 )
	, Baseline( 0 )
{
#if WITH_EDITORONLY_DATA
	const FCompositeFont* const CompositeFont = InFontKey.GetFontInfo().GetCompositeFont();
	if( CompositeFont )
	{
		CompositeFontHistoryRevision = CompositeFont->HistoryRevision;
	}
#endif	// WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
bool FCharacterList::IsStale() const
{
	const FCompositeFont* const CompositeFont = FontKey.GetFontInfo().GetCompositeFont();
	return CompositeFontHistoryRevision != (CompositeFont ? CompositeFont->HistoryRevision : INDEX_NONE);
}
#endif	// WITH_EDITORONLY_DATA

int8 FCharacterList::GetKerning(TCHAR FirstChar, TCHAR SecondChar, const EFontFallback MaxFontFallback)
{
	const FCharacterEntry First = GetCharacter(FirstChar, MaxFontFallback);
	const FCharacterEntry Second = GetCharacter(SecondChar, MaxFontFallback);
	return GetKerning(First, Second);
}

int8 FCharacterList::GetKerning( const FCharacterEntry& FirstCharacterEntry, const FCharacterEntry& SecondCharacterEntry )
{
	// We can only get kerning if both characters are using the same font
	if (FirstCharacterEntry.Valid &&
		SecondCharacterEntry.Valid &&
		FirstCharacterEntry.FontData && 
		FirstCharacterEntry.HasKerning && 
		*FirstCharacterEntry.FontData == *SecondCharacterEntry.FontData
		)
	{
		return FontCache.GetKerning(*FirstCharacterEntry.FontData, FontKey.GetFontInfo().Size, FirstCharacterEntry.Character, SecondCharacterEntry.Character, FirstCharacterEntry.FontScale);
	}

	return 0;
}

uint16 FCharacterList::GetMaxHeight() const
{
	if( MaxHeight == 0 )
	{
		MaxHeight = FontCache.GetMaxCharacterHeight( FontKey.GetFontInfo(), FontKey.GetScale() );
	}

	return MaxHeight;
}

int16 FCharacterList::GetBaseline() const
{
	if (Baseline == 0)
	{
		Baseline = FontCache.GetBaseline( FontKey.GetFontInfo(), FontKey.GetScale() );
	}

	return Baseline;
}

bool FCharacterList::CanCacheCharacter(TCHAR Character, const EFontFallback MaxFontFallback) const
{
	bool bReturnVal = false;

	if (Character == SlateFontRendererUtils::InvalidSubChar)
	{
		bReturnVal = true;
	}
	else
	{
		float SubFontScalingFactor = 1.0f;
		const FFontData& FontData = FontCache.CompositeFontCache->GetFontDataForCharacter(FontKey.GetFontInfo(), Character, SubFontScalingFactor);

		bReturnVal = FontCache.FontRenderer->CanLoadCharacter(FontData, Character, MaxFontFallback);
	}

	return bReturnVal;
}

FCharacterEntry FCharacterList::GetCharacter(TCHAR Character, const EFontFallback MaxFontFallback)
{
	TOptional<FCharacterListEntry> InternalEntry;
	const bool bDirectIndexChar = Character < MaxDirectIndexedEntries;

	// First get a reference to the character, if it is already mapped (mapped does not mean cached though)
	if (bDirectIndexChar)
	{
		if (DirectIndexEntries.IsValidIndex(Character))
		{
			InternalEntry = DirectIndexEntries[Character];
		}
	}
	else
	{
		const FCharacterListEntry* const FoundEntry = MappedEntries.Find(Character);
		if (FoundEntry)
		{
			InternalEntry = *FoundEntry;
		}
	}

	// Determine whether the character needs caching, and map it if needed
	bool bNeedCaching = false;

	if (InternalEntry.IsSet())
	{
		bNeedCaching = !InternalEntry->Valid;

		// If the character needs caching, but can't be cached, reject the character
		if (bNeedCaching && !CanCacheCharacter(Character, MaxFontFallback))
		{
			bNeedCaching = false;
			InternalEntry.Reset();
		}
	}
	// Only map the character if it can be cached
	else if (CanCacheCharacter(Character, MaxFontFallback))
	{
		bNeedCaching = true;

		if (bDirectIndexChar)
		{
			DirectIndexEntries.AddZeroed((Character - DirectIndexEntries.Num()) + 1);
			InternalEntry = DirectIndexEntries[Character];
		}
		else
		{
			InternalEntry = MappedEntries.Add(Character);
		}
	}

	if (InternalEntry.IsSet())
	{
		if (bNeedCaching)
		{
			InternalEntry = CacheCharacter(Character);
		}
		// For already-cached characters, reject characters that don't fall within maximum font fallback level requirements
		else if (Character != SlateFontRendererUtils::InvalidSubChar && MaxFontFallback < InternalEntry->FallbackLevel)
		{
			InternalEntry.Reset();
		}
	}

	if (InternalEntry.IsSet())
	{
		return MakeCharacterEntry(Character, InternalEntry.GetValue());
	}

	return GetCharacter(SlateFontRendererUtils::InvalidSubChar, MaxFontFallback);
}

FCharacterList::FCharacterListEntry FCharacterList::CacheCharacter(TCHAR Character)
{
	FCharacterListEntry NewInternalEntry;

#if WITH_FREETYPE
	// Fake shape the character
	{
		const FSlateFontInfo& FontInfo = FontKey.GetFontInfo();

		// Get the data needed to render this character
		float SubFontScalingFactor = 1.0f;
		const FFontData* FontDataPtr = &FontCache.CompositeFontCache->GetFontDataForCharacter(FontInfo, Character, SubFontScalingFactor);
		FFreeTypeFaceGlyphData FaceGlyphData = FontCache.FontRenderer->GetFontFaceForCharacter(*FontDataPtr, Character, FontInfo.FontFallback);

		// Found a valid font face?
		if (FaceGlyphData.FaceAndMemory.IsValid())
		{
			const float FinalFontScale = FontKey.GetScale() * SubFontScalingFactor;

			uint32 GlyphFlags = 0;
			SlateFontRendererUtils::AppendGlyphFlags(*FontDataPtr, GlyphFlags);

			const bool bHasKerning = FT_HAS_KERNING(FaceGlyphData.FaceAndMemory->GetFace()) != 0;

			const bool bIsWhitespace = FText::IsWhitespace(Character);
			const uint32 GlyphIndex = FT_Get_Char_Index(FaceGlyphData.FaceAndMemory->GetFace(), Character);

			int16 XAdvance = 0;
			{
				FT_Fixed CachedAdvanceData = 0;
				if (FontCache.FTAdvanceCache->FindOrCache(FaceGlyphData.FaceAndMemory->GetFace(), GlyphIndex, GlyphFlags, FontInfo.Size, FinalFontScale, CachedAdvanceData))
				{
					XAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>((CachedAdvanceData + (1<<9)) >> 10);
				}
			}

			NewInternalEntry.ShapedGlyphEntry.FontFaceData = MakeShared<FShapedGlyphFaceData>(FaceGlyphData.FaceAndMemory, GlyphFlags, FontInfo.Size, FinalFontScale);
			NewInternalEntry.ShapedGlyphEntry.GlyphIndex = GlyphIndex;
			NewInternalEntry.ShapedGlyphEntry.XAdvance = XAdvance;
			NewInternalEntry.ShapedGlyphEntry.bIsVisible = !bIsWhitespace;

			NewInternalEntry.FontData = FontDataPtr;
			NewInternalEntry.FallbackLevel = FaceGlyphData.CharFallbackLevel;
			NewInternalEntry.HasKerning = bHasKerning;
			NewInternalEntry.Valid = Character == 0 || GlyphIndex != 0;
		}
	}

	// Cache the shaped entry in the font cache
	if (NewInternalEntry.Valid)
	{
		FontCache.GetShapedGlyphFontAtlasData(NewInternalEntry.ShapedGlyphEntry, FontKey.GetFontOutlineSettings());

		if (Character < MaxDirectIndexedEntries)
		{
			DirectIndexEntries[Character] = NewInternalEntry;
			return DirectIndexEntries[Character];
		}
		else
		{
			return MappedEntries.Add(Character, NewInternalEntry);
		}
	}
#endif // WITH_FREETYPE

	return NewInternalEntry;
}

FCharacterEntry FCharacterList::MakeCharacterEntry(TCHAR Character, const FCharacterListEntry& InternalEntry) const
{
	FCharacterEntry CharEntry;

	CharEntry.Valid = InternalEntry.Valid;
	if (CharEntry.Valid)
	{
		FShapedGlyphFontAtlasData ShapedGlyphFontAtlasData = FontCache.GetShapedGlyphFontAtlasData(InternalEntry.ShapedGlyphEntry, FontKey.GetFontOutlineSettings());
		CharEntry.Valid = ShapedGlyphFontAtlasData.Valid;

		if (CharEntry.Valid)
		{
			CharEntry.Character = Character;
			CharEntry.GlyphIndex = InternalEntry.ShapedGlyphEntry.GlyphIndex;
			CharEntry.FontData = InternalEntry.FontData;
			CharEntry.FontScale = InternalEntry.ShapedGlyphEntry.FontFaceData->FontScale;
			CharEntry.StartU = ShapedGlyphFontAtlasData.StartU;
			CharEntry.StartV = ShapedGlyphFontAtlasData.StartV;
			CharEntry.USize = ShapedGlyphFontAtlasData.USize;
			CharEntry.VSize = ShapedGlyphFontAtlasData.VSize;
			CharEntry.VerticalOffset = ShapedGlyphFontAtlasData.VerticalOffset;
			CharEntry.HorizontalOffset = ShapedGlyphFontAtlasData.HorizontalOffset;
			CharEntry.GlobalDescender = GetBaseline(); // All fonts within a composite font need to use the baseline of the default font
			CharEntry.XAdvance = InternalEntry.ShapedGlyphEntry.XAdvance;
			CharEntry.TextureIndex = ShapedGlyphFontAtlasData.TextureIndex;
			CharEntry.HasKerning = InternalEntry.HasKerning;
			CharEntry.FallbackLevel = InternalEntry.FallbackLevel;
		}
	}

	return CharEntry;
}

FSlateFontCache::FSlateFontCache( TSharedRef<ISlateFontAtlasFactory> InFontAtlasFactory )
	: FTLibrary( new FFreeTypeLibrary() )
	, FTGlyphCache( new FFreeTypeGlyphCache() )
	, FTAdvanceCache( new FFreeTypeAdvanceCache() )
	, FTKerningPairCache( new FFreeTypeKerningPairCache() )
	, CompositeFontCache( new FCompositeFontCache( FTLibrary.Get() ) )
	, FontRenderer( new FSlateFontRenderer( FTLibrary.Get(), FTGlyphCache.Get(), FTKerningPairCache.Get(), CompositeFontCache.Get() ) )
	, TextShaper( new FSlateTextShaper( FTGlyphCache.Get(), FTAdvanceCache.Get(), FTKerningPairCache.Get(), CompositeFontCache.Get(), FontRenderer.Get(), this ) )
	, FontAtlasFactory( InFontAtlasFactory )
	, bFlushRequested( false )
	, MaxAtlasPagesBeforeFlushRequest( 1 )
	, MaxNonAtlasedTexturesBeforeFlushRequest( 1 )
	, FrameCounterLastFlushRequest( 0 )
{
	UE_LOG(LogSlate, Log, TEXT("SlateFontCache - WITH_FREETYPE: %d, WITH_HARFBUZZ: %d"), WITH_FREETYPE, WITH_HARFBUZZ);

	FInternationalization::Get().OnCultureChanged().AddRaw(this, &FSlateFontCache::HandleCultureChanged);
}

FSlateFontCache::~FSlateFontCache()
{
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);

	// Make sure things get destroyed in the correct order
	TextShaper.Reset();
	FontRenderer.Reset();
	CompositeFontCache.Reset();
	FTKerningPairCache.Reset();
	FTAdvanceCache.Reset();
	FTGlyphCache.Reset();
	FTLibrary.Reset();
}

int32 FSlateFontCache::GetNumAtlasPages() const
{
	return FontAtlases.Num();
}

FIntPoint FSlateFontCache::GetAtlasPageSize() const
{
	return FontAtlasFactory->GetAtlasSize();
}

FSlateShaderResource* FSlateFontCache::GetAtlasPageResource(const int32 InIndex) const
{
	return FontAtlases[InIndex]->GetSlateTexture();
}

bool FSlateFontCache::IsAtlasPageResourceAlphaOnly() const
{
	return true;
}

bool FSlateFontCache::AddNewEntry(const FShapedGlyphEntry& InShapedGlyph, const FFontOutlineSettings& InOutlineSettings, FShapedGlyphFontAtlasData& OutAtlasData)
{
	// Render the glyph
	FCharacterRenderData RenderData;
	const bool bDidRender = FontRenderer->GetRenderData(InShapedGlyph, InOutlineSettings, RenderData);

	OutAtlasData.Valid = bDidRender && AddNewEntry(RenderData, OutAtlasData.TextureIndex, OutAtlasData.StartU, OutAtlasData.StartV, OutAtlasData.USize, OutAtlasData.VSize);
	if (OutAtlasData.Valid)
	{
		OutAtlasData.VerticalOffset = RenderData.MeasureInfo.VerticalOffset;
		OutAtlasData.HorizontalOffset = RenderData.MeasureInfo.HorizontalOffset;
	}

	return OutAtlasData.Valid;
}

bool FSlateFontCache::AddNewEntry( const FCharacterRenderData InRenderData, uint8& OutTextureIndex, uint16& OutGlyphX, uint16& OutGlyphY, uint16& OutGlyphWidth, uint16& OutGlyphHeight )
{
	// Will this entry fit within any atlas texture?
	if (InRenderData.MeasureInfo.SizeX > FontAtlasFactory->GetAtlasSize().X || InRenderData.MeasureInfo.SizeY > FontAtlasFactory->GetAtlasSize().Y)
	{
		TSharedPtr<ISlateFontTexture> NonAtlasedTexture = FontAtlasFactory->CreateNonAtlasedTexture(InRenderData.MeasureInfo.SizeX, InRenderData.MeasureInfo.SizeY, InRenderData.RawPixels);
		if (NonAtlasedTexture.IsValid())
		{
			INC_DWORD_STAT_BY(STAT_SlateNumFontNonAtlasedTextures, 1);

			UE_LOG(LogSlate, Warning, TEXT("SlateFontCache - Glyph texture is too large to store in the font atlas, so we're falling back to a non-atlased texture for this glyph. This may have SERIOUS performance implications. Atlas page size: { %d, %d }. Glyph render size: { %d, %d }"),
				FontAtlasFactory->GetAtlasSize().X, FontAtlasFactory->GetAtlasSize().Y,
				InRenderData.MeasureInfo.SizeX, InRenderData.MeasureInfo.SizeY
				);

			NonAtlasedTextures.Add(NonAtlasedTexture.ToSharedRef());
			OutTextureIndex = AllFontTextures.Add(NonAtlasedTexture.ToSharedRef());
			OutGlyphX = 0;
			OutGlyphY = 0;
			OutGlyphWidth = InRenderData.MeasureInfo.SizeX;
			OutGlyphHeight = InRenderData.MeasureInfo.SizeY;

			if (NonAtlasedTextures.Num() > MaxNonAtlasedTexturesBeforeFlushRequest && !bFlushRequested)
			{
				// If we grew back up to this number of non-atlased textures within the same or next frame of the previous flush request, then we likely legitimately have 
				// a lot of font data cached. We should update MaxNonAtlasedTexturesBeforeFlushRequest to give us a bit more flexibility before the next flush request
				if (GFrameCounter == FrameCounterLastFlushRequest || GFrameCounter == FrameCounterLastFlushRequest + 1)
				{
					MaxNonAtlasedTexturesBeforeFlushRequest = NonAtlasedTextures.Num();
					UE_LOG(LogSlate, Warning, TEXT("SlateFontCache - Setting the threshold to trigger a flush to %d non-atlased textures as there is a lot of font data being cached."), MaxNonAtlasedTexturesBeforeFlushRequest);
				}
				else
				{
					// We've grown beyond our current stable limit - try and request a flush
					RequestFlushCache();
				}
			}

			return true;
		}

		UE_LOG(LogSlate, Warning, TEXT("SlateFontCache - Glyph texture is too large to store in the font atlas, but we cannot support rendering such a large texture. Atlas page size: { %d, %d }. Glyph render size: { %d, %d }"),
			FontAtlasFactory->GetAtlasSize().X, FontAtlasFactory->GetAtlasSize().Y,
			InRenderData.MeasureInfo.SizeX, InRenderData.MeasureInfo.SizeY
			);
		return false;
	}

	auto FillOutputParamsFromAtlasedTextureSlot = [&](const FAtlasedTextureSlot& AtlasedTextureSlot)
	{
		OutGlyphX = AtlasedTextureSlot.X + AtlasedTextureSlot.Padding;
		OutGlyphY = AtlasedTextureSlot.Y + AtlasedTextureSlot.Padding;
		OutGlyphWidth = AtlasedTextureSlot.Width - (2 * AtlasedTextureSlot.Padding);
		OutGlyphHeight = AtlasedTextureSlot.Height - (2 * AtlasedTextureSlot.Padding);
	};

	for( OutTextureIndex = 0; OutTextureIndex < FontAtlases.Num(); ++OutTextureIndex ) 
	{
		// Add the character to the texture
		const FAtlasedTextureSlot* NewSlot = FontAtlases[OutTextureIndex]->AddCharacter(InRenderData);
		if( NewSlot )
		{
			FillOutputParamsFromAtlasedTextureSlot(*NewSlot);
			return true;
		}
	}

	TSharedRef<FSlateFontAtlas> FontAtlas = FontAtlasFactory->CreateFontAtlas();

	// Add the character to the texture
	const FAtlasedTextureSlot* NewSlot = FontAtlas->AddCharacter(InRenderData);
	if( NewSlot )
	{
		FillOutputParamsFromAtlasedTextureSlot(*NewSlot);
	}

	FontAtlases.Add( FontAtlas );
	OutTextureIndex = AllFontTextures.Add(FontAtlas);

	INC_DWORD_STAT_BY( STAT_SlateNumFontAtlases, 1 );

	if( FontAtlases.Num() > MaxAtlasPagesBeforeFlushRequest && !bFlushRequested )
	{
		// If we grew back up to this number of atlas pages within the same or next frame of the previous flush request, then we likely legitimately have 
		// a lot of font data cached. We should update MaxAtlasPagesBeforeFlushRequest to give us a bit more flexibility before the next flush request
		if( GFrameCounter == FrameCounterLastFlushRequest || GFrameCounter == FrameCounterLastFlushRequest + 1 )
		{
			MaxAtlasPagesBeforeFlushRequest = FontAtlases.Num();
			UE_LOG(LogSlate, Warning, TEXT("SlateFontCache - Setting the threshold to trigger a flush to %d atlas pages as there is a lot of font data being cached."), MaxAtlasPagesBeforeFlushRequest);
		}
		else
		{
			// We've grown beyond our current stable limit - try and request a flush
			RequestFlushCache();
		}
	}

	return NewSlot != nullptr;
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeBidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return ShapeBidirectionalText(*InText, 0, InText.Len(), InFontInfo, InFontScale, InBaseDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeBidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return TextShaper->ShapeBidirectionalText(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InBaseDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeUnidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return ShapeUnidirectionalText(*InText, 0, InText.Len(), InFontInfo, InFontScale, InTextDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeUnidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return TextShaper->ShapeUnidirectionalText(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, InTextShapingMethod);
}

FCharacterList& FSlateFontCache::GetCharacterList( const FSlateFontInfo &InFontInfo, float FontScale, const FFontOutlineSettings& InOutlineSettings )
{
	// Create a key for looking up each character
	const FSlateFontKey FontKey( InFontInfo, InOutlineSettings, FontScale );

	TSharedRef< class FCharacterList >* CachedCharacterList = FontToCharacterListCache.Find( FontKey );

	if( CachedCharacterList )
	{
#if WITH_EDITORONLY_DATA
		// Clear out this entry if it's stale so that we make a new one
		if( (*CachedCharacterList)->IsStale() )
		{
			FontToCharacterListCache.Remove( FontKey );
			FlushData();
		}
		else
#endif	// WITH_EDITORONLY_DATA
		{
			return CachedCharacterList->Get();
		}
	}

	return FontToCharacterListCache.Add( FontKey, MakeShareable( new FCharacterList( FontKey, *this ) ) ).Get();
}

FShapedGlyphFontAtlasData FSlateFontCache::GetShapedGlyphFontAtlasData( const FShapedGlyphEntry& InShapedGlyph, const FFontOutlineSettings& InOutlineSettings )
{
	uint8 CachedTypeIndex = (uint8)(InOutlineSettings.OutlineSize <= 0 ? EFontCacheAtlasDataType::Regular : EFontCacheAtlasDataType::Outline);
	const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

	const int32 CachedAtlasDataThreadIndex = (AtlasThreadId == ESlateTextureAtlasThreadId::Game) ? 0 : 1;

	// Has the atlas data already been cached on the glyph?
	{
		TSharedPtr<FShapedGlyphFontAtlasData> CachedAtlasDataPin = InShapedGlyph.CachedAtlasData[CachedTypeIndex][CachedAtlasDataThreadIndex].Pin();
		if (CachedAtlasDataPin.IsValid())
		{
			return *CachedAtlasDataPin;
		}
	}

	// Not cached on the glyph, so create a key for to look up this glyph, as it may
	// have already been cached by another shaped text sequence
	const FShapedGlyphEntryKey GlyphKey(*InShapedGlyph.FontFaceData, InShapedGlyph.GlyphIndex, InOutlineSettings);

	// Has the atlas data already been cached by another shaped text sequence?
	const TSharedRef<FShapedGlyphFontAtlasData>* FoundAtlasData = ShapedGlyphToAtlasData.Find(GlyphKey);
	if (FoundAtlasData)
	{
		InShapedGlyph.CachedAtlasData[CachedTypeIndex][CachedAtlasDataThreadIndex] = *FoundAtlasData;
		return **FoundAtlasData;
	}

	// Not cached at all... create a new entry
	TSharedRef<FShapedGlyphFontAtlasData> NewAtlasData = MakeShareable(new FShapedGlyphFontAtlasData());
	AddNewEntry(InShapedGlyph, InOutlineSettings, *NewAtlasData);

	if (NewAtlasData->Valid)
	{
		InShapedGlyph.CachedAtlasData[CachedTypeIndex][CachedAtlasDataThreadIndex] = NewAtlasData;
		ShapedGlyphToAtlasData.Add(GlyphKey, NewAtlasData);
	}

	return *NewAtlasData;
}

const FFontData& FSlateFontCache::GetDefaultFontData( const FSlateFontInfo& InFontInfo ) const
{
	return CompositeFontCache->GetDefaultFontData(InFontInfo);
}

const FFontData& FSlateFontCache::GetFontDataForCharacter( const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor ) const
{
	return CompositeFontCache->GetFontDataForCharacter(InFontInfo, InChar, OutScalingFactor);
}

uint16 FSlateFontCache::GetMaxCharacterHeight( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FontRenderer->GetMaxHeight(InFontInfo, FontScale);
}

int16 FSlateFontCache::GetBaseline( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FontRenderer->GetBaseline(InFontInfo, FontScale);
}

void FSlateFontCache::GetUnderlineMetrics( const FSlateFontInfo& InFontInfo, const float FontScale, int16& OutUnderlinePos, int16& OutUnderlineThickness ) const
{
	FontRenderer->GetUnderlineMetrics(InFontInfo, FontScale, OutUnderlinePos, OutUnderlineThickness);
}

int8 FSlateFontCache::GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, float Scale ) const
{
	return FontRenderer->GetKerning(InFontData, InSize, First, Second, Scale);
}

bool FSlateFontCache::HasKerning( const FFontData& InFontData ) const
{
	return FontRenderer->HasKerning(InFontData);
}

const TSet<FName>& FSlateFontCache::GetFontAttributes( const FFontData& InFontData ) const
{
	return CompositeFontCache->GetFontAttributes(InFontData);
}

uint16 FSlateFontCache::GetLocalizedFallbackFontRevision() const
{
	return FLegacySlateFontInfoCache::Get().GetLocalizedFallbackFontRevision();
}

void FSlateFontCache::RequestFlushCache()
{
	bFlushRequested = true;
	MaxAtlasPagesBeforeFlushRequest = 1;
	MaxNonAtlasedTexturesBeforeFlushRequest = 1;
	FrameCounterLastFlushRequest = GFrameCounter;
}

void FSlateFontCache::FlushObject( const UObject* const InObject )
{
	if (InObject)
	{
		// Add it to the list of pending objects to flush
		FScopeLock ScopeLock(&FontObjectsToFlushCS);
		FontObjectsToFlush.AddUnique(InObject);
	}
}

void FSlateFontCache::FlushCompositeFont(const FCompositeFont& InCompositeFont)
{
	CompositeFontCache->FlushCompositeFont(InCompositeFont);
}

bool FSlateFontCache::ConditionalFlushCache()
{
	bool bFlushed = false;
	if (bFlushRequested)
	{
		bFlushRequested = false;
		FlushCache();
		bFlushed = !bFlushRequested;
	}

	if (!bFlushed && IsInGameThread())
	{
		// Only bother calling this if we didn't do a full flush
		FlushFontObjects();
	}

	return bFlushed;
}

void FSlateFontCache::UpdateCache()
{
	for (const TSharedRef<FSlateFontAtlas>& FontAtlas : FontAtlases)
	{
		FontAtlas->ConditionalUpdateTexture();
	}
}

void FSlateFontCache::ReleaseResources()
{
	for (const TSharedRef<FSlateFontAtlas>& FontAtlas : FontAtlases)
	{
		FontAtlas->ReleaseResources();
	}

	for (const TSharedRef<ISlateFontTexture>& NonAtlasedTexture : NonAtlasedTextures)
	{
		NonAtlasedTexture->ReleaseResources();
	}
}

void FSlateFontCache::FlushCache()
{
	if ( IsInGameThread() )
	{
		FlushData();
		ReleaseResources();

		// hack
		FSlateApplicationBase::Get().GetRenderer()->FlushCommands();

		SET_DWORD_STAT(STAT_SlateNumFontAtlases, 0);
		SET_DWORD_STAT(STAT_SlateNumFontNonAtlasedTextures, 0);

		FontAtlases.Empty();
		NonAtlasedTextures.Empty();
		AllFontTextures.Empty();

		{
			FScopeLock ScopeLock(&FontObjectsToFlushCS);
			FontObjectsToFlush.Empty();
		}

		UE_LOG(LogSlate, Verbose, TEXT("Slate font cache was flushed"));
	}
	else
	{
		RequestFlushCache();
	}
}

void FSlateFontCache::FlushData()
{
	// Ensure all invalidation panels are cleared of cached widgets
	FSlateApplicationBase::Get().InvalidateAllWidgets();

	FTGlyphCache->FlushCache();
	FTAdvanceCache->FlushCache();
	FTKerningPairCache->FlushCache();
	CompositeFontCache->FlushCache();

	FontToCharacterListCache.Empty();
	ShapedGlyphToAtlasData.Empty();
}

void FSlateFontCache::FlushFontObjects()
{
	check(IsInGameThread());

	bool bHasRemovedEntries = false;
	{
		FScopeLock ScopeLock(&FontObjectsToFlushCS);

		if (FontObjectsToFlush.Num() > 0)
		{
			for (auto It = FontToCharacterListCache.CreateIterator(); It; ++It)
			{
				if (FontObjectsToFlush.Contains(It.Key().GetFontInfo().FontObject))
				{
					bHasRemovedEntries = true;
					It.RemoveCurrent();
				}
			}

			FontObjectsToFlush.Empty();
		}
	}

	if (bHasRemovedEntries)
	{
		FlushData();
	}
}

void FSlateFontCache::HandleCultureChanged()
{
	// The culture has changed, so request the font cache be flushed once it is safe to do so
	// We don't flush immediately as the request may come in from a different thread than the one that owns the font cache
	RequestFlushCache();
}
