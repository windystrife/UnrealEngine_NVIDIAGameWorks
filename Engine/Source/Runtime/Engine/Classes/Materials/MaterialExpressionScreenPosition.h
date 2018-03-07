// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionScreenPosition.generated.h"

UENUM()
enum EMaterialExpressionScreenPositionMapping
{
	/** A UV in the 0..1 range for use with the ScreeTnexture material expression.*/
	MESP_SceneTextureUV UMETA(DisplayName = "SceneTextureUV"),
	/** A UV in the 0..1 range that maps to the local viewport */
	MESP_ViewportUV     UMETA(DisplayName = "ViewportUV"),

	MESP_Max,
};

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionScreenPosition : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** View input property to be accessed */
	UPROPERTY(EditAnywhere, Category = UMaterialExpressionScreenPosition, meta = (DisplayName = "Mapping"))
	TEnumAsByte<EMaterialExpressionScreenPositionMapping> Mapping;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



