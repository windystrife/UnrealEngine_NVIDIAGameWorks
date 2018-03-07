// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleBeamTrailVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "ParticleBeamTrailVertexFactory.h"
#include "ParticleHelper.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FParticleBeamTrailUniformParameters,TEXT("BeamTrailVF"));

/**
 * Shader parameters for the beam/trail vertex factory.
 */
class FParticleBeamTrailVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
	}

	virtual void Serialize(FArchive& Ar) override
	{
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		FParticleBeamTrailVertexFactory* BeamTrailVF = (FParticleBeamTrailVertexFactory*)VertexFactory;
		SetUniformBufferParameter(RHICmdList, Shader->GetVertexShader(), Shader->GetUniformBufferParameter<FParticleBeamTrailUniformParameters>(), BeamTrailVF->GetBeamTrailUniformBuffer() );
	}
};

///////////////////////////////////////////////////////////////////////////////
/**
 * The particle system beam trail vertex declaration resource type.
 */
class FParticleBeamTrailVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	FParticleBeamTrailVertexDeclaration(bool bInUsesDynamicParameter)
		: bUsesDynamicParameter(bInUsesDynamicParameter)
	{
	}
	virtual ~FParticleBeamTrailVertexDeclaration() {}

	virtual void FillDeclElements(FVertexDeclarationElementList& Elements, int32& Offset)
	{
		uint16 Stride = sizeof(FParticleBeamTrailVertex);
		/** The stream to read the vertex position from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 0, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex old position from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float3, 1, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the vertex size/rot/subimage from. */
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 2, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the color from.					*/
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 4, Stride));
		Offset += sizeof(float) * 4;
		/** The stream to read the texture coordinates from.	*/
		Elements.Add(FVertexElement(0, Offset, VET_Float4, 3, Stride));
		Offset += sizeof(float) * 4;
		
		/** Dynamic parameters come from a second stream */
		Elements.Add(FVertexElement(1, 0, VET_Float4, 5, bUsesDynamicParameter ? sizeof(FVector4) : 0));
	}

	virtual void InitDynamicRHI()
	{
		FVertexDeclarationElementList Elements;
		int32	Offset = 0;
		FillDeclElements(Elements, Offset);

		// Create the vertex declaration for rendering the factory normally.
		// This is done in InitDynamicRHI instead of InitRHI to allow FParticleBeamTrailVertexFactory::InitRHI
		// to rely on it being initialized, since InitDynamicRHI is called before InitRHI.
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseDynamicRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}

protected:
	bool bUsesDynamicParameter;
};

/** The simple element vertex declaration. */
static TGlobalResource<FParticleBeamTrailVertexDeclaration> GParticleBeamTrailVertexDeclaration(false);
static TGlobalResource<FParticleBeamTrailVertexDeclaration> GParticleBeamTrailVertexDeclarationDynamic(true);

///////////////////////////////////////////////////////////////////////////////

bool FParticleBeamTrailVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return Material->IsUsedWithBeamTrails() || Material->IsSpecialEngineMaterial();
}

/**
 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
 */
void FParticleBeamTrailVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	FParticleVertexFactoryBase::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("PARTICLE_BEAMTRAIL_FACTORY"),TEXT("1"));
}

/**
 *	Initialize the Render Hardware Interface for this vertex factory
 */
void FParticleBeamTrailVertexFactory::InitRHI()
{
	SetDeclaration(bUsesDynamicParameter ? GParticleBeamTrailVertexDeclarationDynamic.VertexDeclarationRHI
		: GParticleBeamTrailVertexDeclaration.VertexDeclarationRHI);

	FVertexStream* VertexStream = new(Streams) FVertexStream;
	FVertexStream* DynamicParameterStream = new(Streams) FVertexStream;
}

FVertexFactoryShaderParameters* FParticleBeamTrailVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleBeamTrailVertexFactoryShaderParameters() : NULL;
}

void FParticleBeamTrailVertexFactory::SetVertexBuffer(const FVertexBuffer* InBuffer, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 2);
	FVertexStream& VertexStream = Streams[0];
	VertexStream.VertexBuffer = InBuffer;
	VertexStream.Stride = Stride;
	VertexStream.Offset = StreamOffset;
}

void FParticleBeamTrailVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride)
{
	check(Streams.Num() == 2);
	FVertexStream& DynamicParameterStream = Streams[1];
	if (InDynamicParameterBuffer)
	{
		DynamicParameterStream.VertexBuffer = InDynamicParameterBuffer;
		ensure(bUsesDynamicParameter);
		DynamicParameterStream.Stride = Stride;
		DynamicParameterStream.Offset = StreamOffset;
	}
	else
	{
		DynamicParameterStream.VertexBuffer = &GNullDynamicParameterVertexBuffer;
		ensure(!bUsesDynamicParameter);
		DynamicParameterStream.Stride = 0;
		DynamicParameterStream.Offset = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleBeamTrailVertexFactory,"/Engine/Private/ParticleBeamTrailVertexFactory.ush",true,false,true,false,false);
