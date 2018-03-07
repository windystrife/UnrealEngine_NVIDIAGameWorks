// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FFreeTypeAdvanceCache;
class FFreeTypeFace;
class FFreeTypeGlyphCache;
class FFreeTypeKerningPairCache;

#ifndef WITH_HARFBUZZ
	#define WITH_HARFBUZZ	0
#endif // WITH_HARFBUZZ

#if PLATFORM_COMPILER_HAS_GENERIC_KEYWORD
	#define generic __identifier(generic)
#endif	//PLATFORM_COMPILER_HAS_GENERIC_KEYWORD

#if WITH_HARFBUZZ
	THIRD_PARTY_INCLUDES_START
	#include "hb.h"
	#include "hb-ft.h"
	THIRD_PARTY_INCLUDES_END
#endif // #if WITH_HARFBUZZ

#if PLATFORM_COMPILER_HAS_GENERIC_KEYWORD
	#undef generic
#endif	//PLATFORM_COMPILER_HAS_GENERIC_KEYWORD

namespace HarfBuzzUtils
{

#if WITH_HARFBUZZ

/** Utility function to append an FString into a hb_buffer_t in the most efficient way based on the string encoding method of the current platform */
void AppendStringToBuffer(const FString& InString, hb_buffer_t* InHarfBuzzTextBuffer);

/** Utility function to append an FString into a hb_buffer_t in the most efficient way based on the string encoding method of the current platform */
void AppendStringToBuffer(const FString& InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer);

#endif // #if WITH_HARFBUZZ

} // namespace HarfBuzzUtils

class FHarfBuzzFontFactory
{
public:
	FHarfBuzzFontFactory(FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache);
	~FHarfBuzzFontFactory();

#if WITH_HARFBUZZ
	/** Create a HarfBuzz font from the given face - must be destroyed with hb_font_destroy when done */
	hb_font_t* CreateFont(const FFreeTypeFace& InFace, const uint32 InGlyphFlags, const int32 InFontSize, const float InFontScale) const;
#endif // WITH_HARFBUZZ

private:
	FFreeTypeGlyphCache* FTGlyphCache;
	FFreeTypeAdvanceCache* FTAdvanceCache;
	FFreeTypeKerningPairCache* FTKerningPairCache;

#if WITH_HARFBUZZ
	hb_font_funcs_t* CustomHarfBuzzFuncs;
#endif // WITH_HARFBUZZ
};
