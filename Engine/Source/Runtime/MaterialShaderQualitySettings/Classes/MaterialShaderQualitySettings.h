// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "MaterialShaderQualitySettings.generated.h"

class UShaderPlatformQualitySettings;

//UCLASS(config = Engine, defaultconfig)
UCLASS()
class MATERIALSHADERQUALITYSETTINGS_API UMaterialShaderQualitySettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UShaderPlatformQualitySettings* GetShaderPlatformQualitySettings(FName PlatformName);

	const UShaderPlatformQualitySettings* GetShaderPlatformQualitySettings(EShaderPlatform ShaderPlatform);

	bool HasPlatformQualitySettings(EShaderPlatform ShaderPlatform, EMaterialQualityLevel::Type QualityLevel);

#if WITH_EDITOR
	// Override GetShaderPlatformQualitySettings() return value with the specified platform's settings.
	// An empty PlatformName or otherwise non existent platform will cause GetShaderPlatformQualitySettings() 
	// to revert to its default behaviour.
	void SetPreviewPlatform(FName PlatformName);
	const FName& GetPreviewPlatform();
#endif

	static UMaterialShaderQualitySettings* Get();

private:
	UShaderPlatformQualitySettings* GetOrCreatePlatformSettings(FName PlatformName);

	UPROPERTY()
	TMap<FName, UShaderPlatformQualitySettings*> ForwardSettingMap;

#if WITH_EDITOR
	UShaderPlatformQualitySettings* PreviewPlatformSettings;
	FName PreviewPlatformName;
#endif

	static class UMaterialShaderQualitySettings* RenderQualitySingleton;
};
