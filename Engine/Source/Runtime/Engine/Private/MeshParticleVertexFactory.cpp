// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshParticleVertexFactory.cpp: Mesh particle vertex factory implementation
=============================================================================*/

#include "MeshParticleVertexFactory.h"
#include "ParticleHelper.h"
#include "ShaderParameterUtils.h"


class FMeshParticleVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap) override
	{
		Transform1.Bind(ParameterMap,TEXT("Transform1"));
		Transform2.Bind(ParameterMap,TEXT("Transform2"));
		Transform3.Bind(ParameterMap,TEXT("Transform3"));
		SubUVParams.Bind(ParameterMap,TEXT("SubUVParams"));
		SubUVLerp.Bind(ParameterMap,TEXT("SubUVLerp"));
		ParticleDirection.Bind(ParameterMap, TEXT("ParticleDirection"));
		RelativeTime.Bind(ParameterMap, TEXT("RelativeTime"));
		DynamicParameter.Bind(ParameterMap, TEXT("DynamicParameter"));
		ParticleColor.Bind(ParameterMap, TEXT("ParticleColor"));
		PrevTransform0.Bind(ParameterMap, TEXT("PrevTransform0"));
		PrevTransform1.Bind(ParameterMap, TEXT("PrevTransform1"));
		PrevTransform2.Bind(ParameterMap, TEXT("PrevTransform2"));
		PrevTransformBuffer.Bind(ParameterMap, TEXT("PrevTransformBuffer"));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << Transform1;
		Ar << Transform2;
		Ar << Transform3;
		Ar << SubUVParams;
		Ar << SubUVLerp;
		Ar << ParticleDirection;
		Ar << RelativeTime;
		Ar << DynamicParameter;
		Ar << ParticleColor;
		Ar << PrevTransform0;
		Ar << PrevTransform1;
		Ar << PrevTransform2;
		Ar << PrevTransformBuffer;
	}

	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View,const FMeshBatchElement& BatchElement,uint32 DataFlags) const override
	{
		const bool bInstanced = GRHISupportsInstancing;
		FMeshParticleVertexFactory* MeshParticleVF = (FMeshParticleVertexFactory*)VertexFactory;
		FVertexShaderRHIParamRef VertexShaderRHI = Shader->GetVertexShader();
		SetUniformBufferParameter(RHICmdList, VertexShaderRHI, Shader->GetUniformBufferParameter<FMeshParticleUniformParameters>(), MeshParticleVF->GetUniformBuffer() );

		if (!bInstanced)
		{
			const FMeshParticleVertexFactory::FBatchParametersCPU* BatchParameters = (const FMeshParticleVertexFactory::FBatchParametersCPU*)BatchElement.UserData;
			const FMeshParticleInstanceVertex* Vertex = BatchParameters->InstanceBuffer + BatchElement.UserIndex;
			const FMeshParticleInstanceVertexDynamicParameter* DynamicVertex = BatchParameters->DynamicParameterBuffer + BatchElement.UserIndex;
			const FMeshParticleInstanceVertexPrevTransform* PrevTransformVertex = BatchParameters->PrevTransformBuffer + BatchElement.UserIndex;

			SetShaderValue(RHICmdList, VertexShaderRHI, Transform1, Vertex->Transform[0]);
			SetShaderValue(RHICmdList, VertexShaderRHI, Transform2, Vertex->Transform[1]);
			SetShaderValue(RHICmdList, VertexShaderRHI, Transform3, Vertex->Transform[2]);
			SetShaderValue(RHICmdList, VertexShaderRHI, SubUVParams, FVector4((float)Vertex->SubUVParams[0], (float)Vertex->SubUVParams[1], (float)Vertex->SubUVParams[2], (float)Vertex->SubUVParams[3]));
			SetShaderValue(RHICmdList, VertexShaderRHI, SubUVLerp, Vertex->SubUVLerp);
			SetShaderValue(RHICmdList, VertexShaderRHI, ParticleDirection, Vertex->Velocity);
			SetShaderValue(RHICmdList, VertexShaderRHI, RelativeTime, Vertex->RelativeTime);

			if (BatchParameters->DynamicParameterBuffer)
			{
				SetShaderValue(RHICmdList, VertexShaderRHI, DynamicParameter, FVector4(DynamicVertex->DynamicValue[0], DynamicVertex->DynamicValue[1], DynamicVertex->DynamicValue[2], DynamicVertex->DynamicValue[3]));
			}

			if (BatchParameters->PrevTransformBuffer && View.FeatureLevel >= ERHIFeatureLevel::SM4)
			{
				SetShaderValue(RHICmdList, VertexShaderRHI, PrevTransform0, PrevTransformVertex->PrevTransform0);
				SetShaderValue(RHICmdList, VertexShaderRHI, PrevTransform1, PrevTransformVertex->PrevTransform1);
				SetShaderValue(RHICmdList, VertexShaderRHI, PrevTransform2, PrevTransformVertex->PrevTransform2);
			}

			SetShaderValue(RHICmdList, VertexShaderRHI, ParticleColor, FVector4(Vertex->Color.Component(0), Vertex->Color.Component(1), Vertex->Color.Component(2), Vertex->Color.Component(3)));
		}
		else if (View.FeatureLevel >= ERHIFeatureLevel::SM4)
		{
			SetSRVParameter(RHICmdList, VertexShaderRHI, PrevTransformBuffer, MeshParticleVF->GetPreviousTransformBufferSRV());
		}
	}

private:
	// Used only when instancing is off (ES2)
	FShaderParameter Transform1;
	FShaderParameter Transform2;
	FShaderParameter Transform3;
	FShaderParameter SubUVParams;
	FShaderParameter SubUVLerp;
	FShaderParameter ParticleDirection;
	FShaderParameter RelativeTime;
	FShaderParameter DynamicParameter;
	FShaderParameter ParticleColor;
	FShaderParameter PrevTransform0;
	FShaderParameter PrevTransform1;
	FShaderParameter PrevTransform2;
	FShaderResourceParameter PrevTransformBuffer;
};


void FMeshParticleVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	const bool bInstanced = GRHISupportsInstancing;

	if (Data.bInitialized)
	{
		if(bInstanced)
		{
			// Stream 0 - Instance data
			{
				checkf(DynamicVertexStride != -1, TEXT("FMeshParticleVertexFactory does not have a valid DynamicVertexStride - likely an empty one was made, but SetStrides was not called"));
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
				checkf(DynamicParameterVertexStride != -1, TEXT("FMeshParticleVertexFactory does not have a valid DynamicParameterVertexStride - likely an empty one was made, but SetStrides was not called"));
				
				FVertexStream VertexStream;
				VertexStream.VertexBuffer = NULL;
				VertexStream.Stride = 0;
				VertexStream.Offset = 0;
				Streams.Add(VertexStream);
	
				Elements.Add(FVertexElement(1, 0, VET_Float4, 13, DynamicParameterVertexStride, true));
			}

			// Add a dummy resource to avoid crash due to missing resource
			if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
			{
				PrevTransformBuffer.Initialize(sizeof(FVector4), 3, PF_A32B32G32R32F, BUF_Dynamic);
			}
		}

		if(Data.PositionComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.PositionComponent,0));
		}

		// only tangent,normal are used by the stream. the binormal is derived in the shader
		uint8 TangentBasisAttributes[2] = { 1, 2 };
		for(int32 AxisIndex = 0;AxisIndex < 2;AxisIndex++)
		{
			if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
			{
				Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisAttributes[AxisIndex]));
			}
		}

		// Vertex color
		if(Data.VertexColorComponent.VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.VertexColorComponent,3));
		}
		else
		{
			//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
			//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
			FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
			Elements.Add(AccessStreamComponent(NullColorComponent,3));
		}
		
		if(Data.TextureCoordinates.Num())
		{
			const int32 BaseTexCoordAttribute = 4;
			for(int32 CoordinateIndex = 0;CoordinateIndex < Data.TextureCoordinates.Num();CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[CoordinateIndex],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}

			for(int32 CoordinateIndex = Data.TextureCoordinates.Num();CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
			{
				Elements.Add(AccessStreamComponent(
					Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
					BaseTexCoordAttribute + CoordinateIndex
					));
			}
		}

		if(Streams.Num() > 0)
		{
			InitDeclaration(Elements);
			check(IsValidRef(GetDeclaration()));
		}
	}
}

void FMeshParticleVertexFactory::SetInstanceBuffer(const FVertexBuffer* InstanceBuffer, uint32 StreamOffset, uint32 Stride)
{
	ensure(Stride == DynamicVertexStride);
	Streams[0].VertexBuffer = InstanceBuffer;
	Streams[0].Offset = StreamOffset;
	Streams[0].Stride = Stride;
}

void FMeshParticleVertexFactory::SetDynamicParameterBuffer(const FVertexBuffer* InDynamicParameterBuffer, uint32 StreamOffset, uint32 Stride)
{
	if (InDynamicParameterBuffer)
	{
		Streams[1].VertexBuffer = InDynamicParameterBuffer;
		ensure(Stride == DynamicParameterVertexStride);
		Streams[1].Stride = DynamicParameterVertexStride;
		Streams[1].Offset = StreamOffset;
	}
	else
	{
		Streams[1].VertexBuffer = &GNullDynamicParameterVertexBuffer;
		ensure(DynamicParameterVertexStride == 0);
		Streams[1].Stride = 0;
		Streams[1].Offset = 0;
	}
}

uint8* FMeshParticleVertexFactory::LockPreviousTransformBuffer(uint32 ParticleCount)
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

void FMeshParticleVertexFactory::UnlockPreviousTransformBuffer()
{
	check(PrevTransformBuffer.MappedBuffer);

	PrevTransformBuffer.Unlock();
}

FShaderResourceViewRHIParamRef FMeshParticleVertexFactory::GetPreviousTransformBufferSRV() const
{
	return PrevTransformBuffer.SRV;
}

bool FMeshParticleVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithMeshParticles() || Material->IsSpecialEngineMaterial());
}

void FMeshParticleVertexFactory::SetData(const FDataType& InData)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI();
}


FVertexFactoryShaderParameters* FMeshParticleVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FMeshParticleVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactory,"/Engine/Private/MeshParticleVertexFactory.ush",true,false,true,false,false);
IMPLEMENT_VERTEX_FACTORY_TYPE(FMeshParticleVertexFactoryEmulatedInstancing,"/Engine/Private/MeshParticleVertexFactory.ush",true,false,true,false,false);
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FMeshParticleUniformParameters,TEXT("MeshParticleVF"));
