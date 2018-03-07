// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionGoogleARCorePassthroughCamera.generated.h"

/**
* Implements a node sampling from the Tango Passthrough external texture.
*/
UCLASS(collapsecategories, hidecategories = Object)
class GOOGLEARCORERENDERING_API UMaterialExpressionGoogleARCorePassthroughCamera : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstCoordinate' if not specified"))
	FExpressionInput Coordinates;

	/** only used if Coordinates are not hooked up. */
	UPROPERTY(EditAnywhere, Category = UMaterialExpressionGoogleARCorePassthroughCamera)
	uint32 ConstCoordinate;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	//~ End UMaterialExpression Interface
#endif // WITH_EDITOR
};

