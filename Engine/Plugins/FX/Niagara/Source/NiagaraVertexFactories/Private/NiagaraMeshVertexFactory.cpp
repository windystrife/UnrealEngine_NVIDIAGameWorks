// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.cpp: Particle vertex factory implementation.
=============================================================================*/

#include "NiagaraMeshVertexFactory.h"
#include "ParticleHelper.h"
#include "ParticleResources.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNiagaraMeshUniformParameters,TEXT("NiagaraMeshVF"));

// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

class FNiagaraMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		PrevTransformBuffer.Bind(ParameterMap, TEXT("PrevTransformBuffer"));
		NiagaraParticleDataFloat.Bind(ParameterMap, TEXT("NiagaraParticleDataFloat"));
		NiagaraParticleDataInt.Bind(ParameterMap, TEXT("NiagaraParticleDataInt"));
		SafeComponentBufferSizeParam.Bind(ParameterMap, TEXT("SafeComponentBufferSize"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << PrevTransformBuffer;
		Ar << NiagaraParticleDataFloat;
		Ar << NiagaraParticleDataInt;
		Ar << SafeComponentBufferSizeParam;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override
	{
		const bool bInstanced = GRHISupportsInstancing;
		FNiagaraMeshVertexFactory* NiagaraMeshVF = (FNiagaraMeshVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FNiagaraMeshUniformParameters>(), NiagaraMeshVF->GetUniformBuffer());

		SetSRVParameter(RHICmdList, VertexShaderRHI, PrevTransformBuffer, NiagaraMeshVF->GetPreviousTransformBufferSRV());

		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataFloat, NiagaraMeshVF->GetFloatDataSRV());
		SetSRVParameter(RHICmdList, VertexShaderRHI, NiagaraParticleDataInt, NiagaraMeshVF->GetIntDataSRV());
		SetShaderValue(RHICmdList, VertexShaderRHI, SafeComponentBufferSizeParam, NiagaraMeshVF->GetComponentBufferSize());
	}

private:

	FShaderResourceParameter PrevTransformBuffer;

	FShaderResourceParameter NiagaraParticleDataFloat;
	FShaderResourceParameter NiagaraParticleDataInt;
	FShaderParameter SafeComponentBufferSizeParam;

};


void FNiagaraMeshVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	check(GRHISupportsInstancing);

	if (Data.bInitialized)
	{

		// TODO: get rid of this; need to modify code further up to not skip entire draw when no streams are present here
			{
				//checkf(DynamicVertexStride != -1, TEXT("FNiagaraMeshVertexFactory does not have a valid DynamicVertexStride - likely an empty one was made, but SetStrides was not called"));
				FVertexStream VertexStream;
				VertexStream.VertexBuffer = NULL;
				VertexStream.Stride = 0;
				VertexStream.Offset = 0;
				Streams.Add(VertexStream);

				// @todo metal: this will need a valid stride when we get to instanced meshes!
				Elements.Add(FVertexElement(0, Data.TransformComponent[0].Offset, Data.TransformComponent[0].Type, 8, DynamicVertexStride, Data.TransformComponent[0].bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.TransformComponent[1].Offset, Data.TransformComponent[1].Type, 9, DynamicVertexStride, Data.TransformComponent[1].bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.TransformComponent[2].Offset, Data.TransformComponent[2].Type, 10, DynamicVertexStride, Data.TransformComponent[2].bUseInstanceIndex));

				Elements.Add(FVertexElement(0, Data.SubUVs.Offset, Data.SubUVs.Type, 11, DynamicVertexStride, Data.SubUVs.bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.SubUVLerpAndRelTime.Offset, Data.SubUVLerpAndRelTime.Type, 12, DynamicVertexStride, Data.SubUVLerpAndRelTime.bUseInstanceIndex));

				Elements.Add(FVertexElement(0, Data.ParticleColorComponent.Offset, Data.ParticleColorComponent.Type, 14, DynamicVertexStride, Data.ParticleColorComponent.bUseInstanceIndex));
				Elements.Add(FVertexElement(0, Data.VelocityComponent.Offset, Data.VelocityComponent.Type, 15, DynamicVertexStride, Data.VelocityComponent.bUseInstanceIndex));
			}

			// Stream 1 - Dynamic parameter
			{
				checkf(DynamicParameterVertexStride != -1, TEXT("FNiagaraMeshVertexFactory does not have a valid DynamicParameterVertexStride - likely an empty one was made, but SetStrides was not called"));

				FVertexStream VertexStream;
				VertexStream.VertexBuffer = NULL;
				VertexStream.Stride = 0;
				VertexStream.Offset = 0;
				Streams.Add(VertexStream);

				Elements.Add(FVertexElement(1, 0, VET_Float4, 13, DynamicParameterVertexStride, true));
			}


		if (Data.PositionComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));
		}

		// only tangent,normal are used by the stream. the binormal is derived in the shader
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for (int32 AxisIndex = 0; AxisIndex < 2; AxisIndex++)
		{
			if (Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
			{
				Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisAttributes[AxisIndex]));
			}
		}

		// Vertex color
		if (Data.VertexColorComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.VertexColorComponent, 3));
		}
		else
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
			Elements.Add(AccessStreamComponent(NullColorComponent, 3));
		}

		if (Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for (int32 CoordinateIndex = 0; CoordinateIndex < Data.TextureCoordinates.Num(); CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}

			for (int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < MAX_TEXCOORDS; CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}
		}

		//if (Streams.Num() > 0)
		{
			InitDeclaration(Elements);
			check(IsValidRef(GetDeclaration()));
		}
	}
}

void FNiagaraMeshVertexFactory::SetInstanceBuffer(const FVertexBuffer* InstanceBuffer, uint32 StreamOffset, uint32 Stride)
{
	Streams[0].VertexBuffer = InstanceBuffer;
	Streams[0].Offset = StreamOffset;
	Streams[0].Stride = Stride;
}

void FNiagaraMeshVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride)
{
	if (InDynamicParameterBuffer)
	{
		Streams[1].VertexBuffer = InDynamicParameterBuffer;
		Streams[1].Offset = StreamOffset;
		Streams[1].Stride = Stride;
	}
	else
	{
		Streams[1].VertexBuffer = &GNullDynamicParameterVertexBuffer;
		Streams[1].Offset = 0;
		Streams[1].Stride = 0;
	}
}

uint8* FNiagaraMeshVertexFactory::LockPreviousTransformBuffer(uint32 ParticleCount)
{
	const static uint32 ElementSize = sizeof(FVector4);
	const static uint32 ParticleSize = ElementSize * 3;
	const uint32 AllocationRequest = ParticleCount * ParticleSize;

	check(!PrevTransformBuffer.MappedBuffer);

	if (AllocationRequest > PrevTransformBuffer.NumBytes)
	{
		PrevTransformBuffer.Release();
		PrevTransformBuffer.Initialize(ElementSize, ParticleCount * 3, PF_A32B32G32R32F, BUF_Dynamic);
	}

	PrevTransformBuffer.Lock();

	return PrevTransformBuffer.MappedBuffer;
}

void FNiagaraMeshVertexFactory::UnlockPreviousTransformBuffer()
{
	check(PrevTransformBuffer.MappedBuffer);

	PrevTransformBuffer.Unlock();
}

FShaderResourceViewRHIParamRef FNiagaraMeshVertexFactory::GetPreviousTransformBufferSRV() const
{
	return PrevTransformBuffer.SRV;
}

bool FNiagaraMeshVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithNiagaraMeshParticles() || Material->IsSpecialEngineMaterial());
}

void FNiagaraMeshVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI();
}


FVertexFactoryShaderParameters* FNiagaraMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FNiagaraMeshVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactory, "/Engine/Private/NiagaraMeshVertexFactory.ush", true, false, true, false, false);
IMPLEMENT_VERTEX_FACTORY_TYPE(FNiagaraMeshVertexFactoryEmulatedInstancing, "/Engine/Private/NiagaraMeshVertexFactory.ush", true, false, true, false, false);

