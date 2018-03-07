// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerWeight.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeLayerWeight : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstBase' if not specified"))
	FExpressionInput Base;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Ignored if not specified"))
	FExpressionInput Layer;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	float PreviewWeight;

	/** only used if Base is not hooked up */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionLandscapeLayerWeight)
	FVector ConstBase;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

public:

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual UTexture* GetReferencedTexture() override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Float | MCT_MaterialAttributes;}
#endif //WITH_EDITOR
	//~ End UMaterialExpression Interface

	virtual FGuid& GetParameterExpressionId() override;

	/**
	 * Called to get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;

	//~ Begin UObject Interface
	/**
	 * Do any object-specific cleanup required immediately after loading an object,
	 * and immediately after any undo/redo.
	 */
	virtual void PostLoad() override;
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface
};



