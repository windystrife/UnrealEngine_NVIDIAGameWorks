// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * MaterialExpressionCollectionParameter.h - a node that references a single parameter in a MaterialParameterCollection
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionCollectionParameter.generated.h"

struct FPropertyChangedEvent;

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionCollectionParameter : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** The Parameter Collection to use. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionCollectionParameter)
	class UMaterialParameterCollection* Collection;

	/** Name of the parameter being referenced. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionCollectionParameter)
	FName ParameterName;

	/** Id that is set from the name, and used to handle renaming of collection parameters. */
	UPROPERTY()
	FGuid ParameterId;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PostLoad() override;
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface.

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool HasAParameterName() const override { return true; }
	virtual FName GetParameterName() const override { return ParameterName; }
	virtual void SetParameterName(const FName& Name) override { ParameterName = Name; }
	virtual void ValidateParameterName() override {};

#endif
	virtual bool MatchesSearchQuery( const TCHAR* SearchQuery ) override;
	//~ End UMaterialExpression Interface
};



