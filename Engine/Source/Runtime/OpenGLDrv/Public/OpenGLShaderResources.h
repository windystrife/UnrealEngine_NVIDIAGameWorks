// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaderResources.h: OpenGL shader resource RHI definitions.
=============================================================================*/

#pragma once

#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Containers/Array.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Misc/SecureHash.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderCore.h"
#include "CrossCompilerCommon.h"

class FOpenGLLinkedProgram;

/**
 * Shader related constants.
 */
enum
{
	OGL_MAX_UNIFORM_BUFFER_BINDINGS = 12,	// @todo-mobile: Remove me
	OGL_FIRST_UNIFORM_BUFFER = 0,			// @todo-mobile: Remove me
	OGL_MAX_COMPUTE_STAGE_UAV_UNITS = 8,	// @todo-mobile: Remove me
	OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT = -1, // for now, only CS supports UAVs/ images
};

struct FOpenGLShaderResourceTable : public FBaseShaderResourceTable
{
	/** Mapping of bound Textures to their location in resource tables. */
	TArray<uint32> TextureMap;
	friend bool operator==(const FOpenGLShaderResourceTable &A, const FOpenGLShaderResourceTable& B)
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

inline FArchive& operator<<(FArchive& Ar, FOpenGLShaderResourceTable& SRT)
{
	Ar << ((FBaseShaderResourceTable&)SRT);
	Ar << SRT.TextureMap;
	return Ar;
}

struct FOpenGLShaderVarying
{
	TArray<ANSICHAR> Varying;
	int32 Location;
	
	friend bool operator==(const FOpenGLShaderVarying& A, const FOpenGLShaderVarying& B)
	{
		if(&A != &B)
		{
			return (A.Location == B.Location) && (A.Varying.Num() == B.Varying.Num()) && (FMemory::Memcmp(A.Varying.GetData(), B.Varying.GetData(), A.Varying.Num() * sizeof(ANSICHAR)) == 0);
		}
		return true;
	}
	
	friend uint32 GetTypeHash(const FOpenGLShaderVarying &Var)
	{
		uint32 Hash = GetTypeHash(Var.Location);
		Hash ^= FCrc::MemCrc32(Var.Varying.GetData(), Var.Varying.Num() * sizeof(ANSICHAR));
		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLShaderVarying& Var)
{
	Ar << Var.Varying;
	Ar << Var.Location;
	return Ar;
}

/**
 * Shader binding information.
 */
struct FOpenGLShaderBindings
{
	TArray<TArray<CrossCompiler::FPackedArrayInfo>>	PackedUniformBuffers;
	TArray<CrossCompiler::FPackedArrayInfo>			PackedGlobalArrays;
	TArray<FOpenGLShaderVarying>					InputVaryings;
	TArray<FOpenGLShaderVarying>					OutputVaryings;
	FOpenGLShaderResourceTable				ShaderResourceTable;

	uint16	InOutMask;
	uint8	NumSamplers;
	uint8	NumUniformBuffers;
	uint8	NumUAVs;
	bool	bFlattenUB;
	uint8	VertexAttributeRemap[16];
	uint8	VertexRemappedMask;

	FOpenGLShaderBindings() :
		InOutMask(0),
		NumSamplers(0),
		NumUniformBuffers(0),
		NumUAVs(0),
		bFlattenUB(false),
		VertexRemappedMask(0)
	{
		FMemory::Memset(VertexAttributeRemap, 0xFF);
	}

	friend bool operator==( const FOpenGLShaderBindings &A, const FOpenGLShaderBindings& B)
	{
		bool bEqual = true;

		bEqual &= A.InOutMask == B.InOutMask;
		bEqual &= A.NumSamplers == B.NumSamplers;
		bEqual &= A.NumUniformBuffers == B.NumUniformBuffers;
		bEqual &= A.NumUAVs == B.NumUAVs;
		bEqual &= A.bFlattenUB == B.bFlattenUB;
		bEqual &= A.PackedGlobalArrays.Num() == B.PackedGlobalArrays.Num();
		bEqual &= A.PackedUniformBuffers.Num() == B.PackedUniformBuffers.Num();
		bEqual &= A.InputVaryings.Num() == B.InputVaryings.Num();
		bEqual &= A.OutputVaryings.Num() == B.OutputVaryings.Num();
		bEqual &= A.ShaderResourceTable == B.ShaderResourceTable;

		if ( !bEqual )
		{
			return bEqual;
		}

		bEqual &= FMemory::Memcmp(A.PackedGlobalArrays.GetData(),B.PackedGlobalArrays.GetData(),A.PackedGlobalArrays.GetTypeSize()*A.PackedGlobalArrays.Num()) == 0; 

		for (int32 Item = 0; Item < A.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo> &ArrayA = A.PackedUniformBuffers[Item];
			const TArray<CrossCompiler::FPackedArrayInfo> &ArrayB = B.PackedUniformBuffers[Item];

			bEqual &= FMemory::Memcmp(ArrayA.GetData(),ArrayB.GetData(),ArrayA.GetTypeSize()*ArrayA.Num()) == 0;
		}
		
		for (int32 Item = 0; bEqual && Item < A.InputVaryings.Num(); Item++)
		{
			bEqual &= A.InputVaryings[Item] == B.InputVaryings[Item];
		}
		
		for (int32 Item = 0; bEqual && Item < A.OutputVaryings.Num(); Item++)
		{
			bEqual &= A.OutputVaryings[Item] == B.OutputVaryings[Item];
		}

		return bEqual;
	}

	friend uint32 GetTypeHash(const FOpenGLShaderBindings &Binding)
	{
		uint32 Hash = 0;
		Hash = Binding.InOutMask;
		Hash |= Binding.NumSamplers << 16;
		Hash |= Binding.NumUniformBuffers << 24;
		Hash ^= Binding.NumUAVs;
		Hash ^= Binding.bFlattenUB << 8;
		Hash ^= FCrc::MemCrc_DEPRECATED( Binding.PackedGlobalArrays.GetData(), Binding.PackedGlobalArrays.GetTypeSize()*Binding.PackedGlobalArrays.Num());

		//@todo-rco: Do we need to calc Binding.ShaderResourceTable.GetTypeHash()?

		for (int32 Item = 0; Item < Binding.PackedUniformBuffers.Num(); Item++)
		{
			const TArray<CrossCompiler::FPackedArrayInfo> &Array = Binding.PackedUniformBuffers[Item];
			Hash ^= FCrc::MemCrc_DEPRECATED( Array.GetData(), Array.GetTypeSize()* Array.Num());
		}
		
		for (int32 Item = 0; Item < Binding.InputVaryings.Num(); Item++)
		{
			Hash ^= GetTypeHash(Binding.InputVaryings[Item]);
		}
		
		for (int32 Item = 0; Item < Binding.OutputVaryings.Num(); Item++)
		{
			Hash ^= GetTypeHash(Binding.OutputVaryings[Item]);
		}
		return Hash;
	}
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLShaderBindings& Bindings)
{
	Ar << Bindings.PackedUniformBuffers;
	Ar << Bindings.PackedGlobalArrays;
	Ar << Bindings.InputVaryings;
	Ar << Bindings.OutputVaryings;
	Ar << Bindings.ShaderResourceTable;
	Ar << Bindings.InOutMask;
	Ar << Bindings.NumSamplers;
	Ar << Bindings.NumUniformBuffers;
	Ar << Bindings.NumUAVs;
	Ar << Bindings.bFlattenUB;
	for (uint32 i = 0; i < ARRAY_COUNT(Bindings.VertexAttributeRemap); i++)
	{
		Ar << Bindings.VertexAttributeRemap[i];
	}
	Ar << Bindings.VertexRemappedMask;
	return Ar;
}

/**
 * Code header information.
 */
struct FOpenGLCodeHeader
{
	uint32 GlslMarker;
	uint16 FrequencyMarker;
	FOpenGLShaderBindings Bindings;
	FString ShaderName;
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
};

inline FArchive& operator<<(FArchive& Ar, FOpenGLCodeHeader& Header)
{
	Ar << Header.GlslMarker;
	Ar << Header.FrequencyMarker;
	Ar << Header.Bindings;
	Ar << Header.ShaderName;
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
    return Ar;
}

class FOpenGLLinkedProgram;

/**
 * OpenGL shader resource.
 */
template <typename RHIResourceType, GLenum GLTypeEnum, EShaderFrequency FrequencyT>
class TOpenGLShader : public RHIResourceType
{
public:
	enum
	{
		StaticFrequency = FrequencyT
	};
	static const GLenum TypeEnum = GLTypeEnum;

	/** The OpenGL resource ID. */
	GLuint Resource;
	/** true if the shader has compiled successfully. */
	bool bSuccessfullyCompiled;

	/** External bindings for this shader. */
	FOpenGLShaderBindings Bindings;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

#if DEBUG_GL_SHADERS
	TArray<ANSICHAR> GlslCode;
	const ANSICHAR*  GlslCodeString; // make it easier in VS to see shader code in debug mode; points to begin of GlslCode
#endif

	/** Constructor. */
	TOpenGLShader()
		: Resource(0)
		, bSuccessfullyCompiled(false)
	{
		FMemory::Memzero( &Bindings, sizeof(Bindings) );
	}

	/** Destructor. */
	~TOpenGLShader()
	{
//		if (Resource)
//		{
//			glDeleteShader(Resource);
//		}
	}
};


typedef TOpenGLShader<FRHIVertexShader, GL_VERTEX_SHADER, SF_Vertex> FOpenGLVertexShader;
typedef TOpenGLShader<FRHIPixelShader, GL_FRAGMENT_SHADER, SF_Pixel> FOpenGLPixelShader;
typedef TOpenGLShader<FRHIGeometryShader, GL_GEOMETRY_SHADER, SF_Geometry> FOpenGLGeometryShader;
typedef TOpenGLShader<FRHIHullShader, GL_TESS_CONTROL_SHADER, SF_Hull> FOpenGLHullShader;
typedef TOpenGLShader<FRHIDomainShader, GL_TESS_EVALUATION_SHADER, SF_Domain> FOpenGLDomainShader;


class FOpenGLComputeShader : public TOpenGLShader<FRHIComputeShader, GL_COMPUTE_SHADER, SF_Compute>
{
public:
	FOpenGLComputeShader():
		LinkedProgram(0)
	{

	}

	bool NeedsTextureStage(int32 TextureStageIndex);
	int32 MaxTextureStageUsed();
	bool NeedsUAVStage(int32 UAVStageIndex);
	FOpenGLLinkedProgram* LinkedProgram;
};



/**
 * Caching of OpenGL uniform parameters.
 */
class FOpenGLShaderParameterCache
{
public:
	/** Constructor. */
	FOpenGLShaderParameterCache();

	/** Destructor. */
	~FOpenGLShaderParameterCache();

	void InitializeResources(int32 UniformArraySize);

	/**
	 * Marks all uniform arrays as dirty.
	 */
	void MarkAllDirty();

	/**
	 * Sets values directly into the packed uniform array
	 */
	void Set(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValues);

	/**
	 * Commit shader parameters to the currently bound program.
	 * @param ParameterTable - Information on the bound uniform arrays for the program.
	 */
	void CommitPackedGlobals(const FOpenGLLinkedProgram* LinkedProgram, int32 Stage);

	void CommitPackedUniformBuffers(FOpenGLLinkedProgram* LinkedProgram, int32 Stage, FUniformBufferRHIRef* UniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo);

private:
	/** CPU memory block for storing uniform values. */
	uint8* PackedGlobalUniforms[CrossCompiler::PACKED_TYPEINDEX_MAX];
	
	struct FRange
	{
		uint32	StartVector;
		uint32	NumVectors;

		void MarkDirtyRange(uint32 NewStartVector, uint32 NewNumVectors);
	};
	/** Dirty ranges for each uniform array. */
	FRange	PackedGlobalUniformDirty[CrossCompiler::PACKED_TYPEINDEX_MAX];

	/** Scratch CPU memory block for uploading packed uniforms. */
	uint8* PackedUniformsScratch[CrossCompiler::PACKED_TYPEINDEX_MAX];

	/** in bytes */
	int32 GlobalUniformArraySize;
};

struct FOpenGLBindlessSamplerInfo
{
	GLuint Slot;	// Texture unit
	GLuint Handle;	// Sampler slot
};

class FOpenGLLinkedProgramConfiguration
{
public:

	struct ShaderInfo
	{
		FOpenGLShaderBindings Bindings;
		FSHAHash Hash;
		GLuint Resource;

	}
	Shaders[CrossCompiler::NUM_SHADER_STAGES];

	FOpenGLLinkedProgramConfiguration()
	{
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Shaders[Stage].Resource = 0;
		}
	}

	friend bool operator ==(const FOpenGLLinkedProgramConfiguration& A, const FOpenGLLinkedProgramConfiguration& B)
	{
		bool bEqual = true;
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES && bEqual; Stage++)
		{
			bEqual &= A.Shaders[Stage].Resource == B.Shaders[Stage].Resource;
			bEqual &= A.Shaders[Stage].Bindings == B.Shaders[Stage].Bindings;
		}
		return bEqual;
	}

	friend uint32 GetTypeHash(const FOpenGLLinkedProgramConfiguration &Config)
	{
		uint32 Hash = 0;
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Hash ^= GetTypeHash(Config.Shaders[Stage].Bindings);
			Hash ^= Config.Shaders[Stage].Resource;
		}
		return Hash;
	}

	friend FArchive& operator<<(FArchive& Ar, FOpenGLLinkedProgramConfiguration& Config)
	{
		for (int32 Stage = 0; Stage < CrossCompiler::NUM_SHADER_STAGES; Stage++)
		{
			Ar << Config.Shaders[Stage].Bindings << Config.Shaders[Stage].Hash;
		}
		return Ar;
	}
};

class FOpenGLProgramBinaryCache
{
public:
	static void Initialize();
	static void Shutdown();

	static bool IsEnabled();
	
	/** Defer shader compilation until we link a program, so we will have a chance to load cached binary and skip compilation  */
	static bool DeferShaderCompilation(GLuint Shader, const TArray<ANSICHAR>& GlslCode);
	
	/** Compile required shaders for a program, only in case binary program was not found in the cache   */
	static void CompilePendingShaders(const FOpenGLLinkedProgramConfiguration& Config);
	
	/** Try to find and load program binary from cache */
	static bool UseCachedProgram(GLuint Program, const FOpenGLLinkedProgramConfiguration& Config);
	
	/** Store program binary on disk in case ProgramBinaryCache is enabled */
	static void CacheProgram(GLuint Program, const FOpenGLLinkedProgramConfiguration& Config);
	
private:
	FOpenGLProgramBinaryCache(const FString& InCachePath);
	~FOpenGLProgramBinaryCache();

	/** Composes program file name using shader hash values */
	FString GetProgramBinaryFilename(const FOpenGLLinkedProgramConfiguration& Config) const;
	
	/** Loads program binary from disk. */
	bool LoadProgramBinary(const FOpenGLLinkedProgramConfiguration& Config, TArray<uint8>& OutBinary) const;
	
	/** Saves program binary to disk. */
	void SaveProgramBinary(const FOpenGLLinkedProgramConfiguration& Config, const TArray<uint8>& InBinary) const;

	struct FPendingShaderCode
	{
		TArray<ANSICHAR> GlslCode;
		int32 UncompressedSize;
		bool bCompressed;
	};

	static void CompressShader(const TArray<ANSICHAR>& InGlslCode, FPendingShaderCode& OutCompressedShader);
	static void UncompressShader(const FPendingShaderCode& InCompressedShader, TArray<ANSICHAR>& OutGlslCode);

private:
	static TAutoConsoleVariable<int32> CVarUseProgramBinaryCache;
	static FOpenGLProgramBinaryCache* CachePtr;
	
	/**  Path to directory where binary programs will be stored */
	FString CachePath;
	
	/**
	* Shaders that were requested for compilation
	* They will be compiled just before linking a program only in case when there is no saved binary program
	*/
	TMap<GLuint, FPendingShaderCode> ShadersPendingCompilation;
};
