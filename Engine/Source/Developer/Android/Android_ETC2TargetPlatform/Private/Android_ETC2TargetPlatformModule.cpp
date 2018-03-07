// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidETC2_TargetPlatformModule.cpp: Implements the FAndroidETC2_TargetPlatformModule class.
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

#define LOCTEXT_NAMESPACE "FAndroid_ETC2TargetPlatformModule" 


/**
 * Android cooking platform which cooks only ETC2 based textures.
 */
class FAndroid_ETC2TargetPlatform
	: public FAndroidTargetPlatform<FAndroid_ETC2PlatformProperties>
{
public:

	// Begin FAndroidTargetPlatform overrides

	virtual FText DisplayName() const override
	{
		return LOCTEXT("Android_ETC2", "Android (ETC2)");
	}

	virtual FString GetAndroidVariantName()
	{
		return TEXT("ETC2");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_ETC2PlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat(FName Format) const override
	{
		if (Format == AndroidTexFormat::NameETC2_RGB ||
			Format == AndroidTexFormat::NameETC2_RGBA ||
			Format == AndroidTexFormat::NameAutoETC2)
		{
			return true;
		}

		return false;
	}

	// End FAndroidTargetPlatform overrides

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return GLESVersion >= 0x30000;
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_ETC2_ShortName", "ETC2");
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC2"), Priority, GEngineIni) ?
			Priority : 0.2f;
	}
};


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_ETC2TargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_ETC2TargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}

public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_ETC2TargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_ETC2TargetPlatformModule, Android_ETC2TargetPlatform);
