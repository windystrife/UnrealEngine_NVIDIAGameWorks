// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Textures/TextureAtlas.h"

struct SLATECORE_API FSlateFontKey
{
public:
	FSlateFontKey( const FSlateFontInfo& InInfo, const FFontOutlineSettings& InFontOutlineSettings, const float InScale )
		: FontInfo( InInfo )
		, OutlineSettings(InFontOutlineSettings)
		, Scale( InScale )
		, KeyHash( 0 )
	{
		KeyHash = HashCombine(KeyHash, GetTypeHash(FontInfo));
		KeyHash = HashCombine(KeyHash, GetTypeHash(OutlineSettings.OutlineSize));
		KeyHash = HashCombine(KeyHash, GetTypeHash(OutlineSettings.bSeparateFillAlpha));
		KeyHash = HashCombine(KeyHash, GetTypeHash(Scale));
	}

	FORCEINLINE const FSlateFontInfo& GetFontInfo() const
	{
		return FontInfo;
	}

	FORCEINLINE float GetScale() const
	{
		return Scale;
	}

	FORCEINLINE const FFontOutlineSettings& GetFontOutlineSettings() const
	{
		return OutlineSettings;
	}

	FORCEINLINE bool operator==(const FSlateFontKey& Other ) const
	{
		return FontInfo == Other.FontInfo 
			&& OutlineSettings.OutlineSize == Other.OutlineSettings.OutlineSize 
			&& OutlineSettings.bSeparateFillAlpha == Other.OutlineSettings.bSeparateFillAlpha 
			&& Scale == Other.Scale;
	}

	FORCEINLINE bool operator!=(const FSlateFontKey& Other ) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash( const FSlateFontKey& Key )
	{
		return Key.KeyHash;
	}

private:
	FSlateFontInfo FontInfo;
	FFontOutlineSettings OutlineSettings;
	float Scale;
	uint32 KeyHash;
};

/** Measurement details for a specific character */
struct FCharacterMeasureInfo
{
	/** Width of the character in pixels */
	int16 SizeX;
	/** Height of the character in pixels */
	int16 SizeY;
	/** How many pixels to advance in X after drawing this character (when its part of a string)*/
	int16 XAdvance;
	/** The largest vertical distance above the baseline for any character in the font */
	int16 GlobalAscender;
	/** The largest vertical distance below the baseline for any character in the font */
	int16 GlobalDescender;
	/** The vertical distance from the baseline to the topmost border of the glyph bitmap */
	int16 VerticalOffset;
	/** The horizontal distance from the origin to the leftmost border of the character */
	int16 HorizontalOffset;
	FCharacterMeasureInfo( int16 InSizeX = 0, int16 InSizeY = 0, int16 InXAdvance = 0, int16 InVerticalOffset = 0, int16 InHorizontalOffset = 0 )
		: SizeX(InSizeX),SizeY(InSizeY),XAdvance(InXAdvance),VerticalOffset(InVerticalOffset), HorizontalOffset(InHorizontalOffset)
	{
	}
};

/** Contains pixel data for a character rendered from freetype as well as measurement info */
struct FCharacterRenderData
{
	FCharacterMeasureInfo MeasureInfo;
	/** Measurement data for this character */
	/** Raw pixels created by freetype */
	TArray<uint8> RawPixels;
	/** @todo Doesnt belong here. */
	uint16 MaxHeight;
	/** The character that was rendered */
	TCHAR Char;
	/** The index of the glyph from the FreeType face that was rendered */
	uint32 GlyphIndex;
	/** Whether or not the character has kerning */
	bool HasKerning;
};

/**
 * Interface to all Slate font textures, both atlased and non-atlased
 */
class ISlateFontTexture
{
public:
	virtual ~ISlateFontTexture() {}

	/**
	 * Returns the texture resource used by Slate
	 */
	virtual class FSlateShaderResource* GetSlateTexture() = 0;

	/**
	 * Returns the texture resource used the Engine
	 */
	virtual class FTextureResource* GetEngineTexture() = 0;

	/**
	 * Releases rendering resources of this texture
	 */
	virtual void ReleaseResources() {}
};

/** 
 * Representation of a texture for fonts in which characters are packed tightly based on their bounding rectangle 
 */
class SLATECORE_API FSlateFontAtlas : public ISlateFontTexture, public FSlateTextureAtlas
{
public:
	FSlateFontAtlas( uint32 InWidth, uint32 InHeight );
	virtual ~FSlateFontAtlas();

	/**
	 * Flushes all cached data.
	 */
	void Flush();

	/** 
	 * Adds a character to the texture.
	 *
	 * @param CharInfo	Information about the size of the character
	 */
	const struct FAtlasedTextureSlot* AddCharacter( const FCharacterRenderData& CharInfo );
};

class ISlateFontAtlasFactory
{
public:
	virtual FIntPoint GetAtlasSize() const = 0;
	virtual TSharedRef<FSlateFontAtlas> CreateFontAtlas() const = 0;
	virtual TSharedPtr<ISlateFontTexture> CreateNonAtlasedTexture(const uint32 InWidth, const uint32 InHeight, const TArray<uint8>& InRawData) const = 0;
};
