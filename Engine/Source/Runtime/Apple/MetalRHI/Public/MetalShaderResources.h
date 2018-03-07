// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaderResources.h: Metal shader resource RHI definitions.
=============================================================================*/

#pragma once

#include "CrossCompilerCommon.h"

/**
* Shader related constants.
*/
enum
{
	METAL_MAX_UNIFORM_BUFFER_BINDINGS = 12,	// @todo-mobile: Remove me
	METAL_FIRST_UNIFORM_BUFFER = 0,			// @todo-mobile: Remove me
	METAL_MAX_COMPUTE_STAGE_UAV_UNITS = 8,	// @todo-mobile: Remove me
	METAL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT = -1, // for now, only CS supports UAVs/ images
};

struct FMetalShaderResourceTable : public FBaseShaderResourceTable
{
	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;
	friend bool operator==(const FMetalShaderResourceTable &A, const FMetalShaderResourceTable& B)
	{
		if (!(((FBaseShaderResourceTable&)A) == ((FBaseShaderResourceTable&)B)))
		{
			return false;
		}
		if (A.TextureMap.Num() != B.TextureMap.Num())
		{
			return false;
		}
		if (FMemory::Memcmp(A.TextureMap.GetData(), B.TextureMap.GetData(), A.TextureMap.GetTypeSize()*A.TextureMap.Num()) != 0)
		{
			return false;
		}
		return true;
	}
};

inline FArchive& operator<<(FArchive& Ar, FMetalShaderResourceTable& SRT)
{
	Ar << ((FBaseShaderResourceTable&)SRT);
	Ar << SRT.TextureMap;
	return Ar;
}


struct FMetalShaderBindings
{
	TArray<TArray<CrossCompiler::FPackedArrayInfo>>	PackedUniformBuffers;
	TArray<CrossCompiler::FPackedArrayInfo>			PackedGlobalArrays;
	FMetalShaderResourceTable				ShaderResourceTable;

	uint16	InOutMask;
	uint8	NumSamplers;
	uint8	NumUniformBuffers;
	uint8	NumUAVs;
	uint8	AtomicUAVs;
	bool	bHasRegularUniformBuffers;

	FMetalShaderBindings() :
		InOutMask(0),
		NumSamplers(0),
		NumUniformBuffers(0),
		NumUAVs(0),
		AtomicUAVs(0),
		bHasRegularUniformBuffers(false)
	{
	}

	friend bool operator==(const FMetalShaderBindings &A, const FMetalShaderBindings &B)
	{
		bool bEqual = true;

		bEqual &= A.InOutMask == B.InOutMask;
		bEqual &= A.NumSamplers == B.NumSamplers;
		bEqual &= A.NumUniformBuffers == B.NumUniformBuffers;
		bEqual &= A.NumUAVs == B.NumUAVs;
		bEqual &= A.AtomicUAVs == B.AtomicUAVs;
		bEqual &= A.bHasRegularUniformBuffers == B.bHasRegularUniformBuffers;
		bEqual &= A.PackedGlobalArrays.Num() == B.PackedGlobalArrays.Num();
		bEqual &= A.PackedUniformBuffers.Num() == B.PackedUniformBuffers.Num();
		bEqual &= A.ShaderResourceTable == B.ShaderResourceTable;

		if (!bEqual)
		{
			return bEqual;
		}

		bEqual &= FMemory::Memcmp(A.PackedGlobalArrays.GetData(), B.PackedGlobalArrays.GetData(), A.PackedGlobalArrays.GetTypeSize()*A.PackedGlobalArrays.Num()) == 0;

		for (int32 Item = 0; Item < A.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo> &ArrayA = A.PackedUniformBuffers[Item];
			const TArray<CrossCompiler::FPackedArrayInfo> &ArrayB = B.PackedUniformBuffers[Item];

			bEqual &= FMemory::Memcmp(ArrayA.GetData(), ArrayB.GetData(), ArrayA.GetTypeSize()*ArrayA.Num()) == 0;
		}

		return bEqual;
	}

	friend uint32 GetTypeHash(const FMetalShaderBindings &Binding)
	{
		uint32 Hash = 0;
		Hash = Binding.InOutMask;
		Hash |= Binding.NumSamplers << 16;
		Hash |= Binding.NumUniformBuffers << 24;
		Hash ^= Binding.NumUAVs;
		Hash ^= Binding.AtomicUAVs;
		Hash ^= Binding.bHasRegularUniformBuffers << 8;
		Hash ^= FCrc::MemCrc_DEPRECATED(Binding.PackedGlobalArrays.GetData(), Binding.PackedGlobalArrays.GetTypeSize()*Binding.PackedGlobalArrays.Num());

		for (int32 Item = 0; Item < Binding.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo> &Array = Binding.PackedUniformBuffers[Item];
			Hash ^= FCrc::MemCrc_DEPRECATED(Array.GetData(), Array.GetTypeSize()* Array.Num());
		}
		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FMetalShaderBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	Ar << Bindings.ShaderResourceTable;
	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;
	Ar << Bindings.AtomicUAVs;
	Ar << Bindings.bHasRegularUniformBuffers;
	return Ar;
}

enum class EMetalOutputWindingMode : uint8
{
	Clockwise = 0,
	CounterClockwise = 1,
};

enum class EMetalPartitionMode : uint8
{
	Pow2 = 0,
	Integer = 1,
	FractionalOdd = 2,
	FractionalEven = 3,
};

enum class EMetalComponentType : uint8
{
	Uint = 0,
	Int,
	Half,
	Float,
	Bool,
	Max
};

struct FMetalAttribute
{
	uint32 Index;
	EMetalComponentType Type;
	uint32 Components;
	uint32 Offset;
	
	friend FArchive& operator<<(FArchive& Ar, FMetalAttribute& Attr)
	{
		Ar << Attr.Index;
		Ar << Attr.Type;
		Ar << Attr.Components;
		Ar << Attr.Offset;
		return Ar;
	}
};

struct FMetalTessellationOutputs
{
	uint32 HSOutSize;
	uint32 HSTFOutSize;
	uint32 PatchControlPointOutSize;
	TArray<FMetalAttribute> HSOut;
	TArray<FMetalAttribute> PatchControlPointOut;
	
	friend FArchive& operator<<(FArchive& Ar, FMetalTessellationOutputs& Attrs)
	{
		Ar << Attrs.HSOutSize;
		Ar << Attrs.HSTFOutSize;
		Ar << Attrs.PatchControlPointOutSize;
		Ar << Attrs.HSOut;
		Ar << Attrs.PatchControlPointOut;
		return Ar;
	}
};

struct FMetalCodeHeader
{
	uint32 Frequency;
	FMetalShaderBindings Bindings;
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
    FString ShaderName;
    
    FMetalTessellationOutputs TessellationOutputAttribs;

	uint64 CompilerBuild;
	uint32 CompilerVersion;

	uint32 TessellationOutputControlPoints;
	uint32 TessellationDomain; // 3 = tri, 4 = quad // TODO unused
	uint32 TessellationInputControlPoints; // TODO unused
	uint32 TessellationPatchesPerThreadGroup;
    uint32 TessellationPatchCountBuffer;
    uint32 TessellationIndexBuffer;
    uint32 TessellationHSOutBuffer;
    uint32 TessellationHSTFOutBuffer;
    uint32 TessellationControlPointOutBuffer;
    uint32 TessellationControlPointIndexBuffer;
	float  TessellationMaxTessFactor;

	uint32 SourceLen;
	uint32 SourceCRC;
	
	uint16 CompileFlags;
	
	uint8 NumThreadsX;
	uint8 NumThreadsY;
	uint8 NumThreadsZ;
	
	uint8 Version;
	int8 SideTable;
	
	EMetalOutputWindingMode TessellationOutputWinding;
	EMetalPartitionMode TessellationPartitioning;
	
	bool bFunctionConstants;
};

inline FArchive& operator<<(FArchive& Ar, FMetalCodeHeader& Header)
{
	Ar << Header.Frequency;
	Ar << Header.Bindings;

	int32 NumInfos = Header.UniformBuffersCopyInfo.Num();
	Ar << NumInfos;
	if (Ar.IsSaving())
	{
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			Ar << Header.UniformBuffersCopyInfo[Index];
		}
	}
	else if (Ar.IsLoading())
	{
		Header.UniformBuffersCopyInfo.Empty(NumInfos);
		for (int32 Index = 0; Index < NumInfos; ++Index)
		{
			CrossCompiler::FUniformBufferCopyInfo Info;
			Ar << Info;
			Header.UniformBuffersCopyInfo.Add(Info);
		}
	}
	
	Ar << Header.ShaderName;

    Ar << Header.TessellationOutputAttribs;

	Ar << Header.CompilerVersion;
	Ar << Header.CompilerBuild;
    
	Ar << Header.TessellationOutputControlPoints;
	Ar << Header.TessellationDomain;
	Ar << Header.TessellationInputControlPoints;
	Ar << Header.TessellationPatchesPerThreadGroup;
	Ar << Header.TessellationMaxTessFactor;
    
    Ar << Header.TessellationPatchCountBuffer;
    Ar << Header.TessellationIndexBuffer;
    Ar << Header.TessellationHSOutBuffer;
    Ar << Header.TessellationHSTFOutBuffer;
    Ar << Header.TessellationControlPointOutBuffer;
    Ar << Header.TessellationControlPointIndexBuffer;
	
	Ar << Header.SourceLen;
	Ar << Header.SourceCRC;
	
	Ar << Header.CompileFlags;
	
	Ar << Header.NumThreadsX;
	Ar << Header.NumThreadsY;
	Ar << Header.NumThreadsZ;
	
	Ar << Header.Version;
	Ar << Header.SideTable;
	
	Ar << Header.TessellationOutputWinding;
	Ar << Header.TessellationPartitioning;
	Ar << Header.bFunctionConstants;

    return Ar;
}

struct FMetalShaderMap
{
	FString Format;
	TMap<FSHAHash, TPair<uint8, TArray<uint8>>> HashMap;
	
	friend FArchive& operator<<(FArchive& Ar, FMetalShaderMap& Header)
	{
		return Ar << Header.Format << Header.HashMap;
	}
};
