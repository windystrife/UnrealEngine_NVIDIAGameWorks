// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/FontCacheHarfBuzz.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontCacheFreeType.h"
#include "Fonts/SlateFontRenderer.h"

#if WITH_HARFBUZZ
	#if PLATFORM_WINDOWS
		#include "WindowsHWrapper.h"
		#include "AllowWindowsPlatformAtomics.h"
	#endif
	#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
		#pragma warning(push)
		#pragma warning(disable:4996) // warning C4996: 'strncpy': This function or variable may be unsafe. Consider using strncpy_s instead.
		#pragma warning(disable:28113) // warning C28113: Accessing a local variable dummy via an Interlocked function:  This is an unusual usage which could be reconsidered.
		#ifndef snprintf
			#define snprintf _snprintf
		#endif
	#endif // #if PLATFORM_WINDOWS

	// Include some private headers needed for our font implementation
	THIRD_PARTY_INCLUDES_START
	#include "hb-private.hh"
	#include "hb-font-private.hh"
	THIRD_PARTY_INCLUDES_END

	#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
		#pragma warning(pop)
	#endif // #if PLATFORM_WINDOWS
	#if PLATFORM_WINDOWS
		#include "HideWindowsPlatformAtomics.h"
	#endif
#endif // #if WITH_HARFBUZZ

#if WITH_HARFBUZZ

extern "C"
{

void* HarfBuzzMalloc(size_t InSizeBytes)
{
	return FMemory::Malloc(InSizeBytes);
}

void* HarfBuzzCalloc(size_t InNumItems, size_t InItemSizeBytes)
{
	const size_t AllocSizeBytes = InNumItems * InItemSizeBytes;
	if (AllocSizeBytes > 0)
	{
		void* Ptr = FMemory::Malloc(AllocSizeBytes);
		FMemory::Memzero(Ptr, AllocSizeBytes);
		return Ptr;
	}
	return nullptr;
}

void* HarfBuzzRealloc(void* InPtr, size_t InSizeBytes)
{
	return FMemory::Realloc(InPtr, InSizeBytes);
}

void HarfBuzzFree(void* InPtr)
{
	FMemory::Free(InPtr);
}

} // extern "C"

#endif // #if WITH_HARFBUZZ

namespace HarfBuzzUtils
{

#if WITH_HARFBUZZ

namespace Internal
{

template <bool IsUnicode, size_t TCHARSize>
void AppendStringToBuffer(const FString& InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// todo: SHAPING - This is losing the context information that may be required to shape a sub-section of text.
	//				   In practice this may not be an issue as our platforms should all use the other functions, but to fix it we'd need UTF-8 iteration functions to find the correct points the buffer
	hb_buffer_add_utf8(InHarfBuzzTextBuffer, TCHAR_TO_UTF8(*InString.Mid(InStartIndex, InLength)), -1, 0, -1);
}

template <>
void AppendStringToBuffer<true, 2>(const FString& InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// A unicode encoding with a TCHAR size of 2 bytes is assumed to be UTF-16
	hb_buffer_add_utf16(InHarfBuzzTextBuffer, reinterpret_cast<const uint16_t*>(InString.GetCharArray().GetData()), InString.Len(), InStartIndex, InLength);
}

template <>
void AppendStringToBuffer<true, 4>(const FString& InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	// A unicode encoding with a TCHAR size of 4 bytes is assumed to be UTF-32
	hb_buffer_add_utf32(InHarfBuzzTextBuffer, reinterpret_cast<const uint32_t*>(InString.GetCharArray().GetData()), InString.Len(), InStartIndex, InLength);
}

} // namespace Internal

void AppendStringToBuffer(const FString& InString, hb_buffer_t* InHarfBuzzTextBuffer)
{
	return Internal::AppendStringToBuffer<FPlatformString::IsUnicodeEncoded, sizeof(TCHAR)>(InString, 0, InString.Len(), InHarfBuzzTextBuffer);
}

void AppendStringToBuffer(const FString& InString, const int32 InStartIndex, const int32 InLength, hb_buffer_t* InHarfBuzzTextBuffer)
{
	return Internal::AppendStringToBuffer<FPlatformString::IsUnicodeEncoded, sizeof(TCHAR)>(InString, InStartIndex, InLength, InHarfBuzzTextBuffer);
}

#endif // #if WITH_HARFBUZZ

} // namespace HarfBuzzUtils


#if WITH_FREETYPE && WITH_HARFBUZZ

namespace HarfBuzzFontFunctions
{

hb_user_data_key_t UserDataKey;

struct FUserData
{
	FUserData(const int32 InFontSize, const float InFontScale, FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache)
		: FontSize(InFontSize)
		, FontScale(InFontScale)
		, FTGlyphCache(InFTGlyphCache)
		, FTAdvanceCache(InFTAdvanceCache)
		, FTKerningPairCache(InFTKerningPairCache)
	{
	}

	int32 FontSize;
	float FontScale;
	FFreeTypeGlyphCache* FTGlyphCache;
	FFreeTypeAdvanceCache* FTAdvanceCache;
	FFreeTypeKerningPairCache* FTKerningPairCache;
};

void* CreateUserData(const int32 InFontSize, const float InFontScale, FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache)
{
	return new FUserData(InFontSize, InFontScale, InFTGlyphCache, InFTAdvanceCache, InFTKerningPairCache);
}

void DestroyUserData(void* UserData)
{
	FUserData* UserDataPtr = static_cast<FUserData*>(UserData);
	delete UserDataPtr;
}

namespace Internal
{

FORCEINLINE FT_Face get_ft_face(hb_font_t* InFont)
{
	check(InFont->parent);
	return hb_ft_font_get_face(InFont->parent);
}

FORCEINLINE int32 get_ft_flags(hb_font_t* InFont)
{
	check(InFont->parent);
	return hb_ft_font_get_load_flags(InFont->parent);
}

hb_bool_t get_nominal_glyph(hb_font_t* InFont, void* InFontData, hb_codepoint_t InUnicodeChar, hb_codepoint_t *OutGlyphIndex, void *InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);

	*OutGlyphIndex = FT_Get_Char_Index(FreeTypeFace, InUnicodeChar);

	// If the given font can't render that character (as the fallback font may be missing), try again with the fallback character
	if (InUnicodeChar != 0 && *OutGlyphIndex == 0)
	{
		*OutGlyphIndex = FT_Get_Char_Index(FreeTypeFace, SlateFontRendererUtils::InvalidSubChar);
	}

	return InUnicodeChar == 0 || *OutGlyphIndex != 0;
}

hb_position_t get_glyph_h_advance(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FT_Fixed CachedAdvanceData = 0;
	if (UserDataPtr->FTAdvanceCache->FindOrCache(FreeTypeFace, InGlyphIndex, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale, CachedAdvanceData))
	{
		return (CachedAdvanceData + (1<<9)) >> 10;
	}

	return 0;
}

hb_position_t get_glyph_v_advance(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FT_Fixed CachedAdvanceData = 0;
	if (UserDataPtr->FTAdvanceCache->FindOrCache(FreeTypeFace, InGlyphIndex, FreeTypeFlags | FT_LOAD_VERTICAL_LAYOUT, UserDataPtr->FontSize, UserDataPtr->FontScale, CachedAdvanceData))
	{
		// Note: FreeType's vertical metrics grows downward while other FreeType coordinates have a Y growing upward.  Hence the extra negation.
		return (-CachedAdvanceData + (1<<9)) >> 10;
	}

	return 0;
}

hb_bool_t get_glyph_v_origin(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, hb_position_t* OutX, hb_position_t* OutY, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (UserDataPtr->FTGlyphCache->FindOrCache(FreeTypeFace, InGlyphIndex, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale, CachedGlyphData))
	{
		// Note: FreeType's vertical metrics grows downward while other FreeType coordinates have a Y growing upward.  Hence the extra negation.
		*OutX = CachedGlyphData.GlyphMetrics.horiBearingX -   CachedGlyphData.GlyphMetrics.vertBearingX;
		*OutY = CachedGlyphData.GlyphMetrics.horiBearingY - (-CachedGlyphData.GlyphMetrics.vertBearingY);

		if (InFont->x_scale < 0)
		{
			*OutX = -*OutX;
		}

		if (InFont->y_scale < 0)
		{
			*OutY = -*OutY;
		}

		return true;
	}

	return false;
}

hb_position_t get_glyph_h_kerning(hb_font_t* InFont, void* InFontData, hb_codepoint_t InLeftGlyphIndex, hb_codepoint_t InRightGlyphIndex, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FT_Vector KerningVector;
	if (UserDataPtr->FTKerningPairCache->FindOrCache(FreeTypeFace, FFreeTypeKerningPairCache::FKerningPair(InLeftGlyphIndex, InRightGlyphIndex), FT_KERNING_DEFAULT, UserDataPtr->FontSize, UserDataPtr->FontScale, KerningVector))
	{
		return KerningVector.x;
	}

	return 0;
}

hb_bool_t get_glyph_extents(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, hb_glyph_extents_t* OutExtents, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (UserDataPtr->FTGlyphCache->FindOrCache(FreeTypeFace, InGlyphIndex, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale, CachedGlyphData))
	{
		OutExtents->x_bearing	=  CachedGlyphData.GlyphMetrics.horiBearingX;
		OutExtents->y_bearing	=  CachedGlyphData.GlyphMetrics.horiBearingY;
		OutExtents->width		=  CachedGlyphData.GlyphMetrics.width;
		OutExtents->height		= -CachedGlyphData.GlyphMetrics.height;
		return true;
	}

	return false;
}

hb_bool_t get_glyph_contour_point(hb_font_t* InFont, void* InFontData, hb_codepoint_t InGlyphIndex, unsigned int InPointIndex, hb_position_t* OutX, hb_position_t* OutY, void* InUserData)
{
	FT_Face FreeTypeFace = get_ft_face(InFont);
	const int32 FreeTypeFlags = get_ft_flags(InFont);
	const FUserData* UserDataPtr = static_cast<FUserData*>(hb_font_get_user_data(InFont, &UserDataKey));

	FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
	if (UserDataPtr->FTGlyphCache->FindOrCache(FreeTypeFace, InGlyphIndex, FreeTypeFlags, UserDataPtr->FontSize, UserDataPtr->FontScale, CachedGlyphData))
	{
		if (InPointIndex < static_cast<unsigned int>(CachedGlyphData.OutlinePoints.Num()))
		{
			*OutX = CachedGlyphData.OutlinePoints[InPointIndex].x;
			*OutY = CachedGlyphData.OutlinePoints[InPointIndex].y;
			return true;
		}
	}

	return false;
}

} // namespace Internal

} // namespace HarfBuzzFontFunctions

#endif // WITH_FREETYPE && WITH_HARFBUZZ

FHarfBuzzFontFactory::FHarfBuzzFontFactory(FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache)
	: FTGlyphCache(InFTGlyphCache)
	, FTAdvanceCache(InFTAdvanceCache)
	, FTKerningPairCache(InFTKerningPairCache)
{
	check(FTGlyphCache);
	check(FTAdvanceCache);
	check(FTKerningPairCache);

#if WITH_HARFBUZZ
	CustomHarfBuzzFuncs = hb_font_funcs_create();

	hb_font_funcs_set_nominal_glyph_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_nominal_glyph, nullptr, nullptr);
	hb_font_funcs_set_glyph_h_advance_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_h_advance, nullptr, nullptr);
	hb_font_funcs_set_glyph_v_advance_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_v_advance, nullptr, nullptr);
	hb_font_funcs_set_glyph_v_origin_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_v_origin, nullptr, nullptr);
	hb_font_funcs_set_glyph_h_kerning_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_h_kerning, nullptr, nullptr);
	hb_font_funcs_set_glyph_extents_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_extents, nullptr, nullptr);
	hb_font_funcs_set_glyph_contour_point_func(CustomHarfBuzzFuncs, &HarfBuzzFontFunctions::Internal::get_glyph_contour_point, nullptr, nullptr);

	hb_font_funcs_make_immutable(CustomHarfBuzzFuncs);
#endif // WITH_HARFBUZZ
}

FHarfBuzzFontFactory::~FHarfBuzzFontFactory()
{
#if WITH_HARFBUZZ
	hb_font_funcs_destroy(CustomHarfBuzzFuncs);
	CustomHarfBuzzFuncs = nullptr;
#endif // WITH_HARFBUZZ
}

#if WITH_HARFBUZZ

hb_font_t* FHarfBuzzFontFactory::CreateFont(const FFreeTypeFace& InFace, const uint32 InGlyphFlags, const int32 InFontSize, const float InFontScale) const
{
	hb_font_t* HarfBuzzFont = nullptr;

#if WITH_FREETYPE
	FT_Face FreeTypeFace = InFace.GetFace();

	FreeTypeUtils::ApplySizeAndScale(FreeTypeFace, InFontSize, InFontScale);

	// Create a sub-font from the default FreeType implementation so we can override some font functions to provide low-level caching
	{
		hb_font_t* HarfBuzzFTFont = hb_ft_font_create(FreeTypeFace, nullptr);
		hb_ft_font_set_load_flags(HarfBuzzFTFont, InGlyphFlags);

		// The default FreeType implementation doesn't apply the font scale, so we have to do that ourselves (in 16.16 space for maximum precision)
		const FT_Long FixedFontScale = FreeTypeUtils::ConvertPixelTo16Dot16<FT_Long>(InFontScale);
		HarfBuzzFTFont->x_scale = FT_MulFix(HarfBuzzFTFont->x_scale, FixedFontScale);
		HarfBuzzFTFont->y_scale = FT_MulFix(HarfBuzzFTFont->y_scale, FixedFontScale);

		HarfBuzzFont = hb_font_create_sub_font(HarfBuzzFTFont);
		
		hb_font_destroy(HarfBuzzFTFont);
	}

	hb_font_set_funcs(HarfBuzzFont, CustomHarfBuzzFuncs, nullptr, nullptr);

	hb_font_set_user_data(
		HarfBuzzFont, 
		&HarfBuzzFontFunctions::UserDataKey, 
		HarfBuzzFontFunctions::CreateUserData(InFontSize, InFontScale, FTGlyphCache, FTAdvanceCache, FTKerningPairCache), 
		&HarfBuzzFontFunctions::DestroyUserData, 
		true
		);
#endif // WITH_FREETYPE

	return HarfBuzzFont;
}

#endif // WITH_HARFBUZZ
