// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"

/**
 * Base class for target platforms.
 */
class FTargetPlatformBase
	: public ITargetPlatform
{
public:

	// ITargetPlatform interface

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual FText DisplayName() const override
	{
		return PlatformInfo->DisplayName;
	}

	virtual const PlatformInfo::FPlatformInfo& GetPlatformInfo() const override
	{
		return *PlatformInfo;
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
	}
#endif //WITH_ENGINE

	virtual bool PackageBuild( const FString& InPackgeDirectory ) override
	{
		return true;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		return true;
	}

	virtual int32 CheckRequirements(const FString& ProjectPath, bool bProjectHasCode, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override
	{
		int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
		if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
		{
			bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
		}
		return bReadyToBuild;
	}

	virtual bool SupportsVariants() const override
	{
		return false;
	}

	virtual FText GetVariantDisplayName() const override
	{
		return FText();
	}

	virtual FText GetVariantTitle() const override
	{
		return FText();
	}

	virtual float GetVariantPriority() const override
	{
		return 0.0f;
	}

	virtual bool SendLowerCaseFilePaths() const override
	{
		return false;
	}

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		// do nothing in the base class
	}

	virtual int32 GetCompressionBitWindow() const override
	{
		return DEFAULT_ZLIB_BIT_WINDOW;
	}

protected:

	FTargetPlatformBase(const PlatformInfo::FPlatformInfo *const InPlatformInfo)
		: PlatformInfo(InPlatformInfo)
	{
		check(PlatformInfo);
	}

	/** Information about this platform */
	const PlatformInfo::FPlatformInfo *PlatformInfo;
};


/**
 * Template for target platforms.
 *
 * @param TPlatformProperties Type of platform properties.
 */
template<typename TPlatformProperties>
class TTargetPlatformBase
	: public FTargetPlatformBase
{
public:

	/** Default constructor. */
	TTargetPlatformBase()
		: FTargetPlatformBase( PlatformInfo::FindPlatformInfo(TPlatformProperties::PlatformName()) )
	{
		// HasEditorOnlyData and RequiresCookedData are mutually exclusive.
		check(TPlatformProperties::HasEditorOnlyData() != TPlatformProperties::RequiresCookedData());
	}

public:

	// ITargetPlatform interface

	virtual bool HasEditorOnlyData() const override
	{
		return TPlatformProperties::HasEditorOnlyData();
	}

	virtual bool IsLittleEndian() const override
	{
		return TPlatformProperties::IsLittleEndian();
	}

	virtual bool IsServerOnly() const override
	{
		return TPlatformProperties::IsServerOnly();
	}

	virtual bool IsClientOnly() const override
	{
		return TPlatformProperties::IsClientOnly();
	}

	virtual FString PlatformName() const override
	{
		return FString(TPlatformProperties::PlatformName());
	}

	virtual FString IniPlatformName() const override
	{
		return FString(TPlatformProperties::IniPlatformName());
	}

	virtual bool RequiresCookedData() const override
	{
		return TPlatformProperties::RequiresCookedData();
	}

	virtual bool RequiresUserCredentials() const override
	{
		return TPlatformProperties::RequiresUserCredentials();
	}

	virtual bool SupportsBuildTarget( EBuildTargets::Type BuildTarget ) const override
	{
		return TPlatformProperties::SupportsBuildTarget(BuildTarget);
	}

	virtual bool SupportsAutoSDK() const override
	{
		return TPlatformProperties::SupportsAutoSDK();
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::AudioStreaming:
			return TPlatformProperties::SupportsAudioStreaming();

		case ETargetPlatformFeatures::DistanceFieldShadows:
			return TPlatformProperties::SupportsDistanceFieldShadows();

		case ETargetPlatformFeatures::GrayscaleSRGB:
			return TPlatformProperties::SupportsGrayscaleSRGB();

		case ETargetPlatformFeatures::HighQualityLightmaps:
			return TPlatformProperties::SupportsHighQualityLightmaps();

		case ETargetPlatformFeatures::LowQualityLightmaps:
			return TPlatformProperties::SupportsLowQualityLightmaps();

		case ETargetPlatformFeatures::MultipleGameInstances:
			return TPlatformProperties::SupportsMultipleGameInstances();

		case ETargetPlatformFeatures::Packaging:
			return false;

		case ETargetPlatformFeatures::Tessellation:
			return TPlatformProperties::SupportsTessellation();

		case ETargetPlatformFeatures::TextureStreaming:
			return TPlatformProperties::SupportsTextureStreaming();

		case ETargetPlatformFeatures::SdkConnectDisconnect:
		case ETargetPlatformFeatures::UserCredentials:
			break;

		case ETargetPlatformFeatures::MobileRendering:
			return false;
		case ETargetPlatformFeatures::DeferredRendering:
			return true;

		case ETargetPlatformFeatures::ShouldSplitPaksIntoSmallerSizes :
			return false;
		}

		return false;
	}
	

#if WITH_ENGINE
	virtual FName GetPhysicsFormat( class UBodySetup* Body ) const override
	{
		return FName(TPlatformProperties::GetPhysicsFormat());
	}
#endif // WITH_ENGINE
};
