// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 *	A material expression that routes particle emitter parameters to the
 *	material.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionDynamicParameter.generated.h"

struct FPropertyChangedEvent;

UCLASS(collapsecategories, hidecategories=Object, MinimalAPI)
class UMaterialExpressionDynamicParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** 
	 *	The names of the parameters.
	 *	These will show up in Cascade when editing a particle system
	 *	that uses the material it is in...
	 */
	UPROPERTY(EditAnywhere, editfixedsize, Category=MaterialExpressionDynamicParameter)
	TArray<FString> ParamNames;

	UPROPERTY(EditAnywhere, Category = MaterialExpressionDynamicParameter)
	FLinearColor DefaultValue;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void PostLoad() override;
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual TArray<FExpressionOutput>& GetOutputs() override;
	virtual int32 GetWidth() const override;
	virtual int32 GetLabelPadding() override { return 8; }
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
	//~ End UMaterialExpression Interface

	/**
	 * Iterate through all of the expression nodes until we find another 
	 * dynamic parameter we can copy the properties from
	 */
	ENGINE_API void UpdateDynamicParameterProperties();

	/**
	 * Copy the properties from the specified dynamic parameter
	 *
	 * @param	FromParam	The param to copy from
	 * @return	true if sucessful
	 */
	bool CopyDynamicParameterProperties(const UMaterialExpressionDynamicParameter* FromParam);
};



