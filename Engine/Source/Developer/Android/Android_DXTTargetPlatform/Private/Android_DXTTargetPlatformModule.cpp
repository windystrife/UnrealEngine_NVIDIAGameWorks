// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidDXT_TargetPlatformModule.cpp: Implements the FAndroidDXT_TargetPlatformModule class.
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

#define LOCTEXT_NAMESPACE "FAndroid_DXTTargetPlatformModule" 


/**
 * Android cooking platform which cooks only DXT based textures.
 */
class FAndroid_DXTTargetPlatform
	: public FAndroidTargetPlatform<FAndroid_DXTPlatformProperties>
{
	virtual FString GetAndroidVariantName( ) override
	{
		return TEXT("DXT");
	}

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_DXT", "Android (DXT)");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_DXTPlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if( Format == AndroidTexFormat::NameDXT1 ||
			Format == AndroidTexFormat::NameDXT5 ||
			Format == AndroidTexFormat::NameAutoDXT )
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_DXT_ShortName", "DXT");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_DXT"), Priority, GEngineIni) ?
			Priority : 0.6f;
	}
};

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_DXTTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_DXTTargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_DXTTargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_DXTTargetPlatformModule, Android_DXTTargetPlatform);
