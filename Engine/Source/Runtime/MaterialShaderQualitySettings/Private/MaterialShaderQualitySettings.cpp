// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialShaderQualitySettings.h"
#include "UObject/Package.h"
#include "ShaderPlatformQualitySettings.h"
#include "Misc/SecureHash.h"
#include "RHI.h"

UMaterialShaderQualitySettings* UMaterialShaderQualitySettings::RenderQualitySingleton = nullptr;

UMaterialShaderQualitySettings::UMaterialShaderQualitySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UMaterialShaderQualitySettings* UMaterialShaderQualitySettings::Get()
{
	if( RenderQualitySingleton == nullptr )
	{
		static const TCHAR* SettingsContainerName = TEXT("MaterialShaderQualitySettingsContainer");

		RenderQualitySingleton = FindObject<UMaterialShaderQualitySettings>(GetTransientPackage(), SettingsContainerName);

		if (RenderQualitySingleton == nullptr)
		{
			RenderQualitySingleton = NewObject<UMaterialShaderQualitySettings>(GetTransientPackage(), UMaterialShaderQualitySettings::StaticClass(), SettingsContainerName);
			RenderQualitySingleton->AddToRoot();
		}
	}
	return RenderQualitySingleton;
}

#if WITH_EDITOR
const FName& UMaterialShaderQualitySettings::GetPreviewPlatform()
{
	return PreviewPlatformName;
}

void UMaterialShaderQualitySettings::SetPreviewPlatform(FName PlatformName)
{
	 UShaderPlatformQualitySettings** FoundPlatform = ForwardSettingMap.Find(PlatformName);
	 PreviewPlatformSettings = FoundPlatform == nullptr ? nullptr : *FoundPlatform;
	 PreviewPlatformName = PlatformName;
}
#endif

UShaderPlatformQualitySettings* UMaterialShaderQualitySettings::GetOrCreatePlatformSettings(FName PlatformName)
{
	auto* PlatformSettings = ForwardSettingMap.Find(PlatformName);
	if (PlatformSettings == nullptr)
	{
		FString ObjectName("ForwardShadingQuality_");
		PlatformName.AppendString(ObjectName);

		auto* ForwardQualitySettings = FindObject<UShaderPlatformQualitySettings>(this, *ObjectName);
		if (ForwardQualitySettings == nullptr)
		{
			FName ForwardSettingsName(*ObjectName);
			ForwardQualitySettings = NewObject<UShaderPlatformQualitySettings>(this, UShaderPlatformQualitySettings::StaticClass(), FName(*ObjectName));
			ForwardQualitySettings->LoadConfig();
		}

		return ForwardSettingMap.Add(PlatformName, ForwardQualitySettings);
	}
	return *PlatformSettings;
}

static const FName GetPlatformNameFromShaderPlatform(EShaderPlatform Platform)
{
	return LegacyShaderPlatformToShaderFormat(Platform);
}

bool UMaterialShaderQualitySettings::HasPlatformQualitySettings(EShaderPlatform ShaderPlatform, EMaterialQualityLevel::Type QualityLevel)
{
	const UShaderPlatformQualitySettings* PlatformShaderPlatformQualitySettings = GetShaderPlatformQualitySettings(ShaderPlatform);
	const FMaterialQualityOverrides& PlatFormQualityOverrides = PlatformShaderPlatformQualitySettings->GetQualityOverrides(QualityLevel);
	return PlatFormQualityOverrides.bEnableOverride && PlatFormQualityOverrides.HasAnyOverridesSet();
}

const UShaderPlatformQualitySettings* UMaterialShaderQualitySettings::GetShaderPlatformQualitySettings(EShaderPlatform ShaderPlatform)
{
 #if WITH_EDITORONLY_DATA
	// TODO: discuss this, in order to preview render quality settings we override the 
	// requested platform's settings.
	// However we do not know if we are asking for the editor preview window (override able) or for thumbnails, cooking purposes etc.. (Must not override)
	// The code below 'works' because desktop platforms do not cook for ES2/ES31 preview.
	if (IsPCPlatform(ShaderPlatform) && GetMaxSupportedFeatureLevel(ShaderPlatform) <= ERHIFeatureLevel::ES3_1)
	{
		// Can check this cant be cooked by iterating through target platforms and shader formats to ensure it's not covered.
		if (PreviewPlatformSettings != nullptr)
		{
			return PreviewPlatformSettings;
		}
	}
#endif
	return GetShaderPlatformQualitySettings(GetPlatformNameFromShaderPlatform(ShaderPlatform));

}

UShaderPlatformQualitySettings* UMaterialShaderQualitySettings::GetShaderPlatformQualitySettings(FName PlatformName)
{
	return GetOrCreatePlatformSettings(PlatformName);
}

//////////////////////////////////////////////////////////////////////////

UShaderPlatformQualitySettings::UShaderPlatformQualitySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// high quality overrides are always enabled by default
	check(IsInGameThread());
	GetQualityOverrides(EMaterialQualityLevel::High).bEnableOverride = true;
}

void UShaderPlatformQualitySettings::BuildHash(EMaterialQualityLevel::Type QualityLevel, FSHAHash& OutHash) const
{
	FSHA1 Hash;

	AppendToHashState(QualityLevel, Hash);

	Hash.Final();
	Hash.GetHash(&OutHash.Hash[0]);
}

void UShaderPlatformQualitySettings::AppendToHashState(EMaterialQualityLevel::Type QualityLevel, FSHA1& HashState) const
{
	const FMaterialQualityOverrides& QualityLevelOverrides = GetQualityOverrides(QualityLevel);
	HashState.Update((const uint8*)&QualityLevelOverrides, sizeof(QualityLevelOverrides));
}

//////////////////////////////////////////////////////////////////////////

bool FMaterialQualityOverrides::HasAnyOverridesSet() const
{
	static const FMaterialQualityOverrides DefaultOverrides;

	return
		MobileCSMQuality != DefaultOverrides.MobileCSMQuality
		|| bForceDisableLMDirectionality != DefaultOverrides.bForceDisableLMDirectionality
		|| bForceFullyRough != DefaultOverrides.bForceFullyRough
		|| bForceNonMetal != DefaultOverrides.bForceNonMetal
		|| bForceLQReflections != DefaultOverrides.bForceLQReflections;
}
