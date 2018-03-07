// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "NiagaraDataSet.h"
#include "SceneView.h"

class FMaterial;

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FNiagaraSpriteUniformParameters, NIAGARAVERTEXFACTORIES_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector, CustomFacingVectorMask, EShaderPrecisionModifier::Half)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, TangentSelector, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, NormalsSphereCenter, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, NormalsCylinderUnitDirection, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, SubImageSize, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector, CameraFacingBlend, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, RemoveHMDRoll, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER( FVector4, MacroUVParameters )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, RotationScale, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, RotationBias, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, NormalsType, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( float, InvDeltaSeconds, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector2D, PivotOffset, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, PositionDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, VelocityDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, RotationDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, SizeDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, SubimageDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, ColorDataOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, FacingOffset)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(int, AlignmentOffset)
	END_UNIFORM_BUFFER_STRUCT(FNiagaraSpriteUniformParameters)

typedef TUniformBufferRef<FNiagaraSpriteUniformParameters> FNiagaraSpriteUniformBufferRef;

/**
 * Vertex factory for rendering particle sprites.
 */
class NIAGARAVERTEXFACTORIES_API FNiagaraSpriteVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraSpriteVertexFactory);

public:

	/** Default constructor. */
	FNiagaraSpriteVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		bCustomAlignment(false)
	{}

	FNiagaraSpriteVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		bCustomAlignment(false)
	{}

	// FRenderResource interface.
	virtual void InitRHI() override;

	virtual bool RendersPrimitivesAsCameraFacingSprites() const override { return true; }

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Set the source vertex buffer that contains particle instance data.
	 */
	void SetInstanceBuffer(const FVertexBuffer* InInstanceBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced);

	void SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer);

	inline void SetNumVertsInInstanceBuffer(int32 InNumVertsInInstanceBuffer)
	{
		NumVertsInInstanceBuffer = InNumVertsInInstanceBuffer;
	}

	/**
	 * Set the source vertex buffer that contains particle dynamic parameter data.
	 */
	void SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride, bool bInstanced);

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetSpriteUniformBuffer( const FNiagaraSpriteUniformBufferRef& InSpriteUniformBuffer )
	{
		SpriteUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FUniformBufferRHIParamRef GetSpriteUniformBuffer()
	{
		return SpriteUniformBuffer;
	}

	void SetCutoutParameters(int32 InNumCutoutVerticesPerFrame, FShaderResourceViewRHIParamRef InCutoutGeometrySRV)
	{
		NumCutoutVerticesPerFrame = InNumCutoutVerticesPerFrame;
		CutoutGeometrySRV = InCutoutGeometrySRV;
	}

	inline int32 GetNumCutoutVerticesPerFrame() const { return NumCutoutVerticesPerFrame; }
	inline FShaderResourceViewRHIParamRef GetCutoutGeometrySRV() const { return CutoutGeometrySRV; }

	void SetParticleData(const FNiagaraDataSet *InDataSet)
	{
		DataSet = InDataSet;
	}

	inline FShaderResourceViewRHIParamRef GetFloatDataSRV() const 
	{ 
		check(!IsInGameThread());
		return DataSet->PrevDataRender().GetGPUBufferFloat()->SRV; 
	}
	inline FShaderResourceViewRHIParamRef GetIntDataSRV() const 
	{ 
		check(!IsInGameThread());
		return DataSet->PrevDataRender().GetGPUBufferInt()->SRV;
	}

	uint32 GetComponentBufferSize() 
	{ 
		check(!IsInGameThread());
		return DataSet->PrevDataRender().GetFloatStride() / sizeof(float);
	}

	void SetCustomAlignment(bool bAlign)
	{
		bCustomAlignment = bAlign;
	}

	void SetVectorAligned(bool bAlign)
	{
		bVectorAligned = bAlign;
	}

	bool GetCustomAlignment()
	{
		return bCustomAlignment;
	}

	bool GetVectorAligned()
	{
		return bVectorAligned;
	}

	void SetCameraPlaneFacing(bool inFacing)
	{
		bCameraPlaneFacing = inFacing;
	}

	bool GetCameraPlaneFacing()
	{
		return bCameraPlaneFacing;
	}

	/**
	 * Construct shader parameters for this type of vertex factory.
	 */
	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

protected:
	/** Initialize streams for this vertex factory. */
	void InitStreams();

private:

	int32 NumVertsInInstanceBuffer;

	/** Uniform buffer with sprite paramters. */
	FUniformBufferRHIParamRef SpriteUniformBuffer;

	int32 NumCutoutVerticesPerFrame;
	FShaderResourceViewRHIParamRef CutoutGeometrySRV;
	const FNiagaraDataSet *DataSet;
	bool bCustomAlignment;	// uses a custom alignment vector from particle data
	bool bVectorAligned;	// is either velocity or custom vector aligned (not aligned to camera up and right)
	bool bCameraPlaneFacing;
};
