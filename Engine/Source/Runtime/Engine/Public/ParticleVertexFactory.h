// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "SceneView.h"

class FMaterial;

/**
 * Enum identifying the type of a particle vertex factory.
 */
enum EParticleVertexFactoryType
{
	PVFT_Sprite,
	PVFT_BeamTrail,
	PVFT_Mesh,
	PVFT_MAX
};

/**
 * Base class for particle vertex factories.
 */
class FParticleVertexFactoryBase : public FVertexFactory
{
public:

	/** Default constructor. */
	explicit FParticleVertexFactoryBase( EParticleVertexFactoryType Type, ERHIFeatureLevel::Type InFeatureLevel )
		: FVertexFactory(InFeatureLevel)
		, LastFrameSetup(MAX_uint32)
		, LastViewFamily(nullptr)
		, LastView(nullptr)
		, LastFrameRealTime(-1.0f)
		, ParticleFactoryType(Type)
		, bInUse(false)
	{
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment) 
	{
		FVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PARTICLE_FACTORY"),TEXT("1"));
	}

	/** Return the vertex factory type */
	FORCEINLINE EParticleVertexFactoryType GetParticleFactoryType() const
	{
		return ParticleFactoryType;
	}

	inline void SetParticleFactoryType(EParticleVertexFactoryType InType)
	{
		ParticleFactoryType = InType;
	}

	/** Specify whether the factory is in use or not. */
	FORCEINLINE void SetInUse( bool bInInUse )
	{
		bInUse = bInInUse;
	}

	/** Return the vertex factory type */
	FORCEINLINE bool GetInUse() const
	{ 
		return bInUse;
	}

	ERHIFeatureLevel::Type GetFeatureLevel() const { check(HasValidFeatureLevel());  return FRenderResource::GetFeatureLevel(); }

	bool CheckAndUpdateLastFrame(const FSceneViewFamily& ViewFamily, const FSceneView *View = nullptr) const
	{
		if (LastFrameSetup != MAX_uint32 && (&ViewFamily == LastViewFamily) && (View == LastView) && ViewFamily.FrameNumber == LastFrameSetup && LastFrameRealTime == ViewFamily.CurrentRealTime)
		{
			return false;
		}
		LastFrameSetup = ViewFamily.FrameNumber;
		LastFrameRealTime = ViewFamily.CurrentRealTime;
		LastViewFamily = &ViewFamily;
		LastView = View;
		return true;
	}

private:

	/** Last state where we set this. We only need to setup these once per frame, so detemine same frame by number, time, and view family. */
	mutable uint32 LastFrameSetup;
	mutable const FSceneViewFamily *LastViewFamily;
	mutable const FSceneView *LastView;
	mutable float LastFrameRealTime;

	/** The type of the vertex factory. */
	EParticleVertexFactoryType ParticleFactoryType;

	/** Whether the vertex factory is in use. */
	bool bInUse;

};

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_UNIFORM_BUFFER_STRUCT( FParticleSpriteUniformParameters, ENGINE_API)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, AxisLockRight, EShaderPrecisionModifier::Half )
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_EX( FVector4, AxisLockUp, EShaderPrecisionModifier::Half )
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
END_UNIFORM_BUFFER_STRUCT( FParticleSpriteUniformParameters )
typedef TUniformBufferRef<FParticleSpriteUniformParameters> FParticleSpriteUniformBufferRef;

/**
 * Vertex factory for rendering particle sprites.
 */
class ENGINE_API FParticleSpriteVertexFactory : public FParticleVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleSpriteVertexFactory);

public:

	/** Default constructor. */
	FParticleSpriteVertexFactory( EParticleVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FParticleVertexFactoryBase(InType, InFeatureLevel),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		bCustomAlignment(false),
		bUsesDynamicParameter(true),
		DynamicParameterStride(0)
	{}

	FParticleSpriteVertexFactory() 
		: FParticleVertexFactoryBase(PVFT_MAX, ERHIFeatureLevel::Num),
		NumVertsInInstanceBuffer(0),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		bCustomAlignment(false),
		bUsesDynamicParameter(true),
		DynamicParameterStride(0)
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
	inline void SetUsesDynamicParameter(bool bInUsesDynamicParameter, uint32 Stride)
	{
		bUsesDynamicParameter = bInUsesDynamicParameter;
		DynamicParameterStride = Stride;
	}

	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetSpriteUniformBuffer( const FParticleSpriteUniformBufferRef& InSpriteUniformBuffer )
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

	void SetCustomAlignment(bool bAlign)
	{
		bCustomAlignment = bAlign;
	}

	bool GetCustomAlignment()
	{
		return bCustomAlignment;
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
	bool bCustomAlignment;
	bool bUsesDynamicParameter;
	uint32 DynamicParameterStride;
};
