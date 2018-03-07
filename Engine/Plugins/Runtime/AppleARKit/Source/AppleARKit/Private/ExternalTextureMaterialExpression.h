// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "ExternalTextureMaterialExpression.generated.h"

UENUM()
enum EARKitTextureType
{
	TextureY,
	TextureCbCr,
};

/**
* Implements a node sampling from the ARKit Passthrough external textures.
*/
UCLASS(collapsecategories, hidecategories = Object)
class UMaterialExpressionARKitPassthroughCamera : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinates;

	/** Only used if Coordinates is not hooked up */
	UPROPERTY(EditAnywhere, Category = UMaterialExpressionARKitPassthroughCamera)
	uint32 ConstCoordinate;

	UPROPERTY(EditAnywhere, Category = UMaterialExpressionARKitPassthroughCamera)
	TEnumAsByte<enum EARKitTextureType> TextureType;

#if WITH_EDITOR
	//~ UMaterialExpression
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	//~ UMaterialExpression
#endif // WITH_EDITOR
};
