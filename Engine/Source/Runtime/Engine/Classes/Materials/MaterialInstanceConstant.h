// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Materials/MaterialInstance.h"

#include "MaterialInstanceConstant.generated.h"

/**
 * Material Instances may be used to change the appearance of a material without incurring an expensive recompilation of the material.
 * General modification of the material cannot be supported without recompilation, so the instances are limited to changing the values of
 * predefined material parameters. The parameters are statically defined in the compiled material by a unique name, type and default value.
 */
UCLASS(hidecategories=Object, collapsecategories, BlueprintType,MinimalAPI)
class UMaterialInstanceConstant : public UMaterialInstance
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Unique ID for this material instance's parameter set 
	 *  Updated on changes in the editor to allow those changes to be detected */
	UPROPERTY()
	FGuid ParameterStateId;
#endif

#if WITH_EDITOR
	/** For constructing new MICs. */
	friend class UMaterialInstanceConstantFactoryNew;
	/** For editing MICs. */
	friend class UMaterialEditorInstanceConstant;

#if WITH_EDITOR
	virtual ENGINE_API void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/**
	 * Set the parent of this material instance. This function may only be called in the Editor!
	 *   WARNING: You MUST call PostEditChange afterwards to propagate changes to other materials in the chain!
	 * @param NewParent - The new parent for this material instance.
	 */
	ENGINE_API void SetParentEditorOnly(class UMaterialInterface* NewParent);

	/**
	 * Set the value parameters. These functions may be called only in the Editor!
	 *   WARNING: You MUST call PostEditChange afterwards to propagate changes to other materials in the chain!
	 * @param ParameterName - The parameter's name.
	 * @param Value - The value to set.
	 */
	ENGINE_API void SetVectorParameterValueEditorOnly(FName ParameterName, FLinearColor Value);
	ENGINE_API void SetScalarParameterValueEditorOnly(FName ParameterName, float Value);
	ENGINE_API void SetTextureParameterValueEditorOnly(FName ParameterName, class UTexture* Value);
	ENGINE_API void SetFontParameterValueEditorOnly(FName ParameterName, class UFont* FontValue, int32 FontPage);

	/**
	 * Clear all parameter overrides on this material instance. This function
	 * may be called only in the Editor!
	 */
	ENGINE_API void ClearParameterValuesEditorOnly();
#endif // #if WITH_EDITOR

	ENGINE_API void PostLoad();
};

