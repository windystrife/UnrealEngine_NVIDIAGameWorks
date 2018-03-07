// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/SlateFontInfo.h"
#include "Templates/Casts.h"
#include "SlateGlobals.h"
#include "Fonts/FontProviderInterface.h"
#include "Fonts/LegacySlateFontInfoCache.h"


/* FSlateFontInfo structors
 *****************************************************************************/

FFontOutlineSettings FFontOutlineSettings::NoOutline;

FSlateFontInfo::FSlateFontInfo( )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(0)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, Hinting_DEPRECATED(EFontHinting::Default)
#endif
{
}


FSlateFontInfo::FSlateFontInfo( TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName, const FFontOutlineSettings& InOutlineSettings )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, OutlineSettings(InOutlineSettings)
	, CompositeFont(InCompositeFont)
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, Hinting_DEPRECATED(EFontHinting::Default)
#endif
{
	if (!InCompositeFont.IsValid())
	{
		UE_LOG(LogSlate, Warning, TEXT("FSlateFontInfo was constructed with a null FCompositeFont. Slate will be forced to use the fallback font path which may be slower."));
	}
}


FSlateFontInfo::FSlateFontInfo( const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName, const FFontOutlineSettings& InOutlineSettings )
	: FontObject(InFontObject)
	, FontMaterial(nullptr)
	, OutlineSettings(InOutlineSettings)
	, CompositeFont()
	, TypefaceFontName(InTypefaceFontName)
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, Hinting_DEPRECATED(EFontHinting::Default)
#endif
{
	if (InFontObject)
	{
		const IFontProviderInterface* FontProvider = Cast<const IFontProviderInterface>(InFontObject);
		if (!FontProvider || !FontProvider->GetCompositeFont())
		{
			UE_LOG(LogSlate, Verbose, TEXT("'%s' does not provide a composite font that can be used with Slate. Slate will be forced to use the fallback font path which may be slower."), *InFontObject->GetName());
		}
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("FSlateFontInfo was constructed with a null UFont. Slate will be forced to use the fallback font path which may be slower."));
	}
}


FSlateFontInfo::FSlateFontInfo( const FString& InFontName, uint16 InSize, EFontHinting InHinting, const FFontOutlineSettings& InOutlineSettings)
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, OutlineSettings(InOutlineSettings)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, FontName_DEPRECATED(*InFontName)
	, Hinting_DEPRECATED(InHinting)
#endif
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo(FName(*InFontName), InHinting);
}


FSlateFontInfo::FSlateFontInfo( const FName& InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
#endif
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo(InFontName, InHinting);
}


FSlateFontInfo::FSlateFontInfo( const ANSICHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
#endif
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo(FName(InFontName), InHinting);
}


FSlateFontInfo::FSlateFontInfo( const WIDECHAR* InFontName, uint16 InSize, EFontHinting InHinting )
	: FontObject(nullptr)
	, FontMaterial(nullptr)
	, CompositeFont()
	, TypefaceFontName()
	, Size(InSize)
	, FontFallback(EFontFallback::FF_Max)
#if WITH_EDITORONLY_DATA
	, FontName_DEPRECATED(InFontName)
	, Hinting_DEPRECATED(InHinting)
#endif
{
	//Useful for debugging style breakages
	//check( FPaths::FileExists( FontName.ToString() ) );

	UpgradeLegacyFontInfo(FName(InFontName), InHinting);
}


bool FSlateFontInfo::HasValidFont() const
{
	return CompositeFont.IsValid() || FontObject != nullptr;
}


const FCompositeFont* FSlateFontInfo::GetCompositeFont() const
{
	const IFontProviderInterface* FontProvider = Cast<const IFontProviderInterface>(FontObject);
	if (FontProvider)
	{
		const FCompositeFont* const ProvidedCompositeFont = FontProvider->GetCompositeFont();
		return (ProvidedCompositeFont) ? ProvidedCompositeFont : FLegacySlateFontInfoCache::Get().GetLastResortFont().Get();
	}

	if (CompositeFont.IsValid())
	{
		return CompositeFont.Get();
	}

	return FLegacySlateFontInfoCache::Get().GetLastResortFont().Get();
}


#if WITH_EDITORONLY_DATA
void FSlateFontInfo::PostSerialize(const FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_SLATE_COMPOSITE_FONTS && !FontObject)
	{
		UpgradeLegacyFontInfo(FontName_DEPRECATED, Hinting_DEPRECATED);
	}
}
#endif


void FSlateFontInfo::UpgradeLegacyFontInfo(FName LegacyFontName, EFontHinting LegacyHinting)
{
	static const FName SpecialName_DefaultSystemFont("DefaultSystemFont");

	// Special case for using the default system font
	CompositeFont = (LegacyFontName == SpecialName_DefaultSystemFont)
		? FLegacySlateFontInfoCache::Get().GetSystemFont()
		: FLegacySlateFontInfoCache::Get().GetCompositeFont(LegacyFontName, LegacyHinting);
}
