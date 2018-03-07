// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "MaterialExpressionTextureSample.generated.h"

struct FPropertyChangedEvent;

/** defines how MipValue is used */
UENUM()
enum ETextureMipValueMode
{
	/* Use hardware computed sample's mip level with automatic anisotropic filtering support. */
	TMVM_None UMETA(DisplayName="None (use computed mip level)"),

	/* Explicitly compute the sample's mip level. Disables anisotropic filtering. */
	TMVM_MipLevel UMETA(DisplayName="MipLevel (absolute, 0 is full resolution)"),
	
	/* Bias the hardware computed sample's mip level. Disables anisotropic filtering. */
	TMVM_MipBias UMETA(DisplayName="MipBias (relative to the computed mip level)"),
	
	/* Explicitly compute the sample's DDX and DDY for anisotropic filtering. */
	TMVM_Derivative UMETA(DisplayName="Derivative (explicit derivative to compute mip level)"),

	TMVM_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureSample : public UMaterialExpressionTextureBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinates;

	/** 
	 * Texture object input which overrides Texture if specified. 
	 * This only shows up in material functions and is used to implement texture parameters without actually putting the texture parameter in the function.
	 */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'Texture' if not specified"))
	FExpressionInput TextureObject;

	/** Meaning depends on MipValueMode, a single unit is one mip level  */
	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstMipValue' if not specified"))
	FExpressionInput MipValue;
	
	/** Enabled only if MipValueMode == TMVM_Derivative */
	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Coordinates derivative over the X axis"))
	FExpressionInput CoordinatesDX;
	
	/** Enabled only if MipValueMode == TMVM_Derivative */
	UPROPERTY(meta = (RequiredInput = "true", ToolTip = "Coordinates derivative over the Y axis"))
	FExpressionInput CoordinatesDY;

	/** Defines how the MipValue property is applied to the texture lookup */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSample, meta=(DisplayName = "MipValueMode"))
	TEnumAsByte<enum ETextureMipValueMode> MipValueMode;

	/** 
	 * Controls where the sampler for this texture lookup will come from.  
	 * Choose 'from texture asset' to make use of the UTexture addressing settings,
	 * Otherwise use one of the global samplers, which will not consume a sampler slot.
	 * This allows materials to use more than 16 unique textures on SM5 platforms.
	 */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSample)
	TEnumAsByte<enum ESamplerSourceMode> SamplerSource;

	/** only used if Coordinates is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSample)
	uint32 ConstCoordinate;

	/** only used if MipValue is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSample)
	int32 ConstMipValue;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual int32 GetWidth() const override;
	virtual int32 GetLabelPadding() override { return 8; }
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

	void UpdateTextureResource(class UTexture* InTexture);
	
#if WITH_EDITOR
	int32 CompileMipValue0(class FMaterialCompiler* Compiler);
	int32 CompileMipValue1(class FMaterialCompiler* Compiler);
#endif // WITH_EDITOR

protected:
	// Inherited parameter expressions can hide unused input pin
	bool bShowTextureInputPin;
};



