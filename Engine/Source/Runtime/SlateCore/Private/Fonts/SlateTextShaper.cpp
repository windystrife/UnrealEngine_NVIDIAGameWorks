// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/SlateTextShaper.h"
#include "Fonts/FontCacheCompositeFont.h"
#include "Fonts/SlateFontRenderer.h"
#include "Internationalization/BreakIterator.h"
#include "SlateGlobals.h"

DECLARE_CYCLE_STAT(TEXT("Shape Bidirectional Text"), STAT_SlateShapeBidirectionalText, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("Shape Unidirectional Text"), STAT_SlateShapeUnidirectionalText, STATGROUP_Slate);

namespace SurrogatePairUtil
{
	template <bool IsUnicode, size_t TCHARSize>
	bool IsSurrogatePairImpl(const TCHAR InHighChar, const TCHAR InLowChar)
	{
		return false;
	}

	template <>
	bool IsSurrogatePairImpl<true, 2>(const TCHAR InHighChar, const TCHAR InLowChar)
	{
		return (InHighChar >= 0xD800 && InHighChar <= 0xDBFF) && (InLowChar >= 0xDC00 && InLowChar <= 0xDFFF);
	}

	bool IsSurrogatePair(const TCHAR InHighChar, const TCHAR InLowChar)
	{
		return IsSurrogatePairImpl<FPlatformString::IsUnicodeEncoded, sizeof(TCHAR)>(InHighChar, InLowChar);
	}
}

namespace
{

struct FKerningOnlyTextSequenceEntry
{
	int32 TextStartIndex;
	int32 TextLength;
	const FFontData* FontDataPtr;
	TSharedPtr<FFreeTypeFace> FaceAndMemory;
	float SubFontScalingFactor;

	FKerningOnlyTextSequenceEntry(const int32 InTextStartIndex, const int32 InTextLength, const FFontData* InFontDataPtr, TSharedPtr<FFreeTypeFace> InFaceAndMemory, const float InSubFontScalingFactor)
		: TextStartIndex(InTextStartIndex)
		, TextLength(InTextLength)
		, FontDataPtr(InFontDataPtr)
		, FaceAndMemory(MoveTemp(InFaceAndMemory))
		, SubFontScalingFactor(InSubFontScalingFactor)
	{
	}
};

#if WITH_HARFBUZZ

struct FHarfBuzzTextSequenceEntry
{
	struct FSubSequenceEntry
	{
		int32 StartIndex;
		int32 Length;
		hb_script_t HarfBuzzScript;

		FSubSequenceEntry(const int32 InStartIndex, const int32 InLength, const hb_script_t InHarfBuzzScript)
			: StartIndex(InStartIndex)
			, Length(InLength)
			, HarfBuzzScript(InHarfBuzzScript)
		{
		}
	};

	int32 TextStartIndex;
	int32 TextLength;
	const FFontData* FontDataPtr;
	TSharedPtr<FFreeTypeFace> FaceAndMemory;
	float SubFontScalingFactor;
	TArray<FSubSequenceEntry> SubSequence;

	FHarfBuzzTextSequenceEntry(const int32 InTextStartIndex, const int32 InTextLength, const FFontData* InFontDataPtr, TSharedPtr<FFreeTypeFace> InFaceAndMemory, const float InSubFontScalingFactor)
		: TextStartIndex(InTextStartIndex)
		, TextLength(InTextLength)
		, FontDataPtr(InFontDataPtr)
		, FaceAndMemory(MoveTemp(InFaceAndMemory))
		, SubFontScalingFactor(InSubFontScalingFactor)
	{
	}
};

#endif // WITH_HARFBUZZ

} // anonymous namespace


FSlateTextShaper::FSlateTextShaper(FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache, FCompositeFontCache* InCompositeFontCache, FSlateFontRenderer* InFontRenderer, FSlateFontCache* InFontCache)
	: FTGlyphCache(InFTGlyphCache)
	, FTAdvanceCache(InFTAdvanceCache)
	, FTKerningPairCache(InFTKerningPairCache)
	, CompositeFontCache(InCompositeFontCache)
	, FontRenderer(InFontRenderer)
	, FontCache(InFontCache)
	, TextBiDiDetection(TextBiDi::CreateTextBiDi())
	, GraphemeBreakIterator(FBreakIterator::CreateCharacterBoundaryIterator())
#if WITH_HARFBUZZ
	, HarfBuzzFontFactory(FTGlyphCache, FTAdvanceCache, FTKerningPairCache)
#endif // WITH_HARFBUZZ
{
	check(FTGlyphCache);
	check(FTAdvanceCache);
	check(FTKerningPairCache);
	check(CompositeFontCache);
	check(FontRenderer);
	check(FontCache);
}

FShapedGlyphSequenceRef FSlateTextShaper::ShapeBidirectionalText(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod TextShapingMethod) const
{
	SCOPE_CYCLE_COUNTER(STAT_SlateShapeBidirectionalText);

	TArray<TextBiDi::FTextDirectionInfo> TextDirectionInfos;
	TextBiDiDetection->ComputeTextDirection(InText, InTextStart, InTextLen, InBaseDirection, TextDirectionInfos);

	TArray<FShapedGlyphEntry> GlyphsToRender;
	for (const TextBiDi::FTextDirectionInfo& TextDirectionInfo : TextDirectionInfos)
	{
		PerformTextShaping(InText, TextDirectionInfo.StartIndex, TextDirectionInfo.Length, InFontInfo, InFontScale, TextDirectionInfo.TextDirection, TextShapingMethod, GlyphsToRender);
	}

	return FinalizeTextShaping(MoveTemp(GlyphsToRender), InFontInfo, InFontScale, FShapedGlyphSequence::FSourceTextRange(InTextStart, InTextLen));
}

FShapedGlyphSequenceRef FSlateTextShaper::ShapeUnidirectionalText(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod TextShapingMethod) const
{
	SCOPE_CYCLE_COUNTER(STAT_SlateShapeUnidirectionalText);

	TArray<FShapedGlyphEntry> GlyphsToRender;
	PerformTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, TextShapingMethod, GlyphsToRender);
	return FinalizeTextShaping(MoveTemp(GlyphsToRender), InFontInfo, InFontScale, FShapedGlyphSequence::FSourceTextRange(InTextStart, InTextLen));
}

void FSlateTextShaper::PerformTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod TextShapingMethod, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	check(InTextDirection != TextBiDi::ETextDirection::Mixed);

#if WITH_FREETYPE
	if (InTextLen > 0)
	{
#if WITH_HARFBUZZ
		auto TextRequiresFullShaping = [&]() -> bool
		{
			// RTL text always requires full shaping
			if (InTextDirection == TextBiDi::ETextDirection::RightToLeft)
			{
				return true;
			}

			// LTR text containing certain scripts or surrogate pairs requires full shaping
			{
				// Note: We deliberately avoid using HarfBuzz/ICU here as we don't care about the script itself, only that the character is within a shaped script range (and testing that is much faster!)
				auto CharRequiresFullShaping = [](const TCHAR InChar) -> bool
				{
					// This isn't an exhaustive list, as it omits some "dead" or uncommon languages, and ranges outside the BMP
					static const TCHAR FullShapingScriptRanges[][2] = {
						// Combining characters
						{ TEXT('\u0300'), TEXT('\u036F') },
						{ TEXT('\u1AB0'), TEXT('\u1AFF') },
						{ TEXT('\u1DC0'), TEXT('\u1DFF') },
						{ TEXT('\u20D0'), TEXT('\u20FF') },
						{ TEXT('\u31C0'), TEXT('\u31EF') },
						{ TEXT('\uFE20'), TEXT('\uFE2F') },

						// Devanagari
						{ TEXT('\u0900'), TEXT('\u097F') },
						{ TEXT('\uA8E0'), TEXT('\uA8FF') },
						{ TEXT('\u1CD0'), TEXT('\u1CFF') },

						// Telugu
						{ TEXT('\u0C00'), TEXT('\u0C7F') },

						// Thai
						{ TEXT('\u0E00'), TEXT('\u0E7F') },
						
						// Tibetan
						{ TEXT('\u0F00'), TEXT('\u0FFF') },

						// Khmer
						{ TEXT('\u1780'), TEXT('\u17FF') },
						{ TEXT('\u19E0'), TEXT('\u19FF') },

						// Sinhala
						{ TEXT('\u0D80'), TEXT('\u0DFF') },

						// Limbu
						{ TEXT('\u1900'), TEXT('\u194F') },

						// Tai Tham
						{ TEXT('\u1A20'), TEXT('\u1AAF') },

						// Tai Viet
						{ TEXT('\uAA80'), TEXT('\uAADF') },

						// Batak
						{ TEXT('\u1BC0'), TEXT('\u1BFF') },
					};

					for (const TCHAR* FullShapingScriptRange : FullShapingScriptRanges)
					{
						if (InChar >= FullShapingScriptRange[0] && InChar <= FullShapingScriptRange[1])
						{
							return true;
						}
					}

					return false;
				};

				const int32 TextEndIndex = InTextStart + InTextLen;
				for (int32 RunningTextIndex = InTextStart; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
				{
					const TCHAR Char = InText[RunningTextIndex];

					if (Char <= TEXT('\u007F'))
					{
						return false;
					}

					if (CharRequiresFullShaping(Char))
					{
						return true;
					}

					{
						const int32 NextTextIndex = RunningTextIndex + 1;
						if (NextTextIndex < TextEndIndex && SurrogatePairUtil::IsSurrogatePair(Char, InText[NextTextIndex]))
						{
							return true;
						}
					}
				}
			}

			return false;
		};

		if (TextShapingMethod == ETextShapingMethod::FullShaping || (TextShapingMethod == ETextShapingMethod::Auto && TextRequiresFullShaping()))
		{
			PerformHarfBuzzTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, OutGlyphsToRender);
		}
		else
#endif // WITH_HARFBUZZ
		{
			PerformKerningOnlyTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, OutGlyphsToRender);
		}
	}
#endif // WITH_FREETYPE
}

FShapedGlyphSequenceRef FSlateTextShaper::FinalizeTextShaping(TArray<FShapedGlyphEntry> InGlyphsToRender, const FSlateFontInfo &InFontInfo, const float InFontScale, const FShapedGlyphSequence::FSourceTextRange& InSourceTextRange) const
{
	int16 TextBaseline = 0;
	uint16 MaxHeight = 0;

#if WITH_FREETYPE
	{
		// Just get info for the null character
		TCHAR Char = 0;
		const FFontData& FontData = CompositeFontCache->GetDefaultFontData(InFontInfo);
		const FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(FontData, Char, InFontInfo.FontFallback);

		if (FaceGlyphData.FaceAndMemory.IsValid())
		{
			FreeTypeUtils::ApplySizeAndScale(FaceGlyphData.FaceAndMemory->GetFace(), InFontInfo.Size, InFontScale);

			TextBaseline = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FaceGlyphData.FaceAndMemory->GetDescender()) * InFontScale);
			MaxHeight = static_cast<uint16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FaceGlyphData.FaceAndMemory->GetScaledHeight()) * InFontScale);
		}
	}
#endif // WITH_FREETYPE

	return MakeShared<FShapedGlyphSequence>(MoveTemp(InGlyphsToRender), TextBaseline, MaxHeight, InFontInfo.FontMaterial, InFontInfo.OutlineSettings, InSourceTextRange);
}

#if WITH_FREETYPE

void FSlateTextShaper::PerformKerningOnlyTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	// We need to work out the correct FFontData for everything so that we can build accurate FShapedGlyphFaceData for rendering later on
	TArray<FKerningOnlyTextSequenceEntry> KerningOnlyTextSequence;

	// Step 1) Split the text into sections that are using the same font face (composite fonts may contain different faces for different character ranges)
	{
		// Data used while detecting font face boundaries
		int32 SplitStartIndex = InTextStart;
		int32 RunningTextIndex = InTextStart;
		const FFontData* RunningFontDataPtr = nullptr;
		TSharedPtr<FFreeTypeFace> RunningFaceAndMemory;
		float RunningSubFontScalingFactor = 1.0f;

		auto AppendPendingFontDataToSequence = [&]()
		{
			if (RunningFontDataPtr)
			{
				KerningOnlyTextSequence.Emplace(
					SplitStartIndex,						// InTextStartIndex
					RunningTextIndex - SplitStartIndex,		// InTextLength
					RunningFontDataPtr,						// InFontDataPtr
					RunningFaceAndMemory,					// InFaceAndMemory
					RunningSubFontScalingFactor				// InSubFontScalingFactor
					);

				RunningFontDataPtr = nullptr;
				RunningFaceAndMemory.Reset();
				RunningSubFontScalingFactor = 1.0f;
			}
		};

		const int32 TextEndIndex = InTextStart + InTextLen;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			const TCHAR CurrentChar = InText[RunningTextIndex];

			// First try with the actual character
			float SubFontScalingFactor = 1.0f;
			const FFontData* FontDataPtr = &CompositeFontCache->GetFontDataForCharacter(InFontInfo, CurrentChar, SubFontScalingFactor);
			FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(*FontDataPtr, CurrentChar, InFontInfo.FontFallback);

			// If none of our fonts can render that character (as the fallback font may be missing), try again with the fallback character
			if (!FaceGlyphData.FaceAndMemory.IsValid())
			{
				FontDataPtr = &CompositeFontCache->GetFontDataForCharacter(InFontInfo, SlateFontRendererUtils::InvalidSubChar, SubFontScalingFactor);
				FaceGlyphData = FontRenderer->GetFontFaceForCharacter(*FontDataPtr, SlateFontRendererUtils::InvalidSubChar, InFontInfo.FontFallback);
			}

			if (!RunningFontDataPtr || RunningFontDataPtr != FontDataPtr || RunningFaceAndMemory != FaceGlyphData.FaceAndMemory || RunningSubFontScalingFactor != SubFontScalingFactor)
			{
				AppendPendingFontDataToSequence();

				SplitStartIndex = RunningTextIndex;
				RunningFontDataPtr = FontDataPtr;
				RunningFaceAndMemory = FaceGlyphData.FaceAndMemory;
				RunningSubFontScalingFactor = SubFontScalingFactor;
			}
		}

		AppendPendingFontDataToSequence();
	}

	// Step 2) Now we use the font cache to get the size for each character, and kerning for each character pair
	{
		OutGlyphsToRender.Reserve(OutGlyphsToRender.Num() + InTextLen);
		for (const FKerningOnlyTextSequenceEntry& KerningOnlyTextSequenceEntry : KerningOnlyTextSequence)
		{
			const float FinalFontScale = InFontScale * KerningOnlyTextSequenceEntry.SubFontScalingFactor;

			uint32 GlyphFlags = 0;
			SlateFontRendererUtils::AppendGlyphFlags(*KerningOnlyTextSequenceEntry.FontDataPtr, GlyphFlags);

			TSharedRef<FShapedGlyphFaceData> ShapedGlyphFaceData = MakeShared<FShapedGlyphFaceData>(KerningOnlyTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale);

			if (!KerningOnlyTextSequenceEntry.FaceAndMemory.IsValid())
			{
				continue;
			}

			const bool bHasKerning = FT_HAS_KERNING(KerningOnlyTextSequenceEntry.FaceAndMemory->GetFace()) != 0;

			for (int32 SequenceCharIndex = 0; SequenceCharIndex < KerningOnlyTextSequenceEntry.TextLength; ++SequenceCharIndex)
			{
				const int32 CurrentCharIndex = KerningOnlyTextSequenceEntry.TextStartIndex + SequenceCharIndex;
				const TCHAR CurrentChar = InText[CurrentCharIndex];

				if (!InsertSubstituteGlyphs(InText, CurrentCharIndex, InFontInfo, InFontScale, ShapedGlyphFaceData, OutGlyphsToRender))
				{
					const bool bIsWhitespace = FText::IsWhitespace(CurrentChar);

					uint32 GlyphIndex = FT_Get_Char_Index(KerningOnlyTextSequenceEntry.FaceAndMemory->GetFace(), CurrentChar);

					// If the given font can't render that character (as the fallback font may be missing), try again with the fallback character
					if (CurrentChar != 0 && GlyphIndex == 0)
					{
						GlyphIndex = FT_Get_Char_Index(KerningOnlyTextSequenceEntry.FaceAndMemory->GetFace(), SlateFontRendererUtils::InvalidSubChar);
					}

					int16 XAdvance = 0;
					{
						FT_Fixed CachedAdvanceData = 0;
						if (FTAdvanceCache->FindOrCache(KerningOnlyTextSequenceEntry.FaceAndMemory->GetFace(), GlyphIndex, GlyphFlags, InFontInfo.Size, FinalFontScale, CachedAdvanceData))
						{
							XAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>((CachedAdvanceData + (1<<9)) >> 10);
						}
					}

					const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
					FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
					ShapedGlyphEntry.FontFaceData = ShapedGlyphFaceData;
					ShapedGlyphEntry.GlyphIndex = GlyphIndex;
					ShapedGlyphEntry.SourceIndex = CurrentCharIndex;
					ShapedGlyphEntry.XAdvance = XAdvance;
					ShapedGlyphEntry.YAdvance = 0;
					ShapedGlyphEntry.XOffset = 0;
					ShapedGlyphEntry.YOffset = 0;
					ShapedGlyphEntry.Kerning = 0;
					ShapedGlyphEntry.NumCharactersInGlyph = 1;
					ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
					ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
					ShapedGlyphEntry.bIsVisible = !bIsWhitespace;

					// Apply the kerning against the previous entry
					if (CurrentGlyphEntryIndex > 0 && bHasKerning && ShapedGlyphEntry.bIsVisible)
					{
						FShapedGlyphEntry& PreviousShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex - 1];

						FT_Vector KerningVector;
						if (FTKerningPairCache->FindOrCache(KerningOnlyTextSequenceEntry.FaceAndMemory->GetFace(), FFreeTypeKerningPairCache::FKerningPair(PreviousShapedGlyphEntry.GlyphIndex, ShapedGlyphEntry.GlyphIndex), FT_KERNING_DEFAULT, InFontInfo.Size, FinalFontScale, KerningVector))
						{
							const int8 Kerning = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int8>(KerningVector.x);
							PreviousShapedGlyphEntry.XAdvance += Kerning;
							PreviousShapedGlyphEntry.Kerning = Kerning;
						}
					}
				}
			}
		}
	}
}

#endif // WITH_FREETYPE

#if WITH_HARFBUZZ

void FSlateTextShaper::PerformHarfBuzzTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	// HarfBuzz can only shape data that uses the same font face, reads in the same direction, and uses the same script so we need to split the given text...
	TArray<FHarfBuzzTextSequenceEntry> HarfBuzzTextSequence;
	hb_unicode_funcs_t* HarfBuzzUnicodeFuncs = hb_unicode_funcs_get_default();

	// Step 1) Split the text into sections that are using the same font face (composite fonts may contain different faces for different character ranges)
	{
		// Data used while detecting font face boundaries
		int32 SplitStartIndex = InTextStart;
		int32 RunningTextIndex = InTextStart;
		const FFontData* RunningFontDataPtr = nullptr;
		TSharedPtr<FFreeTypeFace> RunningFaceAndMemory;
		float RunningSubFontScalingFactor = 1.0f;

		auto AppendPendingFontDataToSequence = [&]()
		{
			if (RunningFontDataPtr)
			{
				HarfBuzzTextSequence.Emplace(
					SplitStartIndex,						// InTextStartIndex
					RunningTextIndex - SplitStartIndex,		// InTextLength
					RunningFontDataPtr,						// InFontDataPtr
					RunningFaceAndMemory,					// InFaceAndMemory
					RunningSubFontScalingFactor				// InSubFontScalingFactor
					);

				RunningFontDataPtr = nullptr;
				RunningFaceAndMemory.Reset();
				RunningSubFontScalingFactor = 1.0f;
			}
		};

		const int32 TextEndIndex = InTextStart + InTextLen;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			const TCHAR CurrentChar = InText[RunningTextIndex];

			// First try with the actual character
			float SubFontScalingFactor = 1.0f;
			const FFontData* FontDataPtr = &CompositeFontCache->GetFontDataForCharacter(InFontInfo, CurrentChar, SubFontScalingFactor);
			FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(*FontDataPtr, CurrentChar, InFontInfo.FontFallback);

			// If none of our fonts can render that character (as the fallback font may be missing), try again with the fallback character
			if (!FaceGlyphData.FaceAndMemory.IsValid())
			{
				FontDataPtr = &CompositeFontCache->GetFontDataForCharacter(InFontInfo, SlateFontRendererUtils::InvalidSubChar, SubFontScalingFactor);
				FaceGlyphData = FontRenderer->GetFontFaceForCharacter(*FontDataPtr, SlateFontRendererUtils::InvalidSubChar, InFontInfo.FontFallback);
			}

			if (!RunningFontDataPtr || RunningFontDataPtr != FontDataPtr || RunningFaceAndMemory != FaceGlyphData.FaceAndMemory || RunningSubFontScalingFactor != SubFontScalingFactor)
			{
				AppendPendingFontDataToSequence();

				SplitStartIndex = RunningTextIndex;
				RunningFontDataPtr = FontDataPtr;
				RunningFaceAndMemory = FaceGlyphData.FaceAndMemory;
				RunningSubFontScalingFactor = SubFontScalingFactor;
			}
		}

		AppendPendingFontDataToSequence();
	}

	// Step 2) Split the font face sections by their their script code
	for (FHarfBuzzTextSequenceEntry& HarfBuzzTextSequenceEntry : HarfBuzzTextSequence)
	{
		// Data used while detecting script code boundaries
		int32 SplitStartIndex = HarfBuzzTextSequenceEntry.TextStartIndex;
		int32 RunningTextIndex = HarfBuzzTextSequenceEntry.TextStartIndex;
		TOptional<hb_script_t> RunningHarfBuzzScript;

		auto StartNewPendingTextSequence = [&](const hb_script_t InHarfBuzzScript)
		{
			SplitStartIndex = RunningTextIndex;
			RunningHarfBuzzScript = InHarfBuzzScript;
		};

		auto AppendPendingTextToSequence = [&]()
		{
			if (RunningHarfBuzzScript.IsSet())
			{
				HarfBuzzTextSequenceEntry.SubSequence.Emplace(
					SplitStartIndex,					// InStartIndex
					RunningTextIndex - SplitStartIndex,	// InLength
					RunningHarfBuzzScript.GetValue()	// InHarfBuzzScript
					);

				RunningHarfBuzzScript.Reset();
			}
		};

		auto IsSpecialCharacter = [](const hb_script_t InHarfBuzzScript) -> bool
		{
			// Characters in the common, inherited, and unknown scripts are allowed (and in the case of inherited, required) to merge with the script 
			// of the character(s) that preceded them. This also helps to minimize shaping batches, as spaces are within the common script.
			return InHarfBuzzScript == HB_SCRIPT_COMMON || InHarfBuzzScript == HB_SCRIPT_INHERITED || InHarfBuzzScript == HB_SCRIPT_UNKNOWN;
		};

		const int32 TextEndIndex = HarfBuzzTextSequenceEntry.TextStartIndex + HarfBuzzTextSequenceEntry.TextLength;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			const hb_script_t CharHarfBuzzScript = hb_unicode_script(HarfBuzzUnicodeFuncs, InText[RunningTextIndex]);				

			if (!RunningHarfBuzzScript.IsSet() || RunningHarfBuzzScript.GetValue() != CharHarfBuzzScript)
			{
				if (!RunningHarfBuzzScript.IsSet())
				{
					// Always start a new run if we're currently un-set
					StartNewPendingTextSequence(CharHarfBuzzScript);
				}
				else if (!IsSpecialCharacter(CharHarfBuzzScript))
				{
					if (IsSpecialCharacter(RunningHarfBuzzScript.GetValue()))
					{
						// If we started our run on a special character, we need to swap the script type to the non-special type as soon as we can
						RunningHarfBuzzScript = CharHarfBuzzScript;
					}
					else
					{
						// Transitioned a non-special character; end the current run and create a new one
						AppendPendingTextToSequence();
						StartNewPendingTextSequence(CharHarfBuzzScript);
					}
				}
			}
		}

		AppendPendingTextToSequence();
	}

	if (InTextDirection == TextBiDi::ETextDirection::RightToLeft)
	{
		// Need to flip the sequence here to mimic what HarfBuzz would do if the text had been a single sequence of right-to-left text
		Algo::Reverse(HarfBuzzTextSequence);
	}

	const int32 InitialNumGlyphsToRender = OutGlyphsToRender.Num();

	// Step 3) Now we use HarfBuzz to shape each font data sequence using its FreeType glyph
	{
		hb_buffer_t* HarfBuzzTextBuffer = hb_buffer_create();

		for (const FHarfBuzzTextSequenceEntry& HarfBuzzTextSequenceEntry : HarfBuzzTextSequence)
		{
			const float FinalFontScale = InFontScale * HarfBuzzTextSequenceEntry.SubFontScalingFactor;

			uint32 GlyphFlags = 0;
			SlateFontRendererUtils::AppendGlyphFlags(*HarfBuzzTextSequenceEntry.FontDataPtr, GlyphFlags);

			TSharedRef<FShapedGlyphFaceData> ShapedGlyphFaceData = MakeShared<FShapedGlyphFaceData>(HarfBuzzTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale);

			if (!HarfBuzzTextSequenceEntry.FaceAndMemory.IsValid())
			{
				continue;
			}

#if WITH_FREETYPE
			const bool bHasKerning = FT_HAS_KERNING(HarfBuzzTextSequenceEntry.FaceAndMemory->GetFace()) != 0;
#else  // WITH_FREETYPE
			const bool bHasKerning = false;
#endif // WITH_FREETYPE
			const hb_feature_t HarfBuzzFeatures[] = {
				{ HB_TAG('k','e','r','n'), bHasKerning, 0, uint32(-1) }
			};
			const int32 HarfBuzzFeaturesCount = ARRAY_COUNT(HarfBuzzFeatures);

			hb_font_t* HarfBuzzFont = HarfBuzzFontFactory.CreateFont(*HarfBuzzTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale);

			for (const FHarfBuzzTextSequenceEntry::FSubSequenceEntry& HarfBuzzTextSubSequenceEntry : HarfBuzzTextSequenceEntry.SubSequence)
			{
				hb_buffer_set_cluster_level(HarfBuzzTextBuffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
				hb_buffer_set_direction(HarfBuzzTextBuffer, (InTextDirection == TextBiDi::ETextDirection::LeftToRight) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
				hb_buffer_set_script(HarfBuzzTextBuffer, HarfBuzzTextSubSequenceEntry.HarfBuzzScript);

				HarfBuzzUtils::AppendStringToBuffer(InText, HarfBuzzTextSubSequenceEntry.StartIndex, HarfBuzzTextSubSequenceEntry.Length, HarfBuzzTextBuffer);
				hb_shape(HarfBuzzFont, HarfBuzzTextBuffer, HarfBuzzFeatures, HarfBuzzFeaturesCount);

				uint32 HarfBuzzGlyphCount = 0;
				hb_glyph_info_t* HarfBuzzGlyphInfos = hb_buffer_get_glyph_infos(HarfBuzzTextBuffer, &HarfBuzzGlyphCount);
				hb_glyph_position_t* HarfBuzzGlyphPositions = hb_buffer_get_glyph_positions(HarfBuzzTextBuffer, &HarfBuzzGlyphCount);

				OutGlyphsToRender.Reserve(OutGlyphsToRender.Num() + static_cast<int32>(HarfBuzzGlyphCount));
				for (uint32 HarfBuzzGlyphIndex = 0; HarfBuzzGlyphIndex < HarfBuzzGlyphCount; ++HarfBuzzGlyphIndex)
				{
					const hb_glyph_info_t& HarfBuzzGlyphInfo = HarfBuzzGlyphInfos[HarfBuzzGlyphIndex];
					const hb_glyph_position_t& HarfBuzzGlyphPosition = HarfBuzzGlyphPositions[HarfBuzzGlyphIndex];

					const int32 CurrentCharIndex = static_cast<int32>(HarfBuzzGlyphInfo.cluster);
					const TCHAR CurrentChar = InText[CurrentCharIndex];
					if (!InsertSubstituteGlyphs(InText, CurrentCharIndex, InFontInfo, InFontScale, ShapedGlyphFaceData, OutGlyphsToRender))
					{
						const bool bIsWhitespace = FText::IsWhitespace(CurrentChar);

						const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
						FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
						ShapedGlyphEntry.FontFaceData = ShapedGlyphFaceData;
						ShapedGlyphEntry.GlyphIndex = HarfBuzzGlyphInfo.codepoint;
						ShapedGlyphEntry.SourceIndex = CurrentCharIndex;
						ShapedGlyphEntry.XAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.x_advance);
						ShapedGlyphEntry.YAdvance = -FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.y_advance);
						ShapedGlyphEntry.XOffset = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.x_offset);
						ShapedGlyphEntry.YOffset = -FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.y_offset);
						ShapedGlyphEntry.Kerning = 0;
						ShapedGlyphEntry.NumCharactersInGlyph = 0; // Filled in later once we've processed each cluster
						ShapedGlyphEntry.NumGraphemeClustersInGlyph = 0; // Filled in later once we have an accurate character count
						ShapedGlyphEntry.TextDirection = InTextDirection;
						ShapedGlyphEntry.bIsVisible = !bIsWhitespace;

						// Apply the kerning against the previous entry
						if (CurrentGlyphEntryIndex > 0 && bHasKerning && ShapedGlyphEntry.bIsVisible)
						{
							FShapedGlyphEntry& PreviousShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex - 1];

#if WITH_FREETYPE
							FT_Vector KerningVector;
							if (FTKerningPairCache->FindOrCache(HarfBuzzTextSequenceEntry.FaceAndMemory->GetFace(), FFreeTypeKerningPairCache::FKerningPair(PreviousShapedGlyphEntry.GlyphIndex, ShapedGlyphEntry.GlyphIndex), FT_KERNING_DEFAULT, InFontInfo.Size, FinalFontScale, KerningVector))
							{
								PreviousShapedGlyphEntry.Kerning = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int8>(KerningVector.x);
							}
#endif // WITH_FREETYPE
						}
					}
				}

				hb_buffer_clear_contents(HarfBuzzTextBuffer);
			}

			hb_font_destroy(HarfBuzzFont);
		}

		hb_buffer_destroy(HarfBuzzTextBuffer);
	}

	const int32 NumGlyphsRendered = OutGlyphsToRender.Num() - InitialNumGlyphsToRender;

	// Step 4) Count the characters that belong to each glyph if they haven't already been set
	if (NumGlyphsRendered > 0)
	{
		const int32 CurrentNumGlyphsToRender = OutGlyphsToRender.Num();

		// The glyphs in the array are in render order, so LTR and RTL text use different start and end points in the source string
		const int32 FirstGlyphPrevSourceIndex = (InTextDirection == TextBiDi::ETextDirection::LeftToRight) ? InTextStart - 1 : InTextStart + InTextLen;
		const int32 LastGlyphNextSourceIndex  = (InTextDirection == TextBiDi::ETextDirection::LeftToRight) ? InTextStart + InTextLen : InTextStart - 1;

		// Start of the loop; process against the "start" of the string range
		{
			FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[InitialNumGlyphsToRender];
			ShapedGlyphEntry.NumCharactersInGlyph = FMath::Abs(FirstGlyphPrevSourceIndex - ShapedGlyphEntry.SourceIndex);
		}

		// Body of the loop; this will process the initial character again, but won't change its value and will walk past its entire cluster
		for (int32 GlyphToRenderIndex = InitialNumGlyphsToRender; GlyphToRenderIndex < CurrentNumGlyphsToRender;)
		{
			FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[GlyphToRenderIndex];

			// Walk forward to find the first glyph in the next cluster; the number of characters in this glyph is the difference between their two source indices
			int32 NextGlyphToRenderIndex = GlyphToRenderIndex + 1;
			for (; NextGlyphToRenderIndex < CurrentNumGlyphsToRender; ++NextGlyphToRenderIndex)
			{
				const FShapedGlyphEntry& NextShapedGlyphEntry = OutGlyphsToRender[NextGlyphToRenderIndex];
				if (ShapedGlyphEntry.SourceIndex != NextShapedGlyphEntry.SourceIndex)
				{
					break;
				}
			}

			if (NextGlyphToRenderIndex < CurrentNumGlyphsToRender)
			{
				FShapedGlyphEntry& NextShapedGlyphEntry = OutGlyphsToRender[NextGlyphToRenderIndex];

				// For LTR text we update ourself based on the next glyph cluster, for RTL text we update the next glyph cluster based on us
				FShapedGlyphEntry& ShapedGlyphEntryToUpdate = (InTextDirection == TextBiDi::ETextDirection::LeftToRight) ? ShapedGlyphEntry : NextShapedGlyphEntry;
				if (ShapedGlyphEntryToUpdate.NumCharactersInGlyph == 0)
				{
					ShapedGlyphEntryToUpdate.NumCharactersInGlyph = FMath::Abs(NextShapedGlyphEntry.SourceIndex - ShapedGlyphEntry.SourceIndex);
				}
			}

			GlyphToRenderIndex = NextGlyphToRenderIndex;
		}

		// End of the loop; process against the "end" of the string range (RTL text is implicitly handled as part of the loop above)
		if (InTextDirection == TextBiDi::ETextDirection::LeftToRight)
		{
			FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentNumGlyphsToRender - 1];
			ShapedGlyphEntry.NumCharactersInGlyph = FMath::Abs(LastGlyphNextSourceIndex - ShapedGlyphEntry.SourceIndex);
		}
	}

	// Step 5) Count the grapheme clusters for any entries that haven't been set yet
	if (NumGlyphsRendered > 0)
	{
		GraphemeBreakIterator->SetString(InText + InTextStart, InTextLen);

		const int32 CurrentNumGlyphsToRender = OutGlyphsToRender.Num();
		for (int32 GlyphToRenderIndex = InitialNumGlyphsToRender; GlyphToRenderIndex < CurrentNumGlyphsToRender; ++GlyphToRenderIndex)
		{
			FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[GlyphToRenderIndex];
			if (ShapedGlyphEntry.NumCharactersInGlyph > 0 && ShapedGlyphEntry.NumGraphemeClustersInGlyph == 0)
			{
				const int32 FirstCharacterIndex = ShapedGlyphEntry.SourceIndex - InTextStart;
				const int32 LastCharacterIndex = (ShapedGlyphEntry.SourceIndex + ShapedGlyphEntry.NumCharactersInGlyph) - InTextStart;

				for (int32 GraphemeIndex = GraphemeBreakIterator->MoveToCandidateAfter(FirstCharacterIndex);
					GraphemeIndex != INDEX_NONE && GraphemeIndex <= LastCharacterIndex;
					GraphemeIndex = GraphemeBreakIterator->MoveToNext()
					)
				{
					++ShapedGlyphEntry.NumGraphemeClustersInGlyph;
				}
			}
		}

		GraphemeBreakIterator->ClearString();
	}
}

#endif // WITH_HARFBUZZ

bool FSlateTextShaper::InsertSubstituteGlyphs(const TCHAR* InText, const int32 InCharIndex, const FSlateFontInfo &InFontInfo, const float InFontScale, const TSharedRef<FShapedGlyphFaceData>& InShapedGlyphFaceData, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	const TCHAR Char = InText[InCharIndex];

	if (TextBiDi::IsControlCharacter(Char))
	{
		// We insert a stub entry for control characters to avoid them being drawn as a visual glyph with size
		const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
		FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
		ShapedGlyphEntry.FontFaceData = InShapedGlyphFaceData;
		ShapedGlyphEntry.GlyphIndex = 0;
		ShapedGlyphEntry.SourceIndex = InCharIndex;
		ShapedGlyphEntry.XAdvance = 0;
		ShapedGlyphEntry.YAdvance = 0;
		ShapedGlyphEntry.XOffset = 0;
		ShapedGlyphEntry.YOffset = 0;
		ShapedGlyphEntry.Kerning = 0;
		ShapedGlyphEntry.NumCharactersInGlyph = 1;
		ShapedGlyphEntry.NumGraphemeClustersInGlyph = 1;
		ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
		ShapedGlyphEntry.bIsVisible = false;
		return true;
	}
	
	if (Char == TEXT('\t'))
	{
		uint32 SpaceGlyphIndex = 0;
		int16 SpaceXAdvance = 0;
#if WITH_FREETYPE
		{
			TSharedPtr<FFreeTypeFace> FTFace = InShapedGlyphFaceData->FontFace.Pin();
			if (FTFace.IsValid())
			{
				SpaceGlyphIndex = FT_Get_Char_Index(FTFace->GetFace(), TEXT(' '));

				FT_Fixed CachedAdvanceData = 0;
				if (FTAdvanceCache->FindOrCache(FTFace->GetFace(), SpaceGlyphIndex, InShapedGlyphFaceData->GlyphFlags, InShapedGlyphFaceData->FontSize, InShapedGlyphFaceData->FontScale, CachedAdvanceData))
				{
					SpaceXAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>((CachedAdvanceData + (1<<9)) >> 10);
				}
			}
		}
#endif // WITH_FREETYPE

		// We insert (up-to) 4 space glyphs in-place of a tab character
		const int32 NumSpacesToInsert = 4 - (OutGlyphsToRender.Num() % 4);
		for (int32 SpaceIndex = 0; SpaceIndex < NumSpacesToInsert; ++SpaceIndex)
		{
			const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
			FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
			ShapedGlyphEntry.FontFaceData = InShapedGlyphFaceData;
			ShapedGlyphEntry.GlyphIndex = SpaceGlyphIndex;
			ShapedGlyphEntry.SourceIndex = InCharIndex;
			ShapedGlyphEntry.XAdvance = SpaceXAdvance;
			ShapedGlyphEntry.YAdvance = 0;
			ShapedGlyphEntry.XOffset = 0;
			ShapedGlyphEntry.YOffset = 0;
			ShapedGlyphEntry.Kerning = 0;
			ShapedGlyphEntry.NumCharactersInGlyph = (SpaceIndex == 0) ? 1 : 0;
			ShapedGlyphEntry.NumGraphemeClustersInGlyph = (SpaceIndex == 0) ? 1 : 0;
			ShapedGlyphEntry.TextDirection = TextBiDi::ETextDirection::LeftToRight;
			ShapedGlyphEntry.bIsVisible = false;
		}

		return true;
	}

	return false;
}
