// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionFunctionInput.generated.h"

class FMaterialCompiler;
struct FPropertyChangedEvent;

/** Supported input types */
UENUM(BlueprintType)
enum EFunctionInputType
{
	FunctionInput_Scalar,
	FunctionInput_Vector2,
	FunctionInput_Vector3,
	FunctionInput_Vector4,
	FunctionInput_Texture2D,
	FunctionInput_TextureCube,
	FunctionInput_StaticBool,
	FunctionInput_MaterialAttributes,
	FunctionInput_MAX,
};

UCLASS(hidecategories=object, MinimalAPI)
class UMaterialExpressionFunctionInput : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Used for previewing when editing the function, or when bUsePreviewValueAsDefault is enabled. */
	UPROPERTY(meta=(RequiredInput = "false"))
	FExpressionInput Preview;

	/** The input's name, which will be drawn on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionFunctionInput)
	FString InputName;

	/** The input's description, which will be used as a tooltip on the connector in function call expressions that use this function. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionFunctionInput)
	FString Description;

	/** Id of this input, used to maintain references through name changes. */
	UPROPERTY()
	FGuid Id;

	/** 
	 * Type of this input.  
	 * Input code chunks will be cast to this type, and a compiler error will be emitted if the cast fails.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionFunctionInput)
	TEnumAsByte<enum EFunctionInputType> InputType;

	/** Value used to preview this input when editing the material function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionFunctionInput, meta=(OverridingInputProperty = "Preview"))
	FVector4 PreviewValue;

	/** Whether to use the preview value or texture as the default value for this input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionFunctionInput)
	uint32 bUsePreviewValueAsDefault:1;

	/** Controls where the input is displayed relative to the other inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=MaterialExpressionFunctionInput)
	int32 SortPriority;

	/** 
	 * true when this expression is being compiled in a function preview, 
	 * false when this expression is being compiled into a material that uses the function.
	 * Only valid in Compile()
	 */
	UPROPERTY(transient)
	uint32 bCompilingFunctionPreview:1;

	/** The Preview input to use during compilation from another material, when bCompilingFunctionPreview is false. */
	FExpressionInput EffectivePreviewDuringCompile;

#if WITH_EDITOR
	/** Returns the appropriate preview expression when compiling a function or material preview. */
	UMaterialExpression* GetEffectivePreviewExpression()
	{
		return bCompilingFunctionPreview ? Preview.Expression : EffectivePreviewDuringCompile.Expression;
	}
#endif

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetExpressionToolTip(TArray<FString>& OutToolTip) override;
	virtual int32 CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

	virtual bool IsResultMaterialAttributes(int32 OutputIndex) override;
	virtual uint32 GetInputType(int32 InputIndex) override;
	virtual uint32 GetOutputType(int32 OutputIndex) override;
#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface

	/** Generate the Id for this input. */
	ENGINE_API void ConditionallyGenerateId(bool bForce);

	/** Validate InputName.  Must be called after InputName is changed to prevent duplicate inputs. */
	ENGINE_API void ValidateName();


private:

#if WITH_EDITOR
	/** Helper function which compiles this expression for previewing. */
	int32 CompilePreviewValue(FMaterialCompiler* Compiler);
#endif // WITH_EDITOR
};



