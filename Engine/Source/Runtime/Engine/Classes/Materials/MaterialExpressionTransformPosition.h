// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTransformPosition.generated.h"

UENUM()
enum EMaterialPositionTransformSource
{
	/** Local space */
	TRANSFORMPOSSOURCE_Local UMETA(DisplayName="Local Space"),
	
	/** Absolute world space */
	TRANSFORMPOSSOURCE_World UMETA(DisplayName="Absolute World Space"),
	
	/** Camera relative world space */
	TRANSFORMPOSSOURCE_TranslatedWorld  UMETA(DisplayName="Camera Relative World Space"),

	/** View space (differs from camera space in the shadow passes) */
	TRANSFORMPOSSOURCE_View  UMETA(DisplayName="View Space"),

	/** Camera space */
	TRANSFORMPOSSOURCE_Camera  UMETA(DisplayName="Camera Space"),

	/** Particle space */
	TRANSFORMPOSSOURCE_Particle UMETA(DisplayName = "Mesh Particle Space"),

	TRANSFORMPOSSOURCE_MAX,
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionTransformPosition : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** input expression for this transform */
	UPROPERTY()
	FExpressionInput Input;

	/** source format of the position that will be transformed */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransformPosition, meta=(DisplayName = "Source"))
	TEnumAsByte<enum EMaterialPositionTransformSource> TransformSourceType;

	/** type of transform to apply to the input expression */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTransformPosition, meta=(DisplayName = "Destination"))
	TEnumAsByte<enum EMaterialPositionTransformSource> TransformType;


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



