// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MaterialExpressionTextureSampleParameter2D.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureSampleParameter2D : public UMaterialExpressionTextureSampleParameter
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
	
	//~ Begin UMaterialExpressionTextureSampleParameter Interface
	virtual bool TextureIsValid( UTexture* InTexture ) override;
	virtual const TCHAR* GetRequirements() override;
	virtual void SetDefaultTexture() override;
	//~ End UMaterialExpressionTextureSampleParameter Interface
};



