// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCacheCompositeFont.h"
#include "Fonts/FontCacheFreeType.h"
#include "SlateGlobals.h"
#include "Fonts/FontBulkData.h"
#include "Misc/FileHelper.h"

DECLARE_CYCLE_STAT(TEXT("Load Font"), STAT_SlateLoadFont, STATGROUP_Slate);

FCachedTypefaceData::FCachedTypefaceData()
	: Typeface(nullptr)
	, SingularFontData(nullptr)
	, NameToFontDataMap()
	, ScalingFactor(1.0f)
{
}

FCachedTypefaceData::FCachedTypefaceData(const FTypeface& InTypeface, const float InScalingFactor)
	: Typeface(&InTypeface)
	, SingularFontData(nullptr)
	, NameToFontDataMap()
	, ScalingFactor(InScalingFactor)
{
	if (InTypeface.Fonts.Num() == 0)
	{
		// We have no entries - don't bother building a map
		SingularFontData = nullptr;
	}
	else if (InTypeface.Fonts.Num() == 1)
	{
		// We have a single entry - don't bother building a map
		SingularFontData = &InTypeface.Fonts[0].Font;
	}
	else
	{
		// Add all the entries from the typeface
		for (const FTypefaceEntry& TypefaceEntry : InTypeface.Fonts)
		{
			NameToFontDataMap.Add(TypefaceEntry.Name, &TypefaceEntry.Font);
		}

		// Add a special "None" entry to return the first font from the typeface
		if (!NameToFontDataMap.Contains(NAME_None))
		{
			NameToFontDataMap.Add(NAME_None, &InTypeface.Fonts[0].Font);
		}
	}
}

const FFontData* FCachedTypefaceData::GetFontData(const FName& InName) const
{
	if (NameToFontDataMap.Num() > 0)
	{
		const FFontData* const * const FoundFontData = NameToFontDataMap.Find(InName);
		return (FoundFontData) ? *FoundFontData : nullptr;
	}
	return SingularFontData;
}

void FCachedTypefaceData::GetCachedFontData(TArray<const FFontData*>& OutFontData) const
{
	if (NameToFontDataMap.Num() > 0)
	{
		for (const auto& NameToFontDataPair : NameToFontDataMap)
		{
			OutFontData.Add(NameToFontDataPair.Value);
		}
	}
	else
	{
		OutFontData.Add(SingularFontData);
	}
}


FCachedCompositeFontData::FCachedCompositeFontData()
	: CompositeFont(nullptr)
	, CachedTypefaces()
	, CachedFontRanges()
{
}

FCachedCompositeFontData::FCachedCompositeFontData(const FCompositeFont& InCompositeFont)
	: CompositeFont(&InCompositeFont)
	, CachedTypefaces()
	, CachedFontRanges()
{
	// Add all the entries from the composite font
	CachedTypefaces.Add(MakeShareable(new FCachedTypefaceData(InCompositeFont.DefaultTypeface)));
	for (const FCompositeSubFont& SubTypeface : InCompositeFont.SubTypefaces)
	{
		TSharedPtr<FCachedTypefaceData> CachedTypeface = MakeShareable(new FCachedTypefaceData(SubTypeface.Typeface, SubTypeface.ScalingFactor));
		CachedTypefaces.Add(CachedTypeface);

		for (const FInt32Range& Range : SubTypeface.CharacterRanges)
		{
			CachedFontRanges.Add(FCachedFontRange(Range, CachedTypeface));
		}
	}

	// Sort the font ranges into ascending order
	CachedFontRanges.Sort([](const FCachedFontRange& RangeOne, const FCachedFontRange& RangeTwo) -> bool
	{
		if (RangeOne.Range.IsEmpty() && !RangeTwo.Range.IsEmpty())
		{
			return true;
		}
		if (!RangeOne.Range.IsEmpty() && RangeTwo.Range.IsEmpty())
		{
			return false;
		}
		return RangeOne.Range.GetLowerBoundValue() < RangeTwo.Range.GetLowerBoundValue();
	});
}

const FCachedTypefaceData* FCachedCompositeFontData::GetTypefaceForCharacter(const TCHAR InChar) const
{
	const int32 CharIndex = static_cast<int32>(InChar);

	for (const FCachedFontRange& CachedRange : CachedFontRanges)
	{
		if (CachedRange.Range.IsEmpty())
		{
			continue;
		}

		// Ranges are sorting in ascending order (by the start position), so if this range starts higher than the character we're looking for, we can bail from the check
		if (CachedRange.Range.GetLowerBoundValue() > CharIndex)
		{
			break;
		}

		if (CachedRange.Range.Contains(CharIndex))
		{
			return CachedRange.CachedTypeface.Get();
		}
	}

	return CachedTypefaces[0].Get();
}

void FCachedCompositeFontData::GetCachedFontData(TArray<const FFontData*>& OutFontData) const
{
	for (const auto& CachedTypeface : CachedTypefaces)
	{
		CachedTypeface->GetCachedFontData(OutFontData);
	}
}


FCompositeFontCache::FCompositeFontCache(const FFreeTypeLibrary* InFTLibrary)
	: FTLibrary(InFTLibrary)
{
	check(FTLibrary);
}

const FFontData& FCompositeFontCache::GetDefaultFontData(const FSlateFontInfo& InFontInfo)
{
	static const FFontData DummyFontData;

	const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
	const FCachedTypefaceData* const CachedTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
	if (CachedTypefaceData)
	{
		// Try to find the correct font from the typeface
		const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
		if (FoundFontData)
		{
			return *FoundFontData;
		}

		// Failing that, return the first font available (the "None" font)
		FoundFontData = CachedTypefaceData->GetFontData(NAME_None);
		if (FoundFontData)
		{
			return *FoundFontData;
		}
	}

	return DummyFontData;
}

const FFontData& FCompositeFontCache::GetFontDataForCharacter(const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor)
{
	static const FFontData DummyFontData;

	auto FontDataHasCharacter = [&](const FFontData& InFontData) -> bool
	{
#if WITH_FREETYPE
		TSharedPtr<FFreeTypeFace> FaceAndMemory = GetFontFace(InFontData);
		return FaceAndMemory.IsValid() && FT_Get_Char_Index(FaceAndMemory->GetFace(), InChar) != 0;
#else  // WITH_FREETYPE
		return false;
#endif // WITH_FREETYPE
	};

	const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
	const FCachedTypefaceData* const CachedTypefaceData = GetCachedTypefaceForCharacter(ResolvedCompositeFont, InChar);
	if (CachedTypefaceData)
	{
		OutScalingFactor = CachedTypefaceData->GetScalingFactor();

		const FCachedTypefaceData* const CachedDefaultTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
		if (CachedDefaultTypefaceData)
		{
			const bool bIsDefaultTypeface = CachedTypefaceData == CachedDefaultTypefaceData;

			// Try to find the correct font from the typeface
			const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
			if (FoundFontData && (bIsDefaultTypeface || FontDataHasCharacter(*FoundFontData)))
			{
				return *FoundFontData;
			}

			// Failing that, try and find a font by the attributes of the default font with the given name
			if (!bIsDefaultTypeface)
			{
				const FFontData* const FoundDefaultFontData = CachedDefaultTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
				if (FoundDefaultFontData)
				{
					const TSet<FName>& DefaultFontAttributes = GetFontAttributes(*FoundDefaultFontData);
					FoundFontData = GetBestMatchFontForAttributes(CachedTypefaceData, DefaultFontAttributes);
					if (FoundFontData && FontDataHasCharacter(*FoundFontData))
					{
						return *FoundFontData;
					}
				}
			}

			// Failing that, return the first font available (the "None" font)
			FoundFontData = CachedTypefaceData->GetFontData(NAME_None);
			if (FoundFontData && (bIsDefaultTypeface || FontDataHasCharacter(*FoundFontData)))
			{
				return *FoundFontData;
			}

			// Failing that, try again using the default font (as the sub-font may not have actually supported the character we needed)
			if (!bIsDefaultTypeface)
			{
				OutScalingFactor = CachedDefaultTypefaceData->GetScalingFactor();

				// Try to find the correct font from the typeface
				FoundFontData = CachedDefaultTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
				if (FoundFontData)
				{
					return *FoundFontData;
				}

				// Failing that, return the first font available (the "None" font)
				FoundFontData = CachedDefaultTypefaceData->GetFontData(NAME_None);
				if (FoundFontData)
				{
					return *FoundFontData;
				}
			}
		}
		else
		{
			// Try to find the correct font from the typeface
			const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
			if (FoundFontData && FontDataHasCharacter(*FoundFontData))
			{
				return *FoundFontData;
			}
		}
	}

	OutScalingFactor = 1.0f;
	return DummyFontData;
}

TSharedPtr<FFreeTypeFace> FCompositeFontCache::GetFontFace(const FFontData& InFontData)
{
	TSharedPtr<FFreeTypeFace> FaceAndMemory = FontFaceMap.FindRef(InFontData);
	if (!FaceAndMemory.IsValid() && InFontData.HasFont())
	{
		SCOPE_CYCLE_COUNTER(STAT_SlateLoadFont);

		// IMPORTANT: Do not log from this function until the new font has been added to the FontFaceMap, as it may be the Output Log font being loaded, which would cause an infinite recursion!
		FString LoadLogMessage;

		{
			// If this font data is referencing an asset, we just need to load it from memory
			FFontFaceDataConstPtr FontFaceData = InFontData.GetFontFaceData();
			if (FontFaceData.IsValid() && FontFaceData->HasData())
			{
				FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, FontFaceData.ToSharedRef(), InFontData.GetLayoutMethod());
			}
		}

		// If no asset was loaded, then we go through the normal font loading process
		if (!FaceAndMemory.IsValid())
		{
			switch (InFontData.GetLoadingPolicy())
			{
			case EFontLoadingPolicy::LazyLoad:
				{
					const double FontDataLoadStartTime = FPlatformTime::Seconds();

					TArray<uint8> FontFaceData;
					if (FFileHelper::LoadFileToArray(FontFaceData, *InFontData.GetFontFilename()))
					{
						const int32 FontDataSizeKB = (FontFaceData.Num() + 1023) / 1024;
						LoadLogMessage = FString::Printf(TEXT("Took %f seconds to synchronously load lazily loaded font '%s' (%dK)"), float(FPlatformTime::Seconds() - FontDataLoadStartTime), *InFontData.GetFontFilename(), FontDataSizeKB);

						FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, FFontFaceData::MakeFontFaceData(MoveTemp(FontFaceData)), InFontData.GetLayoutMethod());
					}
				}
				break;
			case EFontLoadingPolicy::Stream:
				{
					FaceAndMemory = MakeShared<FFreeTypeFace>(FTLibrary, InFontData.GetFontFilename(), InFontData.GetLayoutMethod());
				}
				break;
			default:
				break;
			}
		}

		// Got a valid font?
		if (FaceAndMemory.IsValid() && FaceAndMemory->IsValid())
		{
			FontFaceMap.Add(InFontData, FaceAndMemory);

			if (!LoadLogMessage.IsEmpty())
			{
				const bool bLogLoadAsWarning = GIsRunning && !GIsEditor;
				if (bLogLoadAsWarning)
				{
					UE_LOG(LogSlate, Log, TEXT("%s"), *LoadLogMessage);
				}
				else
				{
					UE_LOG(LogSlate, Log, TEXT("%s"), *LoadLogMessage);
				}
			}
		}
		else
		{
			FaceAndMemory.Reset();
			UE_LOG(LogSlate, Warning, TEXT("GetFontFace failed to load or process '%s'"), *InFontData.GetFontFilename());
		}
	}
	return FaceAndMemory;
}

const TSet<FName>& FCompositeFontCache::GetFontAttributes(const FFontData& InFontData)
{
	static const TSet<FName> DummyAttributes;

	TSharedPtr<FFreeTypeFace> FaceAndMemory = GetFontFace(InFontData);
	return (FaceAndMemory.IsValid()) ? FaceAndMemory->GetAttributes() : DummyAttributes;
}

void FCompositeFontCache::FlushCompositeFont(const FCompositeFont& InCompositeFont)
{
	CompositeFontToCachedDataMap.Remove(&InCompositeFont);
}

void FCompositeFontCache::FlushCache()
{
	CompositeFontToCachedDataMap.Empty();
	FontFaceMap.Empty();
}

const FCachedCompositeFontData* FCompositeFontCache::GetCachedCompositeFont(const FCompositeFont* const InCompositeFont)
{
	if (!InCompositeFont)
	{
		return nullptr;
	}

	TSharedPtr<FCachedCompositeFontData>* const FoundCompositeFontData = CompositeFontToCachedDataMap.Find(InCompositeFont);
	if (FoundCompositeFontData)
	{
		return FoundCompositeFontData->Get();
	}

	return CompositeFontToCachedDataMap.Add(InCompositeFont, MakeShareable(new FCachedCompositeFontData(*InCompositeFont))).Get();
}

const FFontData* FCompositeFontCache::GetBestMatchFontForAttributes(const FCachedTypefaceData* const InCachedTypefaceData, const TSet<FName>& InFontAttributes)
{
	const FFontData* BestMatchFont = nullptr;
	int32 BestMatchCount = 0;

	const FTypeface& Typeface = InCachedTypefaceData->GetTypeface();
	for (const FTypefaceEntry& TypefaceEntry : Typeface.Fonts)
	{
		const TSet<FName>& FontAttributes = GetFontAttributes(TypefaceEntry.Font);

		int32 MatchCount = 0;
		for (const FName& InAttribute : InFontAttributes)
		{
			if (FontAttributes.Contains(InAttribute))
			{
				++MatchCount;
			}
		}

		if (MatchCount > BestMatchCount || !BestMatchFont)
		{
			BestMatchFont = &TypefaceEntry.Font;
			BestMatchCount = MatchCount;
		}
	}

	return BestMatchFont;
}
