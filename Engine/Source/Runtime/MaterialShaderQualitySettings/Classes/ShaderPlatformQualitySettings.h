// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SceneTypes.h"
#include "ShaderPlatformQualitySettings.generated.h"

/**
* 
*/
UENUM()
enum class EMobileCSMQuality : uint8
{
	// Lowest quality, no filtering.
	NoFiltering,
	// Medium quality, 1x1 PCF filtering.
	PCF_1x1 UMETA(DisplayName = "1x1 PCF"),
	// Highest quality, 2x2 PCF filtering.
	PCF_2x2 UMETA(DisplayName = "2x2 PCF"),
};

// FMaterialQualityOverrides represents the full set of possible material overrides per quality level.
USTRUCT()
struct FMaterialQualityOverrides
{
public:
	GENERATED_USTRUCT_BODY()
	
	FMaterialQualityOverrides() 
		: bEnableOverride(false)
		, bForceFullyRough(false)
		, bForceNonMetal(false)
		, bForceDisableLMDirectionality(false)
		, bForceLQReflections(false)
		, MobileCSMQuality(EMobileCSMQuality::PCF_2x2)
	{
	}

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Enable Quality Override"), Category = "Quality")
	bool bEnableOverride;

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Force Fully Rough"), Category = "Quality")
	bool bForceFullyRough;

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Force Non-metal"), Category = "Quality")
	bool bForceNonMetal;

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Disable Lightmap directionality"), Category = "Quality")
	bool bForceDisableLMDirectionality;

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Force low quality reflections"), Category = "Quality")
	bool bForceLQReflections;

	UPROPERTY(EditAnywhere, Config, Meta = (DisplayName = "Cascade shadow mapping quality"), Category = "Quality")
	EMobileCSMQuality MobileCSMQuality;

	bool HasAnyOverridesSet() const;
};


UCLASS(config = Engine, defaultconfig, perObjectConfig)
class MATERIALSHADERQUALITYSETTINGS_API UShaderPlatformQualitySettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Quality")
	FMaterialQualityOverrides QualityOverrides[EMaterialQualityLevel::Num];

	FMaterialQualityOverrides& GetQualityOverrides(EMaterialQualityLevel::Type QualityLevel)
	{
		check(QualityLevel<EMaterialQualityLevel::Num);
		return QualityOverrides[(int32)QualityLevel];
	}

	const FMaterialQualityOverrides& GetQualityOverrides(EMaterialQualityLevel::Type QualityLevel) const
	{
		check(QualityLevel < EMaterialQualityLevel::Num);
		return QualityOverrides[(int32)QualityLevel];
	}

	void BuildHash(EMaterialQualityLevel::Type QualityLevel, class FSHAHash& OutHash) const;
	void AppendToHashState(EMaterialQualityLevel::Type QualityLevel, class FSHA1& HashState) const;
};
