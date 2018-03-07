// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTextureProperty.generated.h"

class UTexture;

/** Selects the texture property to output */
UENUM()
enum EMaterialExposedTextureProperty
{
	/* The texture's size. */
	TMTM_TextureSize UMETA(DisplayName="Texture Size"),

	/* The texture's texel size in the UV space (1 / Texture Size) */
	TMTM_TexelSize UMETA(DisplayName="Texel Size"),
	
	TMTM_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureProperty : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Texture Object to access the property from."))
	FExpressionInput TextureObject;
	
	/** Texture property to be accessed */
	UPROPERTY(EditAnywhere, Category=UMaterialExpressionTextureProperty, meta=(DisplayName = "Texture Property"))
	TEnumAsByte<EMaterialExposedTextureProperty> Property;
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetTexturesForceMaterialRecompile(TArray<UTexture *> &Textures) const override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual uint32 GetInputType(int32 InputIndex) override;
#endif //WITH_EDITOR
	//~ End UMaterialExpression Interface
};
