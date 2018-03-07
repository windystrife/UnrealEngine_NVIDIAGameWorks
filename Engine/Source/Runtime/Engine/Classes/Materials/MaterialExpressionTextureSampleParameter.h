// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MaterialExpressionTextureSampleParameter.generated.h"

class UTexture;

UCLASS(collapsecategories, abstract, hidecategories=Object)
class ENGINE_API UMaterialExpressionTextureSampleParameter : public UMaterialExpressionTextureSample
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSampleParameter)
	FName ParameterName;

	/** GUID that should be unique within the material, this is used for parameter renaming. */
	UPROPERTY()
	FGuid ExpressionGUID;

	/** The name of the parameter Group to display in MaterialInstance Editor. Default is None group */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionTextureSampleParameter)
	FName Group;

#if WITH_EDITORONLY_DATA
	/** Controls where the this parameter is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY(EditAnywhere, Category = MaterialExpressionTextureSampleParameter)
	int32 SortPriority;
#endif

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	virtual bool MatchesSearchQuery(const TCHAR* SearchQuery) override;
#if WITH_EDITOR
	virtual bool CanRenameNode() const override { return true; }
	virtual FString GetEditableName() const override;
	virtual void SetEditableName(const FString& NewName) override;

	virtual bool HasAParameterName() const override { return true; }
	virtual FName GetParameterName() const override { return ParameterName; }
	virtual void SetParameterName(const FName& Name) override { ParameterName = Name; }

#endif
	//~ End UMaterialExpression Interface

	/** Return whether this is the named parameter, and fill in its value */
	bool IsNamedParameter(FName InParameterName, UTexture*& OutValue) const;

	/**
	 * Return true if the texture is a movie texture
	 *
	 * @param	InTexture - texture to test
	 * @return	true/false
	 */	
	virtual bool TextureIsValid( UTexture* InTexture );

	/**
	 * Called when TextureIsValid==false
	 *
	 * @return	Descriptive error text
	 */	
	virtual const TCHAR* GetRequirements();
	
	/**
	 *	Sets the default texture if none is set
	 */
	virtual void SetDefaultTexture();

	virtual FGuid& GetParameterExpressionId() override
	{
		return ExpressionGUID;
	}

	void GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const;
};
