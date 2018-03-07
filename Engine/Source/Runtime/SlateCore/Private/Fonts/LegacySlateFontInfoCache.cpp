// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/LegacySlateFontInfoCache.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"
#include "Fonts/FontBulkData.h"

TSharedPtr<FLegacySlateFontInfoCache> FLegacySlateFontInfoCache::Singleton;

FLegacySlateFontInfoCache& FLegacySlateFontInfoCache::Get()
{
	if (!Singleton.IsValid())
	{
		Singleton = MakeShareable(new FLegacySlateFontInfoCache());

		FInternationalization::Get().OnCultureChanged().AddSP(Singleton.Get(), &FLegacySlateFontInfoCache::HandleCultureChanged);
	}
	return *Singleton;
}

FLegacySlateFontInfoCache::FLegacySlateFontInfoCache()
	: LocalizedFallbackFontRevision(0)
	, LocalizedFallbackFontDataHistoryVersion(0)
	, LocalizedFallbackFontFrameCounter(0)
{
	LastResortFontPath = FPaths::EngineContentDir() / TEXT("SlateDebug/Fonts/LastResort.ttf");
	bIsLastResortFontAvailable = FPaths::FileExists(LastResortFontPath);
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetCompositeFont(const FName& InLegacyFontName, const EFontHinting InLegacyFontHinting)
{
	if (InLegacyFontName.IsNone())
	{
		return nullptr;
	}

	FString LegacyFontPath = InLegacyFontName.ToString();

	// Work out what the given path is supposed to be relative to
	if (!FPaths::FileExists(LegacyFontPath))
	{
		// UMG assets specify the path either relative to the game or engine content directories - test both
		LegacyFontPath = FPaths::ProjectContentDir() / InLegacyFontName.ToString();
		if (!FPaths::FileExists(LegacyFontPath))
		{
			LegacyFontPath = FPaths::EngineContentDir() / InLegacyFontName.ToString();
			if (!FPaths::FileExists(LegacyFontPath))
			{
				// Missing font file - just use what we were given
				LegacyFontPath = InLegacyFontName.ToString();
			}
		}
	}

	const FLegacyFontKey LegacyFontKey(*LegacyFontPath, InLegacyFontHinting);

	{
		TSharedPtr<const FCompositeFont>* const ExistingCompositeFont = LegacyFontNameToCompositeFont.Find(LegacyFontKey);
		if(ExistingCompositeFont)
		{
			return *ExistingCompositeFont;
		}
	}

	TSharedRef<const FCompositeFont> NewCompositeFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, LegacyFontPath, InLegacyFontHinting, EFontLoadingPolicy::LazyLoad));
	LegacyFontNameToCompositeFont.Add(LegacyFontKey, NewCompositeFont);
	return NewCompositeFont;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetSystemFont()
{
	if (!SystemFont.IsValid())
	{
		const TArray<uint8> FontBytes = FPlatformMisc::GetSystemFontBytes();
		if (FontBytes.Num() > 0)
		{
			const FString FontFilename = FPaths::EngineIntermediateDir() / TEXT("DefaultSystemFont.ttf");
			if (FFileHelper::SaveArrayToFile(FontBytes, *FontFilename))
			{
				SystemFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, FontFilename, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
			}
		}
	}

	return SystemFont;
}

const FFontData& FLegacySlateFontInfoCache::GetLocalizedFallbackFontData()
{
	// GetLocalizedFallbackFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LocalizedFallbackFontDataCS);

	// The fallback font can change if the active culture is changed
	const uint16 CurrentHistoryVersion = FTextLocalizationManager::Get().GetTextRevision();
	const uint64 CurrentFrameCounter = GFrameCounter;

	// Only allow the fallback font to be updated once per-frame, as a culture change mid-frame could cause it to change unexpectedly and invalidate some assumptions that the font cache makes
	// By only allowing it to update once per-frame, we ensure that the font cache has been flushed (which happens at the end of the frame) before we return a new font
	if (!LocalizedFallbackFontData.IsValid() || (LocalizedFallbackFontDataHistoryVersion != CurrentHistoryVersion && LocalizedFallbackFontFrameCounter != CurrentFrameCounter))
	{
		LocalizedFallbackFontDataHistoryVersion = CurrentHistoryVersion;
		LocalizedFallbackFontFrameCounter = CurrentFrameCounter;

		TSharedPtr<const FFontData> PreviousLocalizedFallbackFontData = LocalizedFallbackFontData;

		const FString FallbackFontPath = FPaths::EngineContentDir() / TEXT("Slate/Fonts/") / (NSLOCTEXT("Slate", "FallbackFont", "DroidSansFallback").ToString() + TEXT(".ttf"));
		LocalizedFallbackFontData = AllLocalizedFallbackFontData.FindRef(FallbackFontPath);

		if (!LocalizedFallbackFontData.IsValid())
		{
			LocalizedFallbackFontData = MakeShareable(new FFontData(FallbackFontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
			AllLocalizedFallbackFontData.Add(FallbackFontPath, LocalizedFallbackFontData);
		}

		if (LocalizedFallbackFontData != PreviousLocalizedFallbackFontData)
		{
			// Only bump the revision if the font has actually changed
			while (++LocalizedFallbackFontRevision == 0) {} // Zero is special, don't allow an overflow to stay at zero
		}
	}

	return *LocalizedFallbackFontData;
}

uint16 FLegacySlateFontInfoCache::GetLocalizedFallbackFontRevision() const
{
	return LocalizedFallbackFontRevision;
}

bool FLegacySlateFontInfoCache::IsLastResortFontAvailable() const
{
	return bIsLastResortFontAvailable;
}

TSharedPtr<const FCompositeFont> FLegacySlateFontInfoCache::GetLastResortFont()
{
	// GetLastResortFont may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontCS);

	if (!LastResortFont.IsValid() && bIsLastResortFontAvailable)
	{
		const FFontData& FontData = GetLastResortFontData();
		LastResortFont = MakeShareable(new FStandaloneCompositeFont(NAME_None, FontData.GetFontFilename(), FontData.GetHinting(), FontData.GetLoadingPolicy()));
	}

	return LastResortFont;
}

const FFontData& FLegacySlateFontInfoCache::GetLastResortFontData()
{
	// GetLastResortFontData is called directly from the font cache, so may be called from multiple threads at once
	FScopeLock Lock(&LastResortFontDataCS);

	if (!LastResortFontData.IsValid())
	{
		LastResortFontData = MakeShareable(new FFontData(bIsLastResortFontAvailable ? LastResortFontPath : FString(), EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
	}

	return *LastResortFontData;
}

void FLegacySlateFontInfoCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FString, TSharedPtr<const FFontData>>& Pair : AllLocalizedFallbackFontData)
	{
		const_cast<FFontData&>(*Pair.Value).AddReferencedObjects(Collector);
	}

	if (LocalizedFallbackFontData.IsValid())
	{
		const_cast<FFontData&>(*LocalizedFallbackFontData).AddReferencedObjects(Collector);
	}

	if (LastResortFontData.IsValid())
	{
		const_cast<FFontData&>(*LastResortFontData).AddReferencedObjects(Collector);
	}
}

void FLegacySlateFontInfoCache::HandleCultureChanged()
{
	// We set this to the current frame count, as this will prevent the fallback font being updated for the remainder of this frame (as the culture change may have affected the fallback font used)
	LocalizedFallbackFontFrameCounter = GFrameCounter;
}
