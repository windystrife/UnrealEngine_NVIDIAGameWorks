// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Shaders.cpp: D3D shader RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "Serialization/MemoryReader.h"

template <typename TShaderType>
static inline void ReadShaderOptionalData(FShaderCodeReader& InShaderCode, TShaderType& OutShader)
{
	auto PackedResourceCounts = InShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	OutShader.bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	OutShader.ShaderName = InShaderCode.FindOptionalData('n');

	int32 UniformBufferTableSize = 0;
	auto* UniformBufferData = InShaderCode.FindOptionalDataAndSize('u', UniformBufferTableSize);
	if (UniformBufferData && UniformBufferTableSize > 0)
	{
		FBufferReader UBReader((void*)UniformBufferData, UniformBufferTableSize, false);
		TArray<FString> Names;
		UBReader << Names;
		check(OutShader.UniformBuffers.Num() == 0);
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			OutShader.UniformBuffers.Add(FName(*Names[Index]));
		}
	}
#endif
}

FVertexShaderRHIRef FD3D11DynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D11VertexShader* Shader = new FD3D11VertexShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT(Direct3DDevice->CreateVertexShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);
	
	// TEMP
	Shader->Code = Code;
	Shader->Offset = Offset;

	return Shader;
}

FGeometryShaderRHIRef FD3D11DynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	FShaderCodeReader ShaderCode(Code);

	FD3D11GeometryShader* Shader = new FD3D11GeometryShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT(Direct3DDevice->CreateGeometryShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);


	return Shader;
}

FGeometryShaderRHIRef FD3D11DynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) 
{ 
	FShaderCodeReader ShaderCode(Code);

	FD3D11GeometryShader* Shader = new FD3D11GeometryShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	uint32 D3DRasterizedStream = RasterizedStream;
	if (RasterizedStream == -1)
	{
		D3DRasterizedStream = D3D11_SO_NO_RASTERIZED_STREAM;
	}

	D3D11_SO_DECLARATION_ENTRY StreamOutEntries[D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT];

	for (int32 EntryIndex = 0; EntryIndex < ElementList.Num(); EntryIndex++)
	{
		StreamOutEntries[EntryIndex].Stream = ElementList[EntryIndex].Stream;
		StreamOutEntries[EntryIndex].SemanticName = ElementList[EntryIndex].SemanticName;
		StreamOutEntries[EntryIndex].SemanticIndex = ElementList[EntryIndex].SemanticIndex;
		StreamOutEntries[EntryIndex].StartComponent = ElementList[EntryIndex].StartComponent;
		StreamOutEntries[EntryIndex].ComponentCount = ElementList[EntryIndex].ComponentCount;
		StreamOutEntries[EntryIndex].OutputSlot = ElementList[EntryIndex].OutputSlot;
	}

	VERIFYD3D11SHADERRESULT( Direct3DDevice->CreateGeometryShaderWithStreamOutput(
			(void*)CodePtr,
			CodeSize,
			StreamOutEntries,
			ElementList.Num(),
			Strides,
			NumStrides,
			D3DRasterizedStream,
			NULL,
			Shader->Resource.GetInitReference()
			),
		Shader,
		Direct3DDevice);
	
	auto PackedResourceCounts = ShaderCode.FindOptionalData<FShaderCodePackedResourceCounts>();
	check(PackedResourceCounts);
	Shader->bShaderNeedsGlobalConstantBuffer = PackedResourceCounts->bGlobalUniformBufferUsed;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Shader->ShaderName = ShaderCode.FindOptionalData('n');
#endif

	return Shader;
}

FHullShaderRHIRef FD3D11DynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	FShaderCodeReader ShaderCode(Code);

	FD3D11HullShader* Shader = new FD3D11HullShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT( Direct3DDevice->CreateHullShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);

	return Shader;
}

FDomainShaderRHIRef FD3D11DynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	FShaderCodeReader ShaderCode(Code);

	FD3D11DomainShader* Shader = new FD3D11DomainShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT( Direct3DDevice->CreateDomainShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);

	return Shader;
}

FPixelShaderRHIRef FD3D11DynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FShaderCodeReader ShaderCode(Code);

	FD3D11PixelShader* Shader = new FD3D11PixelShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT(Direct3DDevice->CreatePixelShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);

	return Shader;
}

FComputeShaderRHIRef FD3D11DynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	FShaderCodeReader ShaderCode(Code);

	FD3D11ComputeShader* Shader = new FD3D11ComputeShader;

	FMemoryReader Ar( Code, true );
	Ar << Shader->ShaderResourceTable;
	int32 Offset = Ar.Tell();
	const uint8* CodePtr = Code.GetData() + Offset;
	const size_t CodeSize = ShaderCode.GetActualShaderCodeSize() - Offset;

	ReadShaderOptionalData(ShaderCode, *Shader);
	VERIFYD3D11SHADERRESULT( Direct3DDevice->CreateComputeShader( (void*)CodePtr, CodeSize, NULL, Shader->Resource.GetInitReference() ), Shader, Direct3DDevice);

	return Shader;
}

void FD3D11DynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data) 
{ 
	check(Count > 0);
	check(Data);

	// structures are chosen to be directly mappable
	D3D11_VIEWPORT* D3DData = (D3D11_VIEWPORT*)Data;

	StateCache.SetViewports(Count, D3DData );

}

FD3D11BoundShaderState::FD3D11BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	ID3D11Device* Direct3DDevice
	):
	CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	INC_DWORD_STAT(STAT_D3D11NumBoundShaderState);

	FD3D11VertexDeclaration* InVertexDeclaration = FD3D11DynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FD3D11VertexShader* InVertexShader = FD3D11DynamicRHI::ResourceCast(InVertexShaderRHI);
	FD3D11PixelShader* InPixelShader = FD3D11DynamicRHI::ResourceCast(InPixelShaderRHI);
	FD3D11HullShader* InHullShader = FD3D11DynamicRHI::ResourceCast(InHullShaderRHI);
	FD3D11DomainShader* InDomainShader = FD3D11DynamicRHI::ResourceCast(InDomainShaderRHI);
	FD3D11GeometryShader* InGeometryShader = FD3D11DynamicRHI::ResourceCast(InGeometryShaderRHI);

	// Create an input layout for this combination of vertex declaration and vertex shader.
	D3D11_INPUT_ELEMENT_DESC NullInputElement;
	FMemory::Memzero(&NullInputElement,sizeof(D3D11_INPUT_ELEMENT_DESC));

	FShaderCodeReader VertexShaderCode(InVertexShader->Code);

	if (InVertexDeclaration == nullptr)
	{
		InputLayout = nullptr;
	}
	else
	{
		FMemory::Memcpy(StreamStrides, InVertexDeclaration->StreamStrides, sizeof(StreamStrides));

		VERIFYD3D11RESULT_EX(
		Direct3DDevice->CreateInputLayout(
			InVertexDeclaration && InVertexDeclaration->VertexElements.Num() ? InVertexDeclaration->VertexElements.GetData() : &NullInputElement,
			InVertexDeclaration ? InVertexDeclaration->VertexElements.Num() : 0,
			&InVertexShader->Code[ InVertexShader->Offset ],			// TEMP ugly
			VertexShaderCode.GetActualShaderCodeSize() - InVertexShader->Offset,
			InputLayout.GetInitReference()
			),
		Direct3DDevice
		);
	}

	VertexShader = InVertexShader->Resource;
	PixelShader = InPixelShader ? InPixelShader->Resource : NULL;
	HullShader = InHullShader ? InHullShader->Resource : NULL;
	DomainShader = InDomainShader ? InDomainShader->Resource : NULL;
	GeometryShader = InGeometryShader ? InGeometryShader->Resource : NULL;

	FMemory::Memzero(&bShaderNeedsGlobalConstantBuffer,sizeof(bShaderNeedsGlobalConstantBuffer));

	bShaderNeedsGlobalConstantBuffer[SF_Vertex] = InVertexShader->bShaderNeedsGlobalConstantBuffer;
	bShaderNeedsGlobalConstantBuffer[SF_Hull] = InHullShader ? InHullShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[SF_Domain] = InDomainShader ? InDomainShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[SF_Pixel] = InPixelShader ? InPixelShader->bShaderNeedsGlobalConstantBuffer : false;
	bShaderNeedsGlobalConstantBuffer[SF_Geometry] = InGeometryShader ? InGeometryShader->bShaderNeedsGlobalConstantBuffer : false;

	static_assert(ARRAY_COUNT(bShaderNeedsGlobalConstantBuffer) == SF_NumFrequencies, "EShaderFrequency size should match with array count of bShaderNeedsGlobalConstantBuffer.");
}

FD3D11BoundShaderState::~FD3D11BoundShaderState()
{
	DEC_DWORD_STAT(STAT_D3D11NumBoundShaderState);
}


/**
* Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
* @param VertexDeclaration - existing vertex decl
* @param StreamStrides - optional stream strides
* @param VertexShader - existing vertex shader
* @param HullShader - existing hull shader
* @param DomainShader - existing domain shader
* @param PixelShader - existing pixel shader
* @param GeometryShader - existing geometry shader
*/
FBoundShaderStateRHIRef FD3D11DynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	check(IsInRenderingThread());

	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && Direct3DDeviceIMContext,(TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_D3D11NewBoundShaderStateTime);
		return new FD3D11BoundShaderState(VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI,Direct3DDevice);
	}
}
