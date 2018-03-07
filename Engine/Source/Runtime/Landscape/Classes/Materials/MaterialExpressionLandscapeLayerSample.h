// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialExpression.h"

#include "MaterialExpressionLandscapeLayerSample.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeLayerSample : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerWeight)
	float PreviewWeight;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

public:

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual UTexture* GetReferencedTexture() override;
	//~ End UMaterialExpression Interface

	virtual FGuid& GetParameterExpressionId() override;

	/**
	 * Called to get list of parameter names for static parameter sets
	 */
	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;

	//~ Begin UObject Interface
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface
};



