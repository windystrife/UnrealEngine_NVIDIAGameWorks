// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCacheFreeType.h"
#include "SlateGlobals.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"

#if WITH_FREETYPE

// The total amount of memory freetype allocates internally
DECLARE_MEMORY_STAT(TEXT("FreeType Total Allocated Memory"), STAT_SlateFreetypeAllocatedMemory, STATGROUP_SlateMemory);

// The active counts of resident and streaming fonts
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Resident Fonts"), STAT_SlateResidentFontCount, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Streaming Fonts"), STAT_SlateStreamingFontCount, STATGROUP_SlateMemory);

namespace FreeTypeMemory
{

static void* Alloc(FT_Memory Memory, long Size)
{
	void* Result = FMemory::Malloc(Size);

#if STATS
	const SIZE_T ActualSize = FMemory::GetAllocSize( Result );
	INC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, ActualSize);
#endif

	return Result;
}

static void* Realloc(FT_Memory Memory, long CurSize, long NewSize, void* Block)
{

#if STATS
	long DeltaNewSize = NewSize - CurSize;
	INC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, DeltaNewSize);
#endif

	return FMemory::Realloc(Block, NewSize);
}

static void Free(FT_Memory Memory, void* Block)
{	
#if STATS
	const SIZE_T ActualSize = FMemory::GetAllocSize( Block );
	DEC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, ActualSize);
#endif
	return FMemory::Free(Block);
}

} // namespace FreeTypeMemory

#endif // WITH_FREETYPE


namespace FreeTypeUtils
{

#if WITH_FREETYPE

void ApplySizeAndScale(FT_Face InFace, const int32 InFontSize, const float InFontScale)
{
	FT_Error Error = FT_Set_Char_Size(InFace, 0, ConvertPixelTo26Dot6<FT_F26Dot6>(InFontSize), FreeTypeConstants::HorizontalDPI, FreeTypeConstants::VerticalDPI);

	check(Error == 0);

	if (InFontScale != 1.0f)
	{
		FT_Matrix ScaleMatrix;
		ScaleMatrix.xy = 0;
		ScaleMatrix.xx = ConvertPixelTo16Dot16<FT_Fixed>(InFontScale);
		ScaleMatrix.yy = ConvertPixelTo16Dot16<FT_Fixed>(InFontScale);
		ScaleMatrix.yx = 0;
		FT_Set_Transform(InFace, &ScaleMatrix, nullptr);
	}
	else
	{
		FT_Set_Transform(InFace, nullptr, nullptr);
	}

	check(Error == 0);
}

FT_Error LoadGlyph(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale)
{
	ApplySizeAndScale(InFace, InFontSize, InFontScale);
	return FT_Load_Glyph(InFace, InGlyphIndex, InLoadFlags);
}

FT_Pos GetHeight(FT_Face InFace, const EFontLayoutMethod InLayoutMethod)
{
	return (InLayoutMethod == EFontLayoutMethod::Metrics) ? (FT_Pos)InFace->height : (InFace->bbox.yMax - InFace->bbox.yMin);
}

FT_Pos GetScaledHeight(FT_Face InFace, const EFontLayoutMethod InLayoutMethod)
{
	return (InLayoutMethod == EFontLayoutMethod::Metrics) ? InFace->size->metrics.height : FT_MulFix(InFace->bbox.yMax - InFace->bbox.yMin, InFace->size->metrics.y_scale);
}

FT_Pos GetAscender(FT_Face InFace, const EFontLayoutMethod InLayoutMethod)
{
	return (InLayoutMethod == EFontLayoutMethod::Metrics) ? InFace->size->metrics.ascender : FT_MulFix(InFace->bbox.yMax, InFace->size->metrics.y_scale);
}

FT_Pos GetDescender(FT_Face InFace, const EFontLayoutMethod InLayoutMethod)
{
	return (InLayoutMethod == EFontLayoutMethod::Metrics) ? InFace->size->metrics.descender : FT_MulFix(InFace->bbox.yMin, InFace->size->metrics.y_scale);
}

#endif // WITH_FREETYPE

}


FFreeTypeLibrary::FFreeTypeLibrary()
{
#if WITH_FREETYPE
	CustomMemory = static_cast<FT_Memory>(FMemory::Malloc(sizeof(*CustomMemory)));

	// Init FreeType
	CustomMemory->alloc = &FreeTypeMemory::Alloc;
	CustomMemory->realloc = &FreeTypeMemory::Realloc;
	CustomMemory->free = &FreeTypeMemory::Free;
	CustomMemory->user = nullptr;

	FT_Error Error = FT_New_Library(CustomMemory, &FTLibrary);
		
	if (Error)
	{
		checkf(0, TEXT("Could not init FreeType. Error code: %d"), Error);
	}
		
	FT_Add_Default_Modules(FTLibrary);

	static bool bLoggedVersion = false;

	// Write out the version of freetype for debugging purposes
	if(!bLoggedVersion)
	{
		bLoggedVersion = true;
		FT_Int Major, Minor, Patch;

		FT_Library_Version(FTLibrary, &Major, &Minor, &Patch);
		UE_LOG(LogSlate, Log, TEXT("Using Freetype %d.%d.%d"), Major, Minor, Patch);
	}

#endif // WITH_FREETYPE
}

FFreeTypeLibrary::~FFreeTypeLibrary()
{
#if WITH_FREETYPE
	FT_Done_Library(FTLibrary);
	FMemory::Free(CustomMemory);
#endif // WITH_FREETYPE
}


FFreeTypeFace::FFreeTypeFace(const FFreeTypeLibrary* InFTLibrary, FFontFaceDataConstRef InMemory, const EFontLayoutMethod InLayoutMethod)
#if WITH_FREETYPE
	: FTFace(nullptr)
	, Memory(MoveTemp(InMemory))
#endif // WITH_FREETYPE
{
	LayoutMethod = InLayoutMethod;

#if WITH_FREETYPE
	FT_New_Memory_Face(InFTLibrary->GetLibrary(), Memory->GetData().GetData(), static_cast<FT_Long>(Memory->GetData().Num()), 0, &FTFace);

	ParseAttributes();

	if (Memory->HasData())
	{
		INC_DWORD_STAT_BY(STAT_SlateResidentFontCount, 1);
	}
#endif // WITH_FREETYPE
}

FFreeTypeFace::FFreeTypeFace(const FFreeTypeLibrary* InFTLibrary, const FString& InFilename, const EFontLayoutMethod InLayoutMethod)
#if WITH_FREETYPE
	: FTFace(nullptr)
	, FTStreamHandler(InFilename)
#endif // WITH_FREETYPE
{
	LayoutMethod = InLayoutMethod;

#if WITH_FREETYPE
	FMemory::Memzero(FTStream);
	FTStream.size = FTStreamHandler.FontSizeBytes;
	FTStream.descriptor.pointer = &FTStreamHandler;
	FTStream.close = &FFTStreamHandler::CloseFile;
	FTStream.read = &FFTStreamHandler::ReadData;

	FMemory::Memzero(FTFaceOpenArgs);
	FTFaceOpenArgs.flags = FT_OPEN_STREAM;
	FTFaceOpenArgs.stream = &FTStream;

	FT_Open_Face(InFTLibrary->GetLibrary(), &FTFaceOpenArgs, 0, &FTFace);

	ParseAttributes();

	INC_DWORD_STAT_BY(STAT_SlateStreamingFontCount, 1);
#endif // WITH_FREETYPE
}

FFreeTypeFace::~FFreeTypeFace()
{
#if WITH_FREETYPE
	if (FTFace)
	{
		if (Memory.IsValid() && Memory->HasData())
		{
			DEC_DWORD_STAT_BY(STAT_SlateResidentFontCount, 1);
		}
		else
		{
			DEC_DWORD_STAT_BY(STAT_SlateStreamingFontCount, 1);
		}

		FT_Done_Face(FTFace);

		FTStreamHandler = FFTStreamHandler();
	}
#endif // WITH_FREETYPE
}

#if WITH_FREETYPE
void FFreeTypeFace::ParseAttributes()
{
	if (FTFace)
	{
		// Parse out the font attributes
		TArray<FString> Styles;
		FString(FTFace->style_name).ParseIntoArray(Styles, TEXT(" "), true);

		for (const FString& Style : Styles)
		{
			Attributes.Add(*Style);
		}
	}
}

FFreeTypeFace::FFTStreamHandler::FFTStreamHandler()
	: FileHandle(nullptr)
	, FontSizeBytes(0)
{
}

FFreeTypeFace::FFTStreamHandler::FFTStreamHandler(const FString& InFilename)
	: FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*InFilename))
	, FontSizeBytes(FileHandle ? FileHandle->Size() : 0)
{
}

FFreeTypeFace::FFTStreamHandler::~FFTStreamHandler()
{
	check(!FileHandle);
}

void FFreeTypeFace::FFTStreamHandler::CloseFile(FT_Stream InStream)
{
	FFTStreamHandler* MyStream = (FFTStreamHandler*)InStream->descriptor.pointer;

	if (MyStream->FileHandle)
	{
		delete MyStream->FileHandle;
		MyStream->FileHandle = nullptr;
	}
}

unsigned long FFreeTypeFace::FFTStreamHandler::ReadData(FT_Stream InStream, unsigned long InOffset, unsigned char* InBuffer, unsigned long InCount)
{
	FFTStreamHandler* MyStream = (FFTStreamHandler*)InStream->descriptor.pointer;

	if (MyStream->FileHandle)
	{
		if (!MyStream->FileHandle->Seek(InOffset))
		{
			return 0;
		}
	}

	if (InCount > 0)
	{
		if (MyStream->FileHandle)
		{
			if (!MyStream->FileHandle->Read(InBuffer, InCount))
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	return InCount;
}
#endif // WITH_FREETYPE


#if WITH_FREETYPE

bool FFreeTypeGlyphCache::FindOrCache(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale, FCachedGlyphData& OutCachedGlyphData)
{
	const FCachedGlyphKey CachedGlyphKey(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);

	// Try and find the data from the cache...
	{
		const FCachedGlyphData* FoundCachedGlyphData = CachedGlyphDataMap.Find(CachedGlyphKey);
		if (FoundCachedGlyphData)
		{
			OutCachedGlyphData = *FoundCachedGlyphData;
			return true;
		}
	}

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FreeTypeUtils::LoadGlyph(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);
	if (Error == 0)
	{
		OutCachedGlyphData = FCachedGlyphData();
		OutCachedGlyphData.Height = InFace->height;
		OutCachedGlyphData.GlyphMetrics = InFace->glyph->metrics;
		OutCachedGlyphData.SizeMetrics = InFace->size->metrics;

		if (InFace->glyph->outline.n_points > 0)
		{
			const int32 NumPoints = static_cast<int32>(InFace->glyph->outline.n_points);
			OutCachedGlyphData.OutlinePoints.Reserve(NumPoints);
			for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				OutCachedGlyphData.OutlinePoints.Add(InFace->glyph->outline.points[PointIndex]);
			}
		}

		CachedGlyphDataMap.Add(CachedGlyphKey, OutCachedGlyphData);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeGlyphCache::FlushCache()
{
#if WITH_FREETYPE
	CachedGlyphDataMap.Empty();
#endif // WITH_FREETYPE
}


#if WITH_FREETYPE

bool FFreeTypeAdvanceCache::FindOrCache(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale, FT_Fixed& OutCachedAdvance)
{
	const FCachedAdvanceKey CachedAdvanceKey(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);

	// Try and find the advance from the cache...
	{
		const FT_Fixed* FoundCachedAdvance = CachedAdvanceMap.Find(CachedAdvanceKey);
		if (FoundCachedAdvance)
		{
			OutCachedAdvance = *FoundCachedAdvance;
			return true;
		}
	}

	FreeTypeUtils::ApplySizeAndScale(InFace, InFontSize, 1.0f);

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FT_Get_Advance(InFace, InGlyphIndex, InLoadFlags, &OutCachedAdvance);
	if (Error == 0)
	{
		// We apply our own scaling as FreeType doesn't always produce the correct results for all fonts when applying the scale via the transform matrix
		const FT_Long FixedFontScale = FreeTypeUtils::ConvertPixelTo16Dot16<FT_Long>(InFontScale);
		OutCachedAdvance = FT_MulFix(OutCachedAdvance, FixedFontScale);

		CachedAdvanceMap.Add(CachedAdvanceKey, OutCachedAdvance);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeAdvanceCache::FlushCache()
{
#if WITH_FREETYPE
	CachedAdvanceMap.Empty();
#endif // WITH_FREETYPE
}


#if WITH_FREETYPE

bool FFreeTypeKerningPairCache::FindOrCache(FT_Face InFace, const FKerningPair& InKerningPair, const int32 InKerningFlags, const int32 InFontSize, const float InFontScale, FT_Vector& OutKerning)
{
	// Skip the cache if the font itself doesn't have kerning
	if (!FT_HAS_KERNING(InFace))
	{
		OutKerning.x = 0;
		OutKerning.y = 0;
		return true;
	}

	const FCachedKerningPairKey CachedKerningPairKey(InFace, InKerningPair, InKerningFlags, InFontSize, InFontScale);

	// Try and find the kerning from the cache...
	{
		const FT_Vector* FoundCachedKerning = CachedKerningPairMap.Find(CachedKerningPairKey);
		if (FoundCachedKerning)
		{
			OutKerning = *FoundCachedKerning;
			return true;
		}
	}

	FreeTypeUtils::ApplySizeAndScale(InFace, InFontSize, 1.0f);

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FT_Get_Kerning(InFace, InKerningPair.FirstGlyphIndex, InKerningPair.SecondGlyphIndex, InKerningFlags, &OutKerning);
	if (Error == 0)
	{
		if (InKerningFlags != FT_KERNING_UNSCALED)
		{
			// We apply our own scaling as FreeType doesn't always produce the correct results for all fonts when applying the scale via the transform matrix
			const FT_Long FixedFontScale = FreeTypeUtils::ConvertPixelTo16Dot16<FT_Long>(InFontScale);
			OutKerning.x = FT_MulFix(OutKerning.x, FixedFontScale);
			OutKerning.y = FT_MulFix(OutKerning.y, FixedFontScale);
		}

		CachedKerningPairMap.Add(CachedKerningPairKey, OutKerning);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeKerningPairCache::FlushCache()
{
#if WITH_FREETYPE
	CachedKerningPairMap.Empty();
#endif // WITH_FREETYPE
}
