// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VertexFactory.cpp: Vertex factory implementation
=============================================================================*/

#include "VertexFactory.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/DebugSerializationFlags.h"

uint32 FVertexFactoryType::NextHashIndex = 0;
bool FVertexFactoryType::bInitializedSerializationHistory = false;

/**
 * @return The global shader factory list.
 */
TLinkedList<FVertexFactoryType*>*& FVertexFactoryType::GetTypeList()
{
	static TLinkedList<FVertexFactoryType*>* TypeList = NULL;
	return TypeList;
}

/**
 * Finds a FVertexFactoryType by name.
 */
FVertexFactoryType* FVertexFactoryType::GetVFByName(const FString& VFName)
{
	for(TLinkedList<FVertexFactoryType*>::TIterator It(GetTypeList()); It; It.Next())
	{
		FString CurrentVFName = FString(It->GetName());
		if (CurrentVFName == VFName)
		{
			return *It;
		}
	}
	return NULL;
}

void FVertexFactoryType::Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Cache serialization history for each VF type
		// This history is used to detect when shader serialization changes without a corresponding .usf change
		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			FVertexFactoryType* Type = *It;
			GenerateReferencedUniformBuffers(Type->ShaderFilename, Type->Name, ShaderFileToUniformBufferVariables, Type->ReferencedUniformBufferStructsCache);

			for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
			{
				// Construct a temporary shader parameter instance, which is initialized to safe values for serialization
				FVertexFactoryShaderParameters* Parameters = Type->CreateShaderParameters((EShaderFrequency)Frequency);

				if (Parameters)
				{
					// Serialize the temp shader to memory and record the number and sizes of serializations
					TArray<uint8> TempData;
					FMemoryWriter Ar(TempData, true);
					FShaderSaveArchive SaveArchive(Ar, Type->SerializationHistory[Frequency]);
					Parameters->Serialize(SaveArchive);
					delete Parameters;
				}
			}
		}
	}

	bInitializedSerializationHistory = true;
}

void FVertexFactoryType::Uninitialize()
{
	for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
	{
		FVertexFactoryType* Type = *It;

		for (int32 Frequency = 0; Frequency < SF_NumFrequencies; Frequency++)
		{
			Type->SerializationHistory[Frequency] = FSerializationHistory();
		}
	}

	bInitializedSerializationHistory = false;
}

FVertexFactoryType::FVertexFactoryType(
	const TCHAR* InName,
	const TCHAR* InShaderFilename,
	bool bInUsedWithMaterials,
	bool bInSupportsStaticLighting,
	bool bInSupportsDynamicLighting,
	bool bInSupportsPrecisePrevWorldPos,
	bool bInSupportsPositionOnly,
	ConstructParametersType InConstructParameters,
	ShouldCacheType InShouldCache,
	ModifyCompilationEnvironmentType InModifyCompilationEnvironment,
	SupportsTessellationShadersType InSupportsTessellationShaders
	):
	Name(InName),
	ShaderFilename(InShaderFilename),
	TypeName(InName),
	bUsedWithMaterials(bInUsedWithMaterials),
	bSupportsStaticLighting(bInSupportsStaticLighting),
	bSupportsDynamicLighting(bInSupportsDynamicLighting),
	bSupportsPrecisePrevWorldPos(bInSupportsPrecisePrevWorldPos),
	bSupportsPositionOnly(bInSupportsPositionOnly),
	ConstructParameters(InConstructParameters),
	ShouldCacheRef(InShouldCache),
	ModifyCompilationEnvironmentRef(InModifyCompilationEnvironment),
	SupportsTessellationShadersRef(InSupportsTessellationShaders),
	GlobalListLink(this)
{
	// Make sure the format of the source file path is right.
	check(CheckVirtualShaderFilePath(InShaderFilename));

	checkf(FPaths::GetExtension(InShaderFilename) == TEXT("ush"),
		TEXT("Incorrect virtual shader path extension for vertex factory shader header '%s': Only .ush files should be included."),
		InShaderFilename);

	for (int32 Platform = 0; Platform < SP_NumPlatforms; Platform++)
	{
		bCachedUniformBufferStructDeclarations[Platform] = false;
	}

	// This will trigger if an IMPLEMENT_VERTEX_FACTORY_TYPE was in a module not loaded before InitializeShaderTypes
	// Vertex factory types need to be implemented in modules that are loaded before that
	checkf(!bInitializedSerializationHistory, TEXT("VF type was loaded after engine init, use ELoadingPhase::PostConfigInit on your module to cause it to load earlier."));

	// Add this vertex factory type to the global list.
	GlobalListLink.LinkHead(GetTypeList());

	// Assign the vertex factory type the next unassigned hash index.
	HashIndex = NextHashIndex++;
}

FVertexFactoryType::~FVertexFactoryType()
{
	GlobalListLink.Unlink();
}

/** Calculates a Hash based on this vertex factory type's source code and includes */
const FSHAHash& FVertexFactoryType::GetSourceHash() const
{
	return GetShaderFileHash(GetShaderFilename());
}

FArchive& operator<<(FArchive& Ar,FVertexFactoryType*& TypeRef)
{
	if(Ar.IsSaving())
	{
		FName TypeName = TypeRef ? FName(TypeRef->GetName()) : NAME_None;
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		FName TypeName = NAME_None;
		Ar << TypeName;
		TypeRef = FindVertexFactoryType(TypeName);
	}
	return Ar;
}

FVertexFactoryType* FindVertexFactoryType(FName TypeName)
{
	// Search the global vertex factory list for a type with a matching name.
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		if(VertexFactoryTypeIt->GetFName() == TypeName)
		{
			return *VertexFactoryTypeIt;
		}
	}
	return NULL;
}

void FVertexFactory::Set(FRHICommandList& RHICmdList) const
{
	check(IsInitialized());
	for(int32 StreamIndex = 0;StreamIndex < Streams.Num();StreamIndex++)
	{
		const FVertexStream& Stream = Streams[StreamIndex];
		if (!Stream.bSetByVertexFactoryInSetMesh)
		{
			if (!Stream.VertexBuffer)
			{
				RHICmdList.SetStreamSource(StreamIndex, nullptr, 0);
			}
			else
			{
				checkf(Stream.VertexBuffer->IsInitialized(), TEXT("Vertex buffer was not initialized! Stream %u, Stride %u, Name %s"), StreamIndex, Stream.Stride, *Stream.VertexBuffer->GetFriendlyName());
				RHICmdList.SetStreamSource(StreamIndex, Stream.VertexBuffer->VertexBufferRHI, Stream.Offset);
			}
		}
	}
}

void FVertexFactory::OffsetInstanceStreams(FRHICommandList& RHICmdList, uint32 FirstVertex) const
{
	for(int32 StreamIndex = 0;StreamIndex < Streams.Num();StreamIndex++)
	{
		const FVertexStream& Stream = Streams[StreamIndex];
		if (Stream.bUseInstanceIndex)
		{
			RHICmdList.SetStreamSource( StreamIndex, Stream.VertexBuffer->VertexBufferRHI, Stream.Offset + Stream.Stride * FirstVertex);
		}
	}
}

void FVertexFactory::SetPositionStream(FRHICommandList& RHICmdList) const
{
	check(IsInitialized());
	// Set the predefined vertex streams.
	for(int32 StreamIndex = 0;StreamIndex < PositionStream.Num();StreamIndex++)
	{
		const FVertexStream& Stream = PositionStream[StreamIndex];
		check(Stream.VertexBuffer->IsInitialized());
		RHICmdList.SetStreamSource( StreamIndex, Stream.VertexBuffer->VertexBufferRHI, Stream.Offset);
	}
}

void FVertexFactory::OffsetPositionInstanceStreams(FRHICommandList& RHICmdList, uint32 FirstVertex) const
{
	for(int32 StreamIndex = 0;StreamIndex < PositionStream.Num();StreamIndex++)
	{
		const FVertexStream& Stream = PositionStream[StreamIndex];
		if (Stream.bUseInstanceIndex)
		{
			RHICmdList.SetStreamSource( StreamIndex, Stream.VertexBuffer->VertexBufferRHI, Stream.Offset + Stream.Stride * FirstVertex);
		}
	}
}

void FVertexFactory::ReleaseRHI()
{
	Declaration.SafeRelease();
	PositionDeclaration.SafeRelease();
	Streams.Empty();
	PositionStream.Empty();
}

/**
* Fill in array of strides from this factory's vertex streams without shadow/light maps
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
int32 FVertexFactory::GetStreamStrides(uint32 *OutStreamStrides, bool bPadWithZeroes) const
{
	int32 StreamIndex;
	for(StreamIndex = 0;StreamIndex < Streams.Num();++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = Streams[StreamIndex].Stride;
	}
	if (bPadWithZeroes)
	{
		// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
		for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
		{
			OutStreamStrides[StreamIndex] = 0;
		}
	}
	return StreamIndex;
}

/**
* Fill in array of strides from this factory's position only vertex streams
* @param OutStreamStrides - output array of # MaxVertexElementCount stream strides to fill
*/
void FVertexFactory::GetPositionStreamStride(uint32 *OutStreamStrides) const
{
	int32 StreamIndex;
	for(StreamIndex = 0;StreamIndex < PositionStream.Num();++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = PositionStream[StreamIndex].Stride;
	}
	// Pad stream strides with 0's to be safe (they can be used in hashes elsewhere)
	for (;StreamIndex < MaxVertexElementCount;++StreamIndex)
	{
		OutStreamStrides[StreamIndex] = 0;
	}
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = 0;
	VertexStream.bUseInstanceIndex = Component.bUseInstanceIndex;
	VertexStream.bSetByVertexFactoryInSetMesh = Component.bSetByVertexFactoryInSetMesh;

	return FVertexElement(Streams.AddUnique(VertexStream),Component.Offset,Component.Type,AttributeIndex,VertexStream.Stride,Component.bUseInstanceIndex);
}

FVertexElement FVertexFactory::AccessPositionStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = 0;
	VertexStream.bUseInstanceIndex = Component.bUseInstanceIndex;
	VertexStream.bSetByVertexFactoryInSetMesh = Component.bSetByVertexFactoryInSetMesh;

	return FVertexElement(PositionStream.AddUnique(VertexStream),Component.Offset,Component.Type,AttributeIndex,VertexStream.Stride,Component.bUseInstanceIndex);
}

void FVertexFactory::InitDeclaration(FVertexDeclarationElementList& Elements)
{
	// Create the vertex declaration for rendering the factory normally.
	Declaration = RHICreateVertexDeclaration(Elements);
}

void FVertexFactory::InitPositionDeclaration(const FVertexDeclarationElementList& Elements)
{
	PositionDeclaration = RHICreateVertexDeclaration(Elements);
}

FVertexFactoryParameterRef::FVertexFactoryParameterRef(FVertexFactoryType* InVertexFactoryType,const FShaderParameterMap& ParameterMap, EShaderFrequency InShaderFrequency)
: Parameters(NULL)
, VertexFactoryType(InVertexFactoryType)
, ShaderFrequency(InShaderFrequency)
{
	Parameters = VertexFactoryType->CreateShaderParameters(InShaderFrequency);
	VFHash = GetShaderFileHash(VertexFactoryType->GetShaderFilename());

	if(Parameters)
	{
		Parameters->Bind(ParameterMap);
	}
}

bool operator<<(FArchive& Ar,FVertexFactoryParameterRef& Ref)
{
	bool bShaderHasOutdatedParameters = false;

	Ar << Ref.VertexFactoryType;

	uint8 ShaderFrequencyByte = Ref.ShaderFrequency;
	Ar << ShaderFrequencyByte;
	if(Ar.IsLoading())
	{
		Ref.ShaderFrequency = (EShaderFrequency)ShaderFrequencyByte;
	}

	Ar << Ref.VFHash;


	if (Ar.IsLoading())
	{
		delete Ref.Parameters;
		if (Ref.VertexFactoryType)
		{
			Ref.Parameters = Ref.VertexFactoryType->CreateShaderParameters(Ref.ShaderFrequency);
		}
		else
		{
			bShaderHasOutdatedParameters = true;
			Ref.Parameters = NULL;
		}
	}

	// Need to be able to skip over parameters for no longer existing vertex factories.
	int32 SkipOffset = Ar.Tell();
	{
		FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);
		// Write placeholder.
		Ar << SkipOffset;
	}


	if(Ref.Parameters)
	{
		Ref.Parameters->Serialize(Ar);
	}
	else if(Ar.IsLoading())
	{
		Ar.Seek( SkipOffset );
	}

	if( Ar.IsSaving() )
	{
		int32 EndOffset = Ar.Tell();
		Ar.Seek( SkipOffset );
		Ar << EndOffset;
		Ar.Seek( EndOffset );
	}

	return bShaderHasOutdatedParameters;
}

/** Returns the hash of the vertex factory shader file that this shader was compiled with. */
const FSHAHash& FVertexFactoryParameterRef::GetHash() const 
{ 
	return VFHash;
}
