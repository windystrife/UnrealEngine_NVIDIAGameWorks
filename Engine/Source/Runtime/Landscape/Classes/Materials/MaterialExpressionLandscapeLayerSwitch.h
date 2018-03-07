// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerSwitch.generated.h"

class UTexture;

UCLASS(collapsecategories, hidecategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeLayerSwitch : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FExpressionInput LayerUsed;

	UPROPERTY()
	FExpressionInput LayerNotUsed;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerSwitch)
	FName ParameterName;

	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerSwitch)
	uint32 PreviewUsed:1;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;


public:

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual UTexture* GetReferencedTexture() override;
#if WITH_EDITOR
	virtual uint32 GetInputType(int32 InputIndex) override {return MCT_Unknown;}
	virtual uint32 GetOutputType(int32 InputIndex) override {return MCT_Unknown;}
#endif
	//~ End UMaterialExpression Interface

	virtual FGuid& GetParameterExpressionId() override;

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



