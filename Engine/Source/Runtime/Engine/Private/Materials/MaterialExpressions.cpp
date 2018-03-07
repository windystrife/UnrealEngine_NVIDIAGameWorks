// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialExpressions.cpp - Material expressions implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Misc/Guid.h"
#include "UObject/RenderingObjectVersion.h"
#include "Misc/App.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineGlobals.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "MaterialShared.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Materials/Material.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Styling/CoreStyle.h"

#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArccosineFast.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArcsineFast.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangentFast.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"
#include "Materials/MaterialExpressionAtmosphericFogColor.h"
#include "Materials/MaterialExpressionBentNormalCustomOutput.h"
#include "Materials/MaterialExpressionBlackBody.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionDecalDerivative.h"
#include "Materials/MaterialExpressionDecalLifetimeOpacity.h"
#include "Materials/MaterialExpressionDecalMipmapLevel.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDepthOfFieldFunction.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDistanceCullFade.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionFontSample.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGIReplace.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionLightmapUVs.h"
#include "Materials/MaterialExpressionPrecomputedAOMask.h"
#include "Materials/MaterialExpressionLightmassReplace.h"
#include "Materials/MaterialExpressionLightVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLogarithm2.h"
#include "Materials/MaterialExpressionLogarithm10.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMaterialProxyReplace.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectOrientation.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleDirection.h"
#include "Materials/MaterialExpressionParticleMacroUV.h"
#include "Materials/MaterialExpressionParticleMotionBlurFade.h"
#include "Materials/MaterialExpressionParticleRandom.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRadius.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleSize.h"
#include "Materials/MaterialExpressionParticleSpeed.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionSceneTexelSize.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionFlexFluidSurfaceThickness.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSobol.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSphericalParticleOpacity.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionTangentOutput.h"
#include "Materials/MaterialExpressionTemporalSobol.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionParticleSubUV.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionAntialiasedTextureMask.h"
#include "Materials/MaterialExpressionTextureSampleParameterSubUV.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTruncate.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionDistanceToNearestSurface.h"
#include "Materials/MaterialExpressionDistanceFieldGradient.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionAtmosphericLightVector.h"
#include "Materials/MaterialExpressionAtmosphericLightColor.h"

// WaveWorks Start
#include "Materials/MaterialExpressionWaveWorks.h"
// WaveWorks End

// NVCHANGE_BEGIN: Add VXGI
#include "Materials/MaterialExpressionVxgiVoxelization.h"
#include "Materials/MaterialExpressionVxgiTraceCone.h"
// NVCHANGE_END: Add VXGI
#include "EditorSupportDelegates.h"
#include "MaterialCompiler.h"
#if WITH_EDITOR
#include "MaterialGraph/MaterialGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif //WITH_EDITOR


#define LOCTEXT_NAMESPACE "MaterialExpression"

#define SWAP_REFERENCE_TO( ExpressionInput, ToBeRemovedExpression, ToReplaceWithExpression )	\
if( ExpressionInput.Expression == ToBeRemovedExpression )										\
{																								\
	ExpressionInput.Expression = ToReplaceWithExpression;										\
}

#if WITH_EDITOR
FUObjectAnnotationSparseBool GMaterialFunctionsThatNeedExpressionsFlipped;
FUObjectAnnotationSparseBool GMaterialFunctionsThatNeedCoordinateCheck;
FUObjectAnnotationSparseBool GMaterialFunctionsThatNeedCommentFix;
FUObjectAnnotationSparseBool GMaterialFunctionsThatNeedSamplerFixup;
#endif // #if WITH_EDITOR

/** Returns whether the given expression class is allowed. */
bool IsAllowedExpressionType(UClass* Class, bool bMaterialFunction)
{
	// Exclude comments from the expression list, as well as the base parameter expression, as it should not be used directly
	const bool bSharedAllowed = Class != UMaterialExpressionComment::StaticClass() 
		&& Class != UMaterialExpressionParameter::StaticClass();

	if (bMaterialFunction)
	{
		return bSharedAllowed;
	}
	else
	{
		return bSharedAllowed
			&& Class != UMaterialExpressionFunctionInput::StaticClass()
			&& Class != UMaterialExpressionFunctionOutput::StaticClass();
	}
}

/** Parses a string into multiple lines, for use with tooltips. */
void ConvertToMultilineToolTip(const FString& InToolTip, int32 TargetLineLength, TArray<FString>& OutToolTip)
{
	int32 CurrentPosition = 0;
	int32 LastPosition = 0;
	OutToolTip.Empty(1);

	while (CurrentPosition < InToolTip.Len())
	{
		// Move to the target position
		CurrentPosition += TargetLineLength;

		if (CurrentPosition < InToolTip.Len())
		{
			// Keep moving until we get to a space, or the end of the string
			while (CurrentPosition < InToolTip.Len() && InToolTip[CurrentPosition] != TCHAR(' '))
			{
				CurrentPosition++;
			}

			// Move past the space
			if (CurrentPosition < InToolTip.Len() && InToolTip[CurrentPosition] == TCHAR(' '))
			{
				CurrentPosition++;
			}

			// Add a new line, ending just after the space we just found
			OutToolTip.Add(InToolTip.Mid(LastPosition, CurrentPosition - LastPosition));
			LastPosition = CurrentPosition;
		}
		else
		{
			// Add a new line, right up to the end of the input string
			OutToolTip.Add(InToolTip.Mid(LastPosition, InToolTip.Len() - LastPosition));
		}
	}
}

void GetMaterialValueTypeDescriptions(uint32 MaterialValueType, TArray<FText>& OutDescriptions)
{
	// Get exact float type if possible
	uint32 MaskedFloatType = MaterialValueType & MCT_Float;
	if (MaskedFloatType)
	{
		switch (MaskedFloatType)
		{
			case MCT_Float:
			case MCT_Float1:
				OutDescriptions.Add(LOCTEXT("Float", "Float"));
				break;
			case MCT_Float2:
				OutDescriptions.Add(LOCTEXT("Float2", "Float 2"));
				break;
			case MCT_Float3:
				OutDescriptions.Add(LOCTEXT("Float3", "Float 3"));
				break;
			case MCT_Float4:
				OutDescriptions.Add(LOCTEXT("Float4", "Float 4"));
				break;
			default:
				break;
		}
	}

	// Get exact texture type if possible
	uint32 MaskedTextureType = MaterialValueType & MCT_Texture;
	if (MaskedTextureType)
	{
		switch (MaskedTextureType)
		{
			case MCT_Texture2D:
				OutDescriptions.Add(LOCTEXT("Texture2D", "Texture 2D"));
				break;
			case MCT_TextureCube:
				OutDescriptions.Add(LOCTEXT("TextureCube", "Texture Cube"));
				break;
			case MCT_Texture:
				OutDescriptions.Add(LOCTEXT("Texture", "Texture"));
				break;
			default:
				break;
		}
	}

	if (MaterialValueType & MCT_StaticBool)
		OutDescriptions.Add(LOCTEXT("StaticBool", "Bool"));
	if (MaterialValueType & MCT_MaterialAttributes)
		OutDescriptions.Add(LOCTEXT("MaterialAttributes", "Material Attributes"));
	if (MaterialValueType & MCT_Unknown)
		OutDescriptions.Add(LOCTEXT("Unknown", "Unknown"));
}

bool CanConnectMaterialValueTypes(uint32 InputType, uint32 OutputType)
{
	if (InputType & MCT_Unknown)
	{
		// can plug anything into unknown inputs
		return true;
	}
	if (OutputType & MCT_Unknown)
	{
		// TODO: Decide whether these should connect to everything
		// Usually means that inputs haven't been connected yet so makes workflow easier
		return true;
	}
	if (InputType & OutputType)
	{
		return true;
	}
	// Need to do more checks here to see whether types can be cast
	// just check if both are float for now
	if (InputType & MCT_Float && OutputType & MCT_Float)
	{
		return true;
	}
	return false;
}

#if WITH_EDITOR

/**
 * Helper function that wraps the supplied texture coordinates in the necessary math to transform them for external textures
 *
 * @param Compiler                The compiler to add code to
 * @param TexCoordCodeIndex       Index to the code chunk that supplies the vanilla texture coordinates
 * @param TextureReferenceIndex   Index of the texture within the material (used to access the external texture transform at runtime)
 * @param ParameterName           (Optional) Parameter name of the texture parameter that's assigned to the sample (used to access the external texture transform at runtime)
 * @return Index to a new code chunk that supplies the transformed UV coordinates
 */
int32 CompileExternalTextureCoordinates(FMaterialCompiler* Compiler, int32 TexCoordCodeIndex, int32 TextureReferenceIndex, TOptional<FName> ParameterName = TOptional<FName>())
{
	if (TexCoordCodeIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 ScaleRotationCode = Compiler->ExternalTextureCoordinateScaleRotation(TextureReferenceIndex, ParameterName);
	int32 OffsetCode = Compiler->ExternalTextureCoordinateOffset(TextureReferenceIndex, ParameterName);

	return Compiler->RotateScaleOffsetTexCoords(TexCoordCodeIndex, ScaleRotationCode, OffsetCode);
}

/**
 * Compile a texture sample taking into consideration external textures (which may use different sampling code in the shader on some platforms)
 *
 * @param Compiler                The compiler to add code to
 * @param TexCoordCodeIndex       Index to the code chunk that supplies the vanilla texture coordinates
 * @param SamplerType             The type of sampler that is to be used
 * @param ParameterName           (Optional) Parameter name of the texture parameter that's assigned to the sample
 * @param MipValue0Index          (Optional) Mip value (0) code index when mips are being used
 * @param MipValue1Index          (Optional) Mip value (1) code index when mips are being used
 * @param MipValueMode            (Optional) Texture MIP value mode
 * @param SamplerSource           (Optional) Sampler source override
 * @return Index to a new code chunk that samples the texture
 */
int32 CompileTextureSample(
	FMaterialCompiler* Compiler,
	UTexture* Texture,
	int32 TexCoordCodeIndex,
	EMaterialSamplerType SamplerType,
	TOptional<FName> ParameterName = TOptional<FName>(),
	int32 MipValue0Index=INDEX_NONE,
	int32 MipValue1Index=INDEX_NONE,
	ETextureMipValueMode MipValueMode=TMVM_None,
	ESamplerSourceMode SamplerSource=SSM_FromTextureAsset
	)
{
	int32 TextureReferenceIndex = INDEX_NONE;
	int32 TextureCodeIndex = INDEX_NONE;
	if (SamplerType == SAMPLERTYPE_External)
	{
		// External sampler, so generate the necessary external uniform expression based on whether we're using a parameter name or not
		TextureCodeIndex = ParameterName.IsSet() ? Compiler->ExternalTextureParameter(ParameterName.GetValue(), Texture, TextureReferenceIndex) : Compiler->ExternalTexture(Texture, TextureReferenceIndex);

		// External textures need an extra transform applied to the UV coordinates
		TexCoordCodeIndex = CompileExternalTextureCoordinates(Compiler, TexCoordCodeIndex, TextureReferenceIndex, ParameterName);
	}
	else
	{
		TextureCodeIndex = ParameterName.IsSet() ? Compiler->TextureParameter(ParameterName.GetValue(), Texture, TextureReferenceIndex, SamplerSource) : Compiler->Texture(Texture, TextureReferenceIndex, SamplerSource, MipValueMode);
	}

	return Compiler->TextureSample(
					TextureCodeIndex,
					TexCoordCodeIndex,
					SamplerType,
					MipValue0Index,
					MipValue1Index,
					MipValueMode,
					SamplerSource,
					TextureReferenceIndex);
}
#endif

UMaterialExpression::UMaterialExpression(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, GraphNode(NULL)
#endif
{

	Outputs.Add(FExpressionOutput(TEXT("")));

	bShowInputs = true;
	bShowOutputs = true;
	bCollapsed = true;
	bShowMaskColorsOnPin = true;
}


#if WITH_EDITOR
void UMaterialExpression::CopyMaterialExpressions(const TArray<UMaterialExpression*>& SrcExpressions, const TArray<UMaterialExpressionComment*>& SrcExpressionComments, 
	UMaterial* Material, UMaterialFunction* EditFunction, TArray<UMaterialExpression*>& OutNewExpressions, TArray<UMaterialExpression*>& OutNewComments)
{
	OutNewExpressions.Empty();
	OutNewComments.Empty();

	UObject* ExpressionOuter = Material;
	if (EditFunction)
	{
		ExpressionOuter = EditFunction;
	}

	TMap<UMaterialExpression*,UMaterialExpression*> SrcToDestMap;

	// Duplicate source expressions into the editor's material copy buffer.
	for( int32 SrcExpressionIndex = 0 ; SrcExpressionIndex < SrcExpressions.Num() ; ++SrcExpressionIndex )
	{
		UMaterialExpression*	SrcExpression		= SrcExpressions[SrcExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* FunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(SrcExpression);
		bool bIsValidFunctionExpression = true;

		if (EditFunction 
			&& FunctionExpression 
			&& FunctionExpression->MaterialFunction
			&& FunctionExpression->MaterialFunction->IsDependent(EditFunction))
		{
			bIsValidFunctionExpression = false;
		}

		if (bIsValidFunctionExpression && IsAllowedExpressionType(SrcExpression->GetClass(), EditFunction != NULL))
		{
			UMaterialExpression* NewExpression = Cast<UMaterialExpression>(StaticDuplicateObject( SrcExpression, ExpressionOuter, NAME_None, RF_Transactional ));
			NewExpression->Material = Material;
			// Make sure we remove any references to functions the nodes came from
			NewExpression->Function = NULL;

			SrcToDestMap.Add( SrcExpression, NewExpression );

			// Add to list of material expressions associated with the copy buffer.
			Material->Expressions.Add( NewExpression );

			// There can be only one default mesh paint texture.
			UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>( NewExpression );
			if( TextureSample )
			{
				TextureSample->IsDefaultMeshpaintTexture = false;
			}

			NewExpression->UpdateParameterGuid(true, true);
			NewExpression->UpdateMaterialExpressionGuid(true, true);

			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>( NewExpression );
			if( FunctionInput )
			{
				FunctionInput->ConditionallyGenerateId(true);
				FunctionInput->ValidateName();
			}

			UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>( NewExpression );
			if( FunctionOutput )
			{
				FunctionOutput->ConditionallyGenerateId(true);
				FunctionOutput->ValidateName();
			}

			// Record in output list.
			OutNewExpressions.Add( NewExpression );
		}
	}

	// Fix up internal references.  Iterate over the inputs of the new expressions, and for each input that refers
	// to an expression that was duplicated, point the reference to that new expression.  Otherwise, clear the input.
	for( int32 NewExpressionIndex = 0 ; NewExpressionIndex < OutNewExpressions.Num() ; ++NewExpressionIndex )
	{
		UMaterialExpression* NewExpression = OutNewExpressions[NewExpressionIndex];
		const TArray<FExpressionInput*>& ExpressionInputs = NewExpression->GetInputs();
		for ( int32 ExpressionInputIndex = 0 ; ExpressionInputIndex < ExpressionInputs.Num() ; ++ExpressionInputIndex )
		{
			FExpressionInput* Input = ExpressionInputs[ExpressionInputIndex];
			UMaterialExpression* InputExpression = Input->Expression;
			if ( InputExpression )
			{
				UMaterialExpression** NewInputExpression = SrcToDestMap.Find( InputExpression );
				if ( NewInputExpression )
				{
					check( *NewInputExpression );
					Input->Expression = *NewInputExpression;
				}
				else
				{
					Input->Expression = NULL;
				}
			}
		}
	}

	// Copy Selected Comments
	for( int32 CommentIndex=0; CommentIndex<SrcExpressionComments.Num(); CommentIndex++)
	{
		UMaterialExpressionComment* ExpressionComment = SrcExpressionComments[CommentIndex];
		UMaterialExpressionComment* NewComment = Cast<UMaterialExpressionComment>(StaticDuplicateObject(ExpressionComment, ExpressionOuter));
		NewComment->Material = Material;

		// Add reference to the material
		Material->EditorComments.Add(NewComment);

		// Add to the output array.
		OutNewComments.Add(NewComment);
	}
}
#endif

void UMaterialExpression::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	const TArray<FExpressionInput*> Inputs = GetInputs();
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		FExpressionInput* Input = Inputs[InputIndex];
		DoMaterialAttributeReorder(Input, Ar.UE4Ver());
	}
#endif
}

bool UMaterialExpression::NeedsLoadForClient() const
{
	// Expressions that reference texture objects need to be cooked
	UMaterialExpression* MutableThis = const_cast<UMaterialExpression*>(this);
	return MutableThis->GetReferencedTexture() != nullptr;
}

void UMaterialExpression::PostInitProperties()
{
	Super::PostInitProperties();

	UpdateParameterGuid(false, false);
	
	UpdateMaterialExpressionGuid(false, true);
}

void UMaterialExpression::PostLoad()
{
	Super::PostLoad();

	if (!Material && GetOuter()->IsA(UMaterial::StaticClass()))
	{
		Material = CastChecked<UMaterial>(GetOuter());
	}

	if (!Function && GetOuter()->IsA(UMaterialFunction::StaticClass()))
	{
		Function = CastChecked<UMaterialFunction>(GetOuter());
	}

	UpdateParameterGuid(false, false);
	
	UpdateMaterialExpressionGuid(false, false);
}

void UMaterialExpression::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// We do not force a guid regen here because this function is used when the Material Editor makes a copy of a material to edit.
	// If we forced a GUID regen, it would cause all of the guids for a material to change everytime a material was edited.
	UpdateParameterGuid(false, true);
	UpdateMaterialExpressionGuid(false, true);
}

#if WITH_EDITOR

void UMaterialExpression::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!GIsImportingT3D && !GIsTransacting && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		FPropertyChangedEvent SubPropertyChangedEvent(nullptr, PropertyChangedEvent.ChangeType);

		// Don't recompile the outer material if we are in the middle of a transaction or interactively changing properties
		// as there may be many expressions in the transaction buffer and we would just be recompiling over and over again.
		if (Material && !Material->bIsPreviewMaterial)
		{
			Material->PreEditChange(nullptr);
			Material->PostEditChangeProperty(SubPropertyChangedEvent);
		}
		else if (Function)
		{
			Function->PreEditChange(nullptr);
			Function->PostEditChangeProperty(SubPropertyChangedEvent);
		}
	}

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged != NULL )
	{
		// Update the preview for this node if we adjusted a property
		bNeedToUpdatePreview = true;

		const FName PropertyName = PropertyThatChanged->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpression, Desc) && !IsA(UMaterialExpressionComment::StaticClass()))
		{
			if (GraphNode)
			{
				GraphNode->Modify();
				GraphNode->NodeComment = Desc;
			}
			// Don't need to update preview after changing description
			bNeedToUpdatePreview = false;
		}
	}
}

void UMaterialExpression::PostEditImport()
{
	Super::PostEditImport();

	UpdateParameterGuid(true, true);
}

bool UMaterialExpression::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty != NULL)
	{
		// Automatically set property as non-editable if it has OverridingInputProperty metadata
		// pointing to an FExpressionInput property which is hooked up as an input.
		//
		// e.g. in the below snippet, meta=(OverridingInputProperty = "A") indicates that ConstA will
		// be overridden by an FExpressionInput property named 'A' if one is connected, and will thereby
		// be set as non-editable.
		//
		//	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstA' if not specified"))
		//	FExpressionInput A;
		//
		//	UPROPERTY(EditAnywhere, Category = MaterialExpressionAdd, meta = (OverridingInputProperty = "A"))
		//	float ConstA;
		//

		static FName OverridingInputPropertyMetaData(TEXT("OverridingInputProperty"));

		if (InProperty->HasMetaData(OverridingInputPropertyMetaData))
		{
			const FString& OverridingPropertyName = InProperty->GetMetaData(OverridingInputPropertyMetaData);

			UStructProperty* StructProp = FindField<UStructProperty>(GetClass(), *OverridingPropertyName);
			if (ensure(StructProp != nullptr))
			{
				static FName RequiredInputMetaData(TEXT("RequiredInput"));

				// Must be a single FExpressionInput member, not an array, and must be tagged with metadata RequiredInput="false"
				if (ensure(	StructProp->Struct->GetFName() == NAME_ExpressionInput &&
							StructProp->ArrayDim == 1 &&
							StructProp->HasMetaData(RequiredInputMetaData) &&
							!StructProp->GetBoolMetaData(RequiredInputMetaData)))
				{
					const FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(this);

					if (Input->Expression != nullptr && Input->GetTracedInput().Expression != nullptr)
					{
						bIsEditable = false;
					}
				}
			}
		}
	}

	return bIsEditable;
}

#endif // WITH_EDITOR


TArray<FExpressionOutput>& UMaterialExpression::GetOutputs() 
{
	return Outputs;
}


const TArray<FExpressionInput*> UMaterialExpression::GetInputs()
{
	TArray<FExpressionInput*> Result;
	for( TFieldIterator<UStructProperty> InputIt(GetClass(), EFieldIteratorFlags::IncludeSuper,  EFieldIteratorFlags::ExcludeDeprecated) ; InputIt ; ++InputIt )
	{
		UStructProperty* StructProp = *InputIt;
		if( StructProp->Struct->GetFName() == NAME_ExpressionInput)
		{
			for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ArrayIndex++)
			{
				Result.Add(StructProp->ContainerPtrToValuePtr<FExpressionInput>(this, ArrayIndex));
			}
		}
	}
	return Result;
}


FExpressionInput* UMaterialExpression::GetInput(int32 InputIndex)
{
	int32 Index = 0;
	for( TFieldIterator<UStructProperty> InputIt(GetClass(), EFieldIteratorFlags::IncludeSuper,  EFieldIteratorFlags::ExcludeDeprecated) ; InputIt ; ++InputIt )
	{
		UStructProperty* StructProp = *InputIt;
		if( StructProp->Struct->GetFName() == NAME_ExpressionInput)
		{
			for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ArrayIndex++)
			{
			if( Index == InputIndex )
			{
					return StructProp->ContainerPtrToValuePtr<FExpressionInput>(this, ArrayIndex);
			}
			Index++;
		}
	}
	}

	return NULL;
}


FString UMaterialExpression::GetInputName(int32 InputIndex) const
{
	int32 Index = 0;
	for( TFieldIterator<UStructProperty> InputIt(GetClass(),EFieldIteratorFlags::IncludeSuper,  EFieldIteratorFlags::ExcludeDeprecated) ; InputIt ; ++InputIt )
	{
		UStructProperty* StructProp = *InputIt;
		if( StructProp->Struct->GetFName() == NAME_ExpressionInput)
		{
			for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ArrayIndex++)
			{
			if( Index == InputIndex )
			{
					FExpressionInput const* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(this, ArrayIndex);
					FString StructName = StructProp->GetFName().ToString();

					if (StructProp->ArrayDim > 1)
					{
						StructName += FString::FromInt(ArrayIndex);
					}

					return (Input->InputName.Len() > 0) ? Input->InputName : StructName;
			}
			Index++;
		}
	}
	}
	return TEXT("");
}

#if WITH_EDITOR
FText UMaterialExpression::GetCreationDescription() const
{
	return FText::GetEmpty();
}

FText UMaterialExpression::GetCreationName() const
{
	return FText::GetEmpty();
}
#endif

bool UMaterialExpression::IsInputConnectionRequired(int32 InputIndex) const
{
#if WITH_EDITOR
	int32 Index = 0;
	for( TFieldIterator<UStructProperty> InputIt(GetClass(), EFieldIteratorFlags::IncludeSuper,  EFieldIteratorFlags::ExcludeDeprecated) ; InputIt ; ++InputIt )
	{
		UStructProperty* StructProp = *InputIt;
		if( StructProp->Struct->GetFName() == NAME_ExpressionInput)
		{
			for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ArrayIndex++)
			{
				if( Index == InputIndex )
				{
					FExpressionInput const* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(this, ArrayIndex);
					const TCHAR* MetaKey = TEXT("RequiredInput");

					if( StructProp->HasMetaData(MetaKey) )
					{
						return StructProp->GetBoolMetaData(MetaKey);
					}
				}
				Index++;
			}
		}
	}
#endif
	return true;
}

#if WITH_EDITOR
uint32 UMaterialExpression::GetInputType(int32 InputIndex)
{
	// different inputs should be defined by sub classed expressions
	return MCT_Float;
}

uint32 UMaterialExpression::GetOutputType(int32 OutputIndex)
{
	// different outputs should be defined by sub classed expressions
	if (IsResultMaterialAttributes(OutputIndex))
	{
		return MCT_MaterialAttributes;
	}
	else
	{
		FExpressionOutput& Output = GetOutputs()[OutputIndex];
		if (Output.Mask)
		{
			int32 MaskChannelCount = (Output.MaskR ? 1 : 0)
									+ (Output.MaskG ? 1 : 0)
									+ (Output.MaskB ? 1 : 0)
									+ (Output.MaskA ? 1 : 0);
			switch (MaskChannelCount)
			{
			case 1:
				return MCT_Float;
			case 2:
				return MCT_Float2;
			case 3:
				return MCT_Float3;
			case 4:
				return MCT_Float4;
			default:
				return MCT_Unknown;
			}
		}
		else
		{
			return MCT_Float;
		}
	}
}
#endif

int32 UMaterialExpression::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}



int32 UMaterialExpression::GetHeight() const
{
	return FMath::Max(ME_CAPTION_HEIGHT + (Outputs.Num() * ME_STD_TAB_HEIGHT),ME_CAPTION_HEIGHT+ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2));
}



bool UMaterialExpression::UsesLeftGutter() const
{
	return 0;
}



bool UMaterialExpression::UsesRightGutter() const
{
	return 0;
}

#if WITH_EDITOR
void UMaterialExpression::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Expression"));
}

FString UMaterialExpression::GetDescription() const
{
	// Combined captions sufficient for most expressions
	TArray<FString> Captions;
	GetCaption(Captions);

	if (Captions.Num() > 1)
	{
		FString Result = Captions[0];
		for (int32 Index = 1; Index < Captions.Num(); ++Index)
		{
			Result += TEXT(" ");
			Result += Captions[Index];
		}

		return Result;
	}
	else
	{
		return Captions[0];
	}
}

void UMaterialExpression::GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip)
{
	if (InputIndex != INDEX_NONE)
	{
		const TArray<FExpressionInput*> Inputs = GetInputs();

		int32 Index = 0;
		for( TFieldIterator<UStructProperty> InputIt(GetClass()) ; InputIt ; ++InputIt )
		{
			UStructProperty* StructProp = *InputIt;
			if( StructProp->Struct->GetFName() == NAME_ExpressionInput )
			{
				for (int32 ArrayIndex = 0; ArrayIndex < StructProp->ArrayDim; ArrayIndex++)
				{
					if( Index == InputIndex && StructProp->HasMetaData(TEXT("tooltip")) )
					{
						// Set the tooltip from the .h comments
						ConvertToMultilineToolTip(StructProp->GetToolTipText().ToString(), 40, OutToolTip);
						return;
					}
					Index++;
				}
			}
		}
	}
}

int32 UMaterialExpression::CompilerError(FMaterialCompiler* Compiler, const TCHAR* pcMessage)
{
	TArray<FString> Captions;
	GetCaption(Captions);
	return Compiler->Errorf(TEXT("%s> %s"), Desc.Len() > 0 ? *Desc : *Captions[0], pcMessage);
}
#endif // WITH_EDITOR

bool UMaterialExpression::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	bNeedToUpdatePreview = true;
	
	return Super::Modify(bAlwaysMarkDirty);
}

bool UMaterialExpression::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if (FCString::Stristr(SearchQuery, TEXT("NAME=")) != NULL)
	{
		FString SearchString(SearchQuery);
		SearchString = SearchString.Right(SearchString.Len() - 5);
		return (GetName().Contains(SearchString) );
	}
	return Desc.Contains(SearchQuery);
}

#if WITH_EDITOR
void UMaterialExpression::ConnectExpression( FExpressionInput* Input, int32 OutputIndex )
{
	if( Input && OutputIndex >= 0 && OutputIndex < Outputs.Num() )
	{
		FExpressionOutput& Output = Outputs[OutputIndex];
		Input->Expression = this;
		Input->OutputIndex = OutputIndex;
		Input->Mask = Output.Mask;
		Input->MaskR = Output.MaskR;
		Input->MaskG = Output.MaskG;
		Input->MaskB = Output.MaskB;
		Input->MaskA = Output.MaskA;
	}
}
#endif // WITH_EDITOR

void UMaterialExpression::UpdateMaterialExpressionGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty)
{
	// If we are in the editor, and we don't have a valid GUID yet, generate one.
	if (GIsEditor && !FApp::IsGame())
	{
		FGuid& Guid = GetMaterialExpressionId();

		if (bForceGeneration || !Guid.IsValid())
		{
			Guid = FGuid::NewGuid();

			if (bAllowMarkingPackageDirty)
			{
				MarkPackageDirty();
			}
		}
	}
}

void UMaterialExpression::UpdateParameterGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty)
{
	if (bIsParameterExpression)
	{
		// If we are in the editor, and we don't have a valid GUID yet, generate one.
		if(GIsEditor && !FApp::IsGame())
		{
			FGuid& Guid = GetParameterExpressionId();

			if (bForceGeneration || !Guid.IsValid())
			{
				Guid = FGuid::NewGuid();

				if (bAllowMarkingPackageDirty)
				{
					MarkPackageDirty();
				}
			}
		}
	}
}

#if WITH_EDITOR
void UMaterialExpression::ConnectToPreviewMaterial(UMaterial* InMaterial, int32 OutputIndex)
{
	if (InMaterial && OutputIndex >= 0 && OutputIndex < Outputs.Num())
	{
		bool bUseMaterialAttributes = IsResultMaterialAttributes(0);

		if( bUseMaterialAttributes )
		{
			InMaterial->SetShadingModel(MSM_DefaultLit);
			InMaterial->bUseMaterialAttributes = true;
			FExpressionInput* MaterialInput = InMaterial->GetExpressionInputForProperty(MP_MaterialAttributes);
			check(MaterialInput);
			ConnectExpression( MaterialInput, OutputIndex );
		}
		else
		{
			InMaterial->SetShadingModel(MSM_Unlit);
			InMaterial->bUseMaterialAttributes = false;

			// Connect the selected expression to the emissive node of the expression preview material.  The emissive material is not affected by light which is why its a good choice.
			FExpressionInput* MaterialInput = InMaterial->GetExpressionInputForProperty(MP_EmissiveColor);
			check(MaterialInput);
			ConnectExpression( MaterialInput, OutputIndex );
		}
	}
}
#endif // WITH_EDITOR

void UMaterialExpression::ValidateState()
{
	// Disabled for now until issues can be tracked down
	//check(!IsPendingKill());
}

#if WITH_EDITOR
bool UMaterialExpression::GetAllInputExpressions(TArray<UMaterialExpression*>& InputExpressions)
{
	// Make sure we don't end up in a loop
	if (!InputExpressions.Contains(this))
	{
		bool bFoundRepeat = false;
		InputExpressions.Add(this);

		const TArray<FExpressionInput*> Inputs = GetInputs();

		for (int32 Index = 0; Index < Inputs.Num(); Index++)
		{
			if (Inputs[Index]->Expression)
			{
				if (Inputs[Index]->Expression->GetAllInputExpressions(InputExpressions))
				{
					bFoundRepeat = true;
				}
			}
		}

		return bFoundRepeat;
	}
	else
	{
		return true;
	}
}

bool UMaterialExpression::CanRenameNode() const
{
	return false;
}

FString UMaterialExpression::GetEditableName() const
{
	// This function is only safe to call in a class that has implemented CanRenameNode() to return true
	check(false);
	return TEXT("");
}

void UMaterialExpression::SetEditableName(const FString& NewName)
{
	// This function is only safe to call in a class that has implemented CanRenameNode() to return true
	check(false);
}

void UMaterialExpression::ValidateParameterName()
{
	if (Material != nullptr)
	{
		int32 NameIndex = 1;
		bool FoundValidName = false;
		FName PotentialName;

		// Find an available unique name
		while (!FoundValidName)
		{
			PotentialName = GetParameterName();

			if (NameIndex != 1)
			{
				PotentialName.SetNumber(NameIndex);
			}

			FoundValidName = true;

			for (UMaterialExpression* Expression : Material->Expressions)
			{
				if (Expression != nullptr && Expression->HasAParameterName())
				{
					// Name are unique per class type
					if (Expression != this && Expression->GetClass() == GetClass() && Expression->GetParameterName() == PotentialName)
					{
						FoundValidName = false;
						break;
					}
				}
			}

			++NameIndex;
		}

		SetParameterName(PotentialName);
	}
}

#endif // WITH_EDITOR


bool UMaterialExpression::ContainsInputLoop(const bool bStopOnFunctionCall /*= true*/)
{
	TArray<FMaterialExpressionKey> ExpressionStack;
	TSet<FMaterialExpressionKey> VisitedExpressions;
	return ContainsInputLoopInternal(ExpressionStack, VisitedExpressions, bStopOnFunctionCall);
}

bool UMaterialExpression::ContainsInputLoopInternal(TArray<FMaterialExpressionKey>& ExpressionStack, TSet<FMaterialExpressionKey>& VisitedExpressions, const bool bStopOnFunctionCall)
{
#if WITH_EDITORONLY_DATA
	const TArray<FExpressionInput*> Inputs = GetInputs();
	for (int32 Index = 0; Index < Inputs.Num(); ++Index)
	{
		FExpressionInput* Input = Inputs[Index];
		if (Input->Expression)
		{
			// ContainsInputLoop primarily used to detect safe traversal path for IsResultMaterialAttributes.
			// In those cases we can bail on a function as the inputs are strongly typed
			UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Input->Expression);
			if (bStopOnFunctionCall && FunctionCall)
			{
				continue;
			}

			FMaterialExpressionKey InputExpressionKey(Input->Expression, Input->OutputIndex);
			if (ExpressionStack.Contains(InputExpressionKey))
			{
				return true;
			}
			// prevent recurring visits to expressions we've already checked
			else if (!VisitedExpressions.Contains(InputExpressionKey))
			{
				VisitedExpressions.Add(InputExpressionKey);
				ExpressionStack.Add(InputExpressionKey);
				if (Input->Expression->ContainsInputLoopInternal(ExpressionStack, VisitedExpressions, bStopOnFunctionCall))
				{
					return true;
				}
				ExpressionStack.Pop();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	return false;
}

UMaterialExpressionTextureBase::UMaterialExpressionTextureBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, IsDefaultMeshpaintTexture(false)
{}

#if WITH_EDITOR
void UMaterialExpressionTextureBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if (IsDefaultMeshpaintTexture && PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == FName(TEXT("IsDefaultMeshpaintTexture")))
		{
			const TArray<UMaterialExpression*>& Expressions = this->Material->GetMaterial()->Expressions;

			// Check for other defaulted textures in THIS material (does not search sub levels ie functions etc, as these are ignored in the texture painter). 
			for (auto ItExpressions = Expressions.CreateConstIterator(); ItExpressions; ItExpressions++)
			{
				UMaterialExpressionTextureBase* TextureSample = Cast<UMaterialExpressionTextureBase>(*ItExpressions);
				if (TextureSample != NULL && TextureSample != this)
				{
					if(TextureSample->IsDefaultMeshpaintTexture)
					{
						FText ErrorMessage = LOCTEXT("MeshPaintDefaultTextureErrorDefault","Only one texture can be set as the Mesh Paint Default Texture, disabling previous default");
						if (TextureSample->Texture != NULL)
						{
							FFormatNamedArguments Args;
							Args.Add( TEXT("TextureName"), FText::FromString( TextureSample->Texture->GetName() ) );
							ErrorMessage = FText::Format(LOCTEXT("MeshPaintDefaultTextureErrorTextureKnown","Only one texture can be set as the Mesh Paint Default Texture, disabling {TextureName}"), Args );
						}
										
						// Launch notification to inform user of default change
						FNotificationInfo Info( ErrorMessage );
						Info.ExpireDuration = 5.0f;
						Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));

						FSlateNotificationManager::Get().AddNotification(Info);

						// Reset the previous default to false;
						TextureSample->IsDefaultMeshpaintTexture = false;
					}
				}
			}
		}
	}
}

FString UMaterialExpressionTextureBase::GetDescription() const
{
	FString Result = Super::GetDescription();
	Result += TEXT(" (");
	Result += Texture ? Texture->GetName() : TEXT("None");
	Result += TEXT(")");

	return Result;
}
#endif // WITH_EDITOR

void UMaterialExpressionTextureBase::AutoSetSampleType()
{
	if ( Texture )
	{
		SamplerType = GetSamplerTypeForTexture( Texture );
	}
}

EMaterialSamplerType UMaterialExpressionTextureBase::GetSamplerTypeForTexture(const UTexture* Texture)
{
	if (Texture)
	{
		if (Texture->GetMaterialType() == MCT_TextureExternal)
		{
			return SAMPLERTYPE_External;
		}

		switch (Texture->CompressionSettings)
		{
			case TC_Normalmap:
				return SAMPLERTYPE_Normal;
			case TC_Grayscale:
				return Texture->SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
			case TC_Alpha:
				return SAMPLERTYPE_Alpha;
			case TC_Masks:
				return SAMPLERTYPE_Masks;
			case TC_DistanceFieldFont:
				return SAMPLERTYPE_DistanceFieldFont;
			default:
				return Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		}
	}
	return SAMPLERTYPE_Color;
}



UMaterialExpressionTextureSample::UMaterialExpressionTextureSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShowTextureInputPin(true)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Texture;
		FConstructorStatics()
			: NAME_Texture(LOCTEXT( "Texture", "Texture" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));

	MipValueMode = TMVM_None;

	bCollapsed = false;

	ConstCoordinate = 0;
	ConstMipValue = INDEX_NONE;
}

#if WITH_EDITOR
bool UMaterialExpressionTextureSample::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty != NULL)
	{
		FName PropertyFName = InProperty->GetFName();

		if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSample, ConstMipValue))
		{
			bIsEditable = MipValueMode == TMVM_MipLevel || MipValueMode == TMVM_MipBias;
		}
		else if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSample, ConstCoordinate))
		{
			bIsEditable = !Coordinates.GetTracedInput().Expression;
		}
		else if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSample, Texture))
		{
			// The Texture property is overridden by a connection to TextureObject
			bIsEditable = TextureObject.GetTracedInput().Expression == NULL;
		}
	}

	return bIsEditable;
}

void UMaterialExpressionTextureSample::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if ( PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetName() == TEXT("Texture") )
	{
		if ( Texture )
		{
			AutoSetSampleType();
			FEditorSupportDelegates::ForcePropertyWindowRebuild.Broadcast(this);
		}
	}

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionTextureSample, MipValueMode))
		{
			if (GraphNode)
			{
				GraphNode->ReconstructNode();
			}
		}
	}
	
	// Need to update expression properties before super call (which triggers recompile)
	Super::PostEditChangeProperty( PropertyChangedEvent );	
}

void UMaterialExpressionTextureSample::PostLoad()
{
	Super::PostLoad();

	// Clear invalid input reference
	if (!bShowTextureInputPin && TextureObject.Expression)
	{
		TextureObject.Expression = nullptr;
	}
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionTextureSample::GetInputs()
{
	TArray<FExpressionInput*> OutInputs;

	// todo: we should remove GetInputs() and make this the common code for all expressions
	uint32 InputIndex = 0;
	while(FExpressionInput* Ptr = GetInput(InputIndex++))
	{
		OutInputs.Add(Ptr);
	}

	return OutInputs;
}

// this define is only used for the following function
#define IF_INPUT_RETURN(Item) if(!InputIndex) return &Item;	--InputIndex
FExpressionInput* UMaterialExpressionTextureSample::GetInput(int32 InputIndex)
{
	IF_INPUT_RETURN(Coordinates);

	if (bShowTextureInputPin)
	{
	IF_INPUT_RETURN(TextureObject);
	}

	if(MipValueMode == TMVM_Derivative)
	{
		IF_INPUT_RETURN(CoordinatesDX);
		IF_INPUT_RETURN(CoordinatesDY);
	}
	else if(MipValueMode != TMVM_None)
	{
		IF_INPUT_RETURN(MipValue);
	}

	return NULL;
}
#undef IF_INPUT_RETURN

// this define is only used for the following function
#define IF_INPUT_RETURN(Item, Name) if(!InputIndex) return Name; --InputIndex
FString UMaterialExpressionTextureSample::GetInputName(int32 InputIndex) const
{
	IF_INPUT_RETURN(Coordinates, TEXT("Coordinates"));

	if (bShowTextureInputPin)
	{
	IF_INPUT_RETURN(TextureObject, TEXT("TextureObject"));
	}

	if(MipValueMode == TMVM_MipLevel)
	{
		IF_INPUT_RETURN(MipValue, TEXT("MipLevel"));
	}
	else if(MipValueMode == TMVM_MipBias)
	{
		IF_INPUT_RETURN(MipValue, TEXT("MipBias"));
	}
	else if(MipValueMode == TMVM_Derivative)
	{
		IF_INPUT_RETURN(CoordinatesDX, TEXT("DDX(UVs)"));
		IF_INPUT_RETURN(CoordinatesDY, TEXT("DDY(UVs)"));
	}

	return TEXT("");
}
#undef IF_INPUT_RETURN

/**
 * Verify that the texture and sampler type. Generates a compiler waring if 
 * they do not.
 * @param Compiler - The material compiler to which errors will be reported.
 * @param ExpressionDesc - Description of the expression verifying the sampler type.
 * @param Texture - The texture to verify. A NULL texture is considered valid!
 * @param SamplerType - The sampler type to verify.
 */
static bool VerifySamplerType(
	FMaterialCompiler* Compiler,
	const TCHAR* ExpressionDesc,
	const UTexture* Texture,
	EMaterialSamplerType SamplerType )
{
	check( Compiler );
	check( ExpressionDesc );

	if ( Texture )
	{
		EMaterialSamplerType CorrectSamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture( Texture );
		if ( SamplerType != CorrectSamplerType )
		{
			UEnum* SamplerTypeEnum = UMaterialInterface::GetSamplerTypeEnum();
			check( SamplerTypeEnum );

			FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(SamplerType).ToString();
			FString TextureTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(CorrectSamplerType).ToString();

			Compiler->Errorf( TEXT("%s> Sampler type is %s, should be %s for %s"),
				ExpressionDesc,
				*SamplerTypeDisplayName,
				*TextureTypeDisplayName,
				*Texture->GetPathName() );
			return false;
		}
		if((SamplerType == SAMPLERTYPE_Normal || SamplerType == SAMPLERTYPE_Masks) && Texture->SRGB)
		{
			UEnum* SamplerTypeEnum = UMaterialInterface::GetSamplerTypeEnum();
			check( SamplerTypeEnum );

			FString SamplerTypeDisplayName = SamplerTypeEnum->GetDisplayNameTextByValue(SamplerType).ToString();

			Compiler->Errorf( TEXT("%s> To use '%s' as sampler type, SRGB must be disabled for %s"),
				ExpressionDesc,
				*SamplerTypeDisplayName,
				*Texture->GetPathName() );
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Texture || TextureObject.Expression) // We deal with reroute textures later on in this function..
	{
		int32 TextureReferenceIndex = INDEX_NONE;
		int32 TextureCodeIndex = INDEX_NONE;

		if (TextureObject.Expression)
		{
			TextureCodeIndex = TextureObject.Compile(Compiler);
		}
		else if (SamplerType == SAMPLERTYPE_External)
		{
			TextureCodeIndex = Compiler->ExternalTexture(Texture, TextureReferenceIndex);
		}
		else
		{
			TextureCodeIndex = Compiler->Texture(Texture, TextureReferenceIndex, SamplerSource, MipValueMode);
		}

		UTexture* EffectiveTexture = Texture;
		EMaterialSamplerType EffectiveSamplerType = SamplerType;
		TOptional<FName> EffectiveParameterName;
		if (TextureObject.Expression)
		{
			UMaterialExpression* InputExpression = TextureObject.Expression;

			// If we are referencing a texture input through a reroute node, we'll need to backtrack
			// to get to the real texture.
			UMaterialExpressionReroute* RerouteInput = Cast<UMaterialExpressionReroute>(InputExpression);
			if (RerouteInput)
			{
				int32 ExpressionOutputIndex = -1;
				InputExpression  = RerouteInput->TraceInputsToRealExpression(ExpressionOutputIndex);
				if (InputExpression == nullptr)
				{
					return CompilerError(Compiler, TEXT("Missing rerouted input texture"));
				}
				else if (OutputIndex >= 0)
				{
					uint32 OutputType = InputExpression->GetOutputType(OutputIndex);
					if (OutputType != MCT_Texture2D && OutputType != MCT_TextureCube && OutputType != MCT_Texture && OutputType != MCT_TextureExternal)
					{
						return Compiler->Errorf(TEXT("TextureSample> Reroute not bound to proper texture type!"));
					}
				}
			}

			UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(InputExpression);
			if (FunctionInput)
			{	
				UMaterialExpressionFunctionInput* NestedFunctionInput = FunctionInput;

				// Walk the input chain to find the last node in the chain
				while (true)
				{
					UMaterialExpression* PreviewExpression = NestedFunctionInput->GetEffectivePreviewExpression();
					if (PreviewExpression && PreviewExpression->IsA(UMaterialExpressionFunctionInput::StaticClass()))
					{
						NestedFunctionInput = CastChecked<UMaterialExpressionFunctionInput>(PreviewExpression);
					}
					else
					{
						break;
					}
				}
				InputExpression = NestedFunctionInput->GetEffectivePreviewExpression();
			}

			
			UMaterialExpressionTextureObject* TextureObjectExpression = Cast<UMaterialExpressionTextureObject>(InputExpression);
			UMaterialExpressionTextureObjectParameter* TextureObjectParameter = Cast<UMaterialExpressionTextureObjectParameter>(InputExpression);
			if (TextureObjectExpression)
			{
				EffectiveTexture = TextureObjectExpression->Texture;
				EffectiveSamplerType = TextureObjectExpression->SamplerType;
			}
			else if (TextureObjectParameter)
			{
				EffectiveTexture = TextureObjectParameter->Texture;
				EffectiveSamplerType = TextureObjectParameter->SamplerType;
				EffectiveParameterName = TextureObjectParameter->ParameterName;
			}

			TextureReferenceIndex = Compiler->GetTextureReferenceIndex(EffectiveTexture);
		}

		if (EffectiveTexture && VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("TextureSample")), EffectiveTexture, EffectiveSamplerType))
		{
			if (TextureCodeIndex != INDEX_NONE)
			{
				const EMaterialValueType TextureType = Compiler->GetParameterType(TextureCodeIndex);
				if (TextureType == MCT_TextureCube && !Coordinates.GetTracedInput().Expression)
				{
					return CompilerError(Compiler, TEXT("UV input required for cubemap sample"));
				}
			}

			int32 CoordinateIndex = Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);

			// If the sampler type is an external texture, we have might have a scale/bias to apply to the UV coordinates.
			// Generate that code for the TextureReferenceIndex here so we compile it using the correct texture based on possible reroute textures above
			if (EffectiveSamplerType == SAMPLERTYPE_External)
			{
				CoordinateIndex = CompileExternalTextureCoordinates(Compiler, CoordinateIndex, TextureReferenceIndex, EffectiveParameterName);
			}

			return Compiler->TextureSample(
				TextureCodeIndex,
				CoordinateIndex,
				EffectiveSamplerType,
				CompileMipValue0(Compiler),
				CompileMipValue1(Compiler),
				MipValueMode,
				SamplerSource,
				TextureReferenceIndex);
		}
		else
		{
			// TextureObject.Expression is responsible for generating the error message, since it had a NULL texture value
			return INDEX_NONE;
		}
	}
	else
	{
		return CompilerError(Compiler, TEXT("Missing input texture"));
	}
}
#endif // WITH_EDITOR

int32 UMaterialExpressionTextureSample::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

#if WITH_EDITOR
void UMaterialExpressionTextureSample::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Texture Sample"));
}
#endif // WITH_EDITOR

bool UMaterialExpressionTextureSample::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( Texture!=NULL && Texture->GetName().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR
// this define is only used for the following function
#define IF_INPUT_RETURN(Item, Type) if(!InputIndex) return Type; --InputIndex
uint32 UMaterialExpressionTextureSample::GetInputType(int32 InputIndex)
{
	IF_INPUT_RETURN(Coordinates, MCT_Float);

	if (bShowTextureInputPin)
	{
		// TODO: Only show the TextureObject input inside a material function, since that's the only place it is useful
	IF_INPUT_RETURN(TextureObject, MCT_Texture);
	}
	
	if(MipValueMode == TMVM_MipLevel || MipValueMode == TMVM_MipBias)
	{
		IF_INPUT_RETURN(MipValue, MCT_Float);
	}
	else if(MipValueMode == TMVM_Derivative)
	{
		IF_INPUT_RETURN(CoordinatesDX, MCT_Float);
		IF_INPUT_RETURN(CoordinatesDY, MCT_Float);
	}

	return MCT_Unknown;
}
#undef IF_INPUT_RETURN

int32 UMaterialExpressionTextureSample::CompileMipValue0(class FMaterialCompiler* Compiler)
{
	if (MipValueMode == TMVM_Derivative)
	{
		if (CoordinatesDX.GetTracedInput().IsConnected())
		{
			return CoordinatesDX.Compile(Compiler);
		}
	}
	else if (MipValue.GetTracedInput().IsConnected())
	{
		return MipValue.Compile(Compiler);
	}
	else
	{
		return Compiler->Constant(ConstMipValue);
	}

	return INDEX_NONE;
}

int32 UMaterialExpressionTextureSample::CompileMipValue1(class FMaterialCompiler* Compiler)
{
	if (MipValueMode == TMVM_Derivative && CoordinatesDY.GetTracedInput().IsConnected())
	{
		return CoordinatesDY.Compile(Compiler);
	}

	return INDEX_NONE;
}
#endif // WITH_EDITOR

UMaterialExpressionAdd::UMaterialExpressionAdd(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 0.0f;
	ConstB = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

//
//  UMaterialExpressionTextureSampleParameter
//
UMaterialExpressionTextureSampleParameter::UMaterialExpressionTextureSampleParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Obsolete;
		FConstructorStatics()
			: NAME_Obsolete(LOCTEXT( "Obsolete", "Obsolete" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bIsParameterExpression = true;
	bShowTextureInputPin = false;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add( ConstructorStatics.NAME_Obsolete);
	SortPriority = 0;
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureSampleParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Texture == NULL)
	{
		return CompilerError(Compiler, GetRequirements());
	}

	if (!TextureIsValid(Texture))
	{
		return CompilerError(Compiler, GetRequirements());
	}

	if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("TextureSampleParameter")), Texture, SamplerType))
	{
		return INDEX_NONE;
	}

	if (!ParameterName.IsValid() || ParameterName.IsNone())
	{
		return UMaterialExpressionTextureSample::Compile(Compiler, OutputIndex);
	}

	return CompileTextureSample(
		Compiler,
		Texture,
		Coordinates.GetTracedInput().Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false),
		SamplerType,
		ParameterName,
		CompileMipValue0(Compiler),
		CompileMipValue1(Compiler),
		MipValueMode,
		SamplerSource);
}

void UMaterialExpressionTextureSampleParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Texture Param")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionTextureSampleParameter::IsNamedParameter(FName InParameterName, UTexture*& OutValue) const
{
	if (InParameterName == ParameterName)
	{
		OutValue = Texture;
		return true;
	}

	return false;
}

bool UMaterialExpressionTextureSampleParameter::TextureIsValid( UTexture* /*InTexture*/ )
{
	return false;
}

const TCHAR* UMaterialExpressionTextureSampleParameter::GetRequirements()
{
	return TEXT("Invalid texture type");
}


void UMaterialExpressionTextureSampleParameter::SetDefaultTexture()
{
	// Does nothing in the base case...
}

void UMaterialExpressionTextureSampleParameter::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if(CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}

//
//  UMaterialExpressionTextureObjectParameter
//
UMaterialExpressionTextureObjectParameter::UMaterialExpressionTextureObjectParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> DefaultTexture2D;
		FText NAME_Texture;
		FText NAME_Parameters;
		FConstructorStatics()
			: DefaultTexture2D(TEXT("/Engine/EngineResources/DefaultTexture"))
			, NAME_Texture(LOCTEXT( "Texture", "Texture" ))
			, NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.DefaultTexture2D.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif

	Outputs.Empty();
	Outputs.Add(FExpressionOutput(TEXT("")));
}

#if WITH_EDITOR
void UMaterialExpressionTextureObjectParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Param Tex Object")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

const TCHAR* UMaterialExpressionTextureObjectParameter::GetRequirements()
{
	return TEXT("Requires valid texture");
}

const TArray<FExpressionInput*> UMaterialExpressionTextureObjectParameter::GetInputs()
{
	// Hide the texture coordinate input
	return TArray<FExpressionInput*>();
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureObjectParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Texture)
	{
		return CompilerError(Compiler, GetRequirements());
	}

	return SamplerType == SAMPLERTYPE_External ? Compiler->ExternalTextureParameter(ParameterName, Texture) : Compiler->TextureParameter(ParameterName, Texture);
}

int32 UMaterialExpressionTextureObjectParameter::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Texture)
	{
		return CompilerError(Compiler, GetRequirements());
	}

	// Preview the texture object by actually sampling it
	return CompileTextureSample(Compiler, Texture, Compiler->TextureCoordinate(0, false, false), SamplerType, ParameterName);
}
#endif // WITH_EDITOR

//
//  UMaterialExpressionTextureObject
//
UMaterialExpressionTextureObject::UMaterialExpressionTextureObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> Object0;
		FText NAME_Texture;
		FText NAME_Functions;
		FConstructorStatics()
			: Object0(TEXT("/Engine/EngineResources/DefaultTexture"))
			, NAME_Texture(LOCTEXT( "Texture", "Texture" ))
			, NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.Object0.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif

	Outputs.Empty();
	Outputs.Add(FExpressionOutput(TEXT("")));

	bCollapsed = false;
}

#if WITH_EDITOR
void UMaterialExpressionTextureObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty( PropertyChangedEvent );

	if ( PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetName() == TEXT("Texture") )
	{
		if ( Texture )
		{
			AutoSetSampleType();
			FEditorSupportDelegates::ForcePropertyWindowRebuild.Broadcast(this);
		}
	}
}

void UMaterialExpressionTextureObject::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Texture Object")); 
}


int32 UMaterialExpressionTextureObject::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Texture)
	{
		return CompilerError(Compiler, TEXT("Requires valid texture"));
	}
	return SamplerType == SAMPLERTYPE_External ? Compiler->ExternalTexture(Texture) : Compiler->Texture(Texture);
}

int32 UMaterialExpressionTextureObject::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Texture)
	{
		return CompilerError(Compiler, TEXT("Requires valid texture"));
	}

	return CompileTextureSample(Compiler, Texture, Compiler->TextureCoordinate(0, false, false), UMaterialExpressionTextureBase::GetSamplerTypeForTexture( Texture ));
}

uint32 UMaterialExpressionTextureObject::GetOutputType(int32 OutputIndex)
{
	if (Cast<UTextureCube>(Texture) != NULL)
	{
		return MCT_TextureCube;
	}
	else
	{
		return MCT_Texture2D;
	}
}
#endif //WITH_EDITOR

//
//  UMaterialExpressionTextureProperty
//
UMaterialExpressionTextureProperty::UMaterialExpressionTextureProperty(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Texture", "Texture" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Property = TMTM_TextureSize;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
	bShowOutputNameOnPin = false;
	
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("")));
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureProperty::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{	
	if (!TextureObject.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("TextureSample> Missing input texture"));
	}

	const int32 TextureCodeIndex = TextureObject.Compile(Compiler);
	if (TextureCodeIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return Compiler->TextureProperty(TextureCodeIndex, Property);
}

void UMaterialExpressionTextureProperty::GetTexturesForceMaterialRecompile(TArray<UTexture *> &Textures) const
{
	UMaterialExpression *TextureObjectExpression = TextureObject.GetTracedInput().Expression;

	if (TextureObjectExpression && TextureObjectExpression->IsA(UMaterialExpressionTextureBase::StaticClass()))
	{
		UMaterialExpressionTextureBase *TextureExpressionBase = Cast<UMaterialExpressionTextureBase>(TextureObjectExpression);
		if (TextureExpressionBase->Texture)
		{
			Textures.Add(TextureExpressionBase->Texture);
		}
	}
}

void UMaterialExpressionTextureProperty::GetCaption(TArray<FString>& OutCaptions) const
{
#if WITH_EDITOR
	const UEnum* TexturePropertyEnum = FindObject<UEnum>(NULL, TEXT("Engine.EMaterialExposedTextureProperty"));
	check(TexturePropertyEnum);

	const FString PropertyDisplayName = TexturePropertyEnum->GetDisplayNameTextByValue(Property).ToString();
#else
	const FString PropertyDisplayName = TEXT("");
#endif

	OutCaptions.Add(PropertyDisplayName);
}

// this define is only used for the following function
#define IF_INPUT_RETURN(Item, Type) if(!InputIndex) return Type; --InputIndex
uint32 UMaterialExpressionTextureProperty::GetInputType(int32 InputIndex)
{
	IF_INPUT_RETURN(TextureObject, MCT_Texture);
	return MCT_Unknown;
}
#undef IF_INPUT_RETURN
#endif

//
//  UMaterialExpressionTextureSampleParameter2D
//
UMaterialExpressionTextureSampleParameter2D::UMaterialExpressionTextureSampleParameter2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> DefaultTexture;
		FText NAME_Texture;
		FText NAME_Parameters;
		FConstructorStatics()
			: DefaultTexture(TEXT("/Engine/EngineResources/DefaultTexture"))
			, NAME_Texture(LOCTEXT( "Texture", "Texture" ))
			, NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.DefaultTexture.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
}

#if WITH_EDITOR
void UMaterialExpressionTextureSampleParameter2D::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Param2D")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionTextureSampleParameter2D::TextureIsValid( UTexture* InTexture )
{
	bool Result=false;
	if (InTexture)		
	{
		if( InTexture->GetClass() == UTexture2D::StaticClass() ) 
		{
			Result = true;
		}
		if( InTexture->IsA(UTextureRenderTarget2D::StaticClass()) )	
		{
			Result = true;
		}
		if ( InTexture->IsA(UTexture2DDynamic::StaticClass()) )
		{
			Result = true;
		}
		if ( InTexture->GetMaterialType() == MCT_TextureExternal )
		{
			Result = true;
		}
	}
	return Result;
}

const TCHAR* UMaterialExpressionTextureSampleParameter2D::GetRequirements()
{
	return TEXT("Requires Texture2D");
}


void UMaterialExpressionTextureSampleParameter2D::SetDefaultTexture()
{
	Texture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), NULL, LOAD_None, NULL);
}


bool UMaterialExpressionTextureSampleParameter::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( ParameterName.ToString().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR
FString UMaterialExpressionTextureSampleParameter::GetEditableName() const
{
	return ParameterName.ToString();
}

void UMaterialExpressionTextureSampleParameter::SetEditableName(const FString& NewName)
{
	ParameterName = *NewName;
}
#endif


//
//  UMaterialExpressionTextureSampleParameterCube
//
UMaterialExpressionTextureSampleParameterCube::UMaterialExpressionTextureSampleParameterCube(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTextureCube> DefaultTextureCube;
		FText NAME_Texture;
		FText NAME_Parameters;
		FConstructorStatics()
			: DefaultTextureCube(TEXT("/Engine/EngineResources/DefaultTextureCube"))
			, NAME_Texture(LOCTEXT( "Texture", "Texture" ))
			, NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.DefaultTextureCube.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureSampleParameterCube::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Coordinates.GetTracedInput().Expression)
	{
		return CompilerError(Compiler, TEXT("Cube sample needs UV input"));
	}

	return UMaterialExpressionTextureSampleParameter::Compile(Compiler, OutputIndex);
}

void UMaterialExpressionTextureSampleParameterCube::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ParamCube")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionTextureSampleParameterCube::TextureIsValid( UTexture* InTexture )
{
	bool Result=false;
	if (InTexture)
	{
		if( InTexture->GetClass() == UTextureCube::StaticClass() ) {
			Result = true;
		}
		if( InTexture->IsA(UTextureRenderTargetCube::StaticClass()) ) {
			Result = true;
		}
	}
	return Result;
}

const TCHAR* UMaterialExpressionTextureSampleParameterCube::GetRequirements()
{
	return TEXT("Requires TextureCube");
}

void UMaterialExpressionTextureSampleParameterCube::SetDefaultTexture()
{
	Texture = LoadObject<UTextureCube>(NULL, TEXT("/Engine/EngineResources/DefaultTextureCube.DefaultTextureCube"), NULL, LOAD_None, NULL);
}

/** 
 * Performs a SubUV operation, which is doing a texture lookup into a sub rectangle of a texture, and optionally blending with another rectangle.  
 * This supports both sprites and mesh emitters.
 */
static int32 ParticleSubUV(FMaterialCompiler* Compiler, int32 TextureIndex, UTexture* DefaultTexture, EMaterialSamplerType SamplerType, FExpressionInput& Coordinates, bool bBlend)
{
	return Compiler->ParticleSubUV(TextureIndex, SamplerType, bBlend);
}

/** 
 *	UMaterialExpressionTextureSampleParameterSubUV
 */
UMaterialExpressionTextureSampleParameterSubUV::UMaterialExpressionTextureSampleParameterSubUV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bBlend = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureSampleParameterSubUV::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Texture == NULL)
	{
		return CompilerError(Compiler, GetRequirements());
	}

	if (!TextureIsValid(Texture))
	{
		return CompilerError(Compiler, GetRequirements());
	}

	if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("TextureSampleParameterSubUV")), Texture, SamplerType))
	{
		return INDEX_NONE;
	}

	int32 TextureCodeIndex = Compiler->TextureParameter(ParameterName, Texture);
	return ParticleSubUV(Compiler, TextureCodeIndex, Texture, SamplerType, Coordinates, bBlend);
}

void UMaterialExpressionTextureSampleParameterSubUV::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Parameter SubUV"));
}
#endif // WITH_EDITOR

bool UMaterialExpressionTextureSampleParameterSubUV::TextureIsValid( UTexture* InTexture )
{
	return UMaterialExpressionTextureSampleParameter2D::TextureIsValid(InTexture);
}

const TCHAR* UMaterialExpressionTextureSampleParameterSubUV::GetRequirements()
{
	return UMaterialExpressionTextureSampleParameter2D::GetRequirements();
}


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionFlexFluidSurfaceThickness
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionFlexFluidSurfaceThickness::UMaterialExpressionFlexFluidSurfaceThickness(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Texture;
		FConstructorStatics()
			: NAME_Texture(LOCTEXT("Texture", "Texture"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITOR
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
#endif // WITH_EDITOR

	bShaderInputData = true;
	ConstInput = FVector2D(0.f, 0.f);
}

#if WITH_EDITOR

int32 UMaterialExpressionFlexFluidSurfaceThickness::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 OffsetIndex = INDEX_NONE;
	int32 CoordinateIndex = INDEX_NONE;
	bool bUseOffset = false;


	if (InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		if (Input.Expression)
		{
			OffsetIndex = Input.Compile(Compiler);
		}
		else
		{
			OffsetIndex = Compiler->Constant2(ConstInput.X, ConstInput.Y);
		}

		bUseOffset = true;
	}
	else if (InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		if (Input.Expression)
		{
			CoordinateIndex = Input.Compile(Compiler);
		}
	}

	int32 Result = Compiler->FlexFluidSurfaceThickness(OffsetIndex, CoordinateIndex, bUseOffset);
	return Result;
}


void UMaterialExpressionFlexFluidSurfaceThickness::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("FlexFluidSurface Thickness"));
}

#endif // WITH_EDITOR

FString UMaterialExpressionFlexFluidSurfaceThickness::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		// Display the current InputMode enum's display name.
		UByteProperty* InputModeProperty = NULL;
		InputModeProperty = FindField<UByteProperty>(UMaterialExpressionFlexFluidSurfaceThickness::StaticClass(), "InputMode");
		return InputModeProperty->Enum->GetNameStringByIndex((int32)InputMode.GetValue());
	}
	return TEXT("");
}

#if WITH_EDITOR
int32 UMaterialExpressionAdd::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Add(Arg1, Arg2);
}


void UMaterialExpressionAdd::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Add");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();
	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionMultiply
//
UMaterialExpressionMultiply::UMaterialExpressionMultiply(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 0.0f;
	ConstB = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMultiply::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Mul(Arg1, Arg2);
}

void UMaterialExpressionMultiply::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Multiply");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();

	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

UMaterialExpressionDivide::UMaterialExpressionDivide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif

	ConstA = 1.0f;
	ConstB = 2.0f;
}

#if WITH_EDITOR
int32 UMaterialExpressionDivide::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Div(Arg1, Arg2);
}

void UMaterialExpressionDivide::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Divide");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();

	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionSubtract
//
UMaterialExpressionSubtract::UMaterialExpressionSubtract(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 1.0f;
	ConstB = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSubtract::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Sub(Arg1, Arg2);
}

void UMaterialExpressionSubtract::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Subtract");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();
	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionLinearInterpolate
//

UMaterialExpressionLinearInterpolate::UMaterialExpressionLinearInterpolate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 0;
	ConstB = 1;
	ConstAlpha = 0.5f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLinearInterpolate::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg3 = Alpha.GetTracedInput().Expression ? Alpha.Compile(Compiler) : Compiler->Constant(ConstAlpha);

	return Compiler->Lerp(Arg1, Arg2, Arg3);
}

void UMaterialExpressionLinearInterpolate::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Lerp");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();
	FExpressionInput AlphaTraced = Alpha.GetTracedInput();

	if(!ATraced.Expression || !BTraced.Expression || !AlphaTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstB);
		ret += AlphaTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstAlpha);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

UMaterialExpressionConstant::UMaterialExpressionConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionConstant::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->Constant(R);
}

void UMaterialExpressionConstant::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf( TEXT("%.4g"), R ));
}

FString UMaterialExpressionConstant::GetDescription() const
{
	FString Result = FString(*GetClass()->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
	Result += TEXT(" (");
	Result += Super::GetDescription();
	Result += TEXT(")");
	return Result;
}
#endif // WITH_EDITOR

UMaterialExpressionConstant2Vector::UMaterialExpressionConstant2Vector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
			, NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionConstant2Vector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->Constant2(R,G);
}

void UMaterialExpressionConstant2Vector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf( TEXT("%.3g,%.3g"), R, G ));
}

FString UMaterialExpressionConstant2Vector::GetDescription() const
{
	FString Result = FString(*GetClass()->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
	Result += TEXT(" (");
	Result += Super::GetDescription();
	Result += TEXT(")");
	return Result;
}
#endif // WITH_EDITOR

UMaterialExpressionConstant3Vector::UMaterialExpressionConstant3Vector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
			, NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionConstant3Vector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->Constant3(Constant.R,Constant.G,Constant.B);
}

void UMaterialExpressionConstant3Vector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf( TEXT("%.3g,%.3g,%.3g"), Constant.R, Constant.G, Constant.B ));
}

FString UMaterialExpressionConstant3Vector::GetDescription() const
{
	FString Result = FString(*GetClass()->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
	Result += TEXT(" (");
	Result += Super::GetDescription();
	Result += TEXT(")");
	return Result;
}
#endif // WITH_EDITOR

UMaterialExpressionConstant4Vector::UMaterialExpressionConstant4Vector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
			, NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionConstant4Vector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->Constant4(Constant.R,Constant.G,Constant.B,Constant.A);
}


void UMaterialExpressionConstant4Vector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf( TEXT("%.2g,%.2g,%.2g,%.2g"), Constant.R, Constant.G, Constant.B, Constant.A ));
}

FString UMaterialExpressionConstant4Vector::GetDescription() const
{
	FString Result = FString(*GetClass()->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
	Result += TEXT(" (");
	Result += Super::GetDescription();
	Result += TEXT(")");
	return Result;
}
#endif // WITH_EDITOR

UMaterialExpressionClamp::UMaterialExpressionClamp(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ClampMode = CMODE_Clamp;
	MinDefault = 0.0f;
	MaxDefault = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

void UMaterialExpressionClamp::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_RETROFIT_CLAMP_EXPRESSIONS_SWAP)
	{
		if (ClampMode == CMODE_ClampMin)
		{
			ClampMode = CMODE_ClampMax;
		}
		else if (ClampMode == CMODE_ClampMax)
		{
			ClampMode = CMODE_ClampMin;
		}
	}
}

#if WITH_EDITOR
int32 UMaterialExpressionClamp::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Clamp input"));
	}
	else
	{
		const int32 MinIndex = Min.GetTracedInput().Expression ? Min.Compile(Compiler) : Compiler->Constant(MinDefault);
		const int32 MaxIndex = Max.GetTracedInput().Expression ? Max.Compile(Compiler) : Compiler->Constant(MaxDefault);

		if (ClampMode == CMODE_Clamp)
		{
			return Compiler->Clamp(Input.Compile(Compiler), MinIndex, MaxIndex);
		}
		else if (ClampMode == CMODE_ClampMin)
		{
			return Compiler->Max(Input.Compile(Compiler), MinIndex);
		}
		else if (ClampMode == CMODE_ClampMax)
		{
			return Compiler->Min(Input.Compile(Compiler), MaxIndex);
		}
		return INDEX_NONE;
	}
}

void UMaterialExpressionClamp::GetCaption(TArray<FString>& OutCaptions) const
{
	FString	NewCaption = TEXT( "Clamp" );

	if (ClampMode == CMODE_ClampMin || ClampMode == CMODE_Clamp)
	{
		NewCaption += Min.GetTracedInput().Expression ? TEXT(" (Min)") : FString::Printf(TEXT(" (Min=%.4g)"), MinDefault);
	}
	if (ClampMode == CMODE_ClampMax || ClampMode == CMODE_Clamp)
	{
		NewCaption += Max.GetTracedInput().Expression ? TEXT(" (Max)") : FString::Printf(TEXT(" (Max=%.4g)"), MaxDefault);
	}
	OutCaptions.Add(NewCaption);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionSaturate
//
UMaterialExpressionSaturate::UMaterialExpressionSaturate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSaturate::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Saturate input"));
	}
	
	return Compiler->Saturate(Input.Compile(Compiler));
}

void UMaterialExpressionSaturate::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Saturate"));
}

void UMaterialExpressionSaturate::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Clamps the value between 0 and 1. Saturate is free on most modern graphics hardware."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionMin
//

UMaterialExpressionMin::UMaterialExpressionMin(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 0.0f;
	ConstB = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMin::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Min(Arg1, Arg2);
}

void UMaterialExpressionMin::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Min");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();

	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionMax
//

UMaterialExpressionMax::UMaterialExpressionMax(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstA = 0.0f;
	ConstB = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMax::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg1 = A.GetTracedInput().Expression ? A.Compile(Compiler) : Compiler->Constant(ConstA);
	// if the input is hooked up, use it, otherwise use the internal constant
	int32 Arg2 = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	return Compiler->Max(Arg1, Arg2);
}

void UMaterialExpressionMax::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Max");

	FExpressionInput ATraced = A.GetTracedInput();
	FExpressionInput BTraced = B.GetTracedInput();

	if(!ATraced.Expression || !BTraced.Expression)
	{
		ret += TEXT("(");
		ret += ATraced.Expression ? TEXT(",") : FString::Printf( TEXT("%.4g,"), ConstA);
		ret += BTraced.Expression ? TEXT(")") : FString::Printf( TEXT("%.4g)"), ConstB);
	}

	OutCaptions.Add(ret);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionTextureCoordinate
//

UMaterialExpressionTextureCoordinate::UMaterialExpressionTextureCoordinate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	UTiling = 1.0f;
	VTiling = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionTextureCoordinate::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Depending on whether we have U and V scale values that differ, we can perform a multiply by either
	// a scalar or a float2.  These tiling values are baked right into the shader node, so they're always
	// known at compile time.
	if( FMath::Abs( UTiling - VTiling ) > SMALL_NUMBER )
	{
		return Compiler->Mul(Compiler->TextureCoordinate(CoordinateIndex, UnMirrorU, UnMirrorV),Compiler->Constant2(UTiling, VTiling));
	}
	else
	{
		return Compiler->Mul(Compiler->TextureCoordinate(CoordinateIndex, UnMirrorU, UnMirrorV),Compiler->Constant(UTiling));
	}
}

void UMaterialExpressionTextureCoordinate::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("TexCoord[%i]"), CoordinateIndex));
}
#endif // WITH_EDITOR

UMaterialExpressionDotProduct::UMaterialExpressionDotProduct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionDotProduct::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing DotProduct input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing DotProduct input B"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		return Compiler->Dot(
			Arg1,
			Arg2
			);
	}
}

void UMaterialExpressionDotProduct::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Dot"));
}
#endif // WITH_EDITOR

UMaterialExpressionCrossProduct::UMaterialExpressionCrossProduct(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionCrossProduct::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing CrossProduct input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing CrossProduct input B"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		return Compiler->Cross(
			Arg1,
			Arg2
			);
	}
}

void UMaterialExpressionCrossProduct::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Cross"));
}
#endif // WITH_EDITOR

UMaterialExpressionComponentMask::UMaterialExpressionComponentMask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionComponentMask::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ComponentMask input"));
	}

	return Compiler->ComponentMask(
		Input.Compile(Compiler),
		R,
		G,
		B,
		A
		);
}

void UMaterialExpressionComponentMask::GetCaption(TArray<FString>& OutCaptions) const
{
	FString Str(TEXT("Mask ("));
	if ( R ) Str += TEXT(" R");
	if ( G ) Str += TEXT(" G");
	if ( B ) Str += TEXT(" B");
	if ( A ) Str += TEXT(" A");
	Str += TEXT(" )");
	OutCaptions.Add(Str);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionStaticComponentMaskParameter
//
UMaterialExpressionStaticComponentMaskParameter::UMaterialExpressionStaticComponentMaskParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Parameters;
		FConstructorStatics()
			: NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionStaticComponentMaskParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ComponentMaskParameter input"));
	}
	else
	{
		return Compiler->StaticComponentMask(
			Input.Compile(Compiler),
			ParameterName,
			DefaultR,
			DefaultG,
			DefaultB,
			DefaultA
			);
	}
}

void UMaterialExpressionStaticComponentMaskParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Mask Param")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionStaticComponentMaskParameter::IsNamedParameter(FName InParameterName, bool& OutR, bool& OutG, bool& OutB, bool& OutA, FGuid&OutExpressionGuid) const
{
	if (InParameterName == ParameterName)
	{
		OutR = DefaultR;
		OutG = DefaultG;
		OutB = DefaultB;
		OutA = DefaultA;
		OutExpressionGuid = ExpressionGUID;
		return true;
	}

	return false;
}

// NVCHANGE_BEGIN: Add VXGI

//
//	UMaterialExpressionVxgiVoxelization
//

UMaterialExpressionVxgiVoxelization::UMaterialExpressionVxgiVoxelization(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT("Constants", "Constants"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionVxgiVoxelization::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
#if WITH_GFSDK_VXGI
	return Compiler->VxgiVoxelization();
#else
	return Compiler->Constant(0);
#endif
}

void UMaterialExpressionVxgiVoxelization::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("IsVxgiVoxelization"));
}
#endif // WITH_EDITOR
//
//	UMaterialExpressionVxgiTraceCone
//

UMaterialExpressionVxgiTraceCone::UMaterialExpressionVxgiTraceCone(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Lighting;
		FConstructorStatics()
			: NAME_Lighting(LOCTEXT("Lighting", "Lighting"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Lighting);
#endif
	bShaderInputData = true;

	MaxSamples = 128;

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Irradiance"), 1, 1, 1, 1, 0));
}

#if WITH_EDITOR
int32 UMaterialExpressionVxgiTraceCone::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
#if WITH_GFSDK_VXGI
	if (!StartPos.Expression || !Direction.Expression || !ConeFactor.Expression)
	{
		return CompilerError(Compiler, TEXT("Cone tracing requires StartPos, Direction and ConeFactor arguments"));
	}

	int32 StartPosArg = StartPos.Compile(Compiler);
	int32 DirectionArg = Direction.Compile(Compiler);
	int32 ConeFactorArg = ConeFactor.Compile(Compiler);
	int32 InitialOffsetArg = InitialOffset.Expression ? InitialOffset.Compile(Compiler) : Compiler->Constant(1.f);
	int32 TracingStepArg = TracingStep.Expression ? TracingStep.Compile(Compiler) : Compiler->Constant(1.f);

	return Compiler->VxgiTraceCone(StartPosArg, DirectionArg, ConeFactorArg, InitialOffsetArg, TracingStepArg, MaxSamples);
#else
	return Compiler->Constant(0);
#endif
}

void UMaterialExpressionVxgiTraceCone::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("VxgiTraceCone"));
}
#endif // WITH_EDITOR
// NVCHANGE_END: Add VXGI

//
//	UMaterialExpressionTime
//

UMaterialExpressionTime::UMaterialExpressionTime(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
	Period = 0.0f;
	bOverride_Period = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionTime::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return bIgnorePause ? Compiler->RealTime(bOverride_Period, Period) : Compiler->GameTime(bOverride_Period, Period);
}

void UMaterialExpressionTime::GetCaption(TArray<FString>& OutCaptions) const
{
	if (bOverride_Period)
	{
		if (Period == 0.0f)
		{
			OutCaptions.Add(TEXT("Time (Stopped)"));
		}
		else
		{
			OutCaptions.Add(FString::Printf(TEXT("Time (Period of %.2f)"), Period));
		}
	}
	else
	{
		OutCaptions.Add(TEXT("Time"));
	}
}
#endif // WITH_EDITOR

UMaterialExpressionCameraVectorWS::UMaterialExpressionCameraVectorWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionCameraVectorWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->CameraVector();
}

void UMaterialExpressionCameraVectorWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Camera Vector"));
}
#endif // WITH_EDITOR

UMaterialExpressionCameraPositionWS::UMaterialExpressionCameraPositionWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionCameraPositionWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ViewProperty(MEVP_WorldSpaceCameraPosition);
}

void UMaterialExpressionCameraPositionWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Camera Position"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionReflectionVectorWS
//

UMaterialExpressionReflectionVectorWS::UMaterialExpressionReflectionVectorWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionReflectionVectorWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result = CustomWorldNormal.Compile(Compiler);
	if (CustomWorldNormal.Expression) 
	{
		// Don't do anything special here in regards to if the Expression is a Reroute node, the compiler will handle properly internally and return INDEX_NONE if rerouted to nowhere.
		return Compiler->ReflectionAboutCustomWorldNormal(Result, bNormalizeCustomWorldNormal); 
	}
	else
	{
		return Compiler->ReflectionVector();
	}
}

void UMaterialExpressionReflectionVectorWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Reflection Vector"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionPanner
//
UMaterialExpressionPanner::UMaterialExpressionPanner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bCollapsed = true;
	ConstCoordinate = 0;
}

#if WITH_EDITOR
int32 UMaterialExpressionPanner::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 TimeArg = Time.GetTracedInput().Expression ? Time.Compile(Compiler) : Compiler->GameTime(false, 0.0f);
	bool bIsSpeedExpressionValid = Speed.GetTracedInput().Expression != nullptr;
	int32 SpeedVectorArg = bIsSpeedExpressionValid ? Speed.Compile(Compiler) : INDEX_NONE;
	int32 SpeedXArg = bIsSpeedExpressionValid ? Compiler->ComponentMask(SpeedVectorArg, true, false, false, false) : Compiler->Constant(SpeedX);
	int32 SpeedYArg = bIsSpeedExpressionValid ? Compiler->ComponentMask(SpeedVectorArg, false, true, false, false) : Compiler->Constant(SpeedY);
	int32 Arg1;
	int32 Arg2;
	if (bFractionalPart)
	{
		// Note: this is to avoid (delay) divergent accuracy issues as GameTime increases.
		// TODO: C++ to calculate its phase via per frame time delta.
		Arg1 = Compiler->PeriodicHint(Compiler->Frac(Compiler->Mul(TimeArg, SpeedXArg)));
		Arg2 = Compiler->PeriodicHint(Compiler->Frac(Compiler->Mul(TimeArg, SpeedYArg)));
	}
	else
	{
		Arg1 = Compiler->PeriodicHint(Compiler->Mul(TimeArg, SpeedXArg));
		Arg2 = Compiler->PeriodicHint(Compiler->Mul(TimeArg, SpeedYArg));
	}

	int32 Arg3 = Coordinate.GetTracedInput().Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);
	return Compiler->Add(
			Compiler->AppendVector(
				Arg1,
				Arg2
				),
			Arg3
			);
}

void UMaterialExpressionPanner::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Panner"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionRotator
//
UMaterialExpressionRotator::UMaterialExpressionRotator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	CenterX = 0.5f;
	CenterY = 0.5f;
	Speed = 0.25f;
	ConstCoordinate = 0;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionRotator::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32	Cosine = Compiler->Cosine(Compiler->Mul(Time.GetTracedInput().Expression ? Time.Compile(Compiler) : Compiler->GameTime(false, 0.0f),Compiler->Constant(Speed))),
		Sine = Compiler->Sine(Compiler->Mul(Time.GetTracedInput().Expression ? Time.Compile(Compiler) : Compiler->GameTime(false, 0.0f),Compiler->Constant(Speed))),
		RowX = Compiler->AppendVector(Cosine,Compiler->Mul(Compiler->Constant(-1.0f),Sine)),
		RowY = Compiler->AppendVector(Sine,Cosine),
		Origin = Compiler->Constant2(CenterX,CenterY),
		BaseCoordinate = Coordinate.GetTracedInput().Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);

	const int32 Arg1 = Compiler->Dot(RowX,Compiler->Sub(Compiler->ComponentMask(BaseCoordinate,1,1,0,0),Origin));
	const int32 Arg2 = Compiler->Dot(RowY,Compiler->Sub(Compiler->ComponentMask(BaseCoordinate,1,1,0,0),Origin));

	if(Compiler->GetType(BaseCoordinate) == MCT_Float3)
		return Compiler->AppendVector(
				Compiler->Add(
					Compiler->AppendVector(
						Arg1,
						Arg2
						),
					Origin
					),
				Compiler->ComponentMask(BaseCoordinate,0,0,1,0)
				);
	else
	{
		const int32 ArgOne = Compiler->Dot(RowX,Compiler->Sub(BaseCoordinate,Origin));
		const int32 ArgTwo = Compiler->Dot(RowY,Compiler->Sub(BaseCoordinate,Origin));

		return Compiler->Add(
				Compiler->AppendVector(
					ArgOne,
					ArgTwo
					),
				Origin
				);
	}
}

void UMaterialExpressionRotator::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Rotator"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionSine
//
UMaterialExpressionSine::UMaterialExpressionSine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Period = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSine::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Sine input"));
	}

	return Compiler->Sine(Period > 0.0f ? Compiler->Mul(Input.Compile(Compiler),Compiler->Constant(2.0f * (float)PI / Period)) : Input.Compile(Compiler));
}

void UMaterialExpressionSine::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Sine"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionCosine
//
UMaterialExpressionCosine::UMaterialExpressionCosine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Period = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionCosine::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Cosine input"));
	}

	return Compiler->Cosine(Compiler->Mul(Input.Compile(Compiler),Period > 0.0f ? Compiler->Constant(2.0f * (float)PI / Period) : 0));
}

void UMaterialExpressionCosine::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Cosine"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionTangent
//
UMaterialExpressionTangent::UMaterialExpressionTangent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Period = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionTangent::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Tangent input"));
	}

	return Compiler->Tangent(Compiler->Mul(Input.Compile(Compiler),Period > 0.0f ? Compiler->Constant(2.0f * (float)PI / Period) : 0));
}

void UMaterialExpressionTangent::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Tangent"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArcsine
//
UMaterialExpressionArcsine::UMaterialExpressionArcsine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArcsine::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Arcsine input"));
	}

	return Compiler->Arcsine(Input.Compile(Compiler));
}

void UMaterialExpressionArcsine::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arcsine"));
}

void UMaterialExpressionArcsine::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Inverse sine function. This is an expensive operation not reflected by instruction count."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArcsineFast
//
UMaterialExpressionArcsineFast::UMaterialExpressionArcsineFast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArcsineFast::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ArcsineFast input"));
	}

	return Compiler->ArcsineFast(Input.Compile(Compiler));
}

void UMaterialExpressionArcsineFast::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ArcsineFast"));
}

void UMaterialExpressionArcsineFast::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Approximate inverse sine function. Input must be between -1 and 1."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArccosine
//
UMaterialExpressionArccosine::UMaterialExpressionArccosine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArccosine::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Arccosine input"));
	}

	return Compiler->Arccosine(Input.Compile(Compiler));
}

void UMaterialExpressionArccosine::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arccosine"));
}

void UMaterialExpressionArccosine::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Inverse cosine function. This is an expensive operation not reflected by instruction count."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArccosineFast
//
UMaterialExpressionArccosineFast::UMaterialExpressionArccosineFast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArccosineFast::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ArccosineFast input"));
	}

	return Compiler->ArccosineFast(Input.Compile(Compiler));
}

void UMaterialExpressionArccosineFast::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ArccosineFast"));
}

void UMaterialExpressionArccosineFast::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Approximate inverse cosine function. Input must be between -1 and 1."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArctangent
//
UMaterialExpressionArctangent::UMaterialExpressionArctangent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArctangent::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Arctangent input"));
	}

	return Compiler->Arctangent(Input.Compile(Compiler));
}

void UMaterialExpressionArctangent::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arctangent"));
}

void UMaterialExpressionArctangent::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Inverse tangent function. This is an expensive operation not reflected by instruction count."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArctangentFast
//
UMaterialExpressionArctangentFast::UMaterialExpressionArctangentFast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArctangentFast::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ArctangentFast input"));
	}

	return Compiler->ArctangentFast(Input.Compile(Compiler));
}

void UMaterialExpressionArctangentFast::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ArctangentFast"));
}

void UMaterialExpressionArctangentFast::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Approximate inverse tangent function."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArctangent2
//
UMaterialExpressionArctangent2::UMaterialExpressionArctangent2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArctangent2::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Y.GetTracedInput().Expression || !X.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Arctangent2 input"));
	}

	int32 YResult = Y.Compile(Compiler);
	int32 XResult = X.Compile(Compiler);
	return Compiler->Arctangent2(YResult, XResult);
}

void UMaterialExpressionArctangent2::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arctangent2"));
}

void UMaterialExpressionArctangent2::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Inverse tangent of X / Y where input signs are used to determine quadrant. This is an expensive operation not reflected by instruction count."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionArctangent2Fast
//
UMaterialExpressionArctangent2Fast::UMaterialExpressionArctangent2Fast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionArctangent2Fast::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Y.GetTracedInput().Expression || !X.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Arctangent2Fast input"));
	}

	int32 YResult = Y.Compile(Compiler);
	int32 XResult = X.Compile(Compiler);
	return Compiler->Arctangent2Fast(YResult, XResult);
}

void UMaterialExpressionArctangent2Fast::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Arctangent2Fast"));
}

void UMaterialExpressionArctangent2Fast::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Approximate inverse tangent of X / Y where input signs are used to determine quadrant."), 40, OutToolTip);
}
#endif // WITH_EDITOR

UMaterialExpressionBumpOffset::UMaterialExpressionBumpOffset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	HeightRatio = 0.05f;
	ReferencePlane = 0.5f;
	ConstCoordinate = 0;
	bCollapsed = false;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionBumpOffset::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Height.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Height input"));
	}

	return Compiler->Add(
			Compiler->Mul(
				Compiler->ComponentMask(Compiler->TransformVector(MCB_World, MCB_Tangent, Compiler->CameraVector()),1,1,0,0),
				Compiler->Add(
					Compiler->Mul(
						HeightRatioInput.GetTracedInput().Expression ? Compiler->ForceCast(HeightRatioInput.Compile(Compiler),MCT_Float1) : Compiler->Constant(HeightRatio),
						Compiler->ForceCast(Height.Compile(Compiler),MCT_Float1)
						),
					HeightRatioInput.GetTracedInput().Expression ? Compiler->Mul(Compiler->Constant(-ReferencePlane), Compiler->ForceCast(HeightRatioInput.Compile(Compiler),MCT_Float1)) : Compiler->Constant(-ReferencePlane * HeightRatio)
					)
				),
			Coordinate.GetTracedInput().Expression ? Coordinate.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false)
			);
}

void UMaterialExpressionBumpOffset::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("BumpOffset"));
}
#endif // WITH_EDITOR

UMaterialExpressionAppendVector::UMaterialExpressionAppendVector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionAppendVector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing AppendVector input B"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		return Compiler->AppendVector(
			Arg1,
			Arg2
			);
	}
}

void UMaterialExpressionAppendVector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Append"));
}
#endif // WITH_EDITOR

// -----

UMaterialExpressionMakeMaterialAttributes::UMaterialExpressionMakeMaterialAttributes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialAttributes;
		FConstructorStatics()
			: NAME_MaterialAttributes(LOCTEXT( "MaterialAttributes", "Material Attributes" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialAttributes);
#endif
}

void UMaterialExpressionMakeMaterialAttributes::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	
	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::FixedLegacyMaterialAttributeNodeTypes)
	{
		// Update the legacy masks else fail on vec3 to vec2 conversion
		Refraction.SetMask(1, 1, 1, 0, 0);
	}
}

#if WITH_EDITOR
int32 UMaterialExpressionMakeMaterialAttributes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) 
{
	int32 Ret = INDEX_NONE;
	UMaterialExpression* Expression = NULL;

 	static_assert(MP_MAX == 29, 
		"New material properties should be added to the end of the inputs for this expression. \
		The order of properties here should match the material results pins, the make material attriubtes node inputs and the mapping of IO indices to properties in GetMaterialPropertyFromInputOutputIndex().\
		Insertions into the middle of the properties or a change in the order of properties will also require that existing data is fixed up in DoMaterialAttriubtesReorder().\
		");

	EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(Compiler->GetMaterialAttribute());
	// We don't worry about reroute nodes in the switch, as we have a test for their validity afterwards.
	switch (Property)
	{
	case MP_BaseColor: Ret = BaseColor.Compile(Compiler); Expression = BaseColor.Expression; break;
	case MP_Metallic: Ret = Metallic.Compile(Compiler); Expression = Metallic.Expression; break;
	case MP_Specular: Ret = Specular.Compile(Compiler); Expression = Specular.Expression; break;
	case MP_Roughness: Ret = Roughness.Compile(Compiler); Expression = Roughness.Expression; break;
	case MP_EmissiveColor: Ret = EmissiveColor.Compile(Compiler); Expression = EmissiveColor.Expression; break;
	case MP_Opacity: Ret = Opacity.Compile(Compiler); Expression = Opacity.Expression; break;
	case MP_OpacityMask: Ret = OpacityMask.Compile(Compiler); Expression = OpacityMask.Expression; break;
	case MP_Normal: Ret = Normal.Compile(Compiler); Expression = Normal.Expression; break;
	case MP_WorldPositionOffset: Ret = WorldPositionOffset.Compile(Compiler); Expression = WorldPositionOffset.Expression; break;
	case MP_WorldDisplacement: Ret = WorldDisplacement.Compile(Compiler); Expression = WorldDisplacement.Expression; break;
	case MP_TessellationMultiplier: Ret = TessellationMultiplier.Compile(Compiler); Expression = TessellationMultiplier.Expression; break;
	case MP_SubsurfaceColor: Ret = SubsurfaceColor.Compile(Compiler); Expression = SubsurfaceColor.Expression; break;
	case MP_CustomData0: Ret = ClearCoat.Compile(Compiler); Expression = ClearCoat.Expression; break;
	case MP_CustomData1: Ret = ClearCoatRoughness.Compile(Compiler); Expression = ClearCoatRoughness.Expression; break;
	case MP_AmbientOcclusion: Ret = AmbientOcclusion.Compile(Compiler); Expression = AmbientOcclusion.Expression; break;
	case MP_Refraction: Ret = Refraction.Compile(Compiler); Expression = Refraction.Expression; break;
	case MP_PixelDepthOffset: Ret = PixelDepthOffset.Compile(Compiler); Expression = PixelDepthOffset.Expression; break;
	};

	if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
	{
		Ret = CustomizedUVs[Property - MP_CustomizedUVs0].Compile(Compiler); Expression = CustomizedUVs[Property - MP_CustomizedUVs0].Expression;
	}

	//If we've connected an expression but its still returned INDEX_NONE, flag the error. This also catches reroute nodes to nowhere.
	if (Expression && INDEX_NONE == Ret)
	{
		Compiler->Errorf(TEXT("Error on property %s"), *FMaterialAttributeDefinitionMap::GetDisplayName(Property));
	}

	return Ret;
}

void UMaterialExpressionMakeMaterialAttributes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MakeMaterialAttributes"));
}
#endif // WITH_EDITOR

// -----

UMaterialExpressionBreakMaterialAttributes::UMaterialExpressionBreakMaterialAttributes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialAttributes;
		FConstructorStatics()
			: NAME_MaterialAttributes(LOCTEXT( "MaterialAttributes", "Material Attributes" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputNameOnPin = true;
	bShowMaskColorsOnPin = false;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialAttributes);
#endif
	
 	static_assert(MP_MAX == 29, 
		"New material properties should be added to the end of the outputs for this expression. \
		The order of properties here should match the material results pins, the make material attriubtes node inputs and the mapping of IO indices to properties in GetMaterialPropertyFromInputOutputIndex().\
		Insertions into the middle of the properties or a change in the order of properties will also require that existing data is fixed up in DoMaterialAttriubtesReorder().\
		");

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("BaseColor"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("Metallic"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Specular"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Roughness"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("EmissiveColor"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("Opacity"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("OpacityMask"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Normal"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("WorldPositionOffset"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("WorldDisplacement"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("TessellationMultiplier"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("SubsurfaceColor"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("ClearCoat"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("ClearCoatRoughness"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("AmbientOcclusion"), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT("Refraction"), 1, 1, 1, 0, 0));

	for (int32 UVIndex = 0; UVIndex <= MP_CustomizedUVs7 - MP_CustomizedUVs0; UVIndex++)
	{
		Outputs.Add(FExpressionOutput(*FString::Printf(TEXT("CustomizedUV%u"), UVIndex), 1, 1, 1, 0, 0));
	}

	Outputs.Add(FExpressionOutput(TEXT("PixelDepthOffset"), 1, 1, 0, 0, 0));
}

void UMaterialExpressionBreakMaterialAttributes::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::FixedLegacyMaterialAttributeNodeTypes)
	{
		// Update the masks for legacy content
		int32 OutputIndex = 0;

		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // BaseColor
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // Metallic
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // Specular
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // Roughness
		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // EmissiveColor
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // Opacity
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // OpacityMask
		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // Normal
		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // WorldPositionOffset
		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // WorldDisplacement
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // TessellationMultiplier
		Outputs[OutputIndex].SetMask(1, 1, 1, 1, 0); ++OutputIndex; // SubsurfaceColor
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // ClearCoat
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // ClearCoatRoughness 
		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); ++OutputIndex; // AmbientOcclusion
		Outputs[OutputIndex].SetMask(1, 1, 1, 0, 0); ++OutputIndex; // Refraction
		
		for (int32 i = 0; i <= MP_CustomizedUVs7 - MP_CustomizedUVs0; ++i, ++OutputIndex)
		{
			Outputs[OutputIndex].SetMask(1, 1, 1, 0, 0);
		}

		Outputs[OutputIndex].SetMask(1, 1, 0, 0, 0); // PixelDepthOffset
	}
}

#if WITH_EDITOR
int32 UMaterialExpressionBreakMaterialAttributes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	static TMap<EMaterialProperty, int32> PropertyToIOIndexMap;
	if (PropertyToIOIndexMap.Num() == 0)
	{
		PropertyToIOIndexMap.Add(MP_BaseColor, 0);
		PropertyToIOIndexMap.Add(MP_Metallic, 1);
		PropertyToIOIndexMap.Add(MP_Specular, 2);
		PropertyToIOIndexMap.Add(MP_Roughness, 3);
		PropertyToIOIndexMap.Add(MP_EmissiveColor, 4);
		PropertyToIOIndexMap.Add(MP_Opacity, 5);
		PropertyToIOIndexMap.Add(MP_OpacityMask, 6);
		PropertyToIOIndexMap.Add(MP_Normal, 7);
		PropertyToIOIndexMap.Add(MP_WorldPositionOffset, 8);
		PropertyToIOIndexMap.Add(MP_WorldDisplacement, 9);
		PropertyToIOIndexMap.Add(MP_TessellationMultiplier, 10);
		PropertyToIOIndexMap.Add(MP_SubsurfaceColor, 11);
		PropertyToIOIndexMap.Add(MP_CustomData0, 12);
		PropertyToIOIndexMap.Add(MP_CustomData1, 13);
		PropertyToIOIndexMap.Add(MP_AmbientOcclusion, 14);
		PropertyToIOIndexMap.Add(MP_Refraction, 15);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs0, 16);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs1, 17);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs2, 18);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs3, 19);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs4, 20);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs5, 21);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs6, 22);
		PropertyToIOIndexMap.Add(MP_CustomizedUVs7, 23);
		PropertyToIOIndexMap.Add(MP_PixelDepthOffset, 24);
	}

	// Here we don't care about any multiplex index coming in.
	// We pass through our output index as the multiplex index so the MakeMaterialAttriubtes node at the other end can send us the right data.
	const EMaterialProperty* Property = PropertyToIOIndexMap.FindKey(OutputIndex);

	if (!Property)
	{
		return Compiler->Errorf(TEXT("Tried to compile material attributes?"));
	}
	else
	{
		return MaterialAttributes.CompileWithDefault(Compiler, FMaterialAttributeDefinitionMap::GetID(*Property));
	}
}

void UMaterialExpressionBreakMaterialAttributes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("BreakMaterialAttributes"));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionBreakMaterialAttributes::GetInputs()
{
	TArray<FExpressionInput*> Result;
	Result.Add(&MaterialAttributes);
	return Result;
}


FExpressionInput* UMaterialExpressionBreakMaterialAttributes::GetInput(int32 InputIndex)
{
	if( 0 == InputIndex )
	{
		return &MaterialAttributes;
	}

	return NULL;
}

FString UMaterialExpressionBreakMaterialAttributes::GetInputName(int32 InputIndex) const
{
	if( 0 == InputIndex )
	{
		return NSLOCTEXT("BreakMaterialAttributes", "InputName", "Attr").ToString();
	}
	return TEXT("");
}

bool UMaterialExpressionBreakMaterialAttributes::IsInputConnectionRequired(int32 InputIndex) const
{
	return true;
}

// -----

UMaterialExpressionGetMaterialAttributes::UMaterialExpressionGetMaterialAttributes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialAttributes;
		FConstructorStatics()
			: NAME_MaterialAttributes(LOCTEXT( "MaterialAttributes", "Material Attributes" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialAttributes);
#endif

	bShowOutputNameOnPin = true;

#if WITH_EDITOR
	// Add default output pins
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("MaterialAttributes"), 0, 0, 0, 0, 0));
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionGetMaterialAttributes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Verify setup
	const int32 NumOutputPins = AttributeGetTypes.Num();
	for (int32 i = 0; i < NumOutputPins; ++i)
	{
		for (int j = i + 1; j < NumOutputPins; ++j)
		{
			if (AttributeGetTypes[i] == AttributeGetTypes[j])
			{
				return Compiler->Errorf(TEXT("Duplicate attribute types."));
			}
		}

		if (FMaterialAttributeDefinitionMap::GetProperty(AttributeGetTypes[i]) == MP_MAX)
		{
			return Compiler->Errorf(TEXT("Property type doesn't exist, needs re-mapping?"));
		}
	}

	// Compile attribute
	int32 Result = INDEX_NONE;

	if (OutputIndex == 0)
	{
		const FGuid AttributeID = Compiler->GetMaterialAttribute();
		Result = MaterialAttributes.CompileWithDefault(Compiler, AttributeID);
	}
	else if (OutputIndex > 0)
	{
		checkf(OutputIndex <= AttributeGetTypes.Num(), TEXT("Requested non-existent pin."));
		Result = MaterialAttributes.CompileWithDefault(Compiler, AttributeGetTypes[OutputIndex-1]);
	}

	return Result;
}

void UMaterialExpressionGetMaterialAttributes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("GetMaterialAttributes"));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionGetMaterialAttributes::GetInputs()
{
	TArray<FExpressionInput*> Result;
	Result.Add(&MaterialAttributes);
	return Result;
}

FExpressionInput* UMaterialExpressionGetMaterialAttributes::GetInput(int32 InputIndex)
{
	if (InputIndex == 0)
	{
		return &MaterialAttributes;
	}

	return nullptr;
}

FString UMaterialExpressionGetMaterialAttributes::GetInputName(int32 InputIndex) const
{
	return TEXT("");
}

#if WITH_EDITOR
void UMaterialExpressionGetMaterialAttributes::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Backup attribute array so we can re-connect pins
	PreEditAttributeGetTypes.Empty();
	for (const FGuid& AttributeID : AttributeGetTypes)
	{
		PreEditAttributeGetTypes.Add(AttributeID);
	};

	Super::PreEditChange(PropertyAboutToChange);
}

void UMaterialExpressionGetMaterialAttributes::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && GraphNode)
	{
		if (PreEditAttributeGetTypes.Num() < AttributeGetTypes.Num())
		{
			// Attribute type added
			AttributeGetTypes.Last() = FMaterialAttributeDefinitionMap::GetDefaultID();
			FString AttributeName = FMaterialAttributeDefinitionMap::GetDisplayName(AttributeGetTypes.Last());
			Outputs.Add(FExpressionOutput(*AttributeName, 0, 0, 0, 0, 0));

			GraphNode->ReconstructNode();
		}	 
		else if (PreEditAttributeGetTypes.Num() > AttributeGetTypes.Num())
		{
			if (AttributeGetTypes.Num() == 0)
			{
				// All attribute types removed
				while (Outputs.Num() > 1)
				{
					Outputs.Pop();
					GraphNode->RemovePinAt(Outputs.Num(), EGPD_Output);
				}
			}
			else
			{
				// Attribute type removed
				int32 RemovedInputIndex = INDEX_NONE;

				for (int32 Attribute = 0; Attribute < AttributeGetTypes.Num(); ++Attribute)
				{
					// A mismatched attribute type means a middle pin was removed
					if (AttributeGetTypes[Attribute] != PreEditAttributeGetTypes[Attribute])
					{
						RemovedInputIndex = Attribute + 1;
						Outputs.RemoveAt(RemovedInputIndex);
						break;
					}
				};

				if (RemovedInputIndex == INDEX_NONE)
				{
					Outputs.Pop();
					RemovedInputIndex = Outputs.Num();
				}

				GraphNode->RemovePinAt(RemovedInputIndex, EGPD_Output);
			}
		}
		else
		{
			// Type changed, update pin names
			for (int i = 1; i < Outputs.Num(); ++i)
			{
				Outputs[i].OutputName = FMaterialAttributeDefinitionMap::GetDisplayName(AttributeGetTypes[i-1]);
			}

			GraphNode->ReconstructNode();
		}
	}

	Super::PostEditChangeProperty( PropertyChangedEvent );
}

void UMaterialExpressionGetMaterialAttributes::PostLoad()
{
	Super::PostLoad();

	// Verify serialized attributes
	check(Outputs.Num() == AttributeGetTypes.Num() + 1);

	for (int i = 1; i < Outputs.Num(); ++i)
	{
		FString DisplayName = FMaterialAttributeDefinitionMap::GetDisplayName(AttributeGetTypes[i-1]);
		if (Outputs[i].OutputName != DisplayName)
		{
			FString MaterialName;
			if (Material)
			{
				Material->GetName(MaterialName);
			}
			else if (Function)
			{
				Function->GetName(MaterialName);
			}

			UE_LOG(LogMaterial, Warning, TEXT("Serialized attribute that no longer exists (%s) for material \"%s\"."), *(Outputs[i].OutputName), *MaterialName);
			Outputs[i].OutputName = DisplayName;
		}
	}
}
#endif // WITH_EDITOR

// -----

UMaterialExpressionSetMaterialAttributes::UMaterialExpressionSetMaterialAttributes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialAttributes;
		FConstructorStatics()
			: NAME_MaterialAttributes(LOCTEXT( "MaterialAttributes", "Material Attributes" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialAttributes);
#endif

#if WITH_EDITOR
	// Add default input pins
	Inputs.Reset();
	Inputs.Add(FMaterialAttributesInput());
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSetMaterialAttributes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) 
{
	// Verify setup
	const int32 NumInputPins = AttributeSetTypes.Num();
	for (int32 i = 0; i < NumInputPins; ++i)
	{
		for (int j = i + 1; j < NumInputPins; ++j)
		{
			if (AttributeSetTypes[i] == AttributeSetTypes[j])
			{
				return Compiler->Errorf(TEXT("Duplicate attribute types."));
			}
		}

		if (FMaterialAttributeDefinitionMap::GetProperty(AttributeSetTypes[i]) == MP_MAX)
		{
			return Compiler->Errorf(TEXT("Property type doesn't exist, needs re-mapping?"));
		}
	}

	// Compile attribute
	const FGuid AttributeID = Compiler->GetMaterialAttribute();
	FExpressionInput* AttributeInput = nullptr;

	int32 PinIndex;
	if (AttributeSetTypes.Find(AttributeID, PinIndex))
	{
		checkf(PinIndex + 1 < Inputs.Num(), TEXT("Requested non-existent pin."));
		AttributeInput = &Inputs[PinIndex + 1];
	}

	if (AttributeInput)
	{
		EMaterialValueType ValueType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
		return Compiler->ValidCast(AttributeInput->Compile(Compiler), ValueType);
	}
	else
	{
		return Inputs[0].Compile(Compiler);
	}
}

void UMaterialExpressionSetMaterialAttributes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("SetMaterialAttributes"));
}
#endif

const TArray<FExpressionInput*> UMaterialExpressionSetMaterialAttributes::GetInputs()
{
	TArray<FExpressionInput*> Result;
	for (FExpressionInput& Input : Inputs)
	{
		Result.Add(&Input);
	}
	return Result;
}

FExpressionInput* UMaterialExpressionSetMaterialAttributes::GetInput(int32 InputIndex)
{
	return &Inputs[InputIndex];
}

FString UMaterialExpressionSetMaterialAttributes::GetInputName(int32 InputIndex) const
{
	FString Name(TEXT(""));

	if (InputIndex == 0)
	{
		Name = NSLOCTEXT("SetMaterialAttributes", "InputName", "MaterialAttributes").ToString();
	}
	else if (InputIndex > 0)
	{
		Name = FMaterialAttributeDefinitionMap::GetDisplayName(AttributeSetTypes[InputIndex-1]);
	}

	return Name;
}

#if WITH_EDITOR
void UMaterialExpressionSetMaterialAttributes::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Backup attribute array so we can re-connect pins
	PreEditAttributeSetTypes.Empty();
	for (const FGuid& AttributeID : AttributeSetTypes)
	{
		PreEditAttributeSetTypes.Add(AttributeID);
	};

	Super::PreEditChange(PropertyAboutToChange);
}

void UMaterialExpressionSetMaterialAttributes::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && GraphNode)
	{
		if (PreEditAttributeSetTypes.Num() < AttributeSetTypes.Num())
		{
			// Attribute type added
			AttributeSetTypes.Last() = FMaterialAttributeDefinitionMap::GetDefaultID();
			Inputs.Add(FExpressionInput());
			GraphNode->ReconstructNode();
		}	 
		else if (PreEditAttributeSetTypes.Num() > AttributeSetTypes.Num())
		{
			if (AttributeSetTypes.Num() == 0)
			{
				// All attribute types removed
				while (Inputs.Num() > 1)
				{
					Inputs.Pop();
					GraphNode->RemovePinAt(Inputs.Num(), EGPD_Input);
				}
			}
			else
			{
				// Attribute type removed
				int32 RemovedInputIndex = INDEX_NONE;

				for (int32 Attribute = 0; Attribute < AttributeSetTypes.Num(); ++Attribute)
				{
					// A mismatched attribute type means a middle pin was removed
					if (AttributeSetTypes[Attribute] != PreEditAttributeSetTypes[Attribute])
					{
						RemovedInputIndex = Attribute + 1;
						Inputs.RemoveAt(RemovedInputIndex);
						break;
					}
				};

				if (RemovedInputIndex == INDEX_NONE)
				{
					Inputs.Pop();
					RemovedInputIndex = Inputs.Num();
				}

				GraphNode->RemovePinAt(RemovedInputIndex, EGPD_Input);
			}
		}
		else
		{
			// Type changed, update pin names
			GraphNode->ReconstructNode();
		}
	}

	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif // WITH_EDITOR

// -----

UMaterialExpressionBlendMaterialAttributes::UMaterialExpressionBlendMaterialAttributes(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PixelAttributeBlendType(EMaterialAttributeBlend::Blend)
	, VertexAttributeBlendType(EMaterialAttributeBlend::Blend)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_MaterialAttributes;
		FConstructorStatics()
			: NAME_MaterialAttributes(LOCTEXT( "MaterialAttributes", "Material Attributes" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_MaterialAttributes);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 0, 0, 0, 0, 0));
}

#if WITH_EDITOR
int32 UMaterialExpressionBlendMaterialAttributes::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const FGuid AttributeID = Compiler->GetMaterialAttribute();

	// Blending is optional, can skip on a per-node basis
	EMaterialAttributeBlend::Type BlendType;
	EShaderFrequency AttributeFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID);

	switch (AttributeFrequency)
	{
	case SF_Vertex:	BlendType = VertexAttributeBlendType;	break;
	case SF_Hull:	BlendType = VertexAttributeBlendType;	break;
	case SF_Domain:	BlendType = VertexAttributeBlendType;	break;
	case SF_Pixel:	BlendType = PixelAttributeBlendType;	break;
	default:
		return Compiler->Errorf(TEXT("Attribute blending for shader frequency %i not implemented."), AttributeFrequency);
	}

	switch (BlendType)
	{
	case EMaterialAttributeBlend::UseA: return A.CompileWithDefault(Compiler, AttributeID);
	case EMaterialAttributeBlend::UseB: return B.CompileWithDefault(Compiler, AttributeID);
	default:
		check(BlendType == EMaterialAttributeBlend::Blend);
	}

	// Allow custom blends or fallback to standard interpolation
	int32 ResultA = A.CompileWithDefault(Compiler, AttributeID);
	int32 ResultB = B.CompileWithDefault(Compiler, AttributeID);
	int32 ResultAlpha = Alpha.Compile(Compiler);

	MaterialAttributeBlendFunction BlendFunction = FMaterialAttributeDefinitionMap::GetBlendFunction(AttributeID);
	if (BlendFunction)
	{
		return BlendFunction(Compiler, ResultA, ResultB, ResultAlpha);
	}
	else
	{
		return Compiler->Lerp(ResultA, ResultB, ResultAlpha);
	}
}

void UMaterialExpressionBlendMaterialAttributes::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("BlendMaterialAttributes"));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionBlendMaterialAttributes::GetInputs()
{
	TArray<FExpressionInput*> Result;
	Result.Add(&A);
	Result.Add(&B);
	Result.Add(&Alpha);
	return Result;
}

FExpressionInput* UMaterialExpressionBlendMaterialAttributes::GetInput(int32 InputIndex)
{
	if (InputIndex == 0)
	{
		return &A;
	}
	else if (InputIndex == 1)
	{
		return &B;
	}
	else if (InputIndex == 2)
	{
		return &Alpha;
	}

	return nullptr;
}

FString UMaterialExpressionBlendMaterialAttributes::GetInputName(int32 InputIndex) const
{
	FString Name;

	switch (InputIndex)
	{
	case 0: Name = TEXT("A"); break;
	case 1: Name = TEXT("B"); break;
	case 2: Name = TEXT("Alpha"); break;
	};

	return Name;
}

// -----

UMaterialExpressionFloor::UMaterialExpressionFloor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFloor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Floor input"));
	}

	return Compiler->Floor(Input.Compile(Compiler));
}

void UMaterialExpressionFloor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Floor"));
}
#endif // WITH_EDITOR

UMaterialExpressionCeil::UMaterialExpressionCeil(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionCeil::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Ceil input"));
	}
	return Compiler->Ceil(Input.Compile(Compiler));
}


void UMaterialExpressionCeil::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Ceil"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionRound
//
UMaterialExpressionRound::UMaterialExpressionRound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionRound::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Round input"));
	}
	return Compiler->Round(Input.Compile(Compiler));
}

void UMaterialExpressionRound::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Round"));
}

void UMaterialExpressionRound::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Rounds the value up to the next whole number if the fractional part is greater than or equal to half, else rounds down."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionTruncate
//
UMaterialExpressionTruncate::UMaterialExpressionTruncate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionTruncate::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Truncate input"));
	}
	return Compiler->Truncate(Input.Compile(Compiler));
}

void UMaterialExpressionTruncate::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Truncate"));
}

void UMaterialExpressionTruncate::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Truncates a value by discarding the fractional part."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionSign
//
UMaterialExpressionSign::UMaterialExpressionSign(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSign::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Sign input"));
	}
	return Compiler->Sign(Input.Compile(Compiler));
}

void UMaterialExpressionSign::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Sign"));
}

void UMaterialExpressionSign::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns -1 if the input is less than 0, 1 if greater, or 0 if equal."), 40, OutToolTip);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionFmod
//

UMaterialExpressionFmod::UMaterialExpressionFmod(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFmod::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Fmod input A"));
	}
	if (!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Fmod input B"));
	}
	return Compiler->Fmod(A.Compile(Compiler), B.Compile(Compiler));
}

void UMaterialExpressionFmod::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Fmod"));
}
#endif // WITH_EDITOR

UMaterialExpressionFrac::UMaterialExpressionFrac(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFrac::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Frac input"));
	}

	return Compiler->Frac(Input.Compile(Compiler));
}

void UMaterialExpressionFrac::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Frac"));
}
#endif // WITH_EDITOR

UMaterialExpressionDesaturation::UMaterialExpressionDesaturation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Color;
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Color(LOCTEXT( "Color", "Color" ))
			, NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	LuminanceFactors = FLinearColor(0.3f, 0.59f, 0.11f, 0.0f);

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Color);
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionDesaturation::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
		return Compiler->Errorf(TEXT("Missing Desaturation input"));

	int32 Color = Compiler->ForceCast(Input.Compile(Compiler), MCT_Float3, MFCF_ExactMatch|MFCF_ReplicateValue),
		Grey = Compiler->Dot(Color,Compiler->Constant3(LuminanceFactors.R,LuminanceFactors.G,LuminanceFactors.B));

	if(Fraction.GetTracedInput().Expression)
		return Compiler->Lerp(Color,Grey,Fraction.Compile(Compiler));
	else
		return Grey;
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionParameter
//

UMaterialExpressionParameter::UMaterialExpressionParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Parameters;
		FConstructorStatics()
			: NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bIsParameterExpression = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
	SortPriority = 0;
#endif

	bCollapsed = false;
}

bool UMaterialExpressionParameter::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( ParameterName.ToString().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR
FString UMaterialExpressionParameter::GetEditableName() const
{
	return ParameterName.ToString();
}

void UMaterialExpressionParameter::SetEditableName(const FString& NewName)
{
	ParameterName = *NewName;
}

#endif

void UMaterialExpressionParameter::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if(CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}

bool UMaterialExpressionParameter::NeedsLoadForClient() const
{
	// Keep named parameters
	return ParameterName != NAME_None;
}

//
//	UMaterialExpressionVectorParameter
//
UMaterialExpressionVectorParameter::UMaterialExpressionVectorParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));
}

#if WITH_EDITOR
int32 UMaterialExpressionVectorParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->VectorParameter(ParameterName,DefaultValue);
}

void UMaterialExpressionVectorParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(
		 TEXT("Param (%.3g,%.3g,%.3g,%.3g)"),
		 DefaultValue.R,
		 DefaultValue.G,
		 DefaultValue.B,
		 DefaultValue.A ));

	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString())); 
}
#endif // WITH_EDITOR

bool UMaterialExpressionVectorParameter::IsNamedParameter(FName InParameterName, FLinearColor& OutValue) const
{
	if (InParameterName == ParameterName)
	{
		OutValue = DefaultValue;
		return true;
	}

	return false;
}

#if WITH_EDITOR

void UMaterialExpressionVectorParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterialExpressionVectorParameter, DefaultValue))
	{
#if WITH_EDITOR
		// Callback into the editor
		FEditorSupportDelegates::VectorParameterDefaultChanged.Broadcast(this, ParameterName, DefaultValue);
#endif
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

//
//	UMaterialExpressionScalarParameter
//
UMaterialExpressionScalarParameter::UMaterialExpressionScalarParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionScalarParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ScalarParameter(ParameterName,DefaultValue);
}

void UMaterialExpressionScalarParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	 OutCaptions.Add(FString::Printf(
		 TEXT("Param (%.4g)"),
		 DefaultValue )); 

	 OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString())); 
}
#endif // WITH_EDITOR

bool UMaterialExpressionScalarParameter::IsNamedParameter(FName InParameterName, float& OutValue) const
{
	if (InParameterName == ParameterName)
	{
		OutValue = DefaultValue;
		return true;
	}

	return false;
}

#if WITH_EDITOR

void UMaterialExpressionScalarParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FString PropertyName = PropertyThatChanged ? PropertyThatChanged->GetName() : TEXT("");

	if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterialExpressionScalarParameter, DefaultValue))
	{
#if WITH_EDITOR
		// Callback into the editor
		FEditorSupportDelegates::ScalarParameterDefaultChanged.Broadcast(this, ParameterName, DefaultValue);
#endif
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

//
//	UMaterialExpressionStaticSwitchParameter
//
UMaterialExpressionStaticSwitchParameter::UMaterialExpressionStaticSwitchParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
bool UMaterialExpressionStaticSwitchParameter::IsResultMaterialAttributes(int32 OutputIndex)
{
	check(OutputIndex == 0);
	// This one is a little tricky. Since we are treating a dangling reroute as an empty expression, this
	// should early out, whereas IsResultMaterialAttributes on a reroute node will return false as the 
	// reroute node's input is dangling and therefore its type is unknown.
	if ((A.GetTracedInput().Expression && !A.Expression->ContainsInputLoop() && A.Expression->IsResultMaterialAttributes(A.OutputIndex)) ||
		(B.GetTracedInput().Expression && !B.Expression->ContainsInputLoop() && B.Expression->IsResultMaterialAttributes(B.OutputIndex)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

int32 UMaterialExpressionStaticSwitchParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	bool bSucceeded;
	const bool bValue = Compiler->GetStaticBoolValue(Compiler->StaticBoolParameter(ParameterName,DefaultValue), bSucceeded);
	
	//Both A and B must be connected in a parameter. 
	if( !A.GetTracedInput().IsConnected() )
	{
		Compiler->Errorf(TEXT("Missing A input"));
		bSucceeded = false;
	}
	if( !B.GetTracedInput().IsConnected() )
	{
		Compiler->Errorf(TEXT("Missing B input"));
		bSucceeded = false;
	}

	if (!bSucceeded)
	{
		return INDEX_NONE;
	}

	if (bValue)
	{
		return A.Compile(Compiler);
	}
	else
	{
		return B.Compile(Compiler);
	}
}

void UMaterialExpressionStaticSwitchParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Switch Param (%s)"), (DefaultValue ? TEXT("True") : TEXT("False")))); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString())); 
}
#endif // WITH_EDITOR

FString UMaterialExpressionStaticSwitchParameter::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return TEXT("True");
	}
	else
	{
		return TEXT("False");
	}
}


//
//	UMaterialExpressionStaticBoolParameter
//
UMaterialExpressionStaticBoolParameter::UMaterialExpressionStaticBoolParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidePreviewWindow = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionStaticBoolParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->StaticBoolParameter(ParameterName,DefaultValue);
}

int32 UMaterialExpressionStaticBoolParameter::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return INDEX_NONE;
}

void UMaterialExpressionStaticBoolParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString::Printf(TEXT("Static Bool Param (%s)"), (DefaultValue ? TEXT("True") : TEXT("False")))); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString())); 
}
#endif // WITH_EDITOR

bool UMaterialExpressionStaticBoolParameter::IsNamedParameter(FName InParameterName, bool& OutValue, FGuid& OutExpressionGuid) const
{
	if (InParameterName == ParameterName)
	{
		OutValue = DefaultValue;
		OutExpressionGuid = ExpressionGUID;
		return true;
	}

	return false;
}

//
//	UMaterialExpressionStaticBool
//
UMaterialExpressionStaticBool::UMaterialExpressionStaticBool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bHidePreviewWindow = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionStaticBool::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->StaticBool(Value);
}

int32 UMaterialExpressionStaticBool::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return INDEX_NONE;
}

void UMaterialExpressionStaticBool::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Static Bool ")) + (Value ? TEXT("(True)") : TEXT("(False)")));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionStaticSwitch
//
UMaterialExpressionStaticSwitch::UMaterialExpressionStaticSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif
}

#if WITH_EDITOR
bool UMaterialExpressionStaticSwitch::IsResultMaterialAttributes(int32 OutputIndex)
{
	// If there is a loop anywhere in this expression's inputs then we can't risk checking them. 
	// This one is a little tricky with respect to Reroute nodes. Since we are treating a dangling reroute as an empty expression, this
	// should early out, whereas IsResultMaterialAttributes on a reroute node will return false as the 
	// reroute node's input is dangling and therefore its type is unknown.
	check(OutputIndex == 0);
	if ((A.GetTracedInput().Expression && !A.Expression->ContainsInputLoop() && A.Expression->IsResultMaterialAttributes(A.OutputIndex)) ||
		(B.GetTracedInput().Expression && !B.Expression->ContainsInputLoop() && B.Expression->IsResultMaterialAttributes(B.OutputIndex)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

int32 UMaterialExpressionStaticSwitch::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	bool bValue = DefaultValue;

	if (Value.GetTracedInput().Expression)
	{
		bool bSucceeded;
		bValue = Compiler->GetStaticBoolValue(Value.Compile(Compiler), bSucceeded);

		if (!bSucceeded)
		{
			return INDEX_NONE;
		}
	}
	
	// We only call Compile on the branch that is taken to avoid compile errors in the disabled branch.
	if (bValue)
	{
		return A.Compile(Compiler);
	}
	else
	{
		return B.Compile(Compiler);
	}
}

void UMaterialExpressionStaticSwitch::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Switch")));
}
#endif // WITH_EDITOR

FString UMaterialExpressionStaticSwitch::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return TEXT("True");
	}
	else if (InputIndex == 1)
	{
		return TEXT("False");
	}
	else
	{
		return TEXT("Value");
	}
}

#if WITH_EDITOR
uint32 UMaterialExpressionStaticSwitch::GetInputType(int32 InputIndex)
{
	if (InputIndex == 0 || InputIndex == 1)
	{
		return MCT_Unknown;
	}
	else
	{
		return MCT_StaticBool;
	}
}
#endif

//
//	UMaterialExpressionPreviousFrameSwitch
//
UMaterialExpressionPreviousFrameSwitch::UMaterialExpressionPreviousFrameSwitch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT("Functions", "Functions"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif
}

#if WITH_EDITOR
bool UMaterialExpressionPreviousFrameSwitch::IsResultMaterialAttributes(int32 OutputIndex)
{
	// If there is a loop anywhere in this expression's inputs then we can't risk checking them
	check(OutputIndex == 0);
	if ((CurrentFrame.Expression && !CurrentFrame.Expression->ContainsInputLoop() && CurrentFrame.Expression->IsResultMaterialAttributes(CurrentFrame.OutputIndex)) ||
		(PreviousFrame.Expression && !PreviousFrame.Expression->ContainsInputLoop() && PreviousFrame.Expression->IsResultMaterialAttributes(PreviousFrame.OutputIndex)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

int32 UMaterialExpressionPreviousFrameSwitch::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Compiler->IsCurrentlyCompilingForPreviousFrame())
	{
		return PreviousFrame.Compile(Compiler);
	}
	return CurrentFrame.Compile(Compiler);
}

void UMaterialExpressionPreviousFrameSwitch::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("PreviousFrameSwitch")));
}

void UMaterialExpressionPreviousFrameSwitch::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Used to manually provide expressions for motion vector generation caused by changes in world position offset between frames."), 40, OutToolTip);
}
#endif // WITH_EDITOR

FString UMaterialExpressionPreviousFrameSwitch::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return TEXT("Current Frame");
	}
	else
	{
		return TEXT("Previous Frame");
	}
}

#if WITH_EDITOR
uint32 UMaterialExpressionPreviousFrameSwitch::GetInputType(int32 InputIndex)
{
	return MCT_Unknown;
}
#endif

//
//	UMaterialExpressionQualitySwitch
//

UMaterialExpressionQualitySwitch::UMaterialExpressionQualitySwitch(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionQualitySwitch::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const EMaterialQualityLevel::Type QualityLevelToCompile = Compiler->GetQualityLevel();
	check(QualityLevelToCompile < ARRAY_COUNT(Inputs));
	FExpressionInput QualityInput = Inputs[QualityLevelToCompile].GetTracedInput();
	FExpressionInput DefaultTraced = Default.GetTracedInput();

	if (!DefaultTraced.Expression)
	{
		return Compiler->Errorf(TEXT("Quality switch missing default input"));
	}

	if (QualityInput.Expression)
	{
		return QualityInput.Compile(Compiler);
	}

	return DefaultTraced.Compile(Compiler);
}

void UMaterialExpressionQualitySwitch::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Quality Switch")));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionQualitySwitch::GetInputs()
{
	TArray<FExpressionInput*> OutInputs;

	OutInputs.Add(&Default);

	for (int32 InputIndex = 0; InputIndex < ARRAY_COUNT(Inputs); InputIndex++)
	{
		OutInputs.Add(&Inputs[InputIndex]);
	}

	return OutInputs;
}

FExpressionInput* UMaterialExpressionQualitySwitch::GetInput(int32 InputIndex)
{
	if (InputIndex == 0)
	{
		return &Default;
	}

	return &Inputs[InputIndex - 1];
}

FString UMaterialExpressionQualitySwitch::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return TEXT("Default");
	}

	FString QualityLevelName;
	GetMaterialQualityLevelName((EMaterialQualityLevel::Type)(InputIndex - 1), QualityLevelName);
	return QualityLevelName;
}

bool UMaterialExpressionQualitySwitch::IsInputConnectionRequired(int32 InputIndex) const
{
	return InputIndex == 0;
}

#if WITH_EDITOR
bool UMaterialExpressionQualitySwitch::IsResultMaterialAttributes(int32 OutputIndex)
{
	check(OutputIndex == 0);
	TArray<FExpressionInput*> ExpressionInputs = GetInputs();

	for (FExpressionInput* ExpressionInput : ExpressionInputs)
	{
		// If there is a loop anywhere in this expression's inputs then we can't risk checking them
		if (ExpressionInput->Expression && !ExpressionInput->Expression->ContainsInputLoop() && ExpressionInput->Expression->IsResultMaterialAttributes(ExpressionInput->OutputIndex))
		{
			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

bool UMaterialExpressionQualitySwitch::NeedsLoadForClient() const
{
	return true;
}

//
//	UMaterialExpressionFeatureLevelSwitch
//

UMaterialExpressionFeatureLevelSwitch::UMaterialExpressionFeatureLevelSwitch(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFeatureLevelSwitch::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const ERHIFeatureLevel::Type FeatureLevelToCompile = Compiler->GetFeatureLevel();
	check(FeatureLevelToCompile < ARRAY_COUNT(Inputs));
	FExpressionInput& FeatureInput = Inputs[FeatureLevelToCompile];

	if (!Default.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Feature Level switch missing default input"));
	}

	if (FeatureInput.GetTracedInput().Expression)
	{
		return FeatureInput.Compile(Compiler);
	}

	return Default.Compile(Compiler);
}

void UMaterialExpressionFeatureLevelSwitch::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Feature Level Switch")));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionFeatureLevelSwitch::GetInputs()
{
	TArray<FExpressionInput*> OutInputs;

	OutInputs.Add(&Default);

	for (int32 InputIndex = 0; InputIndex < ARRAY_COUNT(Inputs); InputIndex++)
	{
		OutInputs.Add(&Inputs[InputIndex]);
	}

	return OutInputs;
}

FExpressionInput* UMaterialExpressionFeatureLevelSwitch::GetInput(int32 InputIndex)
{
	if (InputIndex == 0)
	{
		return &Default;
	}

	return &Inputs[InputIndex - 1];
}

FString UMaterialExpressionFeatureLevelSwitch::GetInputName(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return TEXT("Default");
	}

	FString FeatureLevelName;
	GetFeatureLevelName((ERHIFeatureLevel::Type)(InputIndex - 1), FeatureLevelName);
	return FeatureLevelName;
}

bool UMaterialExpressionFeatureLevelSwitch::IsInputConnectionRequired(int32 InputIndex) const
{
	return InputIndex == 0;
}

#if WITH_EDITOR
bool UMaterialExpressionFeatureLevelSwitch::IsResultMaterialAttributes(int32 OutputIndex)
{
	check(OutputIndex == 0);
	TArray<FExpressionInput*> ExpressionInputs = GetInputs();

	for (FExpressionInput* ExpressionInput : ExpressionInputs)
	{
		// If there is a loop anywhere in this expression's inputs then we can't risk checking them
		if (ExpressionInput->GetTracedInput().Expression && !ExpressionInput->Expression->ContainsInputLoop() && ExpressionInput->Expression->IsResultMaterialAttributes(ExpressionInput->OutputIndex))
		{
			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

void UMaterialExpressionFeatureLevelSwitch::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_RENAME_SM3_TO_ES3_1)
	{
		// Copy the ES2 input to SM3 (since SM3 will now become ES3_1 and we don't want broken content)
		Inputs[ERHIFeatureLevel::ES3_1] = Inputs[ERHIFeatureLevel::ES2];
	}
}

bool UMaterialExpressionFeatureLevelSwitch::NeedsLoadForClient() const
{
	return true;
}

//
//	UMaterialExpressionNormalize
//
UMaterialExpressionNormalize::UMaterialExpressionNormalize(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
			, NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionNormalize::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!VectorInput.GetTracedInput().Expression)
		return Compiler->Errorf(TEXT("Missing Normalize input"));

	int32	V = VectorInput.Compile(Compiler);

	return Compiler->Div(V,Compiler->SquareRoot(Compiler->Dot(V,V)));
}
#endif // WITH_EDITOR

UMaterialExpressionVertexColor::UMaterialExpressionVertexColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionVertexColor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->VertexColor();
}

void UMaterialExpressionVertexColor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Vertex Color"));
}
#endif // WITH_EDITOR

UMaterialExpressionParticleColor::UMaterialExpressionParticleColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleColor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleColor();
}

void UMaterialExpressionParticleColor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Color"));
}
#endif // WITH_EDITOR

UMaterialExpressionParticlePositionWS::UMaterialExpressionParticlePositionWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Coordinates;
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
			, NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticlePositionWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticlePosition();
}

void UMaterialExpressionParticlePositionWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Position"));
}
#endif // WITH_EDITOR

UMaterialExpressionParticleRadius::UMaterialExpressionParticleRadius(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleRadius::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleRadius();
}

void UMaterialExpressionParticleRadius::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Radius"));
}
#endif // WITH_EDITOR

UMaterialExpressionDynamicParameter::UMaterialExpressionDynamicParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Parameters;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputNameOnPin = true;
	bHidePreviewWindow = true;

	ParamNames.Add(TEXT("Param1"));
	ParamNames.Add(TEXT("Param2"));
	ParamNames.Add(TEXT("Param3"));
	ParamNames.Add(TEXT("Param4"));

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
	
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));

	DefaultValue = FLinearColor::White;

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionDynamicParameter::Compile( FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->DynamicParameter(DefaultValue);
}
#endif // WITH_EDITOR

TArray<FExpressionOutput>& UMaterialExpressionDynamicParameter::GetOutputs()
{
	Outputs[0].OutputName = *(ParamNames[0]);
	Outputs[1].OutputName = *(ParamNames[1]);
	Outputs[2].OutputName = *(ParamNames[2]);
	Outputs[3].OutputName = *(ParamNames[3]);
	return Outputs;
}


int32 UMaterialExpressionDynamicParameter::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

#if WITH_EDITOR
void UMaterialExpressionDynamicParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Dynamic Parameter"));
}
#endif // WITH_EDITOR

bool UMaterialExpressionDynamicParameter::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	for( int32 Index=0;Index<ParamNames.Num();Index++ )
	{
		if( ParamNames[Index].Contains(SearchQuery) )
		{
			return true;
		}
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR

void UMaterialExpressionDynamicParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionDynamicParameter, ParamNames))
		{
			if (GraphNode)
			{
				GraphNode->ReconstructNode();
			}
		}
	}
}

#endif // WITH_EDITOR

void UMaterialExpressionDynamicParameter::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerUE4Version() < VER_UE4_DYNAMIC_PARAMETER_DEFAULT_VALUE)
	{
		DefaultValue = FLinearColor::Black;//Old data should default to 0.0f;
	}
}

bool UMaterialExpressionDynamicParameter::NeedsLoadForClient() const
{
	return true;
}

void UMaterialExpressionDynamicParameter::UpdateDynamicParameterProperties()
{
	check(Material);
	for (int32 ExpIndex = 0; ExpIndex < Material->Expressions.Num(); ExpIndex++)
	{
		const UMaterialExpressionDynamicParameter* DynParam = Cast<UMaterialExpressionDynamicParameter>(Material->Expressions[ExpIndex]);
		if (CopyDynamicParameterProperties(DynParam))
		{
			break;
		}
	}
}

bool UMaterialExpressionDynamicParameter::CopyDynamicParameterProperties(const UMaterialExpressionDynamicParameter* FromParam)
{
	if (FromParam && (FromParam != this))
	{
		for (int32 NameIndex = 0; NameIndex < 4; NameIndex++)
		{
			ParamNames[NameIndex] = FromParam->ParamNames[NameIndex];
		}
		DefaultValue = FromParam->DefaultValue;
		return true;
	}
	return false;
}

//
//	MaterialExpressionParticleSubUV
//
UMaterialExpressionParticleSubUV::UMaterialExpressionParticleSubUV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bBlend = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleSubUV::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Texture)
	{
		if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("ParticleSubUV")), Texture, SamplerType))
		{
			return INDEX_NONE;
		}
		int32 TextureCodeIndex = Compiler->Texture(Texture);
		return ParticleSubUV(Compiler, TextureCodeIndex, Texture, SamplerType, Coordinates, bBlend);
	}
	else
	{
		return Compiler->Errorf(TEXT("Missing ParticleSubUV input texture"));
	}
}
#endif // WITH_EDITOR

int32 UMaterialExpressionParticleSubUV::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

#if WITH_EDITOR
void UMaterialExpressionParticleSubUV::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle SubUV"));
}
#endif // WITH_EDITOR

//
//	MaterialExpressionParticleMacroUV
//
UMaterialExpressionParticleMacroUV::UMaterialExpressionParticleMacroUV(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleMacroUV::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleMacroUV();
}

void UMaterialExpressionParticleMacroUV::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle MacroUV"));
}
#endif // WITH_EDITOR

UMaterialExpressionLightVector::UMaterialExpressionLightVector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionLightVector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->LightVector();
}

void UMaterialExpressionLightVector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Light Vector"));
}
#endif // WITH_EDITOR

UMaterialExpressionScreenPosition::UMaterialExpressionScreenPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionScreenPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ScreenPosition(Mapping);
}

void UMaterialExpressionScreenPosition::GetCaption(TArray<FString>& OutCaptions) const
{

#if WITH_EDITOR
	const UEnum* ScreenPositionMappingEnum = FindObject<UEnum>(NULL, TEXT("Engine.EMaterialExpressionScreenPositionMapping"));
	check(ScreenPositionMappingEnum);

	const FString MappingDisplayName = ScreenPositionMappingEnum->GetDisplayNameTextByValue(Mapping).ToString();
	OutCaptions.Add(MappingDisplayName);
#endif
	OutCaptions.Add(TEXT("ScreenPosition"));
}
#endif // WITH_EDITOR

UMaterialExpressionViewProperty::UMaterialExpressionViewProperty(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Property = MEVP_FieldOfView;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
	bShowOutputNameOnPin = true;
	
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Property")));
	Outputs.Add(FExpressionOutput(TEXT("InvProperty")));
}

#if WITH_EDITOR
int32 UMaterialExpressionViewProperty::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ViewProperty(Property, OutputIndex == 1);
}

void UMaterialExpressionViewProperty::GetCaption(TArray<FString>& OutCaptions) const
{
#if WITH_EDITOR
	const UEnum* ViewPropertyEnum = FindObject<UEnum>(NULL, TEXT("Engine.EMaterialExposedViewProperty"));
	check(ViewPropertyEnum);

	const FString PropertyDisplayName = ViewPropertyEnum->GetDisplayNameTextByValue(Property).ToString();
#else
	const FString PropertyDisplayName = TEXT("");
#endif

	OutCaptions.Add(PropertyDisplayName);
}
#endif // WITH_EDITOR

UMaterialExpressionViewSize::UMaterialExpressionViewSize(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionViewSize::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ViewProperty(MEVP_ViewSize);
}

void UMaterialExpressionViewSize::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ViewSize"));
}
#endif // WITH_EDITOR

UMaterialExpressionSceneTexelSize::UMaterialExpressionSceneTexelSize(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionSceneTexelSize::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ViewProperty(MEVP_BufferSize, /* InvProperty = */ true);
}

void UMaterialExpressionSceneTexelSize::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("SceneTexelSize"));
}
#endif // WITH_EDITOR

UMaterialExpressionSquareRoot::UMaterialExpressionSquareRoot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSquareRoot::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing square root input"));
	}
	return Compiler->SquareRoot(Input.Compile(Compiler));
}

void UMaterialExpressionSquareRoot::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Sqrt"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionPixelDepth
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionPixelDepth::UMaterialExpressionPixelDepth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Depth;
		FConstructorStatics()
			: NAME_Depth(LOCTEXT( "Depth", "Depth" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Depth);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPixelDepth::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// resulting index to compiled code chunk
	// add the code chunk for the pixel's depth     
	int32 Result = Compiler->PixelDepth();
	return Result;
}

void UMaterialExpressionPixelDepth::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("PixelDepth"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSceneDepth
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionSceneDepth::UMaterialExpressionSceneDepth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Depth;
		FConstructorStatics()
			: NAME_Depth(LOCTEXT( "Depth", "Depth" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Depth);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	bShaderInputData = true;
	ConstInput = FVector2D(0.f, 0.f);
}

void UMaterialExpressionSceneDepth::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerUE4Version() < VER_UE4_REFACTOR_MATERIAL_EXPRESSION_SCENECOLOR_AND_SCENEDEPTH_INPUTS)
	{
		// Connect deprecated UV input to new expression input
		InputMode = EMaterialSceneAttributeInputMode::Coordinates;
		Input = Coordinates_DEPRECATED;
	}
}

#if WITH_EDITOR
int32 UMaterialExpressionSceneDepth::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{    
	int32 OffsetIndex = INDEX_NONE;
	int32 CoordinateIndex = INDEX_NONE;
	bool bUseOffset = false;

	if(InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		if (Input.GetTracedInput().Expression)
		{
			OffsetIndex = Input.Compile(Compiler);
		} 
		else
		{
			OffsetIndex = Compiler->Constant2(ConstInput.X, ConstInput.Y);
		}
		bUseOffset = true;
	}
	else if(InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		if (Input.GetTracedInput().Expression)
		{
			CoordinateIndex = Input.Compile(Compiler);
		} 
	}

	int32 Result = Compiler->SceneDepth(OffsetIndex, CoordinateIndex, bUseOffset);
	return Result;
}

void UMaterialExpressionSceneDepth::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Scene Depth"));
}
#endif // WITH_EDITOR

FString UMaterialExpressionSceneDepth::GetInputName(int32 InputIndex) const
{
	if(InputIndex == 0)
	{
		// Display the current InputMode enum's display name.
		UByteProperty* InputModeProperty = FindField<UByteProperty>( UMaterialExpressionSceneDepth::StaticClass(), "InputMode" );
		return InputModeProperty->Enum->GetNameStringByValue((int64)InputMode.GetValue());
	}
	return TEXT("");
}


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSceneTexture
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionSceneTexture::UMaterialExpressionSceneTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Texture;
		FConstructorStatics()
			: NAME_Texture(LOCTEXT( "Texture", "Texture" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
#endif

	bShaderInputData = true;
	
	bShowOutputNameOnPin = true;

	//by default slower but reliable results, if the shader never accesses the texels outside it can be disabled.
	bClampUVs = true;
	// by default faster, most lookup are read/write the same pixel so this is ralrely needed
	bFiltered = false;

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Color"), 1, 1, 1, 1, 1));
	Outputs.Add(FExpressionOutput(TEXT("Size")));
	Outputs.Add(FExpressionOutput(TEXT("InvSize")));
}

#if WITH_EDITOR
int32 UMaterialExpressionSceneTexture::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{    
	int32 UV = INDEX_NONE;

	if (Coordinates.GetTracedInput().Expression)
	{
		UV = Coordinates.Compile(Compiler);
	}

	if(OutputIndex == 0)
	{
		if( INDEX_NONE != UV )
		{
			int32 Max = Compiler->SceneTextureMax(SceneTextureId);
			int32 Min = Compiler->SceneTextureMin(SceneTextureId);
			if( bClampUVs )
			{
				UV = Compiler->Clamp(UV, Min, Max);
			}
		}

		// Color
		return Compiler->SceneTextureLookup(UV, SceneTextureId, bFiltered);
	}
	else if(OutputIndex == 1 || OutputIndex == 2)
	{
		// Size or InvSize
		return Compiler->SceneTextureSize(SceneTextureId, OutputIndex == 2);
	}

	return Compiler->Errorf(TEXT("Invalid input parameter"));
}

void UMaterialExpressionSceneTexture::GetCaption(TArray<FString>& OutCaptions) const
{
	UEnum* Enum = FindObject<UEnum>(NULL, TEXT("Engine.ESceneTextureId"));

	check(Enum);

	FString Name = Enum->GetDisplayNameTextByValue(SceneTextureId).ToString();

	OutCaptions.Add(FString(TEXT("SceneTexture:")) + Name);
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSceneColor
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionSceneColor::UMaterialExpressionSceneColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Texture;
		FConstructorStatics()
			: NAME_Texture(LOCTEXT( "Texture", "Texture" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
#endif

	bShaderInputData = true;
	ConstInput = FVector2D(0.f, 0.f);
}

void UMaterialExpressionSceneColor::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerUE4Version() < VER_UE4_REFACTOR_MATERIAL_EXPRESSION_SCENECOLOR_AND_SCENEDEPTH_INPUTS)
	{
		// Connect deprecated UV input to new expression input
		InputMode = EMaterialSceneAttributeInputMode::OffsetFraction;
		Input = OffsetFraction_DEPRECATED;
	}
}

#if WITH_EDITOR
int32 UMaterialExpressionSceneColor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 OffsetIndex = INDEX_NONE;
	int32 CoordinateIndex = INDEX_NONE;
	bool bUseOffset = false;


	if(InputMode == EMaterialSceneAttributeInputMode::OffsetFraction)
	{
		if (Input.GetTracedInput().Expression)
		{
			OffsetIndex = Input.Compile(Compiler);
		}
		else
		{
			OffsetIndex = Compiler->Constant2(ConstInput.X, ConstInput.Y);
		}

		bUseOffset = true;
	}
	else if(InputMode == EMaterialSceneAttributeInputMode::Coordinates)
	{
		if (Input.GetTracedInput().Expression)
		{
			CoordinateIndex = Input.Compile(Compiler);
		} 
	}	

	int32 Result = Compiler->SceneColor(OffsetIndex, CoordinateIndex, bUseOffset);
	return Result;
}

void UMaterialExpressionSceneColor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Scene Color"));
}
#endif // WITH_EDITOR

UMaterialExpressionPower::UMaterialExpressionPower(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif

	ConstExponent = 2;
}

#if WITH_EDITOR
int32 UMaterialExpressionPower::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Base.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Power Base input"));
	}

	int32 Arg1 = Base.Compile(Compiler);
	int32 Arg2 = Exponent.GetTracedInput().Expression ? Exponent.Compile(Compiler) : Compiler->Constant(ConstExponent);
	return Compiler->Power(
		Arg1,
		Arg2
		);
}

void UMaterialExpressionPower::GetCaption(TArray<FString>& OutCaptions) const
{
	FString ret = TEXT("Power");

	if (!Exponent.GetTracedInput().Expression)
	{
		ret += FString::Printf( TEXT("(X, %.4g)"), ConstExponent);
	}

	OutCaptions.Add(ret);
}

void UMaterialExpressionPower::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns the Base value raised to the power of Exponent. Base value must be positive, values less than 0 will be clamped."), 40, OutToolTip);
}
#endif // WITH_EDITOR

UMaterialExpressionLogarithm2::UMaterialExpressionLogarithm2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLogarithm2::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!X.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Log2 X input"));
	}

	return Compiler->Logarithm2(X.Compile(Compiler));
}

void UMaterialExpressionLogarithm2::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Log2"));
}

void UMaterialExpressionLogarithm2::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns the base-2 logarithm of the input. Input should be greater than 0."), 40, OutToolTip);
}
#endif // WITH_EDITOR

UMaterialExpressionLogarithm10::UMaterialExpressionLogarithm10(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLogarithm10::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!X.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing Log10 X input"));
	}

	return Compiler->Logarithm10(X.Compile(Compiler));
}

void UMaterialExpressionLogarithm10::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Log10"));
}

void UMaterialExpressionLogarithm10::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns the base-10 logarithm of the input. Input should be greater than 0."), 40, OutToolTip);
}
#endif // WITH_EDITOR

FString UMaterialExpressionSceneColor::GetInputName(int32 InputIndex) const
{
	if(InputIndex == 0)
	{
		// Display the current InputMode enum's display name.
		UByteProperty* InputModeProperty = FindField<UByteProperty>( UMaterialExpressionSceneColor::StaticClass(), "InputMode" );
		return InputModeProperty->Enum->GetNameStringByValue((int64)InputMode.GetValue());
	}
	return TEXT("");
}

UMaterialExpressionIf::UMaterialExpressionIf(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif

	EqualsThreshold = 0.00001f;
	ConstB = 0.0f;
}

#if WITH_EDITOR
int32 UMaterialExpressionIf::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing If A input"));
	}
	if(!AGreaterThanB.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing If AGreaterThanB input"));
	}
	if(!ALessThanB.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing If ALessThanB input"));
	}

	int32 CompiledA = A.Compile(Compiler);
	int32 CompiledB = B.GetTracedInput().Expression ? B.Compile(Compiler) : Compiler->Constant(ConstB);

	if(Compiler->GetType(CompiledA) != MCT_Float)
	{
		return Compiler->Errorf(TEXT("If input A must be of type float."));
	}

	if(Compiler->GetType(CompiledB) != MCT_Float)
	{
		return Compiler->Errorf(TEXT("If input B must be of type float."));
	}

	int32 Arg3 = AGreaterThanB.Compile(Compiler);
	int32 Arg4 = AEqualsB.GetTracedInput().Expression ? AEqualsB.Compile(Compiler) : INDEX_NONE;
	int32 Arg5 = ALessThanB.Compile(Compiler);
	int32 ThresholdArg = Compiler->Constant(EqualsThreshold);

	return Compiler->If(CompiledA,CompiledB,Arg3,Arg4,Arg5,ThresholdArg);
}

void UMaterialExpressionIf::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("If"));
}

uint32 UMaterialExpressionIf::GetInputType(int32 InputIndex)
{
	// First two inputs are always float
	if (InputIndex == 0 || InputIndex == 1)
	{
		return MCT_Float;
	}
	else
	{
		return MCT_Unknown;
	}
}
#endif // WITH_EDITOR

UMaterialExpressionOneMinus::UMaterialExpressionOneMinus(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionOneMinus::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing 1-x input"));
	}
	return Compiler->Sub(Compiler->Constant(1.0f),Input.Compile(Compiler));
}

void UMaterialExpressionOneMinus::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("1-x"));
}
#endif // WITH_EDITOR

UMaterialExpressionAbs::UMaterialExpressionAbs(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Math;
		FConstructorStatics()
			: NAME_Math(LOCTEXT( "Math", "Math" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Math);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionAbs::Compile( FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result=INDEX_NONE;

	if( !Input.GetTracedInput().Expression )
	{
		// an input expression must exist
		Result = Compiler->Errorf( TEXT("Missing Abs input") );
	}
	else
	{
		// evaluate the input expression first and use that as
		// the parameter for the Abs expression
		Result = Compiler->Abs( Input.Compile(Compiler) );
	}

	return Result;
}

void UMaterialExpressionAbs::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Abs"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionTransform
///////////////////////////////////////////////////////////////////////////////

static EMaterialCommonBasis GetMaterialCommonBasis(EMaterialVectorCoordTransformSource X)
{
	static const EMaterialCommonBasis ConversionTable[TRANSFORMSOURCE_MAX] = {
		MCB_Tangent,					// TRANSFORMSOURCE_Tangent
		MCB_Local,						// TRANSFORMSOURCE_Local
		MCB_World,						// TRANSFORMSOURCE_World
		MCB_View,						// TRANSFORMSOURCE_View
		MCB_Camera,						// TRANSFORMSOURCE_Camera
		MCB_MeshParticle,
	};
	return ConversionTable[X];
}

static EMaterialCommonBasis GetMaterialCommonBasis(EMaterialVectorCoordTransform X)
{
	static const EMaterialCommonBasis ConversionTable[TRANSFORM_MAX] = {
		MCB_Tangent,					// TRANSFORM_Tangent
		MCB_Local,						// TRANSFORM_Local
		MCB_World,						// TRANSFORM_World
		MCB_View,						// TRANSFORM_View
		MCB_Camera,						// TRANSFORM_Camera
		MCB_MeshParticle,
	};
	return ConversionTable[X];
}

#if WITH_EDITOR
int32 UMaterialExpressionTransform::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result = INDEX_NONE;

	if (!Input.GetTracedInput().Expression)
	{
		Result = Compiler->Errorf(TEXT("Missing Transform input vector"));
	}
	else
	{
		int32 VecInputIdx = Input.Compile(Compiler);
		const auto TransformSourceBasis = GetMaterialCommonBasis(TransformSourceType);
		const auto TransformDestBasis = GetMaterialCommonBasis(TransformType);
		Result = Compiler->TransformVector(TransformSourceBasis, TransformDestBasis, VecInputIdx);
	}

	return Result;
}

void UMaterialExpressionTransform::GetCaption(TArray<FString>& OutCaptions) const
{
#if WITH_EDITOR
	const UEnum* MVCTSEnum = FindObject<UEnum>(nullptr, TEXT("Engine.EMaterialVectorCoordTransformSource"));
	const UEnum* MVCTEnum = FindObject<UEnum>(nullptr, TEXT("Engine.EMaterialVectorCoordTransform"));
	check(MVCTSEnum);
	check(MVCTEnum);
	
	FString TransformDesc;
	TransformDesc += MVCTSEnum->GetDisplayNameTextByValue(TransformSourceType).ToString();
	TransformDesc += TEXT(" to ");
	TransformDesc += MVCTEnum->GetDisplayNameTextByValue(TransformType).ToString();
	OutCaptions.Add(TransformDesc);
#else
	OutCaptions.Add(TEXT(""));
#endif

	OutCaptions.Add(TEXT("TransformVector"));
}
#endif // WITH_EDITOR

UMaterialExpressionTransform::UMaterialExpressionTransform(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif

	TransformSourceType = TRANSFORMSOURCE_Tangent;
	TransformType = TRANSFORM_World;
}


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionTransformPosition
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionTransformPosition::UMaterialExpressionTransformPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif

	TransformSourceType = TRANSFORMPOSSOURCE_Local;
	TransformType = TRANSFORMPOSSOURCE_Local;
}

static EMaterialCommonBasis GetMaterialCommonBasis(EMaterialPositionTransformSource X)
{
	static const EMaterialCommonBasis ConversionTable[TRANSFORMPOSSOURCE_MAX] = {
		MCB_Local,						// TRANSFORMPOSSOURCE_Local
		MCB_World,						// TRANSFORMPOSSOURCE_World
		MCB_TranslatedWorld,			// TRANSFORMPOSSOURCE_TranslatedWorld
		MCB_View,						// TRANSFORMPOSSOURCE_View
		MCB_Camera,						// TRANSFORMPOSSOURCE_Camera
		MCB_MeshParticle,	
	};
	return ConversionTable[X];
}

#if WITH_EDITOR
int32 UMaterialExpressionTransformPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result=INDEX_NONE;
	
	if( !Input.GetTracedInput().Expression )
	{
		Result = Compiler->Errorf(TEXT("Missing Transform Position input vector"));
	}
	else
	{
		int32 VecInputIdx = Input.Compile(Compiler);
		const auto TransformSourceBasis = GetMaterialCommonBasis(TransformSourceType);
		const auto TransformDestBasis = GetMaterialCommonBasis(TransformType);
		Result = Compiler->TransformPosition(TransformSourceBasis, TransformDestBasis, VecInputIdx);
	}

	return Result;
}

void UMaterialExpressionTransformPosition::GetCaption(TArray<FString>& OutCaptions) const
{
#if WITH_EDITOR
	const UEnum* MPTSEnum = FindObject<UEnum>(nullptr, TEXT("Engine.EMaterialPositionTransformSource"));
	check(MPTSEnum);
	
	FString TransformDesc;
	TransformDesc += MPTSEnum->GetDisplayNameTextByValue(TransformSourceType).ToString();
	TransformDesc += TEXT(" to ");
	TransformDesc += MPTSEnum->GetDisplayNameTextByValue(TransformType).ToString();
	OutCaptions.Add(TransformDesc);
#else
	OutCaptions.Add(TEXT(""));
#endif
	
	OutCaptions.Add(TEXT("TransformPosition"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionComment
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionComment::UMaterialExpressionComment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CommentColor(FLinearColor::White)
	, FontSize(18)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR

void UMaterialExpressionComment::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, Text))
		{
			if (GraphNode)
			{
				GraphNode->Modify();
				GraphNode->NodeComment = Text;
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, CommentColor))
		{
			if (GraphNode)
			{
				GraphNode->Modify();
				CastChecked<UMaterialGraphNode_Comment>(GraphNode)->CommentColor = CommentColor;
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionComment, FontSize))
		{
			if (GraphNode)
			{
				GraphNode->Modify();
				CastChecked<UMaterialGraphNode_Comment>(GraphNode)->FontSize = FontSize;
			}
		}

		// Don't need to update preview after changing comments
		bNeedToUpdatePreview = false;
	}
}

#endif // WITH_EDITOR

bool UMaterialExpressionComment::Modify( bool bAlwaysMarkDirty/*=true*/ )
{
	bool bResult = Super::Modify(bAlwaysMarkDirty);

	// Don't need to update preview after changing comments
	bNeedToUpdatePreview = false;

	return bResult;
}

#if WITH_EDITOR
void UMaterialExpressionComment::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Comment"));
}
#endif // WITH_EDITOR

bool UMaterialExpressionComment::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( Text.Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

UMaterialExpressionFresnel::UMaterialExpressionFresnel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Exponent = 5.0f;
	BaseReflectFraction = 0.04f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFresnel::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// pow(1 - max(0,Normal dot Camera),Exponent) * (1 - BaseReflectFraction) + BaseReflectFraction
	//
	int32 NormalArg = Normal.GetTracedInput().Expression ? Normal.Compile(Compiler) : Compiler->PixelNormalWS();
	int32 DotArg = Compiler->Dot(NormalArg,Compiler->CameraVector());
	int32 MaxArg = Compiler->Max(Compiler->Constant(0.f),DotArg);
	int32 MinusArg = Compiler->Sub(Compiler->Constant(1.f),MaxArg);
	int32 ExponentArg = ExponentIn.GetTracedInput().Expression ? ExponentIn.Compile(Compiler) : Compiler->Constant(Exponent);
	// Compiler->Power got changed to call PositiveClampedPow instead of ClampedPow
	// Manually implement ClampedPow to maintain backwards compatibility in the case where the input normal is not normalized (length > 1)
	int32 AbsBaseArg = Compiler->Abs(MinusArg);
	int32 PowArg = Compiler->Power(AbsBaseArg,ExponentArg);
	int32 BaseReflectFractionArg = BaseReflectFractionIn.GetTracedInput().Expression ? BaseReflectFractionIn.Compile(Compiler) : Compiler->Constant(BaseReflectFraction);
	int32 ScaleArg = Compiler->Mul(PowArg, Compiler->Sub(Compiler->Constant(1.f), BaseReflectFractionArg));
	
	return Compiler->Add(ScaleArg, BaseReflectFractionArg);
}
#endif // WITH_EDITOR

/*-----------------------------------------------------------------------------
UMaterialExpressionFontSample
-----------------------------------------------------------------------------*/
UMaterialExpressionFontSample::UMaterialExpressionFontSample(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Font;
		FText NAME_Texture;
		FConstructorStatics()
			: NAME_Font(LOCTEXT( "Font", "Font" ))
			, NAME_Texture(LOCTEXT( "Texture", "Texture" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Font);
	MenuCategories.Add(ConstructorStatics.NAME_Texture);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 1, 0, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 0, 0, 0, 1));

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionFontSample::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result = -1;
#if PLATFORM_EXCEPTIONS_DISABLED
	// if we can't throw the error below, attempt to thwart the error by using the default font
	if( !Font )
	{
		UE_LOG(LogMaterial, Log, TEXT("Using default font instead of real font!"));
		Font = GEngine->GetMediumFont();
		FontTexturePage = 0;
	}
	else if( !Font->Textures.IsValidIndex(FontTexturePage) )
	{
		UE_LOG(LogMaterial, Log, TEXT("Invalid font page %d. Max allowed is %d"),FontTexturePage,Font->Textures.Num());
		FontTexturePage = 0;
	}
#endif
	if( !Font )
	{
		Result = CompilerError(Compiler, TEXT("Missing input Font"));
	}
	else if( Font->FontCacheType == EFontCacheType::Runtime )
	{
		Result = CompilerError(Compiler, *FString::Printf(TEXT("Font '%s' is runtime cached, but only offline cached fonts can be sampled"), *Font->GetName()));
	}
	else if( !Font->Textures.IsValidIndex(FontTexturePage) )
	{
		Result = CompilerError(Compiler, *FString::Printf(TEXT("Invalid font page %d. Max allowed is %d"), FontTexturePage, Font->Textures.Num()));
	}
	else
	{
		UTexture* Texture = Font->Textures[FontTexturePage];
		if( !Texture )
		{
			UE_LOG(LogMaterial, Log, TEXT("Invalid font texture. Using default texture"));
			Texture = GEngine->DefaultTexture;
		}
		check(Texture);

		EMaterialSamplerType ExpectedSamplerType;
		if (Texture->CompressionSettings == TC_DistanceFieldFont)
		{
			ExpectedSamplerType = SAMPLERTYPE_DistanceFieldFont;
		}
		else
		{
			ExpectedSamplerType = Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		}

		if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("FontSample")), Texture, ExpectedSamplerType))
		{
			return INDEX_NONE;
		}

		int32 TextureCodeIndex = Compiler->Texture(Texture);
		Result = Compiler->TextureSample(
			TextureCodeIndex,
			Compiler->TextureCoordinate(0, false, false),
			ExpectedSamplerType
		);
	}
	return Result;
}
#endif // WITH_EDITOR

int32 UMaterialExpressionFontSample::GetWidth() const
{
	return ME_STD_THUMBNAIL_SZ+(ME_STD_BORDER*2);
}

#if WITH_EDITOR
void UMaterialExpressionFontSample::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Font Sample"));
}
#endif // WITH_EDITOR

bool UMaterialExpressionFontSample::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( Font!=NULL && Font->GetName().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

UTexture* UMaterialExpressionFontSample::GetReferencedTexture() 
{
	if (Font && Font->Textures.IsValidIndex(FontTexturePage))
	{
		UTexture* Texture = Font->Textures[FontTexturePage];
		return Texture;
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
UMaterialExpressionFontSampleParameter
-----------------------------------------------------------------------------*/
UMaterialExpressionFontSampleParameter::UMaterialExpressionFontSampleParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Font;
		FText NAME_Parameters;
		FConstructorStatics()
			: NAME_Font(LOCTEXT( "Font", "Font" ))
			, NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bIsParameterExpression = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Font);
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionFontSampleParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 Result = -1;
	if( !ParameterName.IsValid() || 
		ParameterName.IsNone() || 
		!Font ||
		!Font->Textures.IsValidIndex(FontTexturePage) )
	{
		Result = UMaterialExpressionFontSample::Compile(Compiler, OutputIndex);
	}
	else 
	{
		UTexture* Texture = Font->Textures[FontTexturePage];
		if( !Texture )
		{
			UE_LOG(LogMaterial, Log, TEXT("Invalid font texture. Using default texture"));
			Texture = GEngine->DefaultTexture;
		}
		check(Texture);

		EMaterialSamplerType ExpectedSamplerType;
		if (Texture->CompressionSettings == TC_DistanceFieldFont)
		{
			ExpectedSamplerType = SAMPLERTYPE_DistanceFieldFont;
		}
		else
		{
			ExpectedSamplerType = Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		}

		if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("FontSampleParameter")), Texture, ExpectedSamplerType))
		{
			return INDEX_NONE;
		}
		int32 TextureCodeIndex = Compiler->TextureParameter(ParameterName,Texture);
		Result = Compiler->TextureSample(
			TextureCodeIndex,
			Compiler->TextureCoordinate(0, false, false),
			ExpectedSamplerType
		);
	}
	return Result;
}

void UMaterialExpressionFontSampleParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Font Param")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionFontSampleParameter::IsNamedParameter(FName InParameterName, UFont*& OutFontValue, int32& OutFontPage) const
{
	if (InParameterName == ParameterName)
	{
		OutFontValue = Font;
		OutFontPage = FontTexturePage;
		return true;
	}

	return false;
}

void UMaterialExpressionFontSampleParameter::SetDefaultFont()
{
	GEngine->GetMediumFont();
}

bool UMaterialExpressionFontSampleParameter::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if( ParameterName.ToString().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR
FString UMaterialExpressionFontSampleParameter::GetEditableName() const
{
	return ParameterName.ToString();
}

void UMaterialExpressionFontSampleParameter::SetEditableName(const FString& NewName)
{
	ParameterName = *NewName;
}
#endif

void UMaterialExpressionFontSampleParameter::GetAllParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	int32 CurrentSize = OutParameterNames.Num();
	OutParameterNames.AddUnique(ParameterName);

	if(CurrentSize != OutParameterNames.Num())
	{
		OutParameterIds.Add(ExpressionGUID);
	}
}


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionWorldPosition
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionWorldPosition::UMaterialExpressionWorldPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
	WorldPositionShaderOffset = WPT_Default;
}

#if WITH_EDITOR
int32 UMaterialExpressionWorldPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// TODO: should use a separate check box for Including/Excluding Material Shader Offsets
	return Compiler->WorldPosition(WorldPositionShaderOffset);
}

void UMaterialExpressionWorldPosition::GetCaption(TArray<FString>& OutCaptions) const
{
	switch (WorldPositionShaderOffset)
	{
	case WPT_Default:
		{
			OutCaptions.Add(NSLOCTEXT("MaterialExpressions", "WorldPositonText", "Absolute World Position").ToString());
			break;
		}

	case WPT_ExcludeAllShaderOffsets:
		{
			OutCaptions.Add(NSLOCTEXT("MaterialExpressions", "WorldPositonExcludingOffsetsText", "Absolute World Position (Excluding Material Offsets)").ToString());
			break;
		}

	case WPT_CameraRelative:
		{
			OutCaptions.Add(NSLOCTEXT("MaterialExpressions", "CamRelativeWorldPositonText", "Camera Relative World Position").ToString());
			break;
		}

	case WPT_CameraRelativeNoOffsets:
		{
			OutCaptions.Add(NSLOCTEXT("MaterialExpressions", "CamRelativeWorldPositonExcludingOffsetsText", "Camera Relative World Position (Excluding Material Offsets)").ToString());
			break;
		}

	default:
		{
			UE_LOG(LogMaterial, Fatal, TEXT("Unknown world position shader offset type"));
			break;
		}
	}
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionObjectPositionWS
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionObjectPositionWS::UMaterialExpressionObjectPositionWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionObjectPositionWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain == MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Expression not available in the deferred decal material domain."));
	}

	return Compiler->ObjectWorldPosition();
}

void UMaterialExpressionObjectPositionWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Object Position"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionObjectRadius
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionObjectRadius::UMaterialExpressionObjectRadius(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionObjectRadius::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain == MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Expression not available in the deferred decal material domain."));
	}

	return Compiler->ObjectRadius();
}

void UMaterialExpressionObjectRadius::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Object Radius"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionObjectBoundingBox
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionObjectBounds::UMaterialExpressionObjectBounds(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionObjectBounds::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain == MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Expression not available in the deferred decal material domain."));
	}

	return Compiler->ObjectBounds();
}

void UMaterialExpressionObjectBounds::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Object Bounds"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDistanceCullFade
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDistanceCullFade::UMaterialExpressionDistanceCullFade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionDistanceCullFade::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->DistanceCullFade();
}

void UMaterialExpressionDistanceCullFade::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Distance Cull Fade"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionActorPositionWS
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionActorPositionWS::UMaterialExpressionActorPositionWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionActorPositionWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material != nullptr && (Material->MaterialDomain != MD_Surface) && (Material->MaterialDomain != MD_DeferredDecal) && (Material->MaterialDomain != MD_Volume))
	{
		return CompilerError(Compiler, TEXT("Expression only available in the Surface and Deferred Decal material domains."));
	}

	return Compiler->ActorWorldPosition();
}

void UMaterialExpressionActorPositionWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Actor Position"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDeriveNormalZ
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDeriveNormalZ::UMaterialExpressionDeriveNormalZ(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_VectorOps;
		FConstructorStatics()
			: NAME_VectorOps(LOCTEXT( "VectorOps", "VectorOps" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_VectorOps);
#endif
}
	
#if WITH_EDITOR
int32 UMaterialExpressionDeriveNormalZ::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!InXY.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing input normal xy vector whose z should be derived."));
	}

	//z = sqrt(1 - ( x * x + y * y));
	int32 InputVector = Compiler->ForceCast(InXY.Compile(Compiler), MCT_Float2);
	int32 DotResult = Compiler->Dot(InputVector, InputVector);
	int32 InnerResult = Compiler->Sub(Compiler->Constant(1), DotResult);
	int32 DerivedZ = Compiler->SquareRoot(InnerResult);
	int32 AppendedResult = Compiler->ForceCast(Compiler->AppendVector(InputVector, DerivedZ), MCT_Float3);

	return AppendedResult;
}

void UMaterialExpressionDeriveNormalZ::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("DeriveNormalZ"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionConstantBiasScale
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionConstantBiasScale::UMaterialExpressionConstantBiasScale(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Bias = 1.0f;
	Scale = 0.5f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionConstantBiasScale::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Input.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing ConstantBiasScale input"));
	}

	return Compiler->Mul(Compiler->Add(Compiler->Constant(Bias), Input.Compile(Compiler)), Compiler->Constant(Scale));
}


void UMaterialExpressionConstantBiasScale::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ConstantBiasScale"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionCustom
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionCustom::UMaterialExpressionCustom(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Custom;
		FConstructorStatics()
			: NAME_Custom(LOCTEXT( "Custom", "Custom" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Description = TEXT("Custom");
	Code = TEXT("1");

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Custom);
#endif

	OutputType = CMOT_Float3;

	Inputs.Add(FCustomInput());
	Inputs[0].InputName = TEXT("");

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionCustom::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	TArray<int32> CompiledInputs;

	for( int32 i=0;i<Inputs.Num();i++ )
	{
		// skip over unnamed inputs
		if( Inputs[i].InputName.Len()==0 )
		{
			CompiledInputs.Add(INDEX_NONE);
		}
		else
		{
			if(!Inputs[i].Input.GetTracedInput().Expression)
			{
				return Compiler->Errorf(TEXT("Custom material %s missing input %d (%s)"), *Description, i+1, *Inputs[i].InputName);
			}
			int32 InputCode = Inputs[i].Input.Compile(Compiler);
			if( InputCode < 0 )
			{
				return InputCode;
			}
			CompiledInputs.Add( InputCode );
		}
	}

	return Compiler->CustomExpression(this, CompiledInputs);
}


void UMaterialExpressionCustom::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(Description);
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionCustom::GetInputs()
{
	TArray<FExpressionInput*> Result;
	for( int32 i = 0; i < Inputs.Num(); i++ )
	{
		Result.Add(&Inputs[i].Input);
	}
	return Result;
}

FExpressionInput* UMaterialExpressionCustom::GetInput(int32 InputIndex)
{
	if( InputIndex < Inputs.Num() )
	{
		return &Inputs[InputIndex].Input;
	}
	return NULL;
}

FString UMaterialExpressionCustom::GetInputName(int32 InputIndex) const
{
	if( InputIndex < Inputs.Num() )
	{
		return Inputs[InputIndex].InputName;
	}
	return TEXT("");
}

#if WITH_EDITOR
void UMaterialExpressionCustom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// strip any spaces from input name
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("InputName")) )
	{
		for( int32 i=0;i<Inputs.Num();i++ )
		{
			Inputs[i].InputName.ReplaceInline(TEXT(" "),TEXT(""));
		}
	}

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Inputs))
		{
			if (GraphNode)
			{
				GraphNode->ReconstructNode();
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

uint32 UMaterialExpressionCustom::GetOutputType(int32 OutputIndex)
{
	switch (OutputType)
	{
	case CMOT_Float1:
		return MCT_Float;
	case CMOT_Float2:
		return MCT_Float2;
	case CMOT_Float3:
		return MCT_Float3;
	case CMOT_Float4:
		return MCT_Float4;
	default:
		return MCT_Unknown;
	}
}
#endif // WITH_EDITOR

void UMaterialExpressionCustom::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	// Make a copy of the current code before we change it
	const FString PreFixUp = Code;

	bool bDidUpdate = false;

	if (Ar.UE4Ver() < VER_UE4_INSTANCED_STEREO_UNIFORM_UPDATE)
	{
		// Look for WorldPosition rename
		if (Code.ReplaceInline(TEXT("Parameters.WorldPosition"), TEXT("Parameters.AbsoluteWorldPosition"), ESearchCase::CaseSensitive) > 0)
		{
			bDidUpdate = true;
		}
	}
	// Fix up uniform references that were moved from View to Frame as part of the instanced stereo implementation
	else if (Ar.UE4Ver() < VER_UE4_INSTANCED_STEREO_UNIFORM_REFACTOR)
	{
		// Uniform members that were moved from View to Frame
		static const FString UniformMembers[] = {
			FString(TEXT("FieldOfViewWideAngles")),
			FString(TEXT("PrevFieldOfViewWideAngles")),
			FString(TEXT("ViewRectMin")),
			FString(TEXT("ViewSizeAndInvSize")),
			FString(TEXT("BufferSizeAndInvSize")),
			FString(TEXT("ExposureScale")),
			FString(TEXT("DiffuseOverrideParameter")),
			FString(TEXT("SpecularOverrideParameter")),
			FString(TEXT("NormalOverrideParameter")),
			FString(TEXT("RoughnessOverrideParameter")),
			FString(TEXT("PrevFrameGameTime")),
			FString(TEXT("PrevFrameRealTime")),
			FString(TEXT("OutOfBoundsMask")),
			FString(TEXT("WorldCameraMovementSinceLastFrame")),
			FString(TEXT("CullingSign")),
			FString(TEXT("NearPlane")),
			FString(TEXT("AdaptiveTessellationFactor")),
			FString(TEXT("GameTime")),
			FString(TEXT("RealTime")),
			FString(TEXT("Random")),
			FString(TEXT("FrameNumber")),
			FString(TEXT("CameraCut")),
			FString(TEXT("UseLightmaps")),
			FString(TEXT("UnlitViewmodeMask")),
			FString(TEXT("DirectionalLightColor")),
			FString(TEXT("DirectionalLightDirection")),
			FString(TEXT("DirectionalLightShadowTransition")),
			FString(TEXT("DirectionalLightShadowSize")),
			FString(TEXT("DirectionalLightScreenToShadow")),
			FString(TEXT("DirectionalLightShadowDistances")),
			FString(TEXT("UpperSkyColor")),
			FString(TEXT("LowerSkyColor")),
			FString(TEXT("TranslucencyLightingVolumeMin")),
			FString(TEXT("TranslucencyLightingVolumeInvSize")),
			FString(TEXT("TemporalAAParams")),
			FString(TEXT("CircleDOFParams")),
			FString(TEXT("DepthOfFieldFocalDistance")),
			FString(TEXT("DepthOfFieldScale")),
			FString(TEXT("DepthOfFieldFocalLength")),
			FString(TEXT("DepthOfFieldFocalRegion")),
			FString(TEXT("DepthOfFieldNearTransitionRegion")),
			FString(TEXT("DepthOfFieldFarTransitionRegion")),
			FString(TEXT("MotionBlurNormalizedToPixel")),
			FString(TEXT("GeneralPurposeTweak")),
			FString(TEXT("DemosaicVposOffset")),
			FString(TEXT("IndirectLightingColorScale")),
			FString(TEXT("HDR32bppEncodingMode")),
			FString(TEXT("AtmosphericFogSunDirection")),
			FString(TEXT("AtmosphericFogSunPower")),
			FString(TEXT("AtmosphericFogPower")),
			FString(TEXT("AtmosphericFogDensityScale")),
			FString(TEXT("AtmosphericFogDensityOffset")),
			FString(TEXT("AtmosphericFogGroundOffset")),
			FString(TEXT("AtmosphericFogDistanceScale")),
			FString(TEXT("AtmosphericFogAltitudeScale")),
			FString(TEXT("AtmosphericFogHeightScaleRayleigh")),
			FString(TEXT("AtmosphericFogStartDistance")),
			FString(TEXT("AtmosphericFogDistanceOffset")),
			FString(TEXT("AtmosphericFogSunDiscScale")),
			FString(TEXT("AtmosphericFogRenderMask")),
			FString(TEXT("AtmosphericFogInscatterAltitudeSampleNum")),
			FString(TEXT("AtmosphericFogSunColor")),
			FString(TEXT("AmbientCubemapTint")),
			FString(TEXT("AmbientCubemapIntensity")),
			FString(TEXT("RenderTargetSize")),
			FString(TEXT("SkyLightParameters")),
			FString(TEXT("SceneFString(TEXTureMinMax")),
			FString(TEXT("SkyLightColor")),
			FString(TEXT("SkyIrradianceEnvironmentMap")),
			FString(TEXT("MobilePreviewMode")),
			FString(TEXT("HMDEyePaddingOffset")),
			FString(TEXT("DirectionalLightShadowFString(TEXTure")),
			FString(TEXT("SamplerState")),
		};

		const FString ViewUniformName(TEXT("View."));
		const FString FrameUniformName(TEXT("Frame."));
		for (const FString& Member : UniformMembers)
		{
			const FString SearchString = FrameUniformName + Member;
			const FString ReplaceString = ViewUniformName + Member;
			if (Code.ReplaceInline(*SearchString, *ReplaceString, ESearchCase::CaseSensitive) > 0)
			{
				bDidUpdate = true;
			}
		}
	}

	if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::RemovedRenderTargetSize)
	{
		if (Code.ReplaceInline(TEXT("View.RenderTargetSize"), TEXT("View.BufferSizeAndInvSize.xy"), ESearchCase::CaseSensitive) > 0)
		{
			bDidUpdate = true;
		}
	}

	// If we made changes, copy the original into the description just in case
	if (bDidUpdate)
	{
		Desc += TEXT("\n*** Original source before expression upgrade ***\n");
		Desc += PreFixUp;
		UE_LOG(LogMaterial, Log, TEXT("Uniform references updated for custom material expression %s."), *Description);
	}
}

///////////////////////////////////////////////////////////////////////////////
// UMaterialFunction
///////////////////////////////////////////////////////////////////////////////
UMaterialFunction::UMaterialFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	LibraryCategoriesText.Add(LOCTEXT("Misc", "Misc"));
#endif
#if WITH_EDITORONLY_DATA
	PreviewMaterial = NULL;
	ThumbnailInfo = NULL;
#endif

}

#if WITH_EDITOR
UMaterial* UMaterialFunction::GetPreviewMaterial()
{
	if( NULL == PreviewMaterial )
	{
		PreviewMaterial = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		PreviewMaterial->Expressions = FunctionExpressions;

		//Find the first output expression and use that. 
		for( int32 i=0; i < FunctionExpressions.Num() ; ++i)
		{
			UMaterialExpressionFunctionOutput* Output = Cast<UMaterialExpressionFunctionOutput>(FunctionExpressions[i]);
			if( Output )
			{
				Output->ConnectToPreviewMaterial(PreviewMaterial, 0);
			}
		}

		//Compile the material.
		PreviewMaterial->PreEditChange( NULL );
		PreviewMaterial->PostEditChange();
	}
	return PreviewMaterial;
}

void UMaterialFunction::UpdateInputOutputTypes()
{
	CombinedInputTypes = 0;
	CombinedOutputTypes = 0;

	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionFunctionOutput* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(CurrentExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			CombinedInputTypes |= InputExpression->GetInputType(0);
		}
		else if (OutputExpression)
		{
			CombinedOutputTypes |= OutputExpression->GetOutputType(0);
		}
	}
}
#endif

#if WITH_EDITOR
void UMaterialFunction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_EDITORONLY_DATA
	UpdateInputOutputTypes();
#endif

	//@todo - recreate guid only when needed, not when a comment changes
	StateId = FGuid::NewGuid();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UMaterialFunction::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (FPlatformProperties::RequiresCookedData() && Ar.IsLoading())
	{
		FunctionExpressions.Remove(nullptr);
	}

#if WITH_EDITOR
	if (Ar.UE4Ver() < VER_UE4_FLIP_MATERIAL_COORDS)
	{
		GMaterialFunctionsThatNeedExpressionsFlipped.Set(this);
	}
	else if (Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_COORDS)
	{
		GMaterialFunctionsThatNeedCoordinateCheck.Set(this);
	}
	else if (Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_COMMENTS)
	{
		GMaterialFunctionsThatNeedCommentFix.Set(this);
	}

	if (Ar.UE4Ver() < VER_UE4_ADD_LINEAR_COLOR_SAMPLER)
	{
		GMaterialFunctionsThatNeedSamplerFixup.Set(this);
	}

	if (Ar.UE4Ver() < VER_UE4_LIBRARY_CATEGORIES_AS_FTEXT)
	{
		for (FString& Category : LibraryCategories_DEPRECATED)
		{
			LibraryCategoriesText.Add(FText::FromString(Category));
		}
	}
#endif // #if WITH_EDITOR
}

void UMaterialFunction::PostLoad()
{
	LLM_SCOPE(ELLMTag::Materials);

	Super::PostLoad();
	
	if (!StateId.IsValid())
	{
		StateId = FGuid::NewGuid();
	}

	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		// Expressions whose type was removed can be NULL
		if (FunctionExpressions[ExpressionIndex])
		{
			FunctionExpressions[ExpressionIndex]->ConditionalPostLoad();
		}
	}

#if WITH_EDITOR
	if (CombinedOutputTypes == 0)
	{
		UpdateInputOutputTypes();
	}

	if (GIsEditor)
	{
		// Clean up any removed material expression classes	
		if (FunctionExpressions.Remove(NULL) != 0)
		{
			// Force this function to recompile because its expressions have changed
			// Warning: any content taking this path will recompile every load until saved!
			// Which means removing an expression class will cause the need for a resave of all materials affected
			StateId = FGuid::NewGuid();
		}
	}

	if (GMaterialFunctionsThatNeedExpressionsFlipped.Get(this))
	{
		GMaterialFunctionsThatNeedExpressionsFlipped.Clear(this);
		UMaterial::FlipExpressionPositions(FunctionExpressions, FunctionEditorComments, true);
	}
	else if (GMaterialFunctionsThatNeedCoordinateCheck.Get(this))
	{
		GMaterialFunctionsThatNeedCoordinateCheck.Clear(this);
		if (HasFlippedCoordinates())
		{
			UMaterial::FlipExpressionPositions(FunctionExpressions, FunctionEditorComments, false);
		}
		UMaterial::FixCommentPositions(FunctionEditorComments);
	}
	else if (GMaterialFunctionsThatNeedCommentFix.Get(this))
	{
		GMaterialFunctionsThatNeedCommentFix.Clear(this);
		UMaterial::FixCommentPositions(FunctionEditorComments);
	}

	if (GMaterialFunctionsThatNeedSamplerFixup.Get(this))
	{
		GMaterialFunctionsThatNeedSamplerFixup.Clear(this);
		const int32 ExpressionCount = FunctionExpressions.Num();
		for (int32 ExpressionIndex = 0; ExpressionIndex < ExpressionCount; ++ExpressionIndex)
		{
			UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(FunctionExpressions[ExpressionIndex]);
			if (TextureExpression && TextureExpression->Texture)
			{
				switch (TextureExpression->Texture->CompressionSettings)
				{
				case TC_Normalmap:
					TextureExpression->SamplerType = SAMPLERTYPE_Normal;
					break;

				case TC_Grayscale:
					TextureExpression->SamplerType = TextureExpression->Texture->SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
					break;

				case TC_Masks:
					TextureExpression->SamplerType = SAMPLERTYPE_Masks;
					break;

				case TC_Alpha:
					TextureExpression->SamplerType = SAMPLERTYPE_Alpha;
					break;
				default:
					TextureExpression->SamplerType = TextureExpression->Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
					break;
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}

void UMaterialFunction::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

#if WITH_EDITORONLY_DATA
	for (FAssetRegistryTag& AssetTag : OutTags)
	{
		// Hide the combined input/output types as they are only needed in code
		if (AssetTag.Name == GET_MEMBER_NAME_CHECKED(UMaterialFunction, CombinedInputTypes)
		|| AssetTag.Name == GET_MEMBER_NAME_CHECKED(UMaterialFunction, CombinedOutputTypes))
		{
			AssetTag.Type = UObject::FAssetRegistryTag::TT_Hidden;
		}
	}
#endif
}

void UMaterialFunction::UpdateFromFunctionResource()
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(CurrentExpression);
		if (MaterialFunctionExpression)
		{
			MaterialFunctionExpression->UpdateFromFunctionResource();
		}
	}
}

void UMaterialFunction::GetInputsAndOutputs(TArray<FFunctionExpressionInput>& OutInputs, TArray<FFunctionExpressionOutput>& OutOutputs) const
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionFunctionOutput* OutputExpression = Cast<UMaterialExpressionFunctionOutput>(CurrentExpression);
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			// Create an input
			FFunctionExpressionInput NewInput;
			NewInput.ExpressionInput = InputExpression;
			NewInput.ExpressionInputId = InputExpression->Id;
			NewInput.Input.InputName = InputExpression->InputName;
			NewInput.Input.OutputIndex = INDEX_NONE;
			OutInputs.Add(NewInput);
		}
		else if (OutputExpression)
		{
			// Create an output
			FFunctionExpressionOutput NewOutput;
			NewOutput.ExpressionOutput = OutputExpression;
			NewOutput.ExpressionOutputId = OutputExpression->Id;
			NewOutput.Output.OutputName = OutputExpression->OutputName;
			OutOutputs.Add(NewOutput);
		}
	}

	// Sort by display priority
	struct FCompareInputSortPriority
	{
		FORCEINLINE bool operator()( const FFunctionExpressionInput& A, const FFunctionExpressionInput& B ) const 
		{ 
			return A.ExpressionInput->SortPriority < B.ExpressionInput->SortPriority; 
		}
	};
	OutInputs.Sort( FCompareInputSortPriority() );

	struct FCompareOutputSortPriority
	{
		FORCEINLINE bool operator()( const FFunctionExpressionOutput& A, const FFunctionExpressionOutput& B ) const 
		{ 
			return A.ExpressionOutput->SortPriority < B.ExpressionOutput->SortPriority; 
		}
	};
	OutOutputs.Sort( FCompareOutputSortPriority() );
}

/** Finds an input in the passed in array with a matching Id. */
static const FFunctionExpressionInput* FindInputById(const FGuid& Id, const TArray<FFunctionExpressionInput>& Inputs)
{
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); InputIndex++)
	{
		const FFunctionExpressionInput& CurrentInput = Inputs[InputIndex];
		if (CurrentInput.ExpressionInputId == Id)
		{
			return &CurrentInput;
		}
	}
	return NULL;
}

/** Finds an input in the passed in array with a matching name. */
static const FFunctionExpressionInput* FindInputByName(const FString& Name, const TArray<FFunctionExpressionInput>& Inputs)
{
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); InputIndex++)
	{
		const FFunctionExpressionInput& CurrentInput = Inputs[InputIndex];
		if (CurrentInput.ExpressionInput->InputName == Name)
		{
			return &CurrentInput;
		}
	}
	return NULL;
}

/** Finds an input in the passed in array with a matching expression object. */
static const FExpressionInput* FindInputByExpression(UMaterialExpressionFunctionInput* InputExpression, const TArray<FFunctionExpressionInput>& Inputs)
{
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); InputIndex++)
	{
		const FFunctionExpressionInput& CurrentInput = Inputs[InputIndex];
		if (CurrentInput.ExpressionInput == InputExpression)
		{
			return &CurrentInput.Input;
		}
	}
	return NULL;
}

/** Finds an output in the passed in array with a matching Id. */
static int32 FindOutputIndexById(const FGuid& Id, const TArray<FFunctionExpressionOutput>& Outputs)
{
	for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); OutputIndex++)
	{
		const FFunctionExpressionOutput& CurrentOutput = Outputs[OutputIndex];
		if (CurrentOutput.ExpressionOutputId == Id)
		{
			return OutputIndex;
		}
	}
	return INDEX_NONE;
}

/** Finds an output in the passed in array with a matching name. */
static int32 FindOutputIndexByName(const FString& Name, const TArray<FFunctionExpressionOutput>& Outputs)
{
	for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); OutputIndex++)
	{
		const FFunctionExpressionOutput& CurrentOutput = Outputs[OutputIndex];
		if (CurrentOutput.ExpressionOutput->OutputName == Name)
		{
			return OutputIndex;
		}
	}
	return INDEX_NONE;
}

#if WITH_EDITOR
int32 UMaterialFunction::Compile(class FMaterialCompiler* Compiler, const FFunctionExpressionOutput& Output)
{
	int32 ReturnValue = INDEX_NONE;
	if (Output.ExpressionOutput->A.GetTracedInput().Expression)
	{
		// Compile the given function output
		ReturnValue = Output.ExpressionOutput->A.Compile(Compiler);
	}
	else
	{
		ReturnValue = Compiler->Errorf(TEXT("Missing function output connection '%s'"), *Output.ExpressionOutput->OutputName);
	}

	return ReturnValue;
}

void UMaterialFunction::LinkIntoCaller(const TArray<FFunctionExpressionInput>& CallerInputs)
{
	// Go through all the function's input expressions and hook their inputs up to the corresponding expression in the material being compiled.
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			// Mark that we are compiling the function as used in a material
			InputExpression->bCompilingFunctionPreview = false;
			// Initialize for this function call
			InputExpression->EffectivePreviewDuringCompile = InputExpression->Preview;

			// Get the FExpressionInput which stores information about who this input node should be linked to in order to compile
			const FExpressionInput* MatchingInput = FindInputByExpression(InputExpression, CallerInputs);

			if (MatchingInput 
				// Only change the connection if the input has a valid connection,
				// Otherwise we will need what's connected to the Preview input if bCompilingFunctionPreview is true
				&& (MatchingInput->Expression || !InputExpression->bUsePreviewValueAsDefault))
			{
				// Connect this input to the expression in the material that it should be connected to
				InputExpression->EffectivePreviewDuringCompile.Expression = MatchingInput->Expression;
				InputExpression->EffectivePreviewDuringCompile.OutputIndex = MatchingInput->OutputIndex;
				InputExpression->EffectivePreviewDuringCompile.Mask = MatchingInput->Mask;
				InputExpression->EffectivePreviewDuringCompile.MaskR = MatchingInput->MaskR;
				InputExpression->EffectivePreviewDuringCompile.MaskG = MatchingInput->MaskG;
				InputExpression->EffectivePreviewDuringCompile.MaskB = MatchingInput->MaskB;
				InputExpression->EffectivePreviewDuringCompile.MaskA = MatchingInput->MaskA;		
			}
		}
	}
}

void UMaterialFunction::UnlinkFromCaller()
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionFunctionInput* InputExpression = Cast<UMaterialExpressionFunctionInput>(CurrentExpression);

		if (InputExpression)
		{
			// Restore the default value
			InputExpression->bCompilingFunctionPreview = true;
			// Clear the reference to make stale accesses obvious
			InputExpression->EffectivePreviewDuringCompile.Expression = NULL;
		}
	}
}

#endif // WITH_EDITOR

bool UMaterialFunction::IsDependent(UMaterialFunction* OtherFunction)
{
	if (!OtherFunction)
	{
		return false;
	}

	if (OtherFunction == this 
#if WITH_EDITORONLY_DATA
		|| OtherFunction->ParentFunction == this
#endif
		)
	{
		return true;
	}

	bReentrantFlag = true;

	bool bIsDependent = false;
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(CurrentExpression);
		if (MaterialFunctionExpression 
			&& MaterialFunctionExpression->MaterialFunction)
		{
			bIsDependent = bIsDependent 
				|| MaterialFunctionExpression->MaterialFunction->bReentrantFlag
				// Recurse to handle nesting
				|| MaterialFunctionExpression->MaterialFunction->IsDependent(OtherFunction);
		}
	}

	bReentrantFlag = false;

	return bIsDependent;
}

void UMaterialFunction::GetDependentFunctions(TArray<UMaterialFunction*>& DependentFunctions) const
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionExpression = Cast<UMaterialExpressionMaterialFunctionCall>(CurrentExpression);
		if (MaterialFunctionExpression 
			&& MaterialFunctionExpression->MaterialFunction)
		{
			// Recurse to handle nesting
			MaterialFunctionExpression->MaterialFunction->GetDependentFunctions(DependentFunctions);
			DependentFunctions.AddUnique(MaterialFunctionExpression->MaterialFunction);
		}
	}
}

void UMaterialFunction::AppendReferencedTextures(TArray<UTexture*>& InOutTextures) const
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < FunctionExpressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = FunctionExpressions[ExpressionIndex];
		if(CurrentExpression)
		{
			UTexture* ReferencedTexture = CurrentExpression->GetReferencedTexture();

			if (ReferencedTexture)
			{
				InOutTextures.AddUnique(ReferencedTexture);
			}
		}
	}
}

#if WITH_EDITOR
bool UMaterialFunction::HasFlippedCoordinates() const
{
	uint32 ReversedInputCount = 0;
	uint32 StandardInputCount = 0;

	for (int32 Index = 0; Index < FunctionExpressions.Num(); ++Index)
	{
		UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(FunctionExpressions[Index]);
		if (FunctionOutput && FunctionOutput->A.Expression)
		{
			if (FunctionOutput->A.Expression->MaterialExpressionEditorX > FunctionOutput->MaterialExpressionEditorX)
			{
				++ReversedInputCount;
			}
			else
			{
				++StandardInputCount;
			}
		}
	}

	// Can't be sure coords are flipped if most are set out correctly
	return ReversedInputCount > StandardInputCount;
}
#endif //WITH_EDITORONLY_DATA


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionMaterialFunctionCall
///////////////////////////////////////////////////////////////////////////////

UMaterialFunction* SavedMaterialFunction = NULL;

UMaterialExpressionMaterialFunctionCall::UMaterialExpressionMaterialFunctionCall(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputNameOnPin = true;
	bHidePreviewWindow = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif

	BorderColor = FColor(0, 116, 255);

}

void UMaterialExpressionMaterialFunctionCall::PostLoad()
{
	if (MaterialFunction)
	{
		MaterialFunction->ConditionalPostLoad();
	}

	Super::PostLoad();
}

bool UMaterialExpressionMaterialFunctionCall::NeedsLoadForClient() const
{
	return true;
}

#if WITH_EDITOR
void UMaterialExpressionMaterialFunctionCall::PreEditChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == FName(TEXT("MaterialFunction")))
	{
		// Save off the previous MaterialFunction value
		SavedMaterialFunction = MaterialFunction;
	}
	Super::PreEditChange(PropertyAboutToChange);
}

void UMaterialExpressionMaterialFunctionCall::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("MaterialFunction")))
	{
		// Set the new material function
		SetMaterialFunctionEx(SavedMaterialFunction, MaterialFunction);
		SavedMaterialFunction = NULL;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

int32 UMaterialExpressionMaterialFunctionCall::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!MaterialFunction)
	{
		return Compiler->Errorf(TEXT("Missing Material Function"));
	}

	// Verify that all function inputs and outputs are in a valid state to be linked into this material for compiling
	for (int32 i = 0; i < FunctionInputs.Num(); i++)
	{
		check(FunctionInputs[i].ExpressionInput);
	}

	for (int32 i = 0; i < FunctionOutputs.Num(); i++)
	{
		check(FunctionOutputs[i].ExpressionOutput);
	}

	if (!FunctionOutputs.IsValidIndex(OutputIndex))
	{
		return Compiler->Errorf(TEXT("Invalid function output"));
	}

	// Link the function's inputs into the caller graph before entering
	MaterialFunction->LinkIntoCaller(FunctionInputs);

	// Tell the compiler that we are entering a function
	Compiler->PushFunction(FMaterialFunctionCompileState(this));

	// Compile the requested output
	const int32 ReturnValue = MaterialFunction->Compile(Compiler, FunctionOutputs[OutputIndex]);

	// Tell the compiler that we are leaving a function
	const FMaterialFunctionCompileState CompileState = Compiler->PopFunction();
	check(CompileState.ExpressionStack.Num() == 0);

	// Restore the function since we are leaving it
	MaterialFunction->UnlinkFromCaller();

	return ReturnValue;
}

void UMaterialExpressionMaterialFunctionCall::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(MaterialFunction ? MaterialFunction->GetName() : TEXT("Unspecified Function"));
}
#endif // WITH_EDITOR

const TArray<FExpressionInput*> UMaterialExpressionMaterialFunctionCall::GetInputs()
{
	TArray<FExpressionInput*> Result;
	for (int32 i = 0; i < FunctionInputs.Num(); i++)
	{
		Result.Add(&FunctionInputs[i].Input);
	}
	return Result;
}

FExpressionInput* UMaterialExpressionMaterialFunctionCall::GetInput(int32 InputIndex)
{
	if (InputIndex < FunctionInputs.Num())
	{
		return &FunctionInputs[InputIndex].Input;
	}
	return NULL;
}


static const TCHAR* GetInputTypeName(uint8 InputType)
{
	const static TCHAR* TypeNames[FunctionInput_MAX] =
	{
		TEXT("S"),
		TEXT("V2"),
		TEXT("V3"),
		TEXT("V4"),
		TEXT("T2d"),
		TEXT("TCube"),
		TEXT("B"),
		TEXT("MA")
	};

	check(InputType < FunctionInput_MAX);
	return TypeNames[InputType];
}

FString UMaterialExpressionMaterialFunctionCall::GetInputNameWithType(int32 InputIndex, bool bWithType) const
{
	if (InputIndex < FunctionInputs.Num())
	{
		if (FunctionInputs[InputIndex].ExpressionInput != NULL && bWithType)
		{
			return FunctionInputs[InputIndex].Input.InputName + TEXT(" (") + GetInputTypeName(FunctionInputs[InputIndex].ExpressionInput->InputType) + TEXT(")");
		}
		else
		{
			return FunctionInputs[InputIndex].Input.InputName;
		}
	}
	return TEXT("");
}

FString UMaterialExpressionMaterialFunctionCall::GetInputName(int32 InputIndex) const
{
	return GetInputNameWithType(InputIndex, true);
}

bool UMaterialExpressionMaterialFunctionCall::IsInputConnectionRequired(int32 InputIndex) const
{
	if (InputIndex < FunctionInputs.Num() && FunctionInputs[InputIndex].ExpressionInput != NULL)
	{
		return !FunctionInputs[InputIndex].ExpressionInput->bUsePreviewValueAsDefault;
	}
	return true;
}

static FString GetInputDefaultValueString(EFunctionInputType InputType, const FVector4& PreviewValue)
{
	static_assert(FunctionInput_Scalar < FunctionInput_Vector4, "Enum values out of order.");
	check(InputType <= FunctionInput_Vector4);

	FString ValueString = FString::Printf(TEXT("DefaultValue = (%.2f"), PreviewValue.X);
	
	if (InputType >= FunctionInput_Vector2)
	{
		ValueString += FString::Printf(TEXT(", %.2f"), PreviewValue.Y);
	}

	if (InputType >= FunctionInput_Vector3)
	{
		ValueString += FString::Printf(TEXT(", %.2f"), PreviewValue.Z);
	}

	if (InputType >= FunctionInput_Vector4)
	{
		ValueString += FString::Printf(TEXT(", %.2f"), PreviewValue.W);
	}

	return ValueString + TEXT(")");
}

#if WITH_EDITOR
FString UMaterialExpressionMaterialFunctionCall::GetDescription() const
{
	FString Result = FString(*GetClass()->GetName()).Mid(FCString::Strlen(TEXT("MaterialExpression")));
	Result += TEXT(" (");
	Result += Super::GetDescription();
	Result += TEXT(")");
	return Result;
}

void UMaterialExpressionMaterialFunctionCall::GetConnectorToolTip(int32 InputIndex, int32 OutputIndex, TArray<FString>& OutToolTip) 
{
	if (MaterialFunction)
	{
		if (InputIndex != INDEX_NONE)
		{
			if (FunctionInputs.IsValidIndex(InputIndex))
			{
				UMaterialExpressionFunctionInput* InputExpression = FunctionInputs[InputIndex].ExpressionInput;

				ConvertToMultilineToolTip(InputExpression->Description, 40, OutToolTip);

				if (InputExpression->bUsePreviewValueAsDefault)
				{
					// Can't build a tooltip of an arbitrary expression chain
					if (InputExpression->Preview.Expression)
					{
						OutToolTip.Insert(FString(TEXT("DefaultValue = Custom expressions")), 0);

						// Add a line after the default value string
						OutToolTip.Insert(FString(TEXT("")), 1);
					}
					else if (InputExpression->InputType <= FunctionInput_Vector4)
					{
						// Add a string for the default value at the top
						OutToolTip.Insert(GetInputDefaultValueString((EFunctionInputType)InputExpression->InputType, InputExpression->PreviewValue), 0);

						// Add a line after the default value string
						OutToolTip.Insert(FString(TEXT("")), 1);
					}
				}
			}
		}
		else if (OutputIndex != INDEX_NONE)
		{
			if (FunctionOutputs.IsValidIndex(OutputIndex))
			{
				ConvertToMultilineToolTip(FunctionOutputs[OutputIndex].ExpressionOutput->Description, 40, OutToolTip);
			}
		}
	}
}

void UMaterialExpressionMaterialFunctionCall::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	if (MaterialFunction)
	{
		ConvertToMultilineToolTip(MaterialFunction->Description, 40, OutToolTip);
	}
}

bool UMaterialExpressionMaterialFunctionCall::SetMaterialFunction(UMaterialFunction* NewMaterialFunction)
{
	// Remember the current material function
	UMaterialFunction* OldFunction = MaterialFunction;

	return SetMaterialFunctionEx(OldFunction, NewMaterialFunction);
}


bool UMaterialExpressionMaterialFunctionCall::SetMaterialFunctionEx(
	UMaterialFunction* OldFunctionResource, 
	UMaterialFunction* NewFunctionResource)
{
	// See if Outer is another material function
	UMaterialFunction* ThisFunctionResource = Cast<UMaterialFunction>(GetOuter());


	if (NewFunctionResource 
		&& ThisFunctionResource
		&& NewFunctionResource->IsDependent(ThisFunctionResource))
	{
		// Prevent recursive function call graphs
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("MaterialExpressions", "Error_CircularFunctionDependency", "Can't use that material function as it would cause a circular dependency.") );
		NewFunctionResource = NULL;
	}

	MaterialFunction = NewFunctionResource;

	// Store the original inputs and outputs
	TArray<FFunctionExpressionInput> OriginalInputs = FunctionInputs;
	TArray<FFunctionExpressionOutput> OriginalOutputs = FunctionOutputs;

	FunctionInputs.Empty();
	FunctionOutputs.Empty();
	Outputs.Empty();

	if (NewFunctionResource)
	{
		// Get the current inputs and outputs
		NewFunctionResource->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);

		for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); InputIndex++)
		{
			FFunctionExpressionInput& CurrentInput = FunctionInputs[InputIndex];
			check(CurrentInput.ExpressionInput);
			const FFunctionExpressionInput* OriginalInput = FindInputByName(CurrentInput.ExpressionInput->InputName, OriginalInputs);

			if (OriginalInput)
			{
				// If there is an input whose name matches the original input, even if they are from different functions, maintain the connection
				CurrentInput.Input = OriginalInput->Input;
			}
		}

		for (int32 OutputIndex = 0; OutputIndex < FunctionOutputs.Num(); OutputIndex++)
		{
			Outputs.Add(FunctionOutputs[OutputIndex].Output);
		}
	}

	// Fixup even if NewFunctionResource is NULL, because we have to clear old connections
	if (OldFunctionResource && OldFunctionResource != NewFunctionResource)
	{
		TArray<FExpressionInput*> MaterialInputs;
		if (Material)
		{
			MaterialInputs.Empty(MP_MAX);
			for (int32 InputIndex = 0; InputIndex < MP_MAX; InputIndex++)
			{
				auto Input = Material->GetExpressionInputForProperty((EMaterialProperty)InputIndex);

				if(Input)
				{
					MaterialInputs.Add(Input);
				}
			}

			// Fixup any references that the material or material inputs had to the function's outputs, maintaining links with the same output name
			FixupReferencingExpressions(FunctionOutputs, OriginalOutputs, Material->Expressions, MaterialInputs, true);
		}
		else if (Function)
		{
			// Fixup any references that the material function had to the function's outputs, maintaining links with the same output name
			FixupReferencingExpressions(FunctionOutputs, OriginalOutputs, Function->FunctionExpressions, MaterialInputs, true);
		}
	}

	if (GraphNode)
	{
		// Recreate the pins of this node after material function set
		CastChecked<UMaterialGraphNode>(GraphNode)->RecreateAndLinkNode();
	}

	return NewFunctionResource != NULL;
}
#endif // WITH_EDITOR

void UMaterialExpressionMaterialFunctionCall::UpdateFromFunctionResource(bool bRecreateAndLinkNode)
{
	TArray<FFunctionExpressionInput> OriginalInputs = FunctionInputs;
	TArray<FFunctionExpressionOutput> OriginalOutputs = FunctionOutputs;

	FunctionInputs.Empty();
	FunctionOutputs.Empty();
	Outputs.Empty();

	if (MaterialFunction)
	{
		// Recursively update any function call nodes in the function
		MaterialFunction->UpdateFromFunctionResource();

		// Get the function's current inputs and outputs
		MaterialFunction->GetInputsAndOutputs(FunctionInputs, FunctionOutputs);

		for (int32 InputIndex = 0; InputIndex < FunctionInputs.Num(); InputIndex++)
		{
			FFunctionExpressionInput& CurrentInput = FunctionInputs[InputIndex];
			check(CurrentInput.ExpressionInput);
			const FFunctionExpressionInput* OriginalInput = FindInputById(CurrentInput.ExpressionInputId, OriginalInputs);

			if (OriginalInput)
			{
				// Maintain the input connection if an input with matching Id is found, but propagate the new name
				// This way function inputs names can be changed without affecting material connections
				const FString TempInputName = CurrentInput.Input.InputName;
				CurrentInput.Input = OriginalInput->Input;
				CurrentInput.Input.InputName = TempInputName;
			}
		}

		for (int32 OutputIndex = 0; OutputIndex < FunctionOutputs.Num(); OutputIndex++)
		{
			Outputs.Add(FunctionOutputs[OutputIndex].Output);
		}

		TArray<FExpressionInput*> MaterialInputs;
		if (Material)
		{
			MaterialInputs.Empty(MP_MAX);
			for (int32 InputIndex = 0; InputIndex < MP_MAX; InputIndex++)
			{
				auto Input = Material->GetExpressionInputForProperty((EMaterialProperty)InputIndex);

				if(Input)
				{
					MaterialInputs.Add(Input);
				}
			}
			
#if WITH_EDITOR
			// Fixup any references that the material or material inputs had to the function's outputs
			FixupReferencingExpressions(FunctionOutputs, OriginalOutputs, Material->Expressions, MaterialInputs, false);
		}
		else if (Function)
		{
			// Fixup any references that the material function had to the function's outputs
			FixupReferencingExpressions(FunctionOutputs, OriginalOutputs, Function->FunctionExpressions, MaterialInputs, false);
#endif // WITH_EDITOR
		}
	}

#if WITH_EDITOR
	if (GraphNode && bRecreateAndLinkNode)
	{
		// Check whether number of input/outputs or transient pointers have changed
		bool bUpdatedFromFunction = false;
		if (OriginalInputs.Num() != FunctionInputs.Num()
			|| OriginalOutputs.Num() != FunctionOutputs.Num())
		{
			bUpdatedFromFunction = true;
		}
		for (int32 Index = 0; Index < OriginalInputs.Num() && !bUpdatedFromFunction; ++Index)
		{
			if (OriginalInputs[Index].ExpressionInput != FunctionInputs[Index].ExpressionInput)
			{
				bUpdatedFromFunction = true;
			}
		}
		for (int32 Index = 0; Index < OriginalOutputs.Num() && !bUpdatedFromFunction; ++Index)
		{
			if (OriginalOutputs[Index].ExpressionOutput != FunctionOutputs[Index].ExpressionOutput)
			{
				bUpdatedFromFunction = true;
			}
		}
		if (bUpdatedFromFunction)
		{
			// Recreate the pins of this node after Expression links are made
			CastChecked<UMaterialGraphNode>(GraphNode)->RecreateAndLinkNode();
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
/** Goes through the Inputs array and fixes up each input's OutputIndex, or breaks the connection if necessary. */
static void FixupReferencingInputs(
	const TArray<FFunctionExpressionOutput>& NewOutputs,
	const TArray<FFunctionExpressionOutput>& OriginalOutputs,
	const TArray<FExpressionInput*>& Inputs, 
	UMaterialExpressionMaterialFunctionCall* FunctionExpression,
	bool bMatchByName)
{
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); InputIndex++)
	{
		FExpressionInput* CurrentInput = Inputs[InputIndex];

		if (CurrentInput->Expression == FunctionExpression)
		{
			if (OriginalOutputs.IsValidIndex(CurrentInput->OutputIndex))
			{
				if (bMatchByName)
				{
					CurrentInput->OutputIndex = FindOutputIndexByName(OriginalOutputs[CurrentInput->OutputIndex].ExpressionOutput->OutputName, NewOutputs);
				}
				else
				{
					const FGuid OutputId = OriginalOutputs[CurrentInput->OutputIndex].ExpressionOutputId;
					CurrentInput->OutputIndex = FindOutputIndexById(OutputId, NewOutputs);
				}

				if (CurrentInput->OutputIndex == INDEX_NONE)
				{
					// The output that this input was connected to no longer exists, break the connection
					CurrentInput->Expression = NULL;
				}
			}
			else
			{
				// The output that this input was connected to no longer exists, break the connection
				CurrentInput->OutputIndex = INDEX_NONE;
				CurrentInput->Expression = NULL;
			}
		}
	}
}


void UMaterialExpressionMaterialFunctionCall::FixupReferencingExpressions(
	const TArray<FFunctionExpressionOutput>& NewOutputs,
	const TArray<FFunctionExpressionOutput>& OriginalOutputs,
	TArray<UMaterialExpression*>& Expressions, 
	TArray<FExpressionInput*>& MaterialInputs,
	bool bMatchByName)
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* CurrentExpression = Expressions[ExpressionIndex];
		if (CurrentExpression)
		{
			TArray<FExpressionInput*> Inputs = CurrentExpression->GetInputs();
			FixupReferencingInputs(NewOutputs, OriginalOutputs, Inputs, this, bMatchByName);
		}
	}

	FixupReferencingInputs(NewOutputs, OriginalOutputs, MaterialInputs, this, bMatchByName);
}
#endif // WITH_EDITOR

bool UMaterialExpressionMaterialFunctionCall::MatchesSearchQuery( const TCHAR* SearchQuery )
{
	if (MaterialFunction && MaterialFunction->GetName().Contains(SearchQuery) )
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

#if WITH_EDITOR
bool UMaterialExpressionMaterialFunctionCall::IsResultMaterialAttributes(int32 OutputIndex)
{
	if( OutputIndex >= 0 && OutputIndex < FunctionOutputs.Num() && FunctionOutputs[OutputIndex].ExpressionOutput)
	{
		return FunctionOutputs[OutputIndex].ExpressionOutput->IsResultMaterialAttributes(0);
	}
	else
	{
		return false;
	}
}

uint32 UMaterialExpressionMaterialFunctionCall::GetInputType(int32 InputIndex)
{
	if (InputIndex < FunctionInputs.Num())
	{
		if (FunctionInputs[InputIndex].ExpressionInput)
		{
			return FunctionInputs[InputIndex].ExpressionInput->GetInputType(0);
		}
	}
	return MCT_Unknown;
}
#endif // WITH_EDITOR


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionFunctionInput
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionFunctionInput::UMaterialExpressionFunctionInput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bCompilingFunctionPreview = true;
	InputType = FunctionInput_Vector3;
	InputName = TEXT("In");
	bCollapsed = false;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif

	BorderColor = FColor(185, 255, 172);

}

void UMaterialExpressionFunctionInput::PostLoad()
{
	Super::PostLoad();
	ConditionallyGenerateId(false);
}

void UMaterialExpressionFunctionInput::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	ConditionallyGenerateId(false);
}

#if WITH_EDITOR

void UMaterialExpressionFunctionInput::PostEditImport()
{
	Super::PostEditImport();
	ConditionallyGenerateId(true);
}

FString InputNameBackup;

void UMaterialExpressionFunctionInput::PreEditChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == FName(TEXT("InputName")))
	{
		InputNameBackup = InputName;
	}
	Super::PreEditChange(PropertyAboutToChange);
}

void UMaterialExpressionFunctionInput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("InputName")))
	{
		if (Material)
		{
			for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
			{
				UMaterialExpressionFunctionInput* OtherFunctionInput = Cast<UMaterialExpressionFunctionInput>(Material->Expressions[ExpressionIndex]);
				if (OtherFunctionInput && OtherFunctionInput != this && OtherFunctionInput->InputName == InputName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_InputNamesMustBeUnique", "Function input names must be unique"));
					InputName = InputNameBackup;
					break;
				}
			}
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FEditorSupportDelegates::ForcePropertyWindowRebuild.Broadcast(this);
}

void UMaterialExpressionFunctionInput::GetCaption(TArray<FString>& OutCaptions) const
{
	const static TCHAR* TypeNames[FunctionInput_MAX] =
	{
		TEXT("Scalar"),
		TEXT("Vector2"),
		TEXT("Vector3"),
		TEXT("Vector4"),
		TEXT("Texture2D"),
		TEXT("TextureCube"),
		TEXT("StaticBool"),
		TEXT("MaterialAttributes")
	};
	check(InputType < FunctionInput_MAX);
	OutCaptions.Add(FString(TEXT("Input ")) + InputName + TEXT(" (") + TypeNames[InputType] + TEXT(")"));
}

void UMaterialExpressionFunctionInput::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(Description, 40, OutToolTip);
}

int32 UMaterialExpressionFunctionInput::CompilePreviewValue(FMaterialCompiler* Compiler)
{
	if (Preview.GetTracedInput().Expression)
	{
		return Preview.Compile(Compiler);
	}
	else
	{
		// Compile PreviewValue if Preview was not connected
		switch (InputType)
		{
		case FunctionInput_Scalar:
			return Compiler->Constant(PreviewValue.X);
		case FunctionInput_Vector2:
			return Compiler->Constant2(PreviewValue.X, PreviewValue.Y);
		case FunctionInput_Vector3:
			return Compiler->Constant3(PreviewValue.X, PreviewValue.Y, PreviewValue.Z);
		case FunctionInput_Vector4:
			return Compiler->Constant4(PreviewValue.X, PreviewValue.Y, PreviewValue.Z, PreviewValue.W);
		case FunctionInput_Texture2D:
		case FunctionInput_TextureCube:
		case FunctionInput_StaticBool:
		case FunctionInput_MaterialAttributes:
			return Compiler->Errorf(TEXT("Missing Preview connection for function input '%s'"), *InputName);
		default:
			return Compiler->Errorf(TEXT("Unknown input type"));
		}
	}
}

int32 UMaterialExpressionFunctionInput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const static EMaterialValueType FunctionTypeMapping[FunctionInput_MAX] =
	{
		MCT_Float1,
		MCT_Float2,
		MCT_Float3,
		MCT_Float4,
		MCT_Texture2D,
		MCT_TextureCube,
		MCT_StaticBool,
		MCT_MaterialAttributes
	};
	check(InputType < FunctionInput_MAX);

	// If we are being compiled as part of a material which calls this function
	FExpressionInput EffectivePreviewDuringCompileTracedInput = EffectivePreviewDuringCompile.GetTracedInput();
	if (EffectivePreviewDuringCompileTracedInput.Expression && !bCompilingFunctionPreview)
	{
		int32 ExpressionResult;

		// Stay in this function if we are compiling an expression that is in the current function
		// This can happen if bUsePreviewValueAsDefault is true and the calling material didn't override the input
		if (bUsePreviewValueAsDefault && EffectivePreviewDuringCompileTracedInput.Expression->GetOuter() == GetOuter())
		{
			// Compile the function input
			ExpressionResult = EffectivePreviewDuringCompile.Compile(Compiler);
		}
		else
		{
			// Tell the compiler that we are leaving the function
			const FMaterialFunctionCompileState FunctionState = Compiler->PopFunction();

			// Backup EffectivePreviewDuringCompile which will be modified by UnlinkFromCaller and LinkIntoCaller of any potential chained function calls to the same function
			FExpressionInput LocalPreviewDuringCompile = EffectivePreviewDuringCompile;

			// Restore the function since we are leaving it
			FunctionState.FunctionCall->MaterialFunction->UnlinkFromCaller();

			// Compile the function input
			ExpressionResult = LocalPreviewDuringCompile.Compile(Compiler);

			// Link the function's inputs into the caller graph before entering
			FunctionState.FunctionCall->MaterialFunction->LinkIntoCaller(FunctionState.FunctionCall->FunctionInputs);

			// Tell the compiler that we are re-entering the function
			Compiler->PushFunction(FunctionState);
		}

		// Cast to the type that the function author specified
		// This will truncate (float4 -> float3) but not add components (float2 -> float3)
		return Compiler->ValidCast(ExpressionResult, FunctionTypeMapping[InputType]);
	}
	else
	{
		if (bCompilingFunctionPreview || bUsePreviewValueAsDefault)
		{
			// If we are compiling the function in a preview material, such as when editing the function,
			// Compile the preview value or texture and output a texture object.
			return Compiler->ValidCast(CompilePreviewValue(Compiler), FunctionTypeMapping[InputType]);
		}
		else
		{
			return Compiler->Errorf(TEXT("Missing function input '%s'"), *InputName);
		}
	}
}

int32 UMaterialExpressionFunctionInput::CompilePreview(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Compile the preview value, outputting a float type
	return Compiler->ValidCast(CompilePreviewValue(Compiler), MCT_Float3);
}
#endif // WITH_EDITOR

void UMaterialExpressionFunctionInput::ConditionallyGenerateId(bool bForce)
{
	if (bForce || !Id.IsValid())
	{
		Id = FGuid::NewGuid();
	}
}

void UMaterialExpressionFunctionInput::ValidateName()
{
	if (Material)
	{
		int32 InputNameIndex = 0;
		bool bResultNameIndexValid = true;
		FString PotentialInputName;

		// Find an available unique name
		do 
		{
			PotentialInputName = InputName;
			if (InputNameIndex != 0)
			{
				PotentialInputName += FString::FromInt(InputNameIndex);
			}

			bResultNameIndexValid = true;
			for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
			{
				UMaterialExpressionFunctionInput* OtherFunctionInput = Cast<UMaterialExpressionFunctionInput>(Material->Expressions[ExpressionIndex]);
				if (OtherFunctionInput && OtherFunctionInput != this && OtherFunctionInput->InputName == PotentialInputName)
				{
					bResultNameIndexValid = false;
					break;
				}
			}

			InputNameIndex++;
		} 
		while (!bResultNameIndexValid);

		InputName = PotentialInputName;
	}
}

#if WITH_EDITOR
bool UMaterialExpressionFunctionInput::IsResultMaterialAttributes(int32 OutputIndex)
{
	if( FunctionInput_MaterialAttributes == InputType )
	{
		return true;
	}
	else
	{
		return false;
	}
}

uint32 UMaterialExpressionFunctionInput::GetInputType(int32 InputIndex)
{
	switch (InputType)
	{
	case FunctionInput_Scalar:
		return MCT_Float;
	case FunctionInput_Vector2:
		return MCT_Float2;
	case FunctionInput_Vector3:
		return MCT_Float3;
	case FunctionInput_Vector4:
		return MCT_Float4;
	case FunctionInput_Texture2D:
		return MCT_Texture2D;
	case FunctionInput_TextureCube:
		return MCT_TextureCube;
	case FunctionInput_StaticBool:
		return MCT_StaticBool;
	case FunctionInput_MaterialAttributes:
		return MCT_MaterialAttributes;
	default:
		return MCT_Unknown;
	}
}

uint32 UMaterialExpressionFunctionInput::GetOutputType(int32 OutputIndex)
{
	return GetInputType(0);
}
#endif


///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionFunctionOutput
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionFunctionOutput::UMaterialExpressionFunctionOutput(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Functions;
		FConstructorStatics()
			: NAME_Functions(LOCTEXT( "Functions", "Functions" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputs = false;
	OutputName = TEXT("Result");

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Functions);
#endif

	BorderColor = FColor(255, 155, 0);

	bCollapsed = false;
}

void UMaterialExpressionFunctionOutput::PostLoad()
{
	Super::PostLoad();
	ConditionallyGenerateId(false);
}

void UMaterialExpressionFunctionOutput::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	// Ideally we would like to regenerate the Id here, but this is used when propagating 
	// To the preview material function when editing a material function and back
	// So instead we regenerate the Id when copy pasting in the material editor, see UMaterialExpression::CopyMaterialExpressions
	ConditionallyGenerateId(false);
}

#if WITH_EDITOR
void UMaterialExpressionFunctionOutput::PostEditImport()
{
	Super::PostEditImport();
	ConditionallyGenerateId(true);
}
#endif	//#if WITH_EDITOR

FString OutputNameBackup;

#if WITH_EDITOR
void UMaterialExpressionFunctionOutput::PreEditChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == FName(TEXT("OutputName")))
	{
		OutputNameBackup = OutputName;
	}
	Super::PreEditChange(PropertyAboutToChange);
}

void UMaterialExpressionFunctionOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("OutputName")))
	{
		if (Material)
		{
			for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
			{
				UMaterialExpressionFunctionOutput* OtherFunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Material->Expressions[ExpressionIndex]);
				if (OtherFunctionOutput && OtherFunctionOutput != this && OtherFunctionOutput->OutputName == OutputName)
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_OutputNamesMustBeUnique", "Function output names must be unique"));
					OutputName = OutputNameBackup;
					break;
				}
			}
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UMaterialExpressionFunctionOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("Output ")) + OutputName);
}

void UMaterialExpressionFunctionOutput::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(Description, 40, OutToolTip);
}

uint32 UMaterialExpressionFunctionOutput::GetInputType(int32 InputIndex)
{
	// Acceptable types for material function outputs
	return MCT_Float | MCT_MaterialAttributes;
}

int32 UMaterialExpressionFunctionOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing function output '%s'"), *OutputName);
	}
	return A.Compile(Compiler);
}
#endif // WITH_EDITOR

void UMaterialExpressionFunctionOutput::ConditionallyGenerateId(bool bForce)
{
	if (bForce || !Id.IsValid())
	{
		Id = FGuid::NewGuid();
	}
}

void UMaterialExpressionFunctionOutput::ValidateName()
{
	if (Material)
	{
		int32 OutputNameIndex = 0;
		bool bResultNameIndexValid = true;
		FString PotentialOutputName;

		// Find an available unique name
		do 
		{
			PotentialOutputName = OutputName;
			if (OutputNameIndex != 0)
			{
				PotentialOutputName += FString::FromInt(OutputNameIndex);
			}

			bResultNameIndexValid = true;
			for (int32 ExpressionIndex = 0; ExpressionIndex < Material->Expressions.Num(); ExpressionIndex++)
			{
				UMaterialExpressionFunctionOutput* OtherFunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Material->Expressions[ExpressionIndex]);
				if (OtherFunctionOutput && OtherFunctionOutput != this && OtherFunctionOutput->OutputName == PotentialOutputName)
				{
					bResultNameIndexValid = false;
					break;
				}
			}

			OutputNameIndex++;
		} 
		while (!bResultNameIndexValid);

		OutputName = PotentialOutputName;
	}
}

#if WITH_EDITOR
bool UMaterialExpressionFunctionOutput::IsResultMaterialAttributes(int32 OutputIndex)
{
	// If there is a loop anywhere in this expression's inputs then we can't risk checking them
	if( A.GetTracedInput().Expression && !A.Expression->ContainsInputLoop() )
	{
		return A.Expression->IsResultMaterialAttributes(A.OutputIndex);
	}
	else
	{
		return false;
	}
}
#endif // WITH_EDITOR

UMaterialExpressionCollectionParameter::UMaterialExpressionCollectionParameter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Parameters;
		FConstructorStatics()
			: NAME_Parameters(LOCTEXT( "Parameters", "Parameters" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Parameters);
#endif

	bCollapsed = false;
}


void UMaterialExpressionCollectionParameter::PostLoad()
{
	if (Collection)
	{
		Collection->ConditionalPostLoad();
		ParameterName = Collection->GetParameterName(ParameterId);
	}

	Super::PostLoad();
}

bool UMaterialExpressionCollectionParameter::NeedsLoadForClient() const
{
	return true;
}

#if WITH_EDITOR
void UMaterialExpressionCollectionParameter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if (Collection)
	{
		ParameterId = Collection->GetParameterId(ParameterName);
	}
	else
	{
		ParameterId = FGuid();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

int32 UMaterialExpressionCollectionParameter::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 ParameterIndex = -1;
	int32 ComponentIndex = -1;

	if (Collection)
	{
		Collection->GetParameterIndex(ParameterId, ParameterIndex, ComponentIndex);
	}

	if (ParameterIndex != -1)
	{
		return Compiler->AccessCollectionParameter(Collection, ParameterIndex, ComponentIndex);
	}
	else
	{
		if (!Collection)
		{
			return Compiler->Errorf(TEXT("CollectionParameter has invalid Collection!"));
		}
		else
		{
			return Compiler->Errorf(TEXT("CollectionParameter has invalid parameter %s"), *ParameterName.ToString());
		}
	}
}

void UMaterialExpressionCollectionParameter::GetCaption(TArray<FString>& OutCaptions) const
{
	FString TypePrefix;

	if (Collection)
	{
		int32 ParameterIndex = -1;
		int32 ComponentIndex = -1;
		Collection->GetParameterIndex(ParameterId, ParameterIndex, ComponentIndex);

		if (ComponentIndex == -1)
		{
			TypePrefix = TEXT("(float4) ");
		}
		else
		{
			TypePrefix = TEXT("(float1) ");
		}
	}

	OutCaptions.Add(TypePrefix + TEXT("Collection Param"));

	if (Collection)
	{
		OutCaptions.Add(Collection->GetName());
		OutCaptions.Add(FString(TEXT("'")) + ParameterName.ToString() + TEXT("'"));
	}
	else
	{
		OutCaptions.Add(TEXT("Unspecified"));
	}
}
#endif // WITH_EDITOR

bool UMaterialExpressionCollectionParameter::MatchesSearchQuery(const TCHAR* SearchQuery)
{
	if (ParameterName.ToString().Contains(SearchQuery))
	{
		return true;
	}

	if (Collection && Collection->GetName().Contains(SearchQuery))
	{
		return true;
	}

	return Super::MatchesSearchQuery(SearchQuery);
}

//
//	UMaterialExpressionLightmapUVs
//
UMaterialExpressionLightmapUVs::UMaterialExpressionLightmapUVs(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputNameOnPin = true;
	bHidePreviewWindow = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 0, 0));

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionLightmapUVs::Compile( FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->LightmapUVs();
}

	
void UMaterialExpressionLightmapUVs::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("LightmapUVs"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionAOMaterialMask
//
UMaterialExpressionPrecomputedAOMask::UMaterialExpressionPrecomputedAOMask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bShowOutputNameOnPin = true;
	bHidePreviewWindow = true;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("")));

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPrecomputedAOMask::Compile( FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->PrecomputedAOMask();
}

	
void UMaterialExpressionPrecomputedAOMask::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("PrecomputedAOMask"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionLightmassReplace
//
UMaterialExpressionLightmassReplace::UMaterialExpressionLightmassReplace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionLightmassReplace::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Realtime.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing LightmassReplace input Realtime"));
	}
	else if (!Lightmass.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing LightmassReplace input Lightmass"));
	}
	else
	{
		int32 Arg1 = Realtime.Compile(Compiler);
		int32 Arg2 = Lightmass.Compile(Compiler);
		return Compiler->LightmassReplace(Arg1, Arg2);
	}
}

void UMaterialExpressionLightmassReplace::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("LightmassReplace"));
}
#endif // WITH_EDITOR


//
//	UMaterialExpressionMaterialProxy
//
UMaterialExpressionMaterialProxyReplace::UMaterialExpressionMaterialProxyReplace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionMaterialProxyReplace::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!Realtime.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialProxyReplace input Realtime"));
	}
	else if (!MaterialProxy.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing MaterialProxyReplace input MaterialProxy"));
	}
	else
	{
		int32 Arg1 = Realtime.Compile(Compiler);
		int32 Arg2 = MaterialProxy.Compile(Compiler);
		return Compiler->MaterialProxyReplace(Arg1, Arg2);
	}
}

void UMaterialExpressionMaterialProxyReplace::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("MaterialProxyReplace"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionGIReplace
//
UMaterialExpressionGIReplace::UMaterialExpressionGIReplace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionGIReplace::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	FExpressionInput& LocalStaticIndirect = StaticIndirect.GetTracedInput().Expression ? StaticIndirect : Default;
	FExpressionInput& LocalDynamicIndirect = DynamicIndirect.GetTracedInput().Expression ? DynamicIndirect : Default;

	if(!Default.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing GIReplace input 'Default'"));
	}
	else
	{
		int32 Arg1 = Default.Compile(Compiler);
		int32 Arg2 = LocalStaticIndirect.Compile(Compiler);
		int32 Arg3 = LocalDynamicIndirect.Compile(Compiler);
		return Compiler->GIReplace(Arg1, Arg2, Arg3);
	}
}

void UMaterialExpressionGIReplace::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("GIReplace"));
}
#endif // WITH_EDITOR

UMaterialExpressionObjectOrientation::UMaterialExpressionObjectOrientation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionObjectOrientation::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain == MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Expression not available in the deferred decal material domain."));
	}

	return Compiler->ObjectOrientation();
}

void UMaterialExpressionObjectOrientation::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("ObjectOrientation"));
}
#endif // WITH_EDITOR

UMaterialExpressionReroute::UMaterialExpressionReroute(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}


UMaterialExpression* UMaterialExpressionReroute::TraceInputsToRealExpression(int32& OutputIndex) const
{
#if WITH_EDITORONLY_DATA
	TSet<FMaterialExpressionKey> VisitedExpressions;
	FExpressionInput RealInput = TraceInputsToRealExpressionInternal(VisitedExpressions);
	OutputIndex = RealInput.OutputIndex;
	return RealInput.Expression;
#else
	OutputIndex = 0;
	return nullptr;
#endif
}

FExpressionInput UMaterialExpressionReroute::TraceInputsToRealInput() const
{
	TSet<FMaterialExpressionKey> VisitedExpressions;
	FExpressionInput RealInput = TraceInputsToRealExpressionInternal(VisitedExpressions);
	return RealInput;
}

FExpressionInput UMaterialExpressionReroute::TraceInputsToRealExpressionInternal(TSet<FMaterialExpressionKey>& VisitedExpressions) const
{
#if WITH_EDITORONLY_DATA
	// First check to see if this is a terminal node, if it is then we have a reroute to nowhere.
	if (Input.Expression != nullptr)
	{
		// Now check to see if we're also connected to another reroute. If we are, then keep going unless we hit a loop condition.
		UMaterialExpressionReroute* RerouteInput = Cast<UMaterialExpressionReroute>(Input.Expression);
		if (RerouteInput != nullptr)
		{
			FMaterialExpressionKey InputExpressionKey(Input.Expression, Input.OutputIndex);
			// prevent recurring visits to expressions we've already checked
			if (VisitedExpressions.Contains(InputExpressionKey))
			{
				// We have a loop! This should result in not finding the value!
				return FExpressionInput();
			}
			else 
			{
				VisitedExpressions.Add(InputExpressionKey);
				FExpressionInput OutputExpressionInput = RerouteInput->TraceInputsToRealExpressionInternal(VisitedExpressions);
				return OutputExpressionInput;
			}
		}
		else
		{
			// We aren't connected to another Reroute, so we are good.
			return Input;
		}
	}
#endif // WITH_EDITORONLY_DATA
	// We went to nowhere, so bail out.
	return FExpressionInput();
}

#if WITH_EDITOR
int32 UMaterialExpressionReroute::Compile(FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Because we don't want to generate *any* additional instructions, we just forward this request
	// to the node that this input is connected to. If it isn't connected, then the compile will return INDEX_NONE.
	int32 Result = Input.Compile(Compiler);
	return Result;
}

void UMaterialExpressionReroute::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Reroute Node (reroutes wires)"));
}


FText UMaterialExpressionReroute::GetCreationDescription() const 
{
	return LOCTEXT("RerouteNodeCreationDesc", "This node looks like a single pin and can be used to tidy up your graph by adding a movable control point to the connection spline.");
}

FText UMaterialExpressionReroute::GetCreationName() const
{
	return LOCTEXT("RerouteNodeCreationName", "Add Reroute Node...");
}


uint32 UMaterialExpressionReroute::GetInputType(int32 InputIndex)
{
	// Our input type should match the node that we are ultimately connected to, no matter how many reroute nodes lie between us.
	if (InputIndex == 0 && Input.IsConnected() && Input.Expression != nullptr )
	{
		int32 RealExpressionOutputIndex = -1;
		UMaterialExpression* RealExpression = TraceInputsToRealExpression(RealExpressionOutputIndex);

		// If we found a valid connection to a real output, then our type becomes that type.
		if (RealExpression != nullptr && RealExpressionOutputIndex != -1 && RealExpression->Outputs.Num() > RealExpressionOutputIndex && RealExpressionOutputIndex >= 0)
		{
			return RealExpression->GetOutputType(RealExpressionOutputIndex);
		}
	}
	return MCT_Unknown;
}

uint32 UMaterialExpressionReroute::GetOutputType(int32 OutputIndex)
{
	// Our node is a passthrough so input and output types must match.
	return GetInputType(0);
}

bool UMaterialExpressionReroute::IsResultMaterialAttributes(int32 OutputIndex)
{
	// Most code checks to make sure that there aren't loops before going here. In our case, we rely on the fact that
	// UMaterialExpressionReroute's implementation of TraceInputsToRealExpression is resistant to input loops.
	if (Input.IsConnected() && Input.Expression != nullptr && OutputIndex == 0)
	{
		int32 RealExpressionOutputIndex = -1;
		UMaterialExpression* RealExpression = TraceInputsToRealExpression(RealExpressionOutputIndex);
		if (RealExpression != nullptr)
		{
			return RealExpression->IsResultMaterialAttributes(RealExpressionOutputIndex);
		}
	}

	return false;
}

#endif // WITH_EDITOR

UMaterialExpressionRotateAboutAxis::UMaterialExpressionRotateAboutAxis(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Period = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionRotateAboutAxis::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (!NormalizedRotationAxis.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing RotateAboutAxis input NormalizedRotationAxis"));
	}
	else if (!RotationAngle.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing RotateAboutAxis input RotationAngle"));
	}
	else if (!PivotPoint.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing RotateAboutAxis input PivotPoint"));
	}
	else if (!Position.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing RotateAboutAxis input Position"));
	}
	else
	{
		const int32 AngleIndex = Compiler->Mul(RotationAngle.Compile(Compiler), Compiler->Constant(2.0f * (float)PI / Period));
		const int32 RotationIndex = Compiler->AppendVector(
			Compiler->ForceCast(NormalizedRotationAxis.Compile(Compiler), MCT_Float3), 
			Compiler->ForceCast(AngleIndex, MCT_Float1));

		return Compiler->RotateAboutAxis(
			RotationIndex, 
			PivotPoint.Compile(Compiler), 
			Position.Compile(Compiler));
	}
}

void UMaterialExpressionRotateAboutAxis::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("RotateAboutAxis"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// Static functions so it can be used other material expressions.
///////////////////////////////////////////////////////////////////////////////

/** Does not use length() to allow optimizations. */
static int32 CompileHelperLength( FMaterialCompiler* Compiler, int32 A, int32 B )
{
	int32 Delta = Compiler->Sub(A, B);

	if(Compiler->GetType(A) == MCT_Float && Compiler->GetType(B) == MCT_Float)
	{
		// optimized
		return Compiler->Abs(Delta);
	}

	int32 Dist2 = Compiler->Dot(Delta, Delta);
	return Compiler->SquareRoot(Dist2);
}

/** Used FMath::Clamp(), which will be optimized away later to a saturate(). */
static int32 CompileHelperSaturate( FMaterialCompiler* Compiler, int32 A )
{
	return Compiler->Clamp(A, Compiler->Constant(0.0f), Compiler->Constant(1.0f));
}

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSphereMask
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionSphereMask::UMaterialExpressionSphereMask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	AttenuationRadius = 256.0f;
	HardnessPercent = 100.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSphereMask::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing input B"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		int32 Distance = CompileHelperLength(Compiler, Arg1, Arg2);

		int32 ArgInvRadius;
		if(Radius.GetTracedInput().Expression)
		{
			// if the radius input is hooked up, use it
			ArgInvRadius = Compiler->Div(Compiler->Constant(1.0f), Compiler->Max(Compiler->Constant(0.00001f), Radius.Compile(Compiler)));
		}
		else
		{
			// otherwise use the internal constant
			ArgInvRadius = Compiler->Constant(1.0f / FMath::Max(0.00001f, AttenuationRadius));
		}

		int32 NormalizeDistance = Compiler->Mul(Distance, ArgInvRadius);

		int32 ArgInvHardness;
		if(Hardness.GetTracedInput().Expression)
		{
			int32 Softness = Compiler->Sub(Compiler->Constant(1.0f), Hardness.Compile(Compiler));

			// if the radius input is hooked up, use it
			ArgInvHardness = Compiler->Div(Compiler->Constant(1.0f), Compiler->Max(Softness, Compiler->Constant(0.00001f)));
		}
		else
		{
			// Hardness is in percent 0%:soft .. 100%:hard
			// Max to avoid div by 0
			float InvHardness = 1.0f / FMath::Max(1.0f - HardnessPercent * 0.01f, 0.00001f);

			// otherwise use the internal constant
			ArgInvHardness = Compiler->Constant(InvHardness);
		}

		int32 NegNormalizedDistance = Compiler->Sub(Compiler->Constant(1.0f), NormalizeDistance);
		int32 MaskUnclamped = Compiler->Mul(NegNormalizedDistance, ArgInvHardness);

		return CompileHelperSaturate(Compiler, MaskUnclamped);
	}
}

void UMaterialExpressionSphereMask::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("SphereMask"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionSobol
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionSobol::UMaterialExpressionSobol(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstIndex = 0;
	ConstSeed = FVector2D(0.f, 0.f);
}

#if WITH_EDITOR
int32 UMaterialExpressionSobol::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 CellInput = Cell.GetTracedInput().Expression ? Cell.Compile(Compiler) : Compiler->Constant2(0.f, 0.f);
	int32 IndexInput = Index.GetTracedInput().Expression ? Index.Compile(Compiler) : Compiler->Constant(ConstIndex);
	int32 SeedInput = Seed.GetTracedInput().Expression ? Seed.Compile(Compiler) : Compiler->Constant2(ConstSeed.X, ConstSeed.Y);
	return Compiler->Sobol(CellInput, IndexInput, SeedInput);
}

void UMaterialExpressionSobol::GetCaption(TArray<FString>& OutCaptions) const
{
	FString Caption = TEXT("Sobol");

	if (!Index.GetTracedInput().Expression)
	{
		Caption += FString::Printf(TEXT(" (%d)"), ConstIndex);;
	}

	OutCaptions.Add(Caption);
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionTemporalSobol
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionTemporalSobol::UMaterialExpressionTemporalSobol(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstIndex = 0;
	ConstSeed = FVector2D(0.f, 0.f);
}

#if WITH_EDITOR
int32 UMaterialExpressionTemporalSobol::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 IndexInput = Index.GetTracedInput().Expression ? Index.Compile(Compiler) : Compiler->Constant(ConstIndex);
	int32 SeedInput = Seed.GetTracedInput().Expression ? Seed.Compile(Compiler) : Compiler->Constant2(ConstSeed.X, ConstSeed.Y);
	return Compiler->TemporalSobol(IndexInput, SeedInput);
}

void UMaterialExpressionTemporalSobol::GetCaption(TArray<FString>& OutCaptions) const
{
	FString Caption = TEXT("Temporal Sobol");

	if (!Index.GetTracedInput().Expression)
	{
		Caption += FString::Printf(TEXT(" (%d)"), ConstIndex);;
	}

	OutCaptions.Add(Caption);
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionNoise
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionNoise::UMaterialExpressionNoise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Scale = 1.0f;
	Levels = 6;
	Quality = 1;
	OutputMin = -1.0f;
	OutputMax = 1.0f;
	LevelScale = 2.0f;
	NoiseFunction = NOISEFUNCTION_SimplexTex;
	bTurbulence = true;
	bTiling = false;
	RepeatSize = 512;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

}

#if WITH_EDITOR
bool UMaterialExpressionNoise::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty != NULL)
	{
		FName PropertyFName = InProperty->GetFName();

		bool bTilableNoiseType = NoiseFunction == NOISEFUNCTION_GradientALU || NoiseFunction == NOISEFUNCTION_ValueALU 
			|| NoiseFunction == NOISEFUNCTION_GradientTex || NoiseFunction == NOISEFUNCTION_VoronoiALU;

		bool bSupportsQuality = (NoiseFunction == NOISEFUNCTION_VoronoiALU);

		if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionNoise, bTiling))
		{
			bIsEditable = bTilableNoiseType;
		}
		else if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionNoise, RepeatSize))
		{
			bIsEditable = bTilableNoiseType && bTiling;
		}

		if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionNoise, Quality))
		{
			bIsEditable = bSupportsQuality;
		}
	}

	return bIsEditable;
}

int32 UMaterialExpressionNoise::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 PositionInput;

	if(Position.GetTracedInput().Expression)
	{
		PositionInput = Position.Compile(Compiler);
	}
	else
	{
		PositionInput = Compiler->WorldPosition(WPT_Default);
	}

	int32 FilterWidthInput;

	if(FilterWidth.GetTracedInput().Expression)
	{
		FilterWidthInput = FilterWidth.Compile(Compiler);
	}
	else
	{
		FilterWidthInput = Compiler->Constant(0);
	}

	return Compiler->Noise(PositionInput, Scale, Quality, NoiseFunction, bTurbulence, Levels, OutputMin, OutputMax, LevelScale, FilterWidthInput, bTiling, RepeatSize);
}

void UMaterialExpressionNoise::GetCaption(TArray<FString>& OutCaptions) const
{
	const UEnum* NFEnum = FindObject<UEnum>(nullptr, TEXT("Engine.ENoiseFunction"));
	check(NFEnum);
	OutCaptions.Add(NFEnum->GetDisplayNameTextByValue(NoiseFunction).ToString());
	OutCaptions.Add(TEXT("Noise"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionVectorNoise
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionVectorNoise::UMaterialExpressionVectorNoise(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Quality = 1;
	NoiseFunction = VNF_CellnoiseALU;
	bTiling = false;
	TileSize = 300;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
bool UMaterialExpressionVectorNoise::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty != NULL)
	{
		FName PropertyFName = InProperty->GetFName();

		bool bSupportsQuality = (NoiseFunction == VNF_VoronoiALU);

		if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionVectorNoise, TileSize))
		{
			bIsEditable = bTiling;
		}

		else if (PropertyFName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionVectorNoise, Quality))
		{
			bIsEditable = bSupportsQuality;
		}
	}

	return bIsEditable;
}

int32 UMaterialExpressionVectorNoise::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 PositionInput;

	if (Position.GetTracedInput().Expression)
	{
		PositionInput = Position.Compile(Compiler);
	}
	else
	{
		PositionInput = Compiler->WorldPosition(WPT_Default);
	}

	return Compiler->VectorNoise(PositionInput, Quality, NoiseFunction, bTiling, TileSize);
}

void UMaterialExpressionVectorNoise::GetCaption(TArray<FString>& OutCaptions) const
{
	const UEnum* VNFEnum = FindObject<UEnum>(nullptr, TEXT("Engine.EVectorNoiseFunction"));
	check(VNFEnum);
	OutCaptions.Add(VNFEnum->GetDisplayNameTextByValue(NoiseFunction).ToString());
	OutCaptions.Add(TEXT("Vector Noise"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionBlackBody
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionBlackBody::UMaterialExpressionBlackBody(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionBlackBody::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 TempInput = INDEX_NONE;

	if( Temp.GetTracedInput().Expression )
	{
		TempInput = Temp.Compile(Compiler);
	}

	if( TempInput == INDEX_NONE )
	{
		return INDEX_NONE;
	}

	return Compiler->BlackBody( TempInput );
}

void UMaterialExpressionBlackBody::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("BlackBody"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDistanceToNearestSurface
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDistanceToNearestSurface::UMaterialExpressionDistanceToNearestSurface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionDistanceToNearestSurface::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 PositionArg = INDEX_NONE;

	if (Position.GetTracedInput().Expression)
	{
		PositionArg = Position.Compile(Compiler);
	}
	else 
	{
		PositionArg = Compiler->WorldPosition(WPT_Default);
	}

	return Compiler->DistanceToNearestSurface(PositionArg);
}

void UMaterialExpressionDistanceToNearestSurface::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("DistanceToNearestSurface"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDistanceFieldGradient
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDistanceFieldGradient::UMaterialExpressionDistanceFieldGradient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionDistanceFieldGradient::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 PositionArg = INDEX_NONE;

	if (Position.GetTracedInput().Expression)
	{
		PositionArg = Position.Compile(Compiler);
	}
	else 
	{
		PositionArg = Compiler->WorldPosition(WPT_Default);
	}

	return Compiler->DistanceFieldGradient(PositionArg);
}

void UMaterialExpressionDistanceFieldGradient::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("DistanceFieldGradient"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDistance
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDistance::UMaterialExpressionDistance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionDistance::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!A.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing input A"));
	}
	else if(!B.GetTracedInput().Expression)
	{
		return Compiler->Errorf(TEXT("Missing input B"));
	}
	else
	{
		int32 Arg1 = A.Compile(Compiler);
		int32 Arg2 = B.Compile(Compiler);
		return CompileHelperLength(Compiler, Arg1, Arg2);
	}
}

void UMaterialExpressionDistance::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Distance"));
}
#endif // WITH_EDITOR

UMaterialExpressionTwoSidedSign::UMaterialExpressionTwoSidedSign(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionTwoSidedSign::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->TwoSidedSign();
}

void UMaterialExpressionTwoSidedSign::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("TwoSidedSign"));
}
#endif // WITH_EDITOR

UMaterialExpressionVertexNormalWS::UMaterialExpressionVertexNormalWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionVertexNormalWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->VertexNormal();
}

void UMaterialExpressionVertexNormalWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("VertexNormalWS"));
}
#endif // WITH_EDITOR

UMaterialExpressionPixelNormalWS::UMaterialExpressionPixelNormalWS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FText NAME_Coordinates;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT( "Vectors", "Vectors" ))
			, NAME_Coordinates(LOCTEXT( "Coordinates", "Coordinates" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
	MenuCategories.Add(ConstructorStatics.NAME_Coordinates);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPixelNormalWS::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->PixelNormalWS();
}

void UMaterialExpressionPixelNormalWS::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("PixelNormalWS"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionPerInstanceRandom
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionPerInstanceRandom::UMaterialExpressionPerInstanceRandom(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPerInstanceRandom::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->PerInstanceRandom();
}

void UMaterialExpressionPerInstanceRandom::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("PerInstanceRandom"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionPerInstanceFadeAmount
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionPerInstanceFadeAmount::UMaterialExpressionPerInstanceFadeAmount(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPerInstanceFadeAmount::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->PerInstanceFadeAmount();
}

void UMaterialExpressionPerInstanceFadeAmount::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("PerInstanceFadeAmount"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionAntialiasedTextureMask
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionAntialiasedTextureMask::UMaterialExpressionAntialiasedTextureMask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> DefaultTexture;
		FText NAME_Utility;
		FName NAME_None;
		FConstructorStatics()
			: DefaultTexture(TEXT("/Engine/EngineResources/DefaultTexture"))
			, NAME_Utility(LOCTEXT( "Utility", "Utility" ))
			, NAME_None(TEXT("None"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	Texture = ConstructorStatics.DefaultTexture.Object;

#if WITH_EDITORONLY_DATA
	MenuCategories.Empty();
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	Threshold = 0.5f;
	ParameterName = ConstructorStatics.NAME_None;
	Channel = TCC_Alpha;

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 0, 0, 0));

}

#if WITH_EDITOR
int32 UMaterialExpressionAntialiasedTextureMask::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if(!Texture)
	{
		return Compiler->Errorf(TEXT("UMaterialExpressionAntialiasedTextureMask> Missing input texture"));
	}

	int32 ArgCoord = Coordinates.Expression ? Coordinates.Compile(Compiler) : Compiler->TextureCoordinate(ConstCoordinate, false, false);

	if (!TextureIsValid(Texture))
	{
		return CompilerError(Compiler, GetRequirements());
	}

	int32 TextureCodeIndex;

	if (!ParameterName.IsValid() || ParameterName.IsNone())
	{
		TextureCodeIndex = Compiler->Texture(Texture);
	}
	else
	{
		TextureCodeIndex = Compiler->TextureParameter(ParameterName, Texture);
	}

	if (!VerifySamplerType(Compiler, (Desc.Len() > 0 ? *Desc : TEXT("AntialiasedTextureMask")), Texture, SamplerType))
	{
		return INDEX_NONE;
	}

	return Compiler->AntialiasedTextureMask(TextureCodeIndex,ArgCoord,Threshold,Channel);
}

void UMaterialExpressionAntialiasedTextureMask::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("AAMasked Param2D")); 
	OutCaptions.Add(FString::Printf(TEXT("'%s'"), *ParameterName.ToString()));
}
#endif // WITH_EDITOR

bool UMaterialExpressionAntialiasedTextureMask::TextureIsValid( UTexture* InTexture )
{
	bool Result=false;
	if (InTexture)		
	{
		if( InTexture->GetClass() == UTexture2D::StaticClass() ) 
		{
			Result = true;
		}
		if( InTexture->IsA(UTextureRenderTarget2D::StaticClass()) )	
		{
			Result = true;
		}
	}
	return Result;
}

const TCHAR* UMaterialExpressionAntialiasedTextureMask::GetRequirements()
{
	return TEXT("Requires Texture2D");
}

void UMaterialExpressionAntialiasedTextureMask::SetDefaultTexture()
{
	Texture = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"), NULL, LOAD_None, NULL);
}

//
//	UMaterialExpressionDecalDerivative
//
UMaterialExpressionDecalDerivative::UMaterialExpressionDecalDerivative(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT("Utils", "Utils"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	//bCollapsed = true;
	bShaderInputData = true;
	bShowOutputNameOnPin = true;
	
	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("DDX")));
	Outputs.Add(FExpressionOutput(TEXT("DDY")));
}

#if WITH_EDITOR
int32 UMaterialExpressionDecalDerivative::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->TextureDecalDerivative(OutputIndex == 1);
}

void UMaterialExpressionDecalDerivative::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Decal Derivative"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionDecalLifetimeOpacity
//
UMaterialExpressionDecalLifetimeOpacity::UMaterialExpressionDecalLifetimeOpacity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT("Utils", "Utils"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bShaderInputData = true;

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Opacity")));
}

#if WITH_EDITOR
int32 UMaterialExpressionDecalLifetimeOpacity::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain != MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Node only works for the deferred decal material domain."));
	}

	return Compiler->DecalLifetimeOpacity();
}

void UMaterialExpressionDecalLifetimeOpacity::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Decal Lifetime Opacity"));
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionDecalMipmapLevel
//
UMaterialExpressionDecalMipmapLevel::UMaterialExpressionDecalMipmapLevel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ConstWidth(256.0f)
	, ConstHeight(ConstWidth)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Vectors;
		FConstructorStatics()
			: NAME_Vectors(LOCTEXT("Utils", "Utils"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Vectors);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionDecalMipmapLevel::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Material && Material->MaterialDomain != MD_DeferredDecal)
	{
		return CompilerError(Compiler, TEXT("Node only works for the deferred decal material domain."));
	}

	int32 TextureSizeInput = INDEX_NONE;

	if (TextureSize.GetTracedInput().Expression)
	{
		TextureSizeInput = TextureSize.Compile(Compiler);
	}
	else
	{
		TextureSizeInput = Compiler->Constant2(ConstWidth, ConstHeight);
	}

	if (TextureSizeInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return Compiler->TextureDecalMipmapLevel(TextureSizeInput);
}

void UMaterialExpressionDecalMipmapLevel::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Decal Mipmap Level"));
}
#endif // WITH_EDITOR

UMaterialExpressionDepthFade::UMaterialExpressionDepthFade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Depth;
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Depth(LOCTEXT( "Depth", "Depth" ))
			, NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	FadeDistanceDefault = 100.0f;
	OpacityDefault = 1.0f;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Depth);
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionDepthFade::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	// Scales Opacity by a Linear fade based on SceneDepth, from 0 at PixelDepth to 1 at FadeDistance
	// Result = Opacity * saturate((SceneDepth - PixelDepth) / max(FadeDistance, DELTA))
	const int32 OpacityIndex = InOpacity.GetTracedInput().Expression ? InOpacity.Compile(Compiler) : Compiler->Constant(OpacityDefault);
	const int32 FadeDistanceIndex = Compiler->Max(FadeDistance.GetTracedInput().Expression ? FadeDistance.Compile(Compiler) : Compiler->Constant(FadeDistanceDefault), Compiler->Constant(DELTA));
	const int32 FadeIndex = CompileHelperSaturate(Compiler, Compiler->Div(Compiler->Sub(Compiler->SceneDepth(INDEX_NONE, INDEX_NONE, false), Compiler->PixelDepth()), FadeDistanceIndex));
	return Compiler->Mul(OpacityIndex, FadeIndex);
}
#endif // WITH_EDITOR

//
//	UMaterialExpressionSphericalParticleOpacity
//
UMaterialExpressionSphericalParticleOpacity::UMaterialExpressionSphericalParticleOpacity(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	ConstantDensity = 1;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionSphericalParticleOpacity::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	const int32 DensityIndex = Density.GetTracedInput().Expression ? Density.Compile(Compiler) : Compiler->Constant(ConstantDensity);
	return Compiler->SphericalParticleOpacity(DensityIndex);
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDepthOfFieldFunction
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDepthOfFieldFunction::UMaterialExpressionDepthOfFieldFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionDepthOfFieldFunction::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 DepthInput;

	if(Depth.GetTracedInput().Expression)
	{
		// using the input allows more custom behavior
		DepthInput = Depth.Compile(Compiler);
	}
	else
	{
		// no input means we use the PixelDepth
		DepthInput = Compiler->PixelDepth();
	}

	if(DepthInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return Compiler->DepthOfFieldFunction(DepthInput, FunctionValue);
}


void UMaterialExpressionDepthOfFieldFunction::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("DepthOfFieldFunction")));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDDX
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDDX::UMaterialExpressionDDX(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionDDX::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 ValueInput = INDEX_NONE;

	if(Value.GetTracedInput().Expression)
	{
		ValueInput = Value.Compile(Compiler);
	}

	if(ValueInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return Compiler->DDX(ValueInput);
}


void UMaterialExpressionDDX::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("DDX")));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionDDY
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionDDY::UMaterialExpressionDDY(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionDDY::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 ValueInput = INDEX_NONE;

	if(Value.GetTracedInput().Expression)
	{
		ValueInput = Value.Compile(Compiler);
	}

	if(ValueInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return Compiler->DDY(ValueInput);
}


void UMaterialExpressionDDY::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("DDY")));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle relative time material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleRelativeTime::UMaterialExpressionParticleRelativeTime(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleRelativeTime::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleRelativeTime();
}

void UMaterialExpressionParticleRelativeTime::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Relative Time"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle motion blur fade material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleMotionBlurFade::UMaterialExpressionParticleMotionBlurFade(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleMotionBlurFade::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleMotionBlurFade();
}

void UMaterialExpressionParticleMotionBlurFade::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Motion Blur Fade"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle motion blur fade material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleRandom::UMaterialExpressionParticleRandom(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleRandom::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleRandom();
}

void UMaterialExpressionParticleRandom::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Random Value"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle direction material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleDirection::UMaterialExpressionParticleDirection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleDirection::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleDirection();
}

void UMaterialExpressionParticleDirection::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Direction"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle speed material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleSpeed::UMaterialExpressionParticleSpeed(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleSpeed::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleSpeed();
}

void UMaterialExpressionParticleSpeed::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Speed"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Particle size material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionParticleSize::UMaterialExpressionParticleSize(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Particles;
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Particles(LOCTEXT( "Particles", "Particles" ))
			, NAME_Constants(LOCTEXT( "Constants", "Constants" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Particles);
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionParticleSize::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->ParticleSize();
}

void UMaterialExpressionParticleSize::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Particle Size"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	Atmospheric fog material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionAtmosphericFogColor::UMaterialExpressionAtmosphericFogColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Atmosphere;
		FConstructorStatics()
			: NAME_Atmosphere(LOCTEXT( "Atmosphere", "Atmosphere" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Atmosphere);
#endif

	bCollapsed = false;
}

#if WITH_EDITOR
int32 UMaterialExpressionAtmosphericFogColor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	int32 WorldPositionInput = INDEX_NONE;

	if( WorldPosition.GetTracedInput().Expression )
	{
		WorldPositionInput = WorldPosition.Compile(Compiler);
	}

	return Compiler->AtmosphericFogColor( WorldPositionInput );
}

void UMaterialExpressionAtmosphericFogColor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Atmospheric Fog Color"));
}
#endif // WITH_EDITOR

/*------------------------------------------------------------------------------
	SpeedTree material expression.
------------------------------------------------------------------------------*/
UMaterialExpressionSpeedTree::UMaterialExpressionSpeedTree(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_SpeedTree;
		FConstructorStatics()
			: NAME_SpeedTree(LOCTEXT( "SpeedTree", "SpeedTree" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	GeometryType = STG_Branch;
	WindType = STW_None;
	LODType = STLOD_Pop;
	BillboardThreshold = 0.9f;
	bAccurateWindVelocities = false;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_SpeedTree);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionSpeedTree::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->SpeedTree(GeometryType, WindType, LODType, BillboardThreshold, bAccurateWindVelocities);
}

void UMaterialExpressionSpeedTree::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("SpeedTree"));
}
#endif // WITH_EDITOR

void UMaterialExpressionSpeedTree::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UE4Ver() < VER_UE4_SPEEDTREE_WIND_V7)
	{
		// update wind presets for speedtree v7
		switch (WindType)
		{
		case STW_Fastest:
			WindType = STW_Better;
			break;
		case STW_Fast:
			WindType = STW_Palm;
			break;
		case STW_Better:
			WindType = STW_Best;
			break;
		default:
			break;
		}
	}
}

#if WITH_EDITOR

bool UMaterialExpressionSpeedTree::CanEditChange(const UProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);

	if (GeometryType == STG_Billboard)
	{
		if (InProperty->GetFName() == TEXT("LODType"))
		{
			bIsEditable = false;
		}
	}
	else
	{
		if (InProperty->GetFName() == TEXT("BillboardThreshold"))
		{
			bIsEditable = false;
		}
	}

	return bIsEditable;
}

#endif

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionCustomOutput
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionCustomOutput::UMaterialExpressionCustomOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionEyeAdaptation
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionEyeAdaptation::UMaterialExpressionEyeAdaptation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT( "Utility", "Utility" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("EyeAdaptation")));
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionEyeAdaptation::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{    
	return Compiler->EyeAdaptation();
}

void UMaterialExpressionEyeAdaptation::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("EyeAdaptation")));
}
#endif // WITH_EDITOR

// WaveWorks Start
///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionWaveWorks
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionWaveWorks::UMaterialExpressionWaveWorks(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_WaveWorks;
		FConstructorStatics()
			: NAME_WaveWorks(LOCTEXT("WaveWorks", "WaveWorks"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_WaveWorks);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("Foam"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("Normal"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("WorldPosUndisplaced"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("Displacement"), 1, 1, 1, 1, 0));
	Outputs.Add(FExpressionOutput(TEXT("DistanceToShoreline"), 1, 1, 1, 1, 0));
	bShowOutputNameOnPin = true;
}

#if WITH_EDITOR

int32 UMaterialExpressionWaveWorks::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->WaveWorks(GetOutputs()[OutputIndex].OutputName);
}

void UMaterialExpressionWaveWorks::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("WaveWorks"));
}

#endif
// WaveWorks End

//
// UMaterialExpressionTangentOutput
//
UMaterialExpressionTangentOutput::UMaterialExpressionTangentOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Custom;
		FConstructorStatics(const FString& DisplayName, const FString& FunctionName)
			: NAME_Custom(LOCTEXT( "Custom", "Custom" ))
		{
			// Register with attribute map to allow use with material attribute nodes and blending	
			FMaterialAttributeDefinitionMap::AddCustomAttribute(FGuid(0x8EAB2CB2, 0x73634A24, 0x8CD14F47, 0x3F9C8E55), DisplayName, FunctionName, MCT_Float3, FVector4(0,0,0,0));
		}
	};
	static FConstructorStatics ConstructorStatics(GetDisplayName(), GetFunctionName());

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Custom);
#endif

	// No outputs
	Outputs.Reset();
}

#if WITH_EDITOR
int32 UMaterialExpressionTangentOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if( Input.GetTracedInput().Expression )
	{
		return Compiler->CustomOutput(this, OutputIndex, Input.Compile(Compiler));
	}
	else
	{
		return CompilerError(Compiler, TEXT("Input missing"));
	}

	return INDEX_NONE;
}

void UMaterialExpressionTangentOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Tangent output"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// Clear Coat Custom Normal Input
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionClearCoatNormalCustomOutput::UMaterialExpressionClearCoatNormalCustomOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics(const FString& DisplayName, const FString& FunctionName)
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
			// Register with attribute map to allow use with material attribute nodes and blending
			FMaterialAttributeDefinitionMap::AddCustomAttribute(FGuid(0xAA3D5C04, 0x16294716, 0xBBDEC869, 0x6A27DD72), DisplayName, FunctionName, MCT_Float3, FVector4(0,0,1,0));
		}
	};
	static FConstructorStatics ConstructorStatics(GetDisplayName(), GetFunctionName());

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = true;

	// No outputs
	Outputs.Reset();
}

#if WITH_EDITOR
int32  UMaterialExpressionClearCoatNormalCustomOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Input.GetTracedInput().Expression)
	{
		return Compiler->CustomOutput(this, OutputIndex, Input.Compile(Compiler));
	}
	else
	{
		return CompilerError(Compiler, TEXT("Input missing"));
	}
	return INDEX_NONE;
}


void UMaterialExpressionClearCoatNormalCustomOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("ClearCoatBottomNormal")));
}
#endif // WITH_EDITOR

FExpressionInput* UMaterialExpressionClearCoatNormalCustomOutput::GetInput(int32 InputIndex)
{
	return &Input;
}


///////////////////////////////////////////////////////////////////////////////
// Bent Normal Output
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionBentNormalCustomOutput::UMaterialExpressionBentNormalCustomOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics(const FString& DisplayName, const FString& FunctionName)
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
			// Register with attribute map to allow use with material attribute nodes and blending
			FMaterialAttributeDefinitionMap::AddCustomAttribute(FGuid(0xfbd7b46e, 0xb1234824, 0xbde76b23, 0x609f984c), DisplayName, FunctionName, MCT_Float3, FVector4(0,0,1,0));
		}
	};
	static FConstructorStatics ConstructorStatics(GetDisplayName(), GetFunctionName());

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	bCollapsed = true;

	// No outputs
	Outputs.Reset();
}

#if WITH_EDITOR
int32  UMaterialExpressionBentNormalCustomOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Input.GetTracedInput().Expression)
	{
		return Compiler->CustomOutput(this, OutputIndex, Input.Compile(Compiler));
	}
	else
	{
		return CompilerError(Compiler, TEXT("Input missing"));
	}
	return INDEX_NONE;
}


void UMaterialExpressionBentNormalCustomOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("BentNormal")));
}
#endif // WITH_EDITOR

FExpressionInput* UMaterialExpressionBentNormalCustomOutput::GetInput(int32 InputIndex)
{
	return &Input;
}

///////////////////////////////////////////////////////////////////////////////
// Vertex to pixel interpolated data handler
///////////////////////////////////////////////////////////////////////////////

UMaterialExpressionVertexInterpolator::UMaterialExpressionVertexInterpolator(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT("PS"), 0, 0, 0, 0, 0));
	bShowOutputNameOnPin = true;

	InterpolatorIndex = INDEX_NONE;
	InterpolatedType = MCT_Unknown;
	InterpolatorOffset = INDEX_NONE;
}

#if WITH_EDITOR
int32 UMaterialExpressionVertexInterpolator::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Input.GetTracedInput().Expression)
	{
		if (InterpolatorIndex == INDEX_NONE)
		{
			return Compiler->Errorf(TEXT("Failed to compile interpolator input."));
		}
		else
		{
			return Compiler->VertexInterpolator(InterpolatorIndex);
		}
	}
	else
	{
		return CompilerError(Compiler, TEXT("Input missing"));
	}
}

int32 UMaterialExpressionVertexInterpolator::CompileInput(class FMaterialCompiler* Compiler, int32 AssignedInterpolatorIndex)
{
	int32 Ret = INDEX_NONE;
	InterpolatorIndex = INDEX_NONE;
	InterpolatedType = MCT_Unknown;
	InterpolatorOffset = INDEX_NONE;

	if (Input.GetTracedInput().Expression)
	{
		int32 InternalCode = Input.Compile(Compiler);
		Compiler->CustomOutput(this, AssignedInterpolatorIndex, InternalCode);
		InterpolatorIndex = AssignedInterpolatorIndex;
		InterpolatedType = Compiler->GetType(InternalCode);
		Ret = InternalCode;
	}

	return Ret;
}

void UMaterialExpressionVertexInterpolator::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(FString(TEXT("VertexInterpolator")));
}
#endif // WITH_EDITOR

FExpressionInput* UMaterialExpressionVertexInterpolator::GetInput(int32 InputIndex)
{
	return &Input;
}

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionrAtmosphericLightVector
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionAtmosphericLightVector::UMaterialExpressionAtmosphericLightVector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionAtmosphericLightVector::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{

	return Compiler->AtmosphericLightVector();
}

void UMaterialExpressionAtmosphericLightVector::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("AtmosphericLightVector"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionrAtmosphericLightColor
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionAtmosphericLightColor ::UMaterialExpressionAtmosphericLightColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Utility;
		FConstructorStatics()
			: NAME_Utility(LOCTEXT("Utility", "Utility"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Utility);
#endif
}

#if WITH_EDITOR
int32 UMaterialExpressionAtmosphericLightColor::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{

	return Compiler->AtmosphericLightColor();
}

void UMaterialExpressionAtmosphericLightColor::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("AtmosphericLightColor"));
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionPreSkinnedPosition
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionPreSkinnedPosition::UMaterialExpressionPreSkinnedPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPreSkinnedPosition::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (Compiler->GetCurrentShaderFrequency() != SF_Vertex)
	{
		return Compiler->Errorf(TEXT("Pre-skinned position is only available in the vertex shader, pass through custom interpolators if needed."));
	}

	return Compiler->PreSkinnedPosition();
}

void UMaterialExpressionPreSkinnedPosition::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Pre-Skinned Local Position"));
}

void UMaterialExpressionPreSkinnedPosition::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns pre-skinned local position for skeletal meshes, usable in vertex shader only."
		"Returns the local position for non-skeletal meshes. Incompatible with GPU skin cache feature."), 40, OutToolTip);
}
#endif // WITH_EDITOR

///////////////////////////////////////////////////////////////////////////////
// UMaterialExpressionPreSkinnedNormal
///////////////////////////////////////////////////////////////////////////////
UMaterialExpressionPreSkinnedNormal::UMaterialExpressionPreSkinnedNormal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText NAME_Constants;
		FConstructorStatics()
			: NAME_Constants(LOCTEXT( "Vectors", "Vectors" ))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.NAME_Constants);
#endif

	Outputs.Reset();
	Outputs.Add(FExpressionOutput(TEXT(""), 1, 1, 1, 1, 0));
	bShaderInputData = true;
}

#if WITH_EDITOR
int32 UMaterialExpressionPreSkinnedNormal::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	return Compiler->PreSkinnedNormal();
}

void UMaterialExpressionPreSkinnedNormal::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Pre-Skinned Local Normal"));
}

void UMaterialExpressionPreSkinnedNormal::GetExpressionToolTip(TArray<FString>& OutToolTip) 
{
	ConvertToMultilineToolTip(TEXT("Returns pre-skinned local normal for skeletal meshes, usable in vertex shader only."
		"Returns the local normal for non-skeletal meshes. Incompatible with GPU skin cache feature."), 40, OutToolTip);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
