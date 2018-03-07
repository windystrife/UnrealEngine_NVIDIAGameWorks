// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Node acts as a base class for TextureSamples and TextureObjects 
 * to cover their shared functionality. 
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionTextureBase.generated.h"

class UTexture;
struct FPropertyChangedEvent;

UCLASS(abstract, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureBase : public UMaterialExpression 
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionTextureBase)
	class UTexture* Texture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionTextureBase)
	TEnumAsByte<enum EMaterialSamplerType> SamplerType;
	
	/** Is default selected texture when using mesh paint mode texture painting */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureBase)
	uint32 IsDefaultMeshpaintTexture:1;
	
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual FString GetDescription() const override;
#endif
	//~ End UMaterialExpression Interface

	/** 
	 * Callback to get any texture reference this expression emits.
	 * This is used to link the compiled uniform expressions with their default texture values. 
	 * Any UMaterialExpression whose compilation creates a texture uniform expression (eg Compiler->Texture, Compiler->TextureParameter) must implement this.
	 */
	virtual UTexture* GetReferencedTexture() override
	{
		return Texture;
	}

	/**
	 * Automatically determines and set the sampler type for the current texture.
	 */
	void AutoSetSampleType();

	/**
	 * Returns the default sampler type for the specified texture.
	 * @param Texture - The texture for which the default sampler type will be returned.
	 * @returns the default sampler type for the specified texture.
	 */
	static EMaterialSamplerType GetSamplerTypeForTexture( const UTexture* Texture );
};
