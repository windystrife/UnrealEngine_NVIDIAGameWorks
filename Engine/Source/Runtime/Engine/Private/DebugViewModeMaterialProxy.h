// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialProxy.h : Contains definitions the debug view mode material shaders.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Engine/TextureStreamingTypes.h"
#include "DebugViewModeHelpers.h"

class FMaterialCompiler;
class UTexture;

#if WITH_EDITORONLY_DATA

/**
 * Material proxy for debug viewmode. Used to prevent debug view mode shaders from being stored in the default material map.
 */
class FDebugViewModeMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:

	FDebugViewModeMaterialProxy()
		: FMaterial()
		, MaterialInterface(nullptr)
		, Material(nullptr)
		, Usage(EMaterialShaderMapUsage::Default)
		, bValid(true)
		, bSynchronousCompilation(true)
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	FDebugViewModeMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bSynchronousCompilation, EMaterialShaderMapUsage::Type InUsage);

	void MarkAsInvalid() { bValid = false; }
	bool IsValid() const { return bValid; }
	
	virtual bool RequiresSynchronousCompilation() const override;

	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual bool IsPersistent() const override { return false; }

	// Normally this would cause a bug as the shader map would try to be shared by both, but GetShaderMapUsage() allows this to work
	virtual FGuid GetMaterialId() const override { return Material->StateId; }

	virtual EMaterialShaderMapUsage::Type GetShaderMapUsage() const override { return Usage; }


	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override
	{
		const FString ShaderTypeName = ShaderType->GetName();
		return (ShaderTypeName.Contains(TEXT("FMaterialTexCoordScalePS")) && Usage == EMaterialShaderMapUsage::DebugViewModeTexCoordScale) || 
			(ShaderTypeName.Contains(TEXT("FRequiredTextureResolutionPS")) && Usage == EMaterialShaderMapUsage::DebugViewModeRequiredTextureResolution);
	}

	virtual const TArray<UTexture*>& GetReferencedTextures() const override
	{
		return ReferencedTextures;
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const override
	{
		return MaterialInterface ? MaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel)->CompilePropertyAndSetMaterialProperty(Property, Compiler, OverrideShaderFrequency, bUsePreviousFrameTime) : INDEX_NONE;
	}

#if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
	virtual int32 CompileCustomAttribute(const FGuid& AttributeID, FMaterialCompiler* Compiler) const override
	{
		return MaterialInterface ? MaterialInterface->CompilePropertyEx(Compiler, AttributeID) : INDEX_NONE;
	}
#endif

	virtual FString GetMaterialUsageDescription() const override
	{
		return FString::Printf(TEXT("FDebugViewModeMaterialProxy %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL"));
	}

	virtual FString GetFriendlyName() const override { return FString::Printf(TEXT("FDebugViewModeMaterialProxy %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }

	virtual UMaterialInterface* GetMaterialInterface() const override
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FDebugViewModeMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const override;
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const override;

	virtual EMaterialDomain GetMaterialDomain() const override;
	virtual bool IsTwoSided() const  override;
	virtual bool IsDitheredLODTransition() const  override;
	virtual bool IsLightFunction() const override;
	virtual bool IsDeferredDecal() const override;
	virtual bool IsVolumetricPrimitive() const override { return false; }
	virtual bool IsSpecialEngineMaterial() const override;
	virtual bool IsWireframe() const override;
	virtual bool IsMasked() const override;
	virtual enum EBlendMode GetBlendMode() const override;
	virtual enum EMaterialShadingModel GetShadingModel() const override;
	virtual float GetOpacityMaskClipValue() const override;
	virtual bool GetCastDynamicShadowAsMasked() const override;
	virtual void GatherCustomOutputExpressions(TArray<class UMaterialExpressionCustomOutput*>& OutCustomOutputs) const override;
	virtual void GatherExpressionsForCustomInterpolators(TArray<class UMaterialExpression*>& OutExpressions) const override;

	virtual EMaterialShaderMapUsage::Type GetMaterialShaderMapUsage() const { return Usage; }

	////////////////

	static void AddShader(UMaterialInterface* InMaterialInterface, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bSynchronousCompilation, EMaterialShaderMapUsage::Type InUsage);
	static const FMaterial* GetShader(const FMaterial* Material, EMaterialShaderMapUsage::Type Usage);
	static void ClearAllShaders();
	static bool HasAnyShaders() { return DebugMaterialShaderMap.Num() > 0; }
	static void ValidateAllShaders(TSet<UMaterialInterface*>& Materials);

private:

	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	UMaterial* Material;	
	TArray<UTexture*> ReferencedTextures;
	EMaterialShaderMapUsage::Type Usage;

	/** Whether this debug material should be used or not. */
	bool bValid;
	bool bSynchronousCompilation;

	struct FMaterialUsagePair
	{
		FMaterialUsagePair(const FMaterial* InMaterial, EMaterialShaderMapUsage::Type InUsage) : Material(InMaterial), Usage(InUsage) {}
		const FMaterial* Material;
		EMaterialShaderMapUsage::Type Usage;

		friend bool operator==(const FMaterialUsagePair& Lhs, const FMaterialUsagePair& Rhs)
		{
			return Lhs.Material == Rhs.Material && Lhs.Usage == Rhs.Usage;
		}

		friend uint32 GetTypeHash( const FMaterialUsagePair& Pair )
		{
			return GetTypeHash(Pair.Material) ^ GetTypeHash(Pair.Usage);
		}

	};

	static volatile bool bReentrantCall;
	static TMap<FMaterialUsagePair, FDebugViewModeMaterialProxy*> DebugMaterialShaderMap;
};

#endif
