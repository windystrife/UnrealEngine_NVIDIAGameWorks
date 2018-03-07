// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Fonts/CompositeFont.h"
#include "SlateFontInfo.generated.h"

/**
 * Sets the maximum font fallback level, for when a character can't be found in the selected font set.
 *
 * UI code that renders strings from a third party (e.g. player chat in a multiplayer game), should restrict font fallback to localized,
 * (or to no fallback, if international font isn't important), to prevent potential performance problems.
 */
UENUM()
enum class EFontFallback : uint8
{
	/** No fallback font */
	FF_NoFallback,
	/** Fallback to localized font set */
	FF_LocalizedFallback,
	/** Fallback to last resort font set */
	FF_LastResortFallback,
	/** Tries all fallbacks */
	FF_Max
};

/**
 * Settings for applying an outline to a font
 */
USTRUCT(BlueprintType)
struct SLATECORE_API FFontOutlineSettings
{
	GENERATED_USTRUCT_BODY()

	/** Size of the outline in slate units (at 1.0 font scale this unit is a pixel)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=OutlineSettings, meta=(ClampMin="0"))
	int32 OutlineSize;

	/** Optional material to apply to the outline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(AllowedClasses="MaterialInterface"))
	UObject* OutlineMaterial;

	/** The color of the outline for any character in this font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=OutlineSettings)
	FLinearColor OutlineColor;

	/** 
	 * If checked, the outline will be completely translucent where the filled area will be.  This allows for a separate fill alpha value
	 * The trade off when enabling this is slightly worse quality for completely opaque fills where the inner outline border meets the fill area
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=OutlineSettings)
	bool bSeparateFillAlpha;

	FFontOutlineSettings()
		: OutlineSize(0)
		, OutlineMaterial(nullptr)
		, OutlineColor(FLinearColor::Black)
		, bSeparateFillAlpha(false)
	{}

	FFontOutlineSettings(int32 InOutlineSize, FLinearColor InColor = FLinearColor::Black)
		: OutlineSize(InOutlineSize)
		, OutlineMaterial(nullptr)
		, OutlineColor(InColor)
		, bSeparateFillAlpha(false)
	{}

	bool operator==(const FFontOutlineSettings& Other) const
	{
		return OutlineSize == Other.OutlineSize
			&& OutlineMaterial == Other.OutlineMaterial
			&& OutlineColor == Other.OutlineColor
			&& bSeparateFillAlpha == Other.bSeparateFillAlpha;
	}

	friend inline uint32 GetTypeHash(const FFontOutlineSettings& OutlineSettings)
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(OutlineSettings.OutlineSize));
		Hash = HashCombine(Hash, GetTypeHash(OutlineSettings.OutlineMaterial));
		Hash = HashCombine(Hash, GetTypeHash(OutlineSettings.OutlineColor));
		Hash = HashCombine(Hash, GetTypeHash(OutlineSettings.bSeparateFillAlpha));
		return Hash;
	}

	static FFontOutlineSettings NoOutline;
};

/**
 * A representation of a font in Slate.
 */
USTRUCT(BlueprintType)
struct SLATECORE_API FSlateFontInfo
{
	GENERATED_USTRUCT_BODY()

	/** The font object (valid when used from UMG or a Slate widget style asset) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(AllowedClasses="Font", DisplayName="Font Family"))
	const UObject* FontObject;

	/** The material to use when rendering this font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(AllowedClasses="MaterialInterface"))
	UObject* FontMaterial;

	/** Settings for applying an outline to a font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules)
	FFontOutlineSettings OutlineSettings;

	/** The composite font data to use (valid when used with a Slate style set in C++) */
	TSharedPtr<const FCompositeFont> CompositeFont;

	/** The name of the font to use from the default typeface (None will use the first entry) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(DisplayName="Typeface"))
	FName TypefaceFontName;

	/**
	 * The font size is a measure in point values.  The conversion of points to Slate Units is done at 96 dpi.  So if 
	 * you're using a tool like Photoshop to prototype layouts and UI mock ups, be sure to change the default dpi 
	 * measurements from 72 dpi to 96 dpi.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SlateStyleRules, meta=(ClampMin=1, ClampMax=1000))
	int32 Size;

	/** The font fallback level. Runtime only, don't set on shared FSlateFontInfo, as it may change the font elsewhere (make a copy). */
	EFontFallback FontFallback;

#if WITH_EDITORONLY_DATA
private:

	/** The name of the font */
	UPROPERTY()
	FName FontName_DEPRECATED;

	/** The hinting algorithm to use with the font */
	UPROPERTY()
	EFontHinting Hinting_DEPRECATED;
#endif

public:

	/** Default constructor. */
	FSlateFontInfo();

	/**
	 * Creates and initializes a new instance with the specified font, size, and emphasis.
	 *
	 * @param InCompositeFont The font instance to use.
	 * @param InSize The size of the font.
	 * @param InTypefaceFontName The name of the font to use from the default typeface (None will use the first entry)
	 */
	FSlateFontInfo( TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName = NAME_None, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());

	/**
	 * Creates and initializes a new instance with the specified font, size, and emphasis.
	 *
	 * @param InFontObject The font instance to use.
	 * @param InSize The size of the font.
	 * @param InFamilyFontName The name of the font to use from the default typeface (None will use the first entry)
	 */
	FSlateFontInfo( const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName = NAME_None, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const FString& InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default, const FFontOutlineSettings& InOutlineSettings = FFontOutlineSettings());

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const FName& InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const ANSICHAR* InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

	/**
	 * DEPRECATED - Creates and initializes a new instance with the specified font name and size.
	 *
	 * @param InFontName The name of the font.
	 * @param InSize The size of the font.
	 * @param InHinting The type of hinting to use for the font.
	 */
	FSlateFontInfo( const WIDECHAR* InFontName, uint16 InSize, EFontHinting InHinting = EFontHinting::Default );

public:

	/**
	 * Compares this font info with another for equality.
	 *
	 * @param Other The other font info.
	 * @return true if the two font infos are equal, false otherwise.
	 */
	bool operator==( const FSlateFontInfo& Other ) const 
	{
		return FontObject == Other.FontObject
			&& FontMaterial == Other.FontMaterial
			&& OutlineSettings == Other.OutlineSettings
			&& CompositeFont == Other.CompositeFont
			&& TypefaceFontName == Other.TypefaceFontName
			&& Size == Other.Size;
	}

	/**
	 * Compares this font info with another for inequality.
	 *
	 * @param Other The other font info.
	 *
	 * @return false if the two font infos are equal, true otherwise.
	 */
	bool operator!=( const FSlateFontInfo& Other ) const 
	{
		return !(*this == Other);
	}

	/**
	 * Check to see whether this font info has a valid composite font pointer set (either directly or via a UFont)
	 */
	bool HasValidFont() const;

	/**
	 * Get the composite font pointer associated with this font info (either directly or via a UFont)
	 * @note This function will return the fallback font if this font info itself does not contain a valid font. If you want to test whether this font info is empty, use HasValidFont
	 */
	const FCompositeFont* GetCompositeFont() const;

	/**
	 * Calculates a type hash value for a font info.
	 *
	 * Type hashes are used in certain collection types, such as TMap.
	 *
	 * @param FontInfo The font info to calculate the hash for.
	 * @return The hash value.
	 */
	friend inline uint32 GetTypeHash( const FSlateFontInfo& FontInfo )
	{
		uint32 Hash = 0;
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.FontObject));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.FontMaterial));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.OutlineSettings));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.CompositeFont));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.TypefaceFontName));
		Hash = HashCombine(Hash, GetTypeHash(FontInfo.Size));
		return Hash;
	}

#if WITH_EDITORONLY_DATA
	/**
	 * Used to upgrade legacy font into so that it uses composite fonts
	 */
	void PostSerialize(const FArchive& Ar);
#endif

private:

	/**
	 * Used to upgrade legacy font into so that it uses composite fonts
	 */
	void UpgradeLegacyFontInfo(FName LegacyFontName, EFontHinting LegacyHinting);
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FSlateFontInfo>
	: public TStructOpsTypeTraitsBase2<FSlateFontInfo>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
#endif