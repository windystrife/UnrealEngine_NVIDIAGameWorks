// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatformModule.cpp: Implements the FAndroidTargetPlatformModule class.
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Android/AndroidProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "AndroidTargetDevice.h"
#include "AndroidTargetPlatform.h"

#define LOCTEXT_NAMESPACE "FAndroid_ASTCTargetPlatformModule" 

/**
 * Android cooking platform which cooks only ASTC based textures.
 */
class FAndroid_ASTCTargetPlatform
	: public FAndroidTargetPlatform<FAndroid_ASTCPlatformProperties>
{
	virtual FString GetAndroidVariantName( ) override
	{
		return TEXT("ASTC");
	}

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_ASTC", "Android (ASTC)");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_ASTCPlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if( Format == AndroidTexFormat::NameASTC_4x4 ||
			Format == AndroidTexFormat::NameASTC_6x6 ||
			Format == AndroidTexFormat::NameASTC_8x8 ||
			Format == AndroidTexFormat::NameASTC_10x10 ||
			Format == AndroidTexFormat::NameASTC_12x12 ||
			Format == AndroidTexFormat::NameAutoASTC )
		{
			return true;
		}
		return false;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray<FName>& OutFormats) const
	{
		check(Texture);

		// we remap some of the defaults (with PVRTC and ASTC formats)
		static FName FormatRemap[][2] =
		{
			// Default format:				ASTC format:
			{ { FName(TEXT("DXT1")) },		{ FName(TEXT("ASTC_RGB")) } },
			{ { FName(TEXT("DXT5")) },		{ FName(TEXT("ASTC_RGBA")) } },
			{ { FName(TEXT("DXT5n")) },		{ FName(TEXT("ASTC_NormalAG")) } },
			{ { FName(TEXT("BC5")) },		{ FName(TEXT("ASTC_NormalRG")) } },
			{ { FName(TEXT("BC6H")) },		{ FName(TEXT("ASTC_RGB")) } },
			{ { FName(TEXT("BC7")) },		{ FName(TEXT("ASTC_RGBAuto")) } },
			{ { FName(TEXT("AutoDXT")) },	{ FName(TEXT("ASTC_RGBAuto")) } },
		};

		FName TextureFormatName = NAME_None;

		// forward rendering only needs one channel for shadow maps
		if (Texture->LODGroup == TEXTUREGROUP_Shadowmap)
		{
			TextureFormatName = FName(TEXT("G8"));
		}

		// if we didn't assign anything specially, then use the defaults
		if (TextureFormatName == NAME_None)
		{
			TextureFormatName = GetDefaultTextureFormatName(this, Texture, EngineSettings, false);
		}

		// perform any remapping away from defaults
		bool bFoundRemap = false;
		for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); ++RemapIndex)
		{
			if (TextureFormatName == FormatRemap[RemapIndex][0])
			{
				// we found a remapping
				bFoundRemap = true;
				OutFormats.AddUnique(FormatRemap[RemapIndex][1]);
			}
		}

		// if we didn't already remap above, add it now
		if (!bFoundRemap)
		{
			OutFormats.Add(TextureFormatName);
		}
	}


	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		// we remap some of the defaults (with PVRTC and ASTC formats)
		static FName FormatRemap[][2] =
		{
			// Default format:				ASTC format:
			{ { FName(TEXT("DXT1")) },{ FName(TEXT("ASTC_RGB")) } },
			{ { FName(TEXT("DXT5")) },{ FName(TEXT("ASTC_RGBA")) } },
			{ { FName(TEXT("DXT5n")) },{ FName(TEXT("ASTC_NormalAG")) } },
			{ { FName(TEXT("BC5")) },{ FName(TEXT("ASTC_NormalRG")) } },
			{ { FName(TEXT("BC6H")) },{ FName(TEXT("ASTC_RGB")) } },
			{ { FName(TEXT("BC7")) },{ FName(TEXT("ASTC_RGBAuto")) } },
			{ { FName(TEXT("AutoDXT")) },{ FName(TEXT("ASTC_RGBAuto")) } },
		};

		GetAllDefaultTextureFormats(this, OutFormats, false);

		for (int32 RemapIndex = 0; RemapIndex < ARRAY_COUNT(FormatRemap); ++RemapIndex)
		{
			OutFormats.Remove(FormatRemap[RemapIndex][0]);
			OutFormats.AddUnique(FormatRemap[RemapIndex][1]);
		}
	}

#endif

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		return ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ASTC_ShortName", "ASTC");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ASTC"), Priority, GEngineIni) ?
			Priority : 0.9f;
	}
};

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_ASTCTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_ASTCTargetPlatformModule()
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_ASTCTargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FAndroid_ASTCTargetPlatformModule, Android_ASTCTargetPlatform);
