// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionViewProperty.generated.h"

UENUM()
enum EMaterialExposedViewProperty
{	
	/** Horizontal and vertical size of the view's buffer in pixels */
	MEVP_BufferSize UMETA(DisplayName="Render Target Size"),
	/** Horizontal and vertical field of view angles in radian */
	MEVP_FieldOfView UMETA(DisplayName="Field Of View"),
	/** Tan(FieldOfView * 0.5) */
	MEVP_TanHalfFieldOfView UMETA(DisplayName="Tan(0.5 * Field Of View)"),
	/** Horizontal and vertical size of the view in pixels */
	MEVP_ViewSize UMETA(DisplayName="View Size"),
	/** Absolute world space view position (differs from the camera position in the shadow passes) */
	MEVP_WorldSpaceViewPosition UMETA(DisplayName="View Position (Absolute World Space)"),
	/** Absolute world space camera position */
	MEVP_WorldSpaceCameraPosition UMETA(DisplayName = "Camera Position (Absolute World Space)"),
	/** Horizontal and vertical position of the viewport in pixels within the buffer. */
	MEVP_ViewportOffset UMETA(DisplayName = "Viewport Offset"),

	MEVP_MAX,
};

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionViewProperty : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()
	
	/** View input property to be accessed */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionViewProperty, meta=(DisplayName = "View Property"))
	TEnumAsByte<EMaterialExposedViewProperty> Property;
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};
