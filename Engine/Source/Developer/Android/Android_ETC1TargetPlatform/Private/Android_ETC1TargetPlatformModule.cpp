// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidETC1_TargetPlatformModule.cpp: Implements the FAndroidETC1_TargetPlatformModule class.
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

#define LOCTEXT_NAMESPACE "FAndroid_ETC1TargetPlatformModule" 


/**
 * Android cooking platform which cooks only ETC1 based textures.
 */
class FAndroid_ETC1TargetPlatform
	: public FAndroidTargetPlatform<FAndroid_ETC1PlatformProperties>
{
public:

	// Begin FAndroidTargetPlatform overrides

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_ETC1", "Android (ETC1)");
	}

	virtual FString GetAndroidVariantName( )
	{
		return TEXT("ETC1");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_ETC1PlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if (Format == AndroidTexFormat::NameETC1 || 
			Format == AndroidTexFormat::NameAutoETC1)
		{
			return true;
		}

		return false;
	}

	// End FAndroidTargetPlatform overrides

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		return ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ETC1_ShortName", "ETC1");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC1"), Priority, GEngineIni) ?
			Priority : 0.1f;
	}
};


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_ETC1TargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_ETC1TargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}

public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_ETC1TargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_ETC1TargetPlatformModule, Android_ETC1TargetPlatform);
