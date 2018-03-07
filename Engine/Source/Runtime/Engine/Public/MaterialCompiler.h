// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialCompiler.h: Material compiler interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "MaterialShared.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionViewProperty.h"

class Error;
class UMaterialParameterCollection;
class UTexture;

enum EMaterialForceCastFlags
{
	MFCF_ForceCast		= 1<<0,	// Used by caller functions as a helper
	MFCF_ExactMatch		= 1<<2, // If flag set skips the cast on an exact match, else skips on a compatible match
	MFCF_ReplicateValue	= 1<<3	// Replicates a Float1 value when up-casting, else appends zero
};

/** 
 * The interface used to translate material expressions into executable code. 
 * Note: Most member functions should be pure virtual to force a FProxyMaterialCompiler override!
 */
class FMaterialCompiler
{
public:
	virtual ~FMaterialCompiler() { }

	// sets internal state CurrentShaderFrequency 
	// @param OverrideShaderFrequency SF_NumFrequencies to not override
	virtual void SetMaterialProperty(EMaterialProperty InProperty, EShaderFrequency OverrideShaderFrequency = SF_NumFrequencies, bool bUsePreviousFrameTime = false) = 0;
	
	/** Pushes a material attriubtes property onto the stack. Called as we begin compiling a property through a MaterialAttributes pin. */
	virtual void PushMaterialAttribute(const FGuid& InAttributeID) = 0;
	/** Pops a MaterialAttributes property off the stack. Called as we finish compiling a property through a MaterialAttributes pin. */
	virtual FGuid PopMaterialAttribute() = 0;
	/** Gets the current top of the MaterialAttributes property stack. */
	virtual const FGuid GetMaterialAttribute() = 0;
	/** Sets the bottom MaterialAttributes property of the stack. */
	virtual void SetBaseMaterialAttribute(const FGuid& InAttributeID) = 0;

	// gets value stored by SetMaterialProperty()
	virtual EShaderFrequency GetCurrentShaderFrequency() const = 0;
	//
	virtual int32 Error(const TCHAR* Text) = 0;
	ENGINE_API int32 Errorf(const TCHAR* Format,...);

	virtual int32 CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* InCompiler) = 0;

	virtual EMaterialValueType GetType(int32 Code) = 0;

	virtual EMaterialQualityLevel::Type GetQualityLevel() = 0;

	virtual ERHIFeatureLevel::Type GetFeatureLevel() = 0;

	virtual EMaterialShadingModel GetMaterialShadingModel() const = 0;

	virtual EMaterialValueType GetParameterType(int32 Index) const = 0;

	virtual FMaterialUniformExpression* GetParameterUniformExpression(int32 Index) const = 0;

	/** 
	 * Casts the passed in code to DestType, or generates a compile error if the cast is not valid. 
	 * This will truncate a type (float4 -> float3) but not add components (float2 -> float3), however a float1 can be cast to any float type by replication. 
	 */
	virtual int32 ValidCast(int32 Code,EMaterialValueType DestType) = 0;
	virtual int32 ForceCast(int32 Code,EMaterialValueType DestType,uint32 ForceCastFlags = 0) = 0;

	/** Pushes a function onto the compiler's function stack, which indicates that compilation is entering a function. */
	virtual void PushFunction(const FMaterialFunctionCompileState& FunctionState) = 0;

	/** Pops a function from the compiler's function stack, which indicates that compilation is leaving a function. */
	virtual FMaterialFunctionCompileState PopFunction() = 0;

	virtual int32 AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex) = 0;
	virtual int32 VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) = 0;
	virtual int32 ScalarParameter(FName ParameterName,float DefaultValue) = 0;

	virtual int32 Constant(float X) = 0;
	virtual int32 Constant2(float X,float Y) = 0;
	virtual int32 Constant3(float X,float Y,float Z) = 0;
	virtual int32 Constant4(float X,float Y,float Z,float W) = 0;

	virtual	int32 ViewProperty(EMaterialExposedViewProperty Property, bool InvProperty = false) = 0;

	virtual int32 GameTime(bool bPeriodic, float Period) = 0;
	virtual int32 RealTime(bool bPeriodic, float Period) = 0;
	virtual int32 PeriodicHint(int32 PeriodicCode) { return PeriodicCode; }

	virtual int32 Sine(int32 X) = 0;
	virtual int32 Cosine(int32 X) = 0;
	virtual int32 Tangent(int32 X) = 0;
	virtual int32 Arcsine(int32 X) = 0;
	virtual int32 ArcsineFast(int32 X) = 0;
	virtual int32 Arccosine(int32 X) = 0;
	virtual int32 ArccosineFast(int32 X) = 0;
	virtual int32 Arctangent(int32 X) = 0;
	virtual int32 ArctangentFast(int32 X) = 0;
	virtual int32 Arctangent2(int32 Y, int32 X) = 0;
	virtual int32 Arctangent2Fast(int32 Y, int32 X) = 0;

	virtual int32 Floor(int32 X) = 0;
	virtual int32 Ceil(int32 X) = 0;
	virtual int32 Round(int32 X) = 0;
	virtual int32 Truncate(int32 X) = 0;
	virtual int32 Sign(int32 X) = 0;
	virtual int32 Frac(int32 X) = 0;
	virtual int32 Fmod(int32 A, int32 B) = 0;
	virtual int32 Abs(int32 X) = 0;

	virtual int32 ReflectionVector() = 0;
	virtual int32 ReflectionAboutCustomWorldNormal(int32 CustomWorldNormal, int32 bNormalizeCustomWorldNormal) = 0;
	virtual int32 CameraVector() = 0;
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual int32 VxgiVoxelization() = 0;
	virtual int32 VxgiTraceCone(int32 PositionArg, int32 DirectionArg, int32 ConeFactorArg, int32 InitialOffsetArg, int32 TracingStepArg, int32 MaxSamples) = 0;
#endif
	// NVCHANGE_END: Add VXGI
	virtual int32 LightVector() = 0;

	virtual int32 ScreenPosition(EMaterialExpressionScreenPositionMapping Mapping) = 0;
	virtual int32 WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets) = 0;
	virtual int32 ObjectWorldPosition() = 0;
	virtual int32 ObjectRadius() = 0;
	virtual int32 ObjectBounds() = 0;	
	virtual int32 DistanceCullFade() = 0;
	virtual int32 ActorWorldPosition() = 0;
	virtual int32 ParticleMacroUV() = 0;
	virtual int32 ParticleSubUV(int32 TextureIndex, EMaterialSamplerType SamplerType, bool bBlend) = 0;
	virtual int32 ParticleColor() = 0;
	virtual int32 ParticlePosition() = 0;
	virtual int32 ParticleRadius() = 0;
	virtual int32 SphericalParticleOpacity(int32 Density) = 0;
	virtual int32 ParticleRelativeTime() = 0;
	virtual int32 ParticleMotionBlurFade() = 0;
	virtual int32 ParticleRandom() = 0;
	virtual int32 ParticleDirection() = 0;
	virtual int32 ParticleSpeed() = 0;
	virtual int32 ParticleSize() = 0;

	virtual int32 FlexFluidSurfaceThickness(int32 Offset, int32 UV, bool bUseOffset) = 0;

	virtual int32 If(int32 A,int32 B,int32 AGreaterThanB,int32 AEqualsB,int32 ALessThanB,int32 Threshold) = 0;

	virtual int32 TextureCoordinate(uint32 CoordinateIndex, bool UnMirrorU, bool UnMirrorV) = 0;
	virtual int32 TextureSample(int32 Texture,int32 Coordinate,enum EMaterialSamplerType SamplerType,int32 MipValue0Index=INDEX_NONE,int32 MipValue1Index=INDEX_NONE,ETextureMipValueMode MipValueMode=TMVM_None,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset,int32 TextureReferenceIndex=INDEX_NONE) = 0;
	virtual int32 TextureProperty(int32 InTexture, EMaterialExposedTextureProperty Property) = 0;

	virtual int32 TextureDecalMipmapLevel(int32 TextureSizeInput) = 0;
	virtual int32 TextureDecalDerivative(bool bDDY) = 0;
	virtual int32 DecalLifetimeOpacity() = 0;

	virtual int32 Texture(UTexture* Texture,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset,ETextureMipValueMode MipValueMode=TMVM_None) = 0;
	virtual int32 TextureParameter(FName ParameterName,UTexture* DefaultTexture,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset) = 0;

	virtual int32 ExternalTexture(const FGuid& ExternalTextureGuid) = 0;
	virtual int32 ExternalTexture(UTexture* InTexture, int32& TextureReferenceIndex) = 0;
	virtual int32 ExternalTextureParameter(FName ParameterName, UTexture* DefaultValue, int32& TextureReferenceIndex) = 0;
	virtual int32 ExternalTextureCoordinateScaleRotation(int32 TextureReferenceIndex, TOptional<FName> ParameterName) = 0;
	virtual int32 ExternalTextureCoordinateScaleRotation(const FGuid& ExternalTextureGuid) = 0;
	virtual int32 ExternalTextureCoordinateOffset(int32 TextureReferenceIndex, TOptional<FName> ParameterName) = 0;
	virtual int32 ExternalTextureCoordinateOffset(const FGuid& ExternalTextureGuid) = 0;

	virtual int32 GetTextureReferenceIndex(UTexture* Texture) { return INDEX_NONE; }

	int32 Texture(UTexture* InTexture,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset)
	{
		int32 TextureReferenceIndex = INDEX_NONE;
		return Texture(InTexture, TextureReferenceIndex, SamplerSource);
	}

	int32 ExternalTexture(UTexture* DefaultTexture)
	{
		int32 TextureReferenceIndex = INDEX_NONE;
		return ExternalTexture(DefaultTexture, TextureReferenceIndex);
	}

	int32 TextureParameter(FName ParameterName,UTexture* DefaultTexture,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset)
	{
		int32 TextureReferenceIndex = INDEX_NONE;
		return  TextureParameter(ParameterName, DefaultTexture, TextureReferenceIndex, SamplerSource);
	}

	int32 ExternalTextureParameter(FName ParameterName, UTexture* DefaultTexture)
	{
		int32 TextureReferenceIndex = INDEX_NONE;
		return ExternalTextureParameter(ParameterName, DefaultTexture, TextureReferenceIndex);
	}

	virtual	int32 PixelDepth()=0;
	virtual int32 SceneDepth(int32 Offset, int32 UV, bool bUseOffset) = 0;
	virtual int32 SceneColor(int32 Offset, int32 UV, bool bUseOffset) = 0;
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureLookup(int32 UV, uint32 SceneTextureId, bool bFiltered) = 0;
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureSize(uint32 SceneTextureId, bool bInvert) = 0;
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureMax(uint32 InSceneTextureId) = 0;
	// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
	virtual int32 SceneTextureMin(uint32 InSceneTextureId) = 0;

	virtual int32 StaticBool(bool Value) = 0;
	virtual int32 StaticBoolParameter(FName ParameterName,bool bDefaultValue) = 0;
	virtual int32 StaticComponentMask(int32 Vector,FName ParameterName,bool bDefaultR,bool bDefaultG,bool bDefaultB,bool bDefaultA) = 0;
	virtual bool GetStaticBoolValue(int32 BoolIndex, bool& bSucceeded) = 0;
	virtual int32 StaticTerrainLayerWeight(FName ParameterName,int32 Default) = 0;

	virtual int32 VertexColor() = 0;

	virtual int32 PreSkinnedPosition() = 0;
	virtual int32 PreSkinnedNormal() = 0;

	virtual int32 VertexInterpolator(uint32 InterpolatorIndex) = 0;

#if WITH_EDITOR
	virtual int32 MaterialBakingWorldPosition() = 0;
#endif

	virtual int32 Add(int32 A,int32 B) = 0;
	virtual int32 Sub(int32 A,int32 B) = 0;
	virtual int32 Mul(int32 A,int32 B) = 0;
	virtual int32 Div(int32 A,int32 B) = 0;
	virtual int32 Dot(int32 A,int32 B) = 0;
	virtual int32 Cross(int32 A,int32 B) = 0;

	virtual int32 Power(int32 Base,int32 Exponent) = 0;
	virtual int32 Logarithm2(int32 X) = 0;
	virtual int32 Logarithm10(int32 X) = 0;
	virtual int32 SquareRoot(int32 X) = 0;
	virtual int32 Length(int32 X) = 0;

	virtual int32 Lerp(int32 X,int32 Y,int32 A) = 0;
	virtual int32 Min(int32 A,int32 B) = 0;
	virtual int32 Max(int32 A,int32 B) = 0;
	virtual int32 Clamp(int32 X,int32 A,int32 B) = 0;
	virtual int32 Saturate(int32 X) = 0;

	virtual int32 ComponentMask(int32 Vector,bool R,bool G,bool B,bool A) = 0;
	virtual int32 AppendVector(int32 A,int32 B) = 0;
	virtual int32 TransformVector(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) = 0;
	virtual int32 TransformPosition(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) = 0;

	virtual int32 DynamicParameter(FLinearColor& DefaultValue) = 0;
	virtual int32 LightmapUVs() = 0;
	virtual int32 PrecomputedAOMask()  = 0;

	virtual int32 LightmassReplace(int32 Realtime, int32 Lightmass) = 0;
	virtual int32 GIReplace(int32 Direct, int32 StaticIndirect, int32 DynamicIndirect) = 0;
	virtual int32 MaterialProxyReplace(int32 Realtime, int32 MaterialProxy) = 0;

	virtual int32 ObjectOrientation() = 0;
	virtual int32 RotateAboutAxis(int32 NormalizedRotationAxisAndAngleIndex, int32 PositionOnAxisIndex, int32 PositionIndex) = 0;
	virtual int32 TwoSidedSign() = 0;
	virtual int32 VertexNormal() = 0;
	virtual int32 PixelNormalWS() = 0;

	virtual int32 CustomExpression(class UMaterialExpressionCustom* Custom, TArray<int32>& CompiledInputs) = 0;
	virtual int32 CustomOutput(class UMaterialExpressionCustomOutput* Custom, int32 OutputIndex, int32 OutputCode) = 0;

	virtual int32 DDX(int32 X) = 0;
	virtual int32 DDY(int32 X) = 0;

	virtual int32 PerInstanceRandom() = 0;
	virtual int32 PerInstanceFadeAmount() = 0;
	virtual int32 AntialiasedTextureMask(int32 Tex, int32 UV, float Threshold, uint8 Channel) = 0;
	virtual int32 Sobol(int32 Cell, int32 Index, int32 Seed) = 0;
	virtual int32 TemporalSobol(int32 Index, int32 Seed) = 0;
	virtual int32 Noise(int32 Position, float Scale, int32 Quality, uint8 NoiseFunction, bool bTurbulence, int32 Levels, float OutputMin, float OutputMax, float LevelScale, int32 FilterWidth, bool bTiling, uint32 RepeatSize) = 0;
	virtual int32 VectorNoise(int32 Position, int32 Quality, uint8 NoiseFunction, bool bTiling, uint32 RepeatSize) = 0;
	virtual int32 BlackBody( int32 Temp ) = 0;
	virtual int32 DistanceToNearestSurface(int32 PositionArg) = 0;
	virtual int32 DistanceFieldGradient(int32 PositionArg) = 0;
	virtual int32 DepthOfFieldFunction(int32 Depth, int32 FunctionValueIndex) = 0;
	virtual int32 AtmosphericFogColor(int32 WorldPosition) = 0;
	virtual int32 RotateScaleOffsetTexCoords(int32 TexCoordCodeIndex, int32 RotationScale, int32 Offset) = 0;
	virtual int32 SpeedTree(ESpeedTreeGeometryType GeometryType, ESpeedTreeWindType WindType, ESpeedTreeLODType LODType, float BillboardThreshold, bool bAccurateWindVelocities) = 0;
	virtual int32 TextureCoordinateOffset() = 0;
	virtual int32 EyeAdaptation() = 0;
	virtual int32 AtmosphericLightVector() = 0;
	virtual int32 AtmosphericLightColor() = 0;
	// The compiler can run in a different state and this affects caching of sub expression, Expressions are different (e.g. View.PrevWorldViewOrigin) when using previous frame's values
	// If possible we should re-factor this to avoid having to deal with compiler state
	virtual bool IsCurrentlyCompilingForPreviousFrame() const { return false; }

// WaveWorks Start
	virtual int32 WaveWorks(FString OutputName) = 0;
// WaveWorks End
};

/** 
 * A proxy for the material compiler interface which by default passes all function calls unmodified. 
 * Note: Any functions of FMaterialCompiler that change the internal compiler state must be routed!
 */
class FProxyMaterialCompiler : public FMaterialCompiler
{
public:

	// Constructor.
	FProxyMaterialCompiler(FMaterialCompiler* InCompiler):
		Compiler(InCompiler)
	{}

	// Simple pass through all other material operations unmodified.

	virtual EMaterialShadingModel GetMaterialShadingModel() const { return Compiler->GetMaterialShadingModel();  }
	virtual EMaterialValueType GetParameterType(int32 Index) const { return Compiler->GetParameterType(Index); }
	virtual FMaterialUniformExpression* GetParameterUniformExpression(int32 Index) const { return Compiler->GetParameterUniformExpression(Index); }
	virtual void SetMaterialProperty(EMaterialProperty InProperty, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) override { Compiler->SetMaterialProperty(InProperty, OverrideShaderFrequency, bUsePreviousFrameTime); }
	virtual void PushMaterialAttribute(const FGuid& InAttributeID) override { Compiler->PushMaterialAttribute(InAttributeID); }
	virtual FGuid PopMaterialAttribute() override { return Compiler->PopMaterialAttribute(); }
	virtual const FGuid GetMaterialAttribute() override { return Compiler->GetMaterialAttribute(); }
	virtual void SetBaseMaterialAttribute(const FGuid& InAttributeID) override { Compiler->SetBaseMaterialAttribute(InAttributeID); }
	virtual EShaderFrequency GetCurrentShaderFrequency() const override { return Compiler->GetCurrentShaderFrequency(); }
	virtual int32 Error(const TCHAR* Text) override { return Compiler->Error(Text); }

	virtual int32 CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* InCompiler) override { return Compiler->CallExpression(ExpressionKey,InCompiler); }

	virtual void PushFunction(const FMaterialFunctionCompileState& FunctionState) override { Compiler->PushFunction(FunctionState); }
	virtual FMaterialFunctionCompileState PopFunction() override { return Compiler->PopFunction(); }

	virtual EMaterialValueType GetType(int32 Code) override { return Compiler->GetType(Code); }
	virtual EMaterialQualityLevel::Type GetQualityLevel() override { return Compiler->GetQualityLevel(); }
	virtual ERHIFeatureLevel::Type GetFeatureLevel() override { return Compiler->GetFeatureLevel(); }
	virtual int32 ValidCast(int32 Code,EMaterialValueType DestType) override { return Compiler->ValidCast(Code, DestType); }
	virtual int32 ForceCast(int32 Code,EMaterialValueType DestType,uint32 ForceCastFlags = 0) override
	{ return Compiler->ForceCast(Code,DestType,ForceCastFlags); }

	virtual int32 AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex) override { return Compiler->AccessCollectionParameter(ParameterCollection, ParameterIndex, ComponentIndex); }
	virtual int32 VectorParameter(FName ParameterName,const FLinearColor& DefaultValue) override { return Compiler->VectorParameter(ParameterName,DefaultValue); }
	virtual int32 ScalarParameter(FName ParameterName,float DefaultValue) override { return Compiler->ScalarParameter(ParameterName,DefaultValue); }

	virtual int32 Constant(float X) override { return Compiler->Constant(X); }
	virtual int32 Constant2(float X,float Y) override { return Compiler->Constant2(X,Y); }
	virtual int32 Constant3(float X,float Y,float Z) override { return Compiler->Constant3(X,Y,Z); }
	virtual int32 Constant4(float X,float Y,float Z,float W) override { return Compiler->Constant4(X,Y,Z,W); }
	
	virtual	int32 ViewProperty(EMaterialExposedViewProperty Property, bool InvProperty) override { return Compiler->ViewProperty(Property, InvProperty); }

	virtual int32 GameTime(bool bPeriodic, float Period) override { return Compiler->GameTime(bPeriodic, Period); }
	virtual int32 RealTime(bool bPeriodic, float Period) override { return Compiler->RealTime(bPeriodic, Period); }

	virtual int32 PeriodicHint(int32 PeriodicCode) override { return Compiler->PeriodicHint(PeriodicCode); }

	virtual int32 Sine(int32 X) override { return Compiler->Sine(X); }
	virtual int32 Cosine(int32 X) override { return Compiler->Cosine(X); }
	virtual int32 Tangent(int32 X) override { return Compiler->Tangent(X); }
	virtual int32 Arcsine(int32 X) override { return Compiler->Arcsine(X); }
	virtual int32 ArcsineFast(int32 X) override { return Compiler->ArcsineFast(X); }
	virtual int32 Arccosine(int32 X) override { return Compiler->Arccosine(X); }
	virtual int32 ArccosineFast(int32 X) override { return Compiler->ArccosineFast(X); }
	virtual int32 Arctangent(int32 X) override { return Compiler->Arctangent(X); }
	virtual int32 ArctangentFast(int32 X) override { return Compiler->ArctangentFast(X); }
	virtual int32 Arctangent2(int32 Y, int32 X) override { return Compiler->Arctangent2(Y, X); }
	virtual int32 Arctangent2Fast(int32 Y, int32 X) override { return Compiler->Arctangent2Fast(Y, X); }

	virtual int32 Floor(int32 X) override { return Compiler->Floor(X); }
	virtual int32 Ceil(int32 X) override { return Compiler->Ceil(X); }
	virtual int32 Round(int32 X) override { return Compiler->Round(X); }
	virtual int32 Truncate(int32 X) override { return Compiler->Truncate(X); }
	virtual int32 Sign(int32 X) override { return Compiler->Sign(X); }
	virtual int32 Frac(int32 X) override { return Compiler->Frac(X); }
	virtual int32 Fmod(int32 A, int32 B) override { return Compiler->Fmod(A,B); }
	virtual int32 Abs(int32 X) override { return Compiler->Abs(X); }

	virtual int32 ReflectionVector() override { return Compiler->ReflectionVector(); }
	virtual int32 CameraVector() override { return Compiler->CameraVector(); }
	virtual int32 LightVector() override { return Compiler->LightVector(); }

	virtual int32 ScreenPosition(EMaterialExpressionScreenPositionMapping Mapping = MESP_SceneTextureUV) override { return Compiler->ScreenPosition(Mapping); }

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual int32 VxgiVoxelization() override { return Compiler->VxgiVoxelization(); }
	virtual int32 VxgiTraceCone(int32 PositionArg, int32 DirectionArg, int32 ConeFactorArg, int32 InitialOffsetArg, int32 TracingStepArg, int32 MaxSamples) override { return Compiler->VxgiTraceCone(PositionArg, DirectionArg, ConeFactorArg, InitialOffsetArg, TracingStepArg, MaxSamples); }
#endif
	// NVCHANGE_END: Add VXGI

	virtual int32 WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets) override { return Compiler->WorldPosition(WorldPositionIncludedOffsets); }
	virtual int32 ObjectWorldPosition() override { return Compiler->ObjectWorldPosition(); }
	virtual int32 ObjectRadius() override { return Compiler->ObjectRadius(); }
	virtual int32 ObjectBounds() override { return Compiler->ObjectBounds(); }
	virtual int32 DistanceCullFade() override { return Compiler->DistanceCullFade(); }
	virtual int32 ActorWorldPosition() override { return Compiler->ActorWorldPosition(); }
	virtual int32 ParticleMacroUV() override { return Compiler->ParticleMacroUV(); }
	virtual int32 ParticleSubUV(int32 TextureIndex, EMaterialSamplerType SamplerType, bool bBlend) override { return Compiler->ParticleSubUV(TextureIndex, SamplerType, bBlend); }
	virtual int32 ParticleColor() override { return Compiler->ParticleColor(); }
	virtual int32 ParticlePosition() override { return Compiler->ParticlePosition(); }
	virtual int32 ParticleRadius() override { return Compiler->ParticleRadius(); }
	virtual int32 SphericalParticleOpacity(int32 Density) override { return Compiler->SphericalParticleOpacity(Density); }

	virtual int32 FlexFluidSurfaceThickness(int32 Offset, int32 UV, bool bUseOffset) override { return Compiler->FlexFluidSurfaceThickness(Offset, UV, bUseOffset); }

	virtual int32 If(int32 A,int32 B,int32 AGreaterThanB,int32 AEqualsB,int32 ALessThanB,int32 Threshold) override { return Compiler->If(A,B,AGreaterThanB,AEqualsB,ALessThanB,Threshold); }

	virtual int32 TextureSample(int32 InTexture,int32 Coordinate,enum EMaterialSamplerType SamplerType,int32 MipValue0Index,int32 MipValue1Index,ETextureMipValueMode MipValueMode,ESamplerSourceMode SamplerSource,int32 TextureReferenceIndex) override 
		{ return Compiler->TextureSample(InTexture,Coordinate,SamplerType,MipValue0Index,MipValue1Index,MipValueMode,SamplerSource,TextureReferenceIndex); }
	virtual int32 TextureProperty(int32 InTexture, EMaterialExposedTextureProperty Property) override 
		{ return Compiler->TextureProperty(InTexture, Property); }

	virtual int32 TextureCoordinate(uint32 CoordinateIndex, bool UnMirrorU, bool UnMirrorV) override { return Compiler->TextureCoordinate(CoordinateIndex, UnMirrorU, UnMirrorV); }

	virtual int32 TextureDecalMipmapLevel(int32 TextureSizeInput) override { return Compiler->TextureDecalMipmapLevel(TextureSizeInput); }
	virtual int32 TextureDecalDerivative(bool bDDY) override { return Compiler->TextureDecalDerivative(bDDY); }
	virtual int32 DecalLifetimeOpacity() override { return Compiler->DecalLifetimeOpacity(); }

	virtual int32 Texture(UTexture* InTexture,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset,ETextureMipValueMode MipValueMode=TMVM_None) override { return Compiler->Texture(InTexture,TextureReferenceIndex,SamplerSource,MipValueMode); }
	virtual int32 TextureParameter(FName ParameterName,UTexture* DefaultValue,int32& TextureReferenceIndex,ESamplerSourceMode SamplerSource=SSM_FromTextureAsset) override { return Compiler->TextureParameter(ParameterName,DefaultValue,TextureReferenceIndex,SamplerSource); }

	virtual int32 ExternalTexture(const FGuid& ExternalTextureGuid) override { return Compiler->ExternalTexture(ExternalTextureGuid); }
	virtual int32 ExternalTexture(UTexture* InTexture, int32& TextureReferenceIndex) override { return Compiler->ExternalTexture(InTexture, TextureReferenceIndex); }
	virtual int32 ExternalTextureParameter(FName ParameterName, UTexture* DefaultValue, int32& TextureReferenceIndex) override { return Compiler->ExternalTextureParameter(ParameterName, DefaultValue, TextureReferenceIndex); }
	virtual int32 ExternalTextureCoordinateScaleRotation(int32 TextureReferenceIndex, TOptional<FName> ParameterName) override { return Compiler->ExternalTextureCoordinateScaleRotation(TextureReferenceIndex, ParameterName); }
	virtual int32 ExternalTextureCoordinateScaleRotation(const FGuid& ExternalTextureGuid) override { return Compiler->ExternalTextureCoordinateScaleRotation(ExternalTextureGuid); }
	virtual int32 ExternalTextureCoordinateOffset(int32 TextureReferenceIndex, TOptional<FName> ParameterName) override { return Compiler->ExternalTextureCoordinateOffset(TextureReferenceIndex, ParameterName); }
	virtual int32 ExternalTextureCoordinateOffset(const FGuid& ExternalTextureGuid) override { return Compiler->ExternalTextureCoordinateOffset(ExternalTextureGuid); }

	virtual	int32 PixelDepth() override { return Compiler->PixelDepth();	}
	virtual int32 SceneDepth(int32 Offset, int32 UV, bool bUseOffset) override { return Compiler->SceneDepth(Offset, UV, bUseOffset); }
	virtual int32 SceneColor(int32 Offset, int32 UV, bool bUseOffset) override { return Compiler->SceneColor(Offset, UV, bUseOffset); }
	virtual int32 SceneTextureLookup(int32 UV, uint32 InSceneTextureId, bool bFiltered) override { return Compiler->SceneTextureLookup(UV, InSceneTextureId, bFiltered); }
	virtual int32 SceneTextureSize(uint32 InSceneTextureId, bool bInvert) override { return Compiler->SceneTextureSize(InSceneTextureId, bInvert); }
	virtual int32 SceneTextureMax(uint32 InSceneTextureId) override { return Compiler->SceneTextureMax(InSceneTextureId); }
	virtual int32 SceneTextureMin(uint32 InSceneTextureId) override { return Compiler->SceneTextureMin(InSceneTextureId); }

	virtual int32 StaticBool(bool Value) override { return Compiler->StaticBool(Value); }
	virtual int32 StaticBoolParameter(FName ParameterName,bool bDefaultValue) override { return Compiler->StaticBoolParameter(ParameterName,bDefaultValue); }
	virtual int32 StaticComponentMask(int32 Vector,FName ParameterName,bool bDefaultR,bool bDefaultG,bool bDefaultB,bool bDefaultA) override { return Compiler->StaticComponentMask(Vector,ParameterName,bDefaultR,bDefaultG,bDefaultB,bDefaultA); }
	virtual bool GetStaticBoolValue(int32 BoolIndex, bool& bSucceeded) override { return Compiler->GetStaticBoolValue(BoolIndex, bSucceeded); }
	virtual int32 StaticTerrainLayerWeight(FName ParameterName,int32 Default) override { return Compiler->StaticTerrainLayerWeight(ParameterName,Default); }

	virtual int32 VertexColor() override { return Compiler->VertexColor(); }
	
	virtual int32 PreSkinnedPosition() override { return Compiler->PreSkinnedPosition(); }
	virtual int32 PreSkinnedNormal() override { return Compiler->PreSkinnedNormal(); }

	virtual int32 Add(int32 A,int32 B) override { return Compiler->Add(A,B); }
	virtual int32 Sub(int32 A,int32 B) override { return Compiler->Sub(A,B); }
	virtual int32 Mul(int32 A,int32 B) override { return Compiler->Mul(A,B); }
	virtual int32 Div(int32 A,int32 B) override { return Compiler->Div(A,B); }
	virtual int32 Dot(int32 A,int32 B) override { return Compiler->Dot(A,B); }
	virtual int32 Cross(int32 A,int32 B) override { return Compiler->Cross(A,B); }

	virtual int32 Power(int32 Base,int32 Exponent) override { return Compiler->Power(Base,Exponent); }
	virtual int32 Logarithm2(int32 X) override { return Compiler->Logarithm2(X); }
	virtual int32 Logarithm10(int32 X) override { return Compiler->Logarithm10(X); }
	virtual int32 SquareRoot(int32 X) override { return Compiler->SquareRoot(X); }
	virtual int32 Length(int32 X) override { return Compiler->Length(X); }

	virtual int32 Lerp(int32 X,int32 Y,int32 A) override { return Compiler->Lerp(X,Y,A); }
	virtual int32 Min(int32 A,int32 B) override { return Compiler->Min(A,B); }
	virtual int32 Max(int32 A,int32 B) override { return Compiler->Max(A,B); }
	virtual int32 Clamp(int32 X,int32 A,int32 B) override { return Compiler->Clamp(X,A,B); }
	virtual int32 Saturate(int32 X) override { return Compiler->Saturate(X); }

	virtual int32 ComponentMask(int32 Vector,bool R,bool G,bool B,bool A) override { return Compiler->ComponentMask(Vector,R,G,B,A); }
	virtual int32 AppendVector(int32 A,int32 B) override { return Compiler->AppendVector(A,B); }
	virtual int32 TransformVector(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) override
	{
		return Compiler->TransformVector(SourceCoordBasis, DestCoordBasis, A);
	}
	virtual int32 TransformPosition(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A) override
	{
		return Compiler->TransformPosition(SourceCoordBasis, DestCoordBasis, A);
	}

	virtual int32 DynamicParameter(FLinearColor& DefaultValue) override { return Compiler->DynamicParameter(DefaultValue); }
	virtual int32 LightmapUVs() override { return Compiler->LightmapUVs(); }
	virtual int32 PrecomputedAOMask() override { return Compiler->PrecomputedAOMask(); }

	virtual int32 LightmassReplace(int32 Realtime, int32 Lightmass) override { return Realtime; }
	virtual int32 GIReplace(int32 Direct, int32 StaticIndirect, int32 DynamicIndirect) override { return Compiler->GIReplace(Direct, StaticIndirect, DynamicIndirect); }
	virtual int32 MaterialProxyReplace(int32 Realtime, int32 MaterialProxy) override { return Realtime; }
	virtual int32 ObjectOrientation() override { return Compiler->ObjectOrientation(); }
	virtual int32 RotateAboutAxis(int32 NormalizedRotationAxisAndAngleIndex, int32 PositionOnAxisIndex, int32 PositionIndex) override
	{
		return Compiler->RotateAboutAxis(NormalizedRotationAxisAndAngleIndex, PositionOnAxisIndex, PositionIndex);
	}
	virtual int32 TwoSidedSign() override { return Compiler->TwoSidedSign(); }
	virtual int32 VertexNormal() override { return Compiler->VertexNormal(); }
	virtual int32 PixelNormalWS() override { return Compiler->PixelNormalWS(); }

	virtual int32 CustomExpression(class UMaterialExpressionCustom* Custom, TArray<int32>& CompiledInputs) override { return Compiler->CustomExpression(Custom,CompiledInputs); }
	virtual int32 CustomOutput(class UMaterialExpressionCustomOutput* Custom, int32 OutputIndex, int32 OutputCode) override{ return Compiler->CustomOutput(Custom, OutputIndex, OutputCode); }
	virtual int32 DDX(int32 X) override { return Compiler->DDX(X); }
	virtual int32 DDY(int32 X) override { return Compiler->DDY(X); }

	virtual int32 AntialiasedTextureMask(int32 Tex, int32 UV, float Threshold, uint8 Channel) override
	{
		return Compiler->AntialiasedTextureMask(Tex, UV, Threshold, Channel);
	}
	virtual int32 Sobol(int32 Cell, int32 Index, int32 Seed) override {	return Compiler->Sobol(Cell, Index, Seed); }
	virtual int32 TemporalSobol(int32 Index, int32 Seed) override { return Compiler->TemporalSobol(Index, Seed); }
	virtual int32 Noise(int32 Position, float Scale, int32 Quality, uint8 NoiseFunction, bool bTurbulence, int32 Levels, float OutputMin, float OutputMax, float LevelScale, int32 FilterWidth, bool bTiling, uint32 TileSize) override
	{
		return Compiler->Noise(Position, Scale, Quality, NoiseFunction, bTurbulence, Levels, OutputMin, OutputMax, LevelScale, FilterWidth, bTiling, TileSize);
	}
	virtual int32 VectorNoise(int32 Position, int32 Quality, uint8 NoiseFunction, bool bTiling, uint32 TileSize) override
	{
		return Compiler->VectorNoise(Position, Quality, NoiseFunction, bTiling, TileSize);
	}
	virtual int32 BlackBody( int32 Temp ) override { return Compiler->BlackBody(Temp); }
	virtual int32 DistanceToNearestSurface(int32 PositionArg) override { return Compiler->DistanceToNearestSurface(PositionArg); }
	virtual int32 DistanceFieldGradient(int32 PositionArg) override { return Compiler->DistanceFieldGradient(PositionArg); }
	virtual int32 PerInstanceRandom() override { return Compiler->PerInstanceRandom(); }
	virtual int32 PerInstanceFadeAmount() override { return Compiler->PerInstanceFadeAmount(); }
	virtual int32 DepthOfFieldFunction(int32 Depth, int32 FunctionValueIndex) override
	{
		return Compiler->DepthOfFieldFunction(Depth, FunctionValueIndex);
	}

	virtual int32 RotateScaleOffsetTexCoords(int32 TexCoordCodeIndex, int32 RotationScale, int32 Offset) override
	{
		return Compiler->RotateScaleOffsetTexCoords(TexCoordCodeIndex, RotationScale, Offset);
	}

	virtual int32 SpeedTree(ESpeedTreeGeometryType GeometryType, ESpeedTreeWindType WindType, ESpeedTreeLODType LODType, float BillboardThreshold, bool bAccurateWindVelocities) override 
	{ 
		return Compiler->SpeedTree(GeometryType, WindType, LODType, BillboardThreshold, bAccurateWindVelocities); 
	}

	virtual int32 AtmosphericFogColor(int32 WorldPosition) override
	{
		return Compiler->AtmosphericFogColor(WorldPosition);
	}

	virtual int32 AtmosphericLightVector() override
	{
		return Compiler->AtmosphericLightVector();
	}

	virtual int32 AtmosphericLightColor() override
	{
		return Compiler->AtmosphericLightColor();
	}

	virtual int32 TextureCoordinateOffset() override
	{
		return Compiler->TextureCoordinateOffset();
	}

	virtual int32 EyeAdaptation() override
	{
		return Compiler->EyeAdaptation();
	}

	// WaveWorks Start
	virtual int32 WaveWorks(FString OutputName)
	{
		return Compiler->WaveWorks(OutputName);
	}
	// WaveWorks End

protected:
		
	FMaterialCompiler* Compiler;
};

// Helper class to handle MaterialAttribute changes on the compiler stack
class FScopedMaterialCompilerAttribute
{
public:
	FScopedMaterialCompilerAttribute(FMaterialCompiler* InCompiler, const FGuid& InAttributeID)
	: Compiler(InCompiler)
	, AttributeID(InAttributeID)
	{
		check(Compiler);
		Compiler->PushMaterialAttribute(AttributeID);
	}

	~FScopedMaterialCompilerAttribute()
	{
		verify(AttributeID == Compiler->PopMaterialAttribute());
	}

private:
	FMaterialCompiler*	Compiler;
	FGuid				AttributeID;
};
