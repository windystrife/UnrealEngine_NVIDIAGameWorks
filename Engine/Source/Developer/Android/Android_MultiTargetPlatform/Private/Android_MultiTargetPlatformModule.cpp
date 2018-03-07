// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Android_MultiTargetPlatformModule.cpp: Implements the Android_Multi target platform variant
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Android/AndroidProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "IAndroid_MultiTargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "AndroidTargetDevice.h"
#include "AndroidTargetPlatform.h"

#define LOCTEXT_NAMESPACE "FAndroid_MultiTargetPlatformModule" 

/**
 * Android cooking platform which cooks multiple texture formats
 */
class FAndroid_MultiTargetPlatform
	: public FAndroidTargetPlatform<FAndroid_MultiPlatformProperties>
{
	TArray<ITargetPlatform*> FormatTargetPlatforms;
	FString FormatTargetString;

	FAndroid_MultiTargetPlatform()
	{
		LoadFormats();
	}

	void LoadFormats()
	{
		TArray<FString> FormatNames;
		FormatNames.Add(TEXT("ASTC"));
		FormatNames.Add(TEXT("ATC"));
		FormatNames.Add(TEXT("PVRTC"));
		FormatNames.Add(TEXT("DXT"));
		FormatNames.Add(TEXT("ETC2"));
		FormatNames.Add(TEXT("ETC1"));

		// sort formats by priority so higher priority formats are packaged (and thus used by the device) first
		FormatNames.Sort([](const FString& A, const FString& B)
			{
				float PriorityA = 0.f;
				float PriorityB = 0.f;
				GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + A), PriorityA, GEngineIni);
				GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + B), PriorityB, GEngineIni);
				return PriorityA > PriorityB;
			});

		FormatTargetPlatforms.Empty();
		FormatTargetString = TEXT("");

		// Load the TargetPlatform module for each format
		for (int32 PlatformFormatIdx = 0; PlatformFormatIdx < FormatNames.Num(); PlatformFormatIdx++)
		{
			bool bEnabled = false;
			FString SettingsName = FString(TEXT("bMultiTargetFormat_")) + *FormatNames[PlatformFormatIdx];
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *SettingsName, bEnabled, GEngineIni);
			if (bEnabled)
			{
				FString PlatformName = FString(TEXT("Android_")) + *FormatNames[PlatformFormatIdx] + TEXT("TargetPlatform");
				ITargetPlatformModule* Module = FModuleManager::LoadModulePtr<ITargetPlatformModule>(*PlatformName);
				if (Module)
				{
					ITargetPlatform* TargetPlatform = Module->GetTargetPlatform();
					if (TargetPlatform)
					{
						if (FormatTargetPlatforms.Num())
						{
							FormatTargetString += TEXT(",");
						}
						FormatTargetString += *FormatNames[PlatformFormatIdx];
						FormatTargetPlatforms.Add(TargetPlatform);
					}
				}
			}
		}

		PlatformInfo::UpdatePlatformDisplayName(TEXT("Android_Multi"), DisplayName());
	}

	virtual FString GetAndroidVariantName() override
	{
		return TEXT("Multi");
	}

	virtual FText DisplayName() const override
	{
		return FText::Format(LOCTEXT("Android_Multi", "Android (Multi:{0})"), FText::FromString(FormatTargetString));
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_MultiPlatformProperties::PlatformName());
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray<FName>& OutFormats) const
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray<FName> PlatformFormats;
			Platform->GetTextureFormats(Texture, PlatformFormats);
			for (FName Format : PlatformFormats)
			{
				OutFormats.AddUnique(Format);
			}
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray<FName> PlatformFormats;
			Platform->GetAllTextureFormats(PlatformFormats);
			for (FName Format : PlatformFormats)
			{
				OutFormats.AddUnique(Format);
			}
		}
	}
#endif	

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_Multi_ShortName", "Multi");
	}

	virtual float GetVariantPriority() const override
	{
		// lowest priority so specific variants are chosen first
		return 0.f;
	}

	// for constructor
	friend class FAndroid_MultiTargetPlatformModule;
};

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_MultiTargetPlatformModule : public IAndroid_MultiTargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_MultiTargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


	// Begin ITargetPlatformModule interface
	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_MultiTargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}
	// End ITargetPlatformModule interface

	// Begin IAndroid_MultiTargetPlatformModule interface
	virtual void NotifySelectedFormatsChanged() override
	{
		static_cast<FAndroid_MultiTargetPlatform*>(GetTargetPlatform())->LoadFormats();
	}
	// End IAndroid_MultiTargetPlatformModule interface

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_MultiTargetPlatformModule, Android_MultiTargetPlatform);
