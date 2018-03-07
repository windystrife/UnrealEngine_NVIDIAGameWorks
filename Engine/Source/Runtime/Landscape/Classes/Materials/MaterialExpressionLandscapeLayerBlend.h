// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerBlend.generated.h"

class UTexture;
struct FPropertyChangedEvent;

UENUM()
enum ELandscapeLayerBlendType
{
	LB_WeightBlend,
	LB_AlphaBlend,
	LB_HeightBlend,
};

USTRUCT()
struct FLayerBlendInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	TEnumAsByte<ELandscapeLayerBlendType> BlendType;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstLayerInput' if not specified"))
	FExpressionInput LayerInput;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstHeightInput' if not specified"))
	FExpressionInput HeightInput;

	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	float PreviewWeight;

	/** only used if LayerInput is not hooked up */
	UPROPERTY(EditAnywhere, Category = LayerBlendInput)
	FVector ConstLayerInput;

	/** only used if HeightInput is not hooked up */
	UPROPERTY(EditAnywhere, Category=LayerBlendInput)
	float ConstHeightInput;

	FLayerBlendInput()
		: BlendType(0)
		, PreviewWeight(0.0f)
		, ConstLayerInput(0.0f, 0.0f, 0.0f)
		, ConstHeightInput(0.0f)
	{
	}
};

UCLASS(collapsecategories, hidecategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeLayerBlend : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerBlend)
	TArray<FLayerBlendInput> Layers;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;


	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual bool NeedsLoadForClient() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 InputIndex) override { return MCT_Unknown; }
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual const TArray<FExpressionInput*> GetInputs() override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual FString GetInputName(int32 InputIndex) const override;
	virtual UTexture* GetReferencedTexture() override;
	//~ End UMaterialExpression Interface

	virtual FGuid& GetParameterExpressionId() override;

	/**
	 * Get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};



